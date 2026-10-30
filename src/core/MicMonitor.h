#pragma once

#include <QObject>
#include <QString>

class AudioMixer;
class NoiseSuppressor;
class PipeWireFilter;
class QTimer;

// Hear your own microphone in the headset. Two ways:
//  - Hardware: the dongle's secondary ALSA playback control ('PCM',1), believed to be the
//    C-Media hardware mic monitor. Zero latency; unverified (docs/ANALYSIS.md §3).
//  - Software: a PipeWire loopback from the headset mic into the headset sink. Always works,
//    ~5-10 ms latency. Listens to the noise-suppressed source while suppression is active, so
//    you hear what others hear.
class MicMonitor : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int level READ level WRITE setLevel NOTIFY levelChanged)
    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(bool hardwareAvailable READ hardwareAvailable NOTIFY availabilityChanged)
    Q_PROPERTY(bool softwareAvailable READ softwareAvailable CONSTANT)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    enum Mode {
        Hardware,
        Software,
    };
    Q_ENUM(Mode)

    explicit MicMonitor(AudioMixer *mixer, NoiseSuppressor *noiseSuppressor, QObject *parent = nullptr);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    int level() const { return m_level; }
    void setLevel(int level);
    Mode mode() const { return m_mode; }
    void setMode(Mode mode);
    bool hardwareAvailable() const;
    bool softwareAvailable() const;
    bool active() const;
    QString error() const { return m_error; }

signals:
    void enabledChanged();
    void levelChanged();
    void modeChanged();
    void availabilityChanged();
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
    Mode m_mode = Software;
    bool m_hardwareWasAvailable = false;
    QString m_loopbackSource;
    QString m_loopbackSink;
    QString m_error;
};
