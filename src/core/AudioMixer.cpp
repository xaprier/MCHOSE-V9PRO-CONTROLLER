#include "AudioMixer.h"

#include "FilterChainConfig.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

Q_LOGGING_CATEGORY(lcAudio, "v9.audio")

namespace {

// pipewire-pulse names the nodes after the USB product string.
const QString HeadsetNodeTag = QStringLiteral("MCHOSE_V9_PRO");

constexpr int RefreshDebounceMs = 120;
constexpr int WriteDebounceMs = 40;

int averagePercent(const QJsonObject &volume)
{
    int sum = 0;
    int count = 0;
    for (auto it = volume.begin(); it != volume.end(); ++it) {
        QString text = it.value().toObject().value(QStringLiteral("value_percent")).toString();
        text.remove(QLatin1Char('%'));
        bool ok = false;
        const int value = text.trimmed().toInt(&ok);
        if (ok) {
            sum += value;
            ++count;
        }
    }
    return count ? (sum + count / 2) / count : 0;
}

QByteArray pactlSync(const QStringList &arguments)
{
    QProcess process;
    process.start(QStringLiteral("pactl"), arguments);
    if (!process.waitForFinished(2000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return {};
    return process.readAllStandardOutput();
}

} // namespace

AudioMixer::AudioMixer(QObject *parent)
    : QObject(parent)
    , m_pactl(QStandardPaths::findExecutable(QStringLiteral("pactl")))
    , m_refreshDebounce(new QTimer(this))
    , m_writeDebounce(new QTimer(this))
{
    m_refreshDebounce->setSingleShot(true);
    m_refreshDebounce->setInterval(RefreshDebounceMs);
    connect(m_refreshDebounce, &QTimer::timeout, this, &AudioMixer::refresh);

    m_writeDebounce->setSingleShot(true);
    m_writeDebounce->setInterval(WriteDebounceMs);
    connect(m_writeDebounce, &QTimer::timeout, this, &AudioMixer::flushPending);

    if (m_pactl.isEmpty()) {
        setLastError(tr("pactl not found. Install pipewire-pulse / libpulse."));
        return;
    }
    startSubscription();
    refresh();
}

AudioMixer::~AudioMixer()
{
    if (m_subscription) {
        m_subscription->disconnect(this);
        m_subscription->kill();
        m_subscription->waitForFinished(500);
    }
}

std::optional<AudioMixer::Endpoint> AudioMixer::findHeadsetEndpoint(const QByteArray &pactlJson)
{
    const QJsonArray nodes = QJsonDocument::fromJson(pactlJson).array();
    for (const QJsonValue &value : nodes) {
        const QJsonObject node = value.toObject();
        const QString name = node.value(QStringLiteral("name")).toString();
        if (!name.contains(HeadsetNodeTag) || name.endsWith(QStringLiteral(".monitor")))
            continue;

        Endpoint endpoint;
        endpoint.name = name;
        endpoint.volume = averagePercent(node.value(QStringLiteral("volume")).toObject());
        endpoint.muted = node.value(QStringLiteral("mute")).toBool();
        return endpoint;
    }
    return std::nullopt;
}

void AudioMixer::moveStreams(StreamKind kind, const QString &from, const QString &to)
{
    if (from.isEmpty() || to.isEmpty() || from == to)
        return;
    const bool playback = kind == StreamKind::Playback;
    const QString devices = playback ? QStringLiteral("sinks") : QStringLiteral("sources");
    qint64 fromIndex = -1;
    const QJsonArray nodes = QJsonDocument::fromJson(pactlSync({QStringLiteral("-f"), QStringLiteral("json"), QStringLiteral("list"), devices})).array();
    for (const QJsonValue &node : nodes) {
        if (node[QStringLiteral("name")].toString() == from)
            fromIndex = node[QStringLiteral("index")].toInteger(-1);
    }
    if (fromIndex < 0)
        return;

    const QString streams = playback ? QStringLiteral("sink-inputs") : QStringLiteral("source-outputs");
    const QString deviceKey = playback ? QStringLiteral("sink") : QStringLiteral("source");
    const QString move = playback ? QStringLiteral("move-sink-input") : QStringLiteral("move-source-output");
    const QJsonArray list = QJsonDocument::fromJson(pactlSync({QStringLiteral("-f"), QStringLiteral("json"), QStringLiteral("list"), streams})).array();
    for (const QJsonValue &stream : list) {
        if (stream[deviceKey].toInteger(-2) != fromIndex)
            continue;
        if (stream[QStringLiteral("properties")][QStringLiteral("node.name")].toString().startsWith(QStringLiteral("v9pro_")))
            continue;
        const QString index = QString::number(stream[QStringLiteral("index")].toInteger());
        pactlSync({move, index, to});
        qCInfo(lcAudio) << "moved" << streams << index << "to" << to;
    }
}

void AudioMixer::refresh()
{
    if (m_pactl.isEmpty())
        return;

    run(m_pactl, {QStringLiteral("get-default-sink")}, [this](const QByteArray &out) {
        const QString sink = QString::fromUtf8(out).trimmed();
        if (sink != m_defaultSink) {
            m_defaultSink = sink;
            emit defaultsChanged();
        }
        const bool isDefault = isHeadsetDefault();
        if (isDefault != m_outputIsDefault) {
            m_outputIsDefault = isDefault;
            emit outputChanged();
        }
    });
    run(m_pactl, {QStringLiteral("get-default-source")}, [this](const QByteArray &out) {
        const QString source = QString::fromUtf8(out).trimmed();
        if (source != m_defaultSource) {
            m_defaultSource = source;
            emit defaultsChanged();
        }
    });

    run(m_pactl, {QStringLiteral("-f"), QStringLiteral("json"), QStringLiteral("list"), QStringLiteral("sinks")},
        [this](const QByteArray &out) {
            const auto sink = findHeadsetEndpoint(out);
            m_sinkName = sink ? sink->name : QString();
            if (sink && m_pendingOutputVolume < 0) {
                m_outputVolume = sink->volume;
                m_outputMuted = sink->muted;
            }
            m_outputIsDefault = isHeadsetDefault();
            emit outputChanged();
        });

    run(m_pactl, {QStringLiteral("-f"), QStringLiteral("json"), QStringLiteral("list"), QStringLiteral("sources")},
        [this](const QByteArray &out) {
            const auto source = findHeadsetEndpoint(out);
            m_sourceName = source ? source->name : QString();
            if (source && m_pendingMicVolume < 0) {
                m_micVolume = source->volume;
                m_micMuted = source->muted;
            }
            emit micChanged();
        });
}

void AudioMixer::makeDefaultOutput()
{
    if (m_sinkName.isEmpty())
        return;
    run(m_pactl, {QStringLiteral("set-default-sink"), m_sinkName});
    if (!m_sourceName.isEmpty())
        run(m_pactl, {QStringLiteral("set-default-source"), m_sourceName});
}

bool AudioMixer::isHeadsetDefault() const
{
    // Routing through the equalizer still ends in the headset.
    return !m_sinkName.isEmpty() && (m_defaultSink == m_sinkName || m_defaultSink == v9::pw::EqualizerSinkName);
}

void AudioMixer::setDefaultSink(const QString &name)
{
    if (name.isEmpty() || m_pactl.isEmpty())
        return;
    run(m_pactl, {QStringLiteral("set-default-sink"), name});
}

void AudioMixer::setDefaultSource(const QString &name)
{
    if (name.isEmpty() || m_pactl.isEmpty())
        return;
    run(m_pactl, {QStringLiteral("set-default-source"), name});
}

void AudioMixer::setOutputVolume(int percent)
{
    percent = qBound(0, percent, 150);
    if (m_sinkName.isEmpty() || percent == m_outputVolume)
        return;
    m_outputVolume = percent;
    m_pendingOutputVolume = percent;
    m_writeDebounce->start();
    emit outputChanged();
}

void AudioMixer::setOutputMuted(bool muted)
{
    if (m_sinkName.isEmpty() || muted == m_outputMuted)
        return;
    m_outputMuted = muted;
    run(m_pactl, {QStringLiteral("set-sink-mute"), m_sinkName, muted ? QStringLiteral("1") : QStringLiteral("0")});
    emit outputChanged();
}

void AudioMixer::setMicVolume(int percent)
{
    percent = qBound(0, percent, 150);
    if (m_sourceName.isEmpty() || percent == m_micVolume)
        return;
    m_micVolume = percent;
    m_pendingMicVolume = percent;
    m_writeDebounce->start();
    emit micChanged();
}

void AudioMixer::setMicMuted(bool muted)
{
    if (m_sourceName.isEmpty() || muted == m_micMuted)
        return;
    m_micMuted = muted;
    run(m_pactl, {QStringLiteral("set-source-mute"), m_sourceName, muted ? QStringLiteral("1") : QStringLiteral("0")});
    emit micChanged();
}

void AudioMixer::flushPending()
{
    // Sliders emit far faster than pactl can be spawned; only the latest value is written.
    if (m_pendingOutputVolume >= 0) {
        run(m_pactl, {QStringLiteral("set-sink-volume"), m_sinkName, QString::number(m_pendingOutputVolume) + QLatin1Char('%')});
        m_pendingOutputVolume = -1;
    }
    if (m_pendingMicVolume >= 0) {
        run(m_pactl, {QStringLiteral("set-source-volume"), m_sourceName, QString::number(m_pendingMicVolume) + QLatin1Char('%')});
        m_pendingMicVolume = -1;
    }
}

void AudioMixer::startSubscription()
{
    m_subscription = new QProcess(this);
    m_subscription->setProgram(m_pactl);
    m_subscription->setArguments({QStringLiteral("subscribe")});
    connect(m_subscription, &QProcess::readyReadStandardOutput, this, [this] {
        const QByteArray lines = m_subscription->readAllStandardOutput();
        if (lines.contains("sink") || lines.contains("source") || lines.contains("server"))
            m_refreshDebounce->start();
    });
    connect(m_subscription, &QProcess::finished, this, [this] {
        qCWarning(lcAudio) << "pactl subscribe exited, restarting";
        QTimer::singleShot(2000, m_subscription, [this] { m_subscription->start(); });
    });
    m_subscription->start();
}

void AudioMixer::run(const QString &program, const QStringList &arguments, std::function<void(const QByteArray &)> onOutput)
{
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process, program, arguments, onOutput](int exitCode, QProcess::ExitStatus status) {
        process->deleteLater();
        if (status != QProcess::NormalExit || exitCode != 0) {
            const QString error = QString::fromUtf8(process->readAllStandardError()).trimmed();
            qCWarning(lcAudio) << program << arguments << "failed:" << error;
            setLastError(error);
            return;
        }
        if (onOutput)
            onOutput(process->readAllStandardOutput());
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, program](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            setLastError(tr("Failed to start %1").arg(program));
            process->deleteLater();
        }
    });
    process->start(program, arguments);
}

void AudioMixer::setLastError(const QString &error)
{
    if (m_lastError == error)
        return;
    m_lastError = error;
    emit lastErrorChanged();
}
