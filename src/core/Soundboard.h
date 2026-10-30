#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>

class AudioMixer;
class PipeWireFilter;
class QFileSystemWatcher;
class QProcess;

// Plays sound files from a folder, one at a time, into a virtual "V9 PRO Soundboard" sink. Starting a
// sound while another plays replaces it. While the sink runs, the
// patchbay graph links that sink's monitor into every program recording a microphone, so the
// sounds reach the call untouched by noise suppression. Optionally the sounds also play in the
// headset ("listen").
class Soundboard : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString folder READ folder NOTIFY folderChanged)
    Q_PROPERTY(QStringList sounds READ sounds NOTIFY soundsChanged)
    Q_PROPERTY(QStringList playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool listen READ listen WRITE setListen NOTIFY listenChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit Soundboard(AudioMixer *mixer, QObject *parent = nullptr);
    ~Soundboard() override;

    static QStringList supportedSuffixes();

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    static bool available();
    bool active() const;
    QString folder() const { return m_folder; }
    QStringList sounds() const { return m_sounds; }
    QStringList playing() const { return m_current.isEmpty() ? QStringList() : QStringList{m_current}; }
    int volume() const { return m_volume; }
    void setVolume(int volume);
    bool listen() const { return m_listen; }
    void setListen(bool listen);
    QString error() const { return m_error; }

    Q_INVOKABLE void setFolderUrl(const QUrl &url);
    // Starts `name` (a file name from `sounds`), replacing whatever plays; stops it when it is the one
    // already playing.
    Q_INVOKABLE void toggle(const QString &name);
    Q_INVOKABLE void stopAll();
    Q_INVOKABLE void refresh();

signals:
    void enabledChanged();
    void activeChanged();
    void folderChanged();
    void soundsChanged();
    void playingChanged();
    void volumeChanged();
    void listenChanged();
    void errorChanged();

private:
    void apply();
    void setFolder(const QString &folder);
    void setError(const QString &error);
    void start(const QString &name, const QString &path);
    void finish(QProcess *player);

    AudioMixer *m_mixer;
    PipeWireFilter *m_sink;
    QFileSystemWatcher *m_watcher;
    QString m_listenTarget; // headset sink the running config's listen side plays into
    bool m_enabled = false;
    QString m_folder;
    QStringList m_sounds;
    // ffmpeg decodes (every format, unlike libsndfile, which cuts Ogg Opus short) into pw-cat; without
    // ffmpeg pw-play reads the file itself and m_decoder stays null.
    QString m_current;
    QProcess *m_decoder = nullptr;
    QProcess *m_player = nullptr;
    int m_volume = 80;
    bool m_listen = true;
    QString m_error;
};
