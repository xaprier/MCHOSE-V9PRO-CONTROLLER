#pragma once

#include <QLoggingCategory>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

class QProcess;
class QTimer;

Q_DECLARE_LOGGING_CATEGORY(lcAudio)

// Volume and mute of the headset's PipeWire sink/source, plus the ALSA mixer control that looks
// like hardware sidetone. Talks to pipewire-pulse through pactl so no client library is needed;
// `pactl subscribe` keeps the state in sync with changes made elsewhere (keys, desktop applets).
class AudioMixer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool outputAvailable READ outputAvailable NOTIFY outputChanged)
    Q_PROPERTY(int outputVolume READ outputVolume WRITE setOutputVolume NOTIFY outputChanged)
    Q_PROPERTY(bool outputMuted READ outputMuted WRITE setOutputMuted NOTIFY outputChanged)
    Q_PROPERTY(bool outputIsDefault READ outputIsDefault NOTIFY outputChanged)
    Q_PROPERTY(bool micAvailable READ micAvailable NOTIFY micChanged)
    Q_PROPERTY(int micVolume READ micVolume WRITE setMicVolume NOTIFY micChanged)
    Q_PROPERTY(bool micMuted READ micMuted WRITE setMicMuted NOTIFY micChanged)
    Q_PROPERTY(bool sidetoneAvailable READ sidetoneAvailable NOTIFY sidetoneChanged)
    Q_PROPERTY(int sidetoneLevel READ sidetoneLevel WRITE setSidetoneLevel NOTIFY sidetoneChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit AudioMixer(QObject *parent = nullptr);
    ~AudioMixer() override;

    bool outputAvailable() const { return !m_sinkName.isEmpty(); }
    int outputVolume() const { return m_outputVolume; }
    bool outputMuted() const { return m_outputMuted; }
    bool outputIsDefault() const { return m_outputIsDefault; }
    bool micAvailable() const { return !m_sourceName.isEmpty(); }
    int micVolume() const { return m_micVolume; }
    bool micMuted() const { return m_micMuted; }
    bool sidetoneAvailable() const { return m_alsaCard >= 0 && m_sidetoneLevel >= 0; }
    int sidetoneLevel() const { return m_sidetoneLevel; }
    QString lastError() const { return m_lastError; }

    void setOutputVolume(int percent);
    void setOutputMuted(bool muted);
    void setMicVolume(int percent);
    void setMicMuted(bool muted);
    void setSidetoneLevel(int percent);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void makeDefaultOutput();

    // Exposed for tests: parse `pactl -f json list sinks|sources` output.
    struct Endpoint {
        QString name;
        int volume = 0;
        bool muted = false;
        int alsaCard = -1;
    };
    static std::optional<Endpoint> findHeadsetEndpoint(const QByteArray &pactlJson);

signals:
    void outputChanged();
    void micChanged();
    void sidetoneChanged();
    void lastErrorChanged();

private:
    void run(const QString &program, const QStringList &arguments, std::function<void(const QByteArray &)> onOutput = {});
    void startSubscription();
    void flushPending();
    void refreshSidetone();
    void setLastError(const QString &error);

    QString m_pactl;
    QString m_amixer;
    QProcess *m_subscription = nullptr;
    QTimer *m_refreshDebounce = nullptr;
    QTimer *m_writeDebounce = nullptr;

    QString m_sinkName;
    QString m_sourceName;
    QString m_defaultSink;
    int m_alsaCard = -1;

    int m_outputVolume = 0;
    bool m_outputMuted = false;
    bool m_outputIsDefault = false;
    int m_micVolume = 0;
    bool m_micMuted = false;
    int m_sidetoneLevel = -1;

    int m_pendingOutputVolume = -1;
    int m_pendingMicVolume = -1;
    int m_pendingSidetone = -1;

    QString m_lastError;
};
