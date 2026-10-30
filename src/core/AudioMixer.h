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

// Volume and mute of the headset's PipeWire sink/source. Talks to pipewire-pulse through pactl so no client library is needed;
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
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString sinkName READ sinkName NOTIFY outputChanged)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY micChanged)
    Q_PROPERTY(QString defaultSink READ defaultSink NOTIFY defaultsChanged)
    Q_PROPERTY(QString defaultSource READ defaultSource NOTIFY defaultsChanged)

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
    QString lastError() const { return m_lastError; }
    QString sinkName() const { return m_sinkName; }
    QString sourceName() const { return m_sourceName; }
    QString defaultSink() const { return m_defaultSink; }
    QString defaultSource() const { return m_defaultSource; }

    void setDefaultSink(const QString &name);
    void setDefaultSource(const QString &name);

    void setOutputVolume(int percent);
    void setOutputMuted(bool muted);
    void setMicVolume(int percent);
    void setMicMuted(bool muted);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void makeDefaultOutput();

    // Exposed for tests: parse `pactl -f json list sinks|sources` output.
    struct Endpoint {
        QString name;
        int volume = 0;
        bool muted = false;
    };
    static std::optional<Endpoint> findHeadsetEndpoint(const QByteArray &pactlJson);

signals:
    void outputChanged();
    void micChanged();
    void lastErrorChanged();
    void defaultsChanged();

private:
    void run(const QString &program, const QStringList &arguments, std::function<void(const QByteArray &)> onOutput = {});
    bool isHeadsetDefault() const;
    void startSubscription();
    void flushPending();
    void setLastError(const QString &error);

    QString m_pactl;
    QProcess *m_subscription = nullptr;
    QTimer *m_refreshDebounce = nullptr;
    QTimer *m_writeDebounce = nullptr;

    QString m_sinkName;
    QString m_sourceName;
    QString m_defaultSink;
    QString m_defaultSource;

    int m_outputVolume = 0;
    bool m_outputMuted = false;
    bool m_outputIsDefault = false;
    int m_micVolume = 0;
    bool m_micMuted = false;

    int m_pendingOutputVolume = -1;
    int m_pendingMicVolume = -1;

    QString m_lastError;
};
