#include "MicMonitor.h"

#include "AudioMixer.h"
#include "FilterChainConfig.h"
#include "PipeWireFilter.h"

#include <QSettings>
#include <QTimer>

namespace {

constexpr int ApplyDebounceMs = 60;

double loopbackGain(int level)
{
    // Perceived loudness is closer to quadratic; 100 % gives unity gain.
    const double x = qBound(0, level, 100) / 100.0;
    return x * x;
}

} // namespace

MicMonitor::MicMonitor(AudioMixer *mixer, QObject *parent)
    : QObject(parent)
    , m_mixer(mixer)
    , m_loopback(new PipeWireFilter(QStringLiteral("mic-monitor"), QStringLiteral("v9pro_mic_monitor_input"), this))
    , m_applyDebounce(new QTimer(this))
{
    QSettings settings;
    m_enabled = settings.value(QStringLiteral("monitor/enabled"), false).toBool();
    m_level = settings.value(QStringLiteral("monitor/level"), 50).toInt();
    m_mode = settings.value(QStringLiteral("monitor/mode"), int(Software)).toInt() == Hardware ? Hardware : Software;

    m_applyDebounce->setSingleShot(true);
    m_applyDebounce->setInterval(ApplyDebounceMs);
    connect(m_applyDebounce, &QTimer::timeout, this, &MicMonitor::apply);

    connect(m_loopback, &PipeWireFilter::runningChanged, this, &MicMonitor::activeChanged);
    connect(m_loopback, &PipeWireFilter::errorOccurred, this, &MicMonitor::setError);
    connect(m_mixer, &AudioMixer::sidetoneChanged, this, [this] {
        if (hardwareAvailable() != m_hardwareWasAvailable) {
            m_hardwareWasAvailable = hardwareAvailable();
            emit availabilityChanged();
            m_applyDebounce->start();
        }
    });
    auto endpointsChanged = [this] {
        if (m_mixer->sourceName() != m_loopbackSource || m_mixer->sinkName() != m_loopbackSink)
            m_applyDebounce->start();
    };
    connect(m_mixer, &AudioMixer::outputChanged, this, endpointsChanged);
    connect(m_mixer, &AudioMixer::micChanged, this, endpointsChanged);
}

void MicMonitor::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    save();
    emit enabledChanged();
    apply();
}

void MicMonitor::setLevel(int level)
{
    level = qBound(0, level, 100);
    if (m_level == level)
        return;
    m_level = level;
    save();
    emit levelChanged();
    if (m_enabled && m_mode == Software && m_loopback->isRunning())
        m_loopback->setParams(v9::pw::micMonitorParams(loopbackGain(m_level)));
    else
        m_applyDebounce->start();
}

void MicMonitor::setMode(Mode mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;
    save();
    emit modeChanged();
    apply();
}

bool MicMonitor::hardwareAvailable() const
{
    return m_mixer->sidetoneAvailable();
}

bool MicMonitor::softwareAvailable() const
{
    return PipeWireFilter::isAvailable();
}

bool MicMonitor::active() const
{
    if (!m_enabled)
        return false;
    return m_mode == Software ? m_loopback->isRunning() : hardwareAvailable();
}

void MicMonitor::apply()
{
    setError({});
    const bool hardware = m_enabled && m_mode == Hardware;
    const bool software = m_enabled && m_mode == Software;

    // The hardware control is driven to 0 whenever it is not the active path, so the two never add up.
    if (hardwareAvailable())
        m_mixer->setSidetoneLevel(hardware ? m_level : 0);
    else if (hardware)
        setError(tr("Hardware monitor control not found; use the software monitor."));

    const QString source = m_mixer->sourceName();
    const QString sink = m_mixer->sinkName();
    if (software && !source.isEmpty() && !sink.isEmpty()) {
        if (!m_loopback->isRunning() || source != m_loopbackSource || sink != m_loopbackSink) {
            m_loopbackSource = source;
            m_loopbackSink = sink;
            m_loopback->start(v9::pw::micMonitorConfig(source, sink, loopbackGain(m_level)));
        } else {
            m_loopback->setParams(v9::pw::micMonitorParams(loopbackGain(m_level)));
        }
    } else {
        m_loopback->stop();
        m_loopbackSource.clear();
        m_loopbackSink.clear();
    }
    emit activeChanged();
}

void MicMonitor::save() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("monitor/enabled"), m_enabled);
    settings.setValue(QStringLiteral("monitor/level"), m_level);
    settings.setValue(QStringLiteral("monitor/mode"), int(m_mode));
}

void MicMonitor::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}
