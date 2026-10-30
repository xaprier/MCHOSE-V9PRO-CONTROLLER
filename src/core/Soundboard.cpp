#include "Soundboard.h"

#include "AudioMixer.h"
#include "FilterChainConfig.h"
#include "PipeWireFilter.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

namespace pw = v9::pw;

Soundboard::Soundboard(AudioMixer *mixer, QObject *parent)
    : QObject(parent)
    , m_mixer(mixer)
    , m_sink(new PipeWireFilter(QStringLiteral("soundboard"), pw::SoundboardListenNodeName, this))
    , m_watcher(new QFileSystemWatcher(this))
{
    QSettings settings;
    m_enabled = settings.value(QStringLiteral("soundboard/enabled"), false).toBool();
    m_volume = settings.value(QStringLiteral("soundboard/volume"), 80).toInt();
    m_listen = settings.value(QStringLiteral("soundboard/listen"), true).toBool();
    const QString defaultFolder = QStandardPaths::writableLocation(QStandardPaths::MusicLocation) + QStringLiteral("/Soundboard");
    m_folder = settings.value(QStringLiteral("soundboard/folder"), defaultFolder).toString();

    connect(m_sink, &PipeWireFilter::runningChanged, this, &Soundboard::activeChanged);
    connect(m_sink, &PipeWireFilter::ready, this, [this] {
        m_sink->setParams(pw::soundboardListenParams(m_listen));
        emit activeChanged();
    });
    connect(m_sink, &PipeWireFilter::errorOccurred, this, &Soundboard::setError);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &Soundboard::refresh);
    // The listen side targets the headset; follow it when it appears or changes.
    connect(m_mixer, &AudioMixer::outputChanged, this, [this] {
        if (m_enabled && m_mixer->sinkName() != m_listenTarget)
            apply();
    });

    refresh();
    apply();
}

Soundboard::~Soundboard()
{
    stopAll();
}

QStringList Soundboard::supportedSuffixes()
{
    // What pw-play reads through libsndfile.
    return {QStringLiteral("wav"), QStringLiteral("mp3"), QStringLiteral("ogg"), QStringLiteral("oga"),
            QStringLiteral("opus"), QStringLiteral("flac")};
}

bool Soundboard::available()
{
    return PipeWireFilter::isAvailable() && !QStandardPaths::findExecutable(QStringLiteral("pw-play")).isEmpty();
}

bool Soundboard::active() const
{
    return m_sink->isRunning() && m_sink->isReady();
}

void Soundboard::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    QSettings().setValue(QStringLiteral("soundboard/enabled"), enabled);
    emit enabledChanged();
    apply();
}

void Soundboard::setVolume(int volume)
{
    volume = qBound(0, volume, 100);
    if (m_volume == volume)
        return;
    m_volume = volume;
    QSettings().setValue(QStringLiteral("soundboard/volume"), volume);
    emit volumeChanged();
}

void Soundboard::setListen(bool listen)
{
    if (m_listen == listen)
        return;
    m_listen = listen;
    QSettings().setValue(QStringLiteral("soundboard/listen"), listen);
    emit listenChanged();
    if (m_sink->isRunning())
        m_sink->setParams(pw::soundboardListenParams(listen));
}

void Soundboard::apply()
{
    if (!m_enabled || !available()) {
        stopAll();
        m_sink->stop();
        m_listenTarget.clear();
        emit activeChanged();
        return;
    }
    setError({});
    const QString target = m_mixer->sinkName();
    if (m_sink->isRunning() && target == m_listenTarget)
        return;
    stopAll();
    m_listenTarget = target;
    m_sink->start(pw::soundboardConfig(target));
    emit activeChanged();
}

void Soundboard::setFolderUrl(const QUrl &url)
{
    setFolder(url.isLocalFile() ? url.toLocalFile() : url.toString());
}

void Soundboard::setFolder(const QString &folder)
{
    const QString clean = QDir::cleanPath(folder);
    if (clean.isEmpty() || clean == m_folder)
        return;
    stopAll();
    m_folder = clean;
    QSettings().setValue(QStringLiteral("soundboard/folder"), clean);
    emit folderChanged();
    refresh();
}

void Soundboard::refresh()
{
    if (!m_watcher->directories().isEmpty())
        m_watcher->removePaths(m_watcher->directories());
    QStringList sounds;
    const QDir dir(m_folder);
    if (dir.exists()) {
        m_watcher->addPath(dir.absolutePath());
        QStringList filters;
        for (const QString &suffix : supportedSuffixes())
            filters << QStringLiteral("*.") + suffix;
        sounds = dir.entryList(filters, QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
    }
    if (sounds != m_sounds) {
        m_sounds = sounds;
        emit soundsChanged();
    }
}

void Soundboard::toggle(const QString &name)
{
    if (auto *running = m_playing.value(name)) {
        running->terminate();
        return;
    }
    if (!active())
        return;
    // Only plain file names from the listed folder; nothing that could step out of it.
    if (!m_sounds.contains(name) || name.contains(QLatin1Char('/')))
        return;
    const QString path = QDir(m_folder).absoluteFilePath(name);
    if (!QFileInfo(path).isFile())
        return;

    auto *process = new QProcess(this);
    m_playing.insert(name, process);
    connect(process, &QProcess::finished, this, [this, process, name] {
        if (m_playing.value(name) == process) {
            m_playing.remove(name);
            emit playingChanged();
        }
        process->deleteLater();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, name](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        setError(tr("Could not start pw-play"));
        m_playing.remove(name);
        emit playingChanged();
        process->deleteLater();
    });
    process->start(QStringLiteral("pw-play"), {QStringLiteral("--target"), pw::SoundboardSinkName, QStringLiteral("--volume"),
                                                QString::number(m_volume / 100.0, 'f', 2), QStringLiteral("--"), path});
    emit playingChanged();
}

void Soundboard::stopAll()
{
    const QList<QProcess *> processes = m_playing.values();
    m_playing.clear();
    for (QProcess *process : processes) {
        process->disconnect(this);
        process->kill();
        process->waitForFinished(200);
        process->deleteLater();
    }
    if (!processes.isEmpty())
        emit playingChanged();
}

void Soundboard::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}
