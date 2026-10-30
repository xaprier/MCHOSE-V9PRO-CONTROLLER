#include "MicMonitor.h"

#include "AudioMixer.h"
#include "FilterChainConfig.h"
#include "NoiseSuppressor.h"
#include "PipeWireFilter.h"

#include <QSettings>
#include <QTimer>

namespace {

constexpr int ApplyDebounceMs = 60;

double loopbackGain(int level)
{
    // Linear amplitude: 50 % is -6 dB. A squared curve left the useful range quiet.
    return qBound(0, level, 100) / 100.0;
}

} // namespace

MicMonitor::MicMonitor(AudioMixer *mixer, NoiseSuppressor *noiseSuppressor, QObject *parent)
    : QObject(parent)
    , m_mixer(mixer)
    , m_noiseSuppressor(noiseSuppressor)
    , m_loopback(new PipeWireFilter(QStringLiteral("mic-monitor"), QStringLiteral("v9pro_mic_monitor_input"), this))
    , m_applyDebounce(new QTimer(this))
{
    QSettings settings;
    m_enabled = settings.value(QStringLiteral("monitor/enabled"), false).toBool();
    m_level = settings.value(QStringLiteral("monitor/level"), 50).toInt();

    m_applyDebounce->setSingleShot(true);
    m_applyDebounce->setInterval(ApplyDebounceMs);
    connect(m_applyDebounce, &QTimer::timeout, this, &MicMonitor::apply);

    connect(m_loopback, &PipeWireFilter::runningChanged, this, &MicMonitor::activeChanged);
    connect(m_loopback, &PipeWireFilter::errorOccurred, this, &MicMonitor::setError);
    auto endpointsChanged = [this] {
        if (monitoredSource() != m_loopbackSource || m_mixer->sinkName() != m_loopbackSink)
            m_applyDebounce->start();
    };
    connect(m_mixer, &AudioMixer::outputChanged, this, endpointsChanged);
    connect(m_mixer, &AudioMixer::micChanged, this, endpointsChanged);
    connect(m_noiseSuppressor, &NoiseSuppressor::activeChanged, this, endpointsChanged);
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
    if (m_enabled && m_loopback->isRunning())
        m_loopback->setParams(v9::pw::micMonitorParams(loopbackGain(m_level)));
    else
        m_applyDebounce->start();
}

bool MicMonitor::available() const
{
    return PipeWireFilter::isAvailable();
}

bool MicMonitor::active() const
{
    if (!m_enabled)
        return false;
    return m_loopback->isRunning();
}

void MicMonitor::apply()
{
    setError({});
    const QString source = monitoredSource();
    const QString sink = m_mixer->sinkName();
    if (m_enabled && !source.isEmpty() && !sink.isEmpty()) {
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

QString MicMonitor::monitoredSource() const
{
    if (m_mixer->sourceName().isEmpty())
        return {};
    return m_noiseSuppressor->active() ? v9::pw::NoiseSourceName : m_mixer->sourceName();
}

void MicMonitor::save() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("monitor/enabled"), m_enabled);
    settings.setValue(QStringLiteral("monitor/level"), m_level);
}

void MicMonitor::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}
