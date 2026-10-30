#pragma once

#include "FilterChainConfig.h"

#include <QObject>
#include <QString>

class AudioMixer;
class PipeWireFilter;

// Microphone noise suppression as a virtual source ("V9 PRO Microphone (noise suppressed)")
// fed by the headset mic. Replaces Hub's AI_NR / NR_Enable, which are Windows APO effects.
class NoiseSuppressor : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(Engine engine READ engine WRITE setEngine NOTIFY engineChanged)
    Q_PROPERTY(bool rnnoiseAvailable READ rnnoiseAvailable CONSTANT)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(int vadThreshold READ vadThreshold WRITE setVadThreshold NOTIFY vadThresholdChanged)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    enum Engine {
        WebRtc = int(v9::pw::NoiseEngine::WebRtc),
        RNNoise = int(v9::pw::NoiseEngine::RNNoise),
    };
    Q_ENUM(Engine)

    explicit NoiseSuppressor(AudioMixer *mixer, QObject *parent = nullptr);
    ~NoiseSuppressor() override;

    static QString findRnnoisePlugin();

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    Engine engine() const { return m_engine; }
    void setEngine(Engine engine);
    bool rnnoiseAvailable() const { return !m_rnnoisePath.isEmpty(); }
    bool available() const;
    int vadThreshold() const { return m_vadThreshold; }
    void setVadThreshold(int threshold);
    // Same as EqualizerEngine::handOver(), for the capture side.
    void handOver();
    bool active() const;
    QString error() const { return m_error; }

signals:
    void enabledChanged();
    void engineChanged();
    void vadThresholdChanged();
    void activeChanged();
    void errorChanged();

private:
    void start();
    void stop();
    void save() const;
    void setError(const QString &error);

    AudioMixer *m_mixer;
    PipeWireFilter *m_filter = nullptr;
    QString m_rnnoisePath;
    QString m_targetSource;
    bool m_enabled = false;
    Engine m_engine = WebRtc;
    int m_vadThreshold = 50;
    QString m_error;
};
