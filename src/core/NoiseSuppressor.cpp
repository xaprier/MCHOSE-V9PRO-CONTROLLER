#include "NoiseSuppressor.h"

#include "AudioMixer.h"
#include "PipeWireFilter.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>

namespace pw = v9::pw;

NoiseSuppressor::NoiseSuppressor(AudioMixer *mixer, QObject *parent)
    : QObject(parent)
    , m_mixer(mixer)
    , m_rnnoisePath(findRnnoisePlugin())
{
    QSettings settings;
    m_enabled = settings.value(QStringLiteral("noise/enabled"), false).toBool();
    m_engine = settings.value(QStringLiteral("noise/engine"), int(WebRtc)).toInt() == RNNoise ? RNNoise : WebRtc;
    m_vadThreshold = settings.value(QStringLiteral("noise/vadThreshold"), 50).toInt();
    if (m_engine == RNNoise && !rnnoiseAvailable())
        m_engine = WebRtc;

    connect(m_mixer, &AudioMixer::micChanged, this, [this] {
        if (m_enabled && m_mixer->sourceName() != m_targetSource)
            m_mixer->sourceName().isEmpty() ? stop() : start();
    });
    if (m_enabled && !m_mixer->sourceName().isEmpty())
        start();
}

NoiseSuppressor::~NoiseSuppressor()
{
    if (active() && !m_mixer->sourceName().isEmpty()) {
        if (m_mixer->defaultSource() == pw::NoiseSourceName)
            QProcess::execute(QStringLiteral("pactl"), {QStringLiteral("set-default-source"), m_mixer->sourceName()});
        AudioMixer::moveStreams(AudioMixer::StreamKind::Capture, pw::NoiseSourceName, m_mixer->sourceName());
    }
}

void NoiseSuppressor::handOver()
{
    if (!active() || m_mixer->sourceName().isEmpty())
        return;
    if (m_mixer->defaultSource() == pw::NoiseSourceName)
        QProcess::execute(QStringLiteral("pactl"), {QStringLiteral("set-default-source"), m_mixer->sourceName()});
    AudioMixer::moveStreams(AudioMixer::StreamKind::Capture, pw::NoiseSourceName, m_mixer->sourceName());
}

QString NoiseSuppressor::findRnnoisePlugin()
{
    QStringList dirs = qEnvironmentVariable("LADSPA_PATH").split(QLatin1Char(':'), Qt::SkipEmptyParts);
    dirs << QStringLiteral("/usr/lib/ladspa") << QStringLiteral("/usr/lib64/ladspa") << QStringLiteral("/usr/local/lib/ladspa")
         << QStringLiteral("/usr/lib/x86_64-linux-gnu/ladspa") << QDir::homePath() + QStringLiteral("/.ladspa");
    for (const QString &dir : dirs) {
        const QString path = dir + QStringLiteral("/librnnoise_ladspa.so");
        if (QFileInfo::exists(path))
            return path;
    }
    return {};
}

bool NoiseSuppressor::available() const
{
    return PipeWireFilter::isAvailable();
}

bool NoiseSuppressor::active() const
{
    return m_filter && m_filter->isRunning();
}

void NoiseSuppressor::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    save();
    emit enabledChanged();
    if (!enabled)
        stop();
    else if (m_mixer->sourceName().isEmpty())
        setError(tr("Headset microphone not found; noise suppression starts when it appears."));
    else
        start();
}

void NoiseSuppressor::setEngine(Engine engine)
{
    if (engine == RNNoise && !rnnoiseAvailable())
        engine = WebRtc;
    if (m_engine == engine)
        return;
    m_engine = engine;
    save();
    emit engineChanged();
    if (active())
        start();
}

void NoiseSuppressor::setVadThreshold(int threshold)
{
    threshold = qBound(0, threshold, 99);
    if (m_vadThreshold == threshold)
        return;
    m_vadThreshold = threshold;
    save();
    emit vadThresholdChanged();
    if (active() && m_engine == RNNoise)
        m_filter->setParams(pw::noiseSuppressionParams(m_vadThreshold));
}

void NoiseSuppressor::start()
{
    stop();
    setError({});
    m_targetSource = m_mixer->sourceName();
    // The filter-chain graph exposes its controls on the capture node; echo-cancel has none.
    m_filter = new PipeWireFilter(QStringLiteral("noise-suppression"),
                                  m_engine == RNNoise ? QStringLiteral("v9pro_mic_denoise_input") : QString(), this);
    connect(m_filter, &PipeWireFilter::runningChanged, this, &NoiseSuppressor::activeChanged);
    connect(m_filter, &PipeWireFilter::errorOccurred, this, &NoiseSuppressor::setError);
    connect(m_filter, &PipeWireFilter::ready, this, [this] {
        m_mixer->setDefaultSource(pw::NoiseSourceName);
        AudioMixer::moveStreams(AudioMixer::StreamKind::Capture, m_targetSource, pw::NoiseSourceName);
    });
    m_filter->start(pw::noiseSuppressionConfig(static_cast<pw::NoiseEngine>(m_engine), m_targetSource, m_vadThreshold));
}

void NoiseSuppressor::stop()
{
    if (!m_filter)
        return;
    if (!m_mixer->sourceName().isEmpty()) {
        if (m_mixer->defaultSource() == pw::NoiseSourceName)
            m_mixer->setDefaultSource(m_mixer->sourceName());
        AudioMixer::moveStreams(AudioMixer::StreamKind::Capture, pw::NoiseSourceName, m_mixer->sourceName());
    }
    m_filter->stop();
    m_filter->deleteLater();
    m_filter = nullptr;
    m_targetSource.clear();
    emit activeChanged();
}

void NoiseSuppressor::save() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("noise/enabled"), m_enabled);
    settings.setValue(QStringLiteral("noise/engine"), int(m_engine));
    settings.setValue(QStringLiteral("noise/vadThreshold"), m_vadThreshold);
}

void NoiseSuppressor::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}
