#include "PipeWireFilter.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include <utility>

Q_LOGGING_CATEGORY(lcPipeWire, "v9.pipewire")

namespace {

constexpr int LookupIntervalMs = 150;
constexpr int LookupAttempts = 40;

// A standalone `pipewire -c` process is a client of the session daemon and needs these modules
// to connect and to host the filter's streams (mirrors /usr/share/pipewire/filter-chain.conf).
QString standalone(const QString &modules)
{
    static const QString preamble = QStringLiteral(R"CFG(context.properties = {
  log.level = 0
}
context.spa-libs = {
  audio.convert.* = audioconvert/libspa-audioconvert
  support.* = support/libspa-support
}
context.modules = [
  { name = libpipewire-module-rt flags = [ ifexists nofail ] }
  { name = libpipewire-module-protocol-native }
  { name = libpipewire-module-client-node }
  { name = libpipewire-module-adapter }
)CFG");
    QString text = modules;
    return text.replace(QStringLiteral("context.modules = ["), preamble);
}

QString runtimeDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (base.isEmpty())
        base = QDir::tempPath();
    return base + QStringLiteral("/v9pro-control");
}

} // namespace

PipeWireFilter::PipeWireFilter(QString id, QString controlNodeName, QObject *parent)
    : QObject(parent)
    , m_id(std::move(id))
    , m_controlNodeName(std::move(controlNodeName))
    , m_lookupTimer(new QTimer(this))
{
    m_lookupTimer->setInterval(LookupIntervalMs);
    connect(m_lookupTimer, &QTimer::timeout, this, &PipeWireFilter::lookupNode);
}

PipeWireFilter::~PipeWireFilter()
{
    stop();
}

bool PipeWireFilter::isAvailable()
{
    return !QStandardPaths::findExecutable(QStringLiteral("pipewire")).isEmpty()
        && !QStandardPaths::findExecutable(QStringLiteral("pw-cli")).isEmpty()
        && !QStandardPaths::findExecutable(QStringLiteral("pw-dump")).isEmpty();
}

bool PipeWireFilter::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

void PipeWireFilter::start(const QString &config)
{
    stop();

    QDir().mkpath(runtimeDir());
    m_configPath = runtimeDir() + QLatin1Char('/') + m_id + QStringLiteral(".conf");
    QFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_error = tr("Cannot write %1").arg(m_configPath);
        emit errorOccurred(m_error);
        return;
    }
    file.write(standalone(config).toUtf8());
    file.close();

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::readyRead, this, [this] {
        const QByteArray output = m_process->readAll().trimmed();
        if (!output.isEmpty())
            qCDebug(lcPipeWire) << m_id << output;
        if (output.contains("error") || output.contains("can't load"))
            m_error = QString::fromUtf8(output);
    });
    connect(m_process, &QProcess::finished, this, [this](int exitCode) {
        m_lookupTimer->stop();
        m_nodeId = -1;
        if (exitCode != 0 && m_error.isEmpty())
            m_error = tr("pipewire exited with code %1").arg(exitCode);
        if (!m_error.isEmpty())
            emit errorOccurred(m_error);
        emit runningChanged();
    });
    m_error.clear();
    m_process->start(QStringLiteral("pipewire"), {QStringLiteral("-c"), m_configPath});
    qCInfo(lcPipeWire) << "started" << m_id;
    emit runningChanged();

    m_lookupAttempts = 0;
    if (m_controlNodeName.isEmpty())
        QTimer::singleShot(LookupIntervalMs, this, &PipeWireFilter::ready);
    else
        m_lookupTimer->start();
}

void PipeWireFilter::stop()
{
    m_lookupTimer->stop();
    m_nodeId = -1;
    m_pendingParams.clear();
    if (!m_process)
        return;

    QProcess *process = m_process;
    m_process = nullptr;
    process->disconnect(this);
    process->terminate();
    if (!process->waitForFinished(1000))
        process->kill();
    process->deleteLater();
    qCInfo(lcPipeWire) << "stopped" << m_id;
    emit runningChanged();
}

void PipeWireFilter::setParams(const QString &props)
{
    m_pendingParams = props;
    applyPending();
}

void PipeWireFilter::lookupNode()
{
    if (!isRunning() || ++m_lookupAttempts > LookupAttempts) {
        m_lookupTimer->stop();
        if (isRunning())
            emit errorOccurred(tr("Node %1 did not appear").arg(m_controlNodeName));
        return;
    }

    // One-shot pw-dump: small and avoids keeping a registry connection in this process.
    auto *dump = new QProcess(this);
    connect(dump, &QProcess::finished, this, [this, dump] {
        dump->deleteLater();
        const QJsonArray objects = QJsonDocument::fromJson(dump->readAllStandardOutput()).array();
        for (const QJsonValue &value : objects) {
            const QJsonObject object = value.toObject();
            const QJsonObject props = object.value(QStringLiteral("info")).toObject().value(QStringLiteral("props")).toObject();
            if (props.value(QStringLiteral("node.name")).toString() != m_controlNodeName)
                continue;
            m_nodeId = object.value(QStringLiteral("id")).toInt(-1);
            if (m_nodeId >= 0 && m_lookupTimer->isActive()) {
                m_lookupTimer->stop();
                qCInfo(lcPipeWire) << m_id << "control node" << m_nodeId;
                emit ready();
                applyPending();
            }
            return;
        }
    });
    dump->start(QStringLiteral("pw-dump"), {QStringLiteral("Node")});
}

void PipeWireFilter::applyPending()
{
    if (m_applying || m_nodeId < 0 || m_pendingParams.isEmpty())
        return;

    m_applying = true;
    const QString params = std::exchange(m_pendingParams, QString());
    auto *cli = new QProcess(this);
    connect(cli, &QProcess::finished, this, [this, cli](int exitCode) {
        cli->deleteLater();
        if (exitCode != 0)
            qCWarning(lcPipeWire) << m_id << "set-param failed" << cli->readAll();
        m_applying = false;
        applyPending(); // a slider may have moved while this one ran
    });
    cli->start(QStringLiteral("pw-cli"),
               {QStringLiteral("set-param"), QString::number(m_nodeId), QStringLiteral("Props"), params});
}
