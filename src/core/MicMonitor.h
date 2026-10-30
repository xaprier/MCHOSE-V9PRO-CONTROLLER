#pragma once

#include <QObject>
#include <QString>

class AudioMixer;
class NoiseSuppressor;
class PipeWireFilter;
class QTimer;

// Hear your own microphone in the headset through a PipeWire loopback (~5-10 ms latency). Listens
// to the noise-suppressed source while suppression is active, so you hear what others hear. The
// dongle has no working hardware monitor: ALSA 'PCM',1 cuts the headset output.
class MicMonitor : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int level READ level WRITE setLevel NOTIFY levelChanged)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit MicMonitor(AudioMixer *mixer, NoiseSuppressor *noiseSuppressor, QObject *parent = nullptr);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    int level() const { return m_level; }
    void setLevel(int level);
    bool available() const;
    bool active() const;
    QString error() const { return m_error; }

signals:
    void enabledChanged();
    void levelChanged();
    void activeChanged();
    void errorChanged();

private:
    void apply();
    QString monitoredSource() const;
    void save() const;
    void setError(const QString &error);

    AudioMixer *m_mixer;
    NoiseSuppressor *m_noiseSuppressor;
    PipeWireFilter *m_loopback;
    QTimer *m_applyDebounce;
    bool m_enabled = false;
    int m_level = 50;
    QString m_loopbackSource;
    QString m_loopbackSink;
    QString m_error;
};
