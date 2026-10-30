#include "AudioMixer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

Q_LOGGING_CATEGORY(lcAudio, "v9.audio")

namespace {

// pipewire-pulse names the nodes after the USB product string.
const QString HeadsetNodeTag = QStringLiteral("MCHOSE_V9_PRO");

// ALSA 'PCM',1 on the V9 PRO dongle is a mono playback volume next to the stereo master; on
// C-Media parts that is the mic-to-headphone monitor. Unverified, see docs/ANALYSIS.md §3.
const QString SidetoneControl = QStringLiteral("PCM,1");

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

} // namespace

AudioMixer::AudioMixer(QObject *parent)
    : QObject(parent)
    , m_pactl(QStandardPaths::findExecutable(QStringLiteral("pactl")))
    , m_amixer(QStandardPaths::findExecutable(QStringLiteral("amixer")))
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
        const QJsonObject properties = node.value(QStringLiteral("properties")).toObject();
        endpoint.alsaCard = properties.value(QStringLiteral("alsa.card")).toString().toInt();
        if (!properties.contains(QStringLiteral("alsa.card")))
            endpoint.alsaCard = -1;
        return endpoint;
    }
    return std::nullopt;
}

void AudioMixer::refresh()
{
    if (m_pactl.isEmpty())
        return;

    run(m_pactl, {QStringLiteral("get-default-sink")}, [this](const QByteArray &out) {
        m_defaultSink = QString::fromUtf8(out).trimmed();
        const bool isDefault = !m_sinkName.isEmpty() && m_defaultSink == m_sinkName;
        if (isDefault != m_outputIsDefault) {
            m_outputIsDefault = isDefault;
            emit outputChanged();
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
            m_outputIsDefault = sink && m_defaultSink == sink->name;
            const int card = sink ? sink->alsaCard : -1;
            if (card != m_alsaCard) {
                m_alsaCard = card;
                refreshSidetone();
            }
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

void AudioMixer::setSidetoneLevel(int percent)
{
    percent = qBound(0, percent, 100);
    if (!sidetoneAvailable() || percent == m_sidetoneLevel)
        return;
    m_sidetoneLevel = percent;
    m_pendingSidetone = percent;
    m_writeDebounce->start();
    emit sidetoneChanged();
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
    if (m_pendingSidetone >= 0 && !m_amixer.isEmpty()) {
        run(m_amixer, {QStringLiteral("-q"), QStringLiteral("-c"), QString::number(m_alsaCard), QStringLiteral("sset"),
                       SidetoneControl, QString::number(m_pendingSidetone) + QLatin1Char('%')});
        m_pendingSidetone = -1;
    }
}

void AudioMixer::refreshSidetone()
{
    if (m_alsaCard < 0 || m_amixer.isEmpty()) {
        m_sidetoneLevel = -1;
        emit sidetoneChanged();
        return;
    }
    run(m_amixer, {QStringLiteral("-c"), QString::number(m_alsaCard), QStringLiteral("sget"), SidetoneControl},
        [this](const QByteArray &out) {
            static const QRegularExpression percent(QStringLiteral("\\[(\\d+)%\\]"));
            const auto match = percent.match(QString::fromUtf8(out));
            m_sidetoneLevel = match.hasMatch() ? match.captured(1).toInt() : -1;
            emit sidetoneChanged();
        });
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
