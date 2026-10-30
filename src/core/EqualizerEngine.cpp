#include "EqualizerEngine.h"

#include "AudioMixer.h"
#include "FilterChainConfig.h"
#include "PipeWireFilter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace eq = v9::eq;
namespace pw = v9::pw;

namespace {

constexpr int ParamDebounceMs = 25;
constexpr int SaveDebounceMs = 500;

} // namespace

EqualizerEngine::EqualizerEngine(AudioMixer *mixer, QObject *parent)
    : QObject(parent)
    , m_mixer(mixer)
    , m_filter(new PipeWireFilter(QStringLiteral("equalizer"), pw::EqualizerSinkName, this))
    , m_paramDebounce(new QTimer(this))
    , m_saveDebounce(new QTimer(this))
{
    m_paramDebounce->setSingleShot(true);
    m_paramDebounce->setInterval(ParamDebounceMs);
    connect(m_paramDebounce, &QTimer::timeout, this, &EqualizerEngine::pushParams);
    m_saveDebounce->setSingleShot(true);
    m_saveDebounce->setInterval(SaveDebounceMs);
    connect(m_saveDebounce, &QTimer::timeout, this, &EqualizerEngine::saveSettings);

    connect(m_filter, &PipeWireFilter::runningChanged, this, &EqualizerEngine::activeChanged);
    connect(m_filter, &PipeWireFilter::errorOccurred, this, &EqualizerEngine::setError);
    connect(m_filter, &PipeWireFilter::ready, this, [this] {
        emit activeChanged();
        m_mixer->setDefaultSink(pw::EqualizerSinkName);
        AudioMixer::moveStreams(AudioMixer::StreamKind::Playback, m_targetSink, pw::EqualizerSinkName);
    });
    // Follow the headset: start once its sink appears, rebuild if it changes, stop if it goes.
    connect(m_mixer, &AudioMixer::outputChanged, this, [this] {
        if (chainNeeded() && m_mixer->sinkName() != m_targetSink)
            m_mixer->sinkName().isEmpty() ? stop() : start();
    });

    loadProfiles();
    loadSettings();
    updateRunning();
}

EqualizerEngine::~EqualizerEngine()
{
    saveSettings();
    // The virtual sink vanishes with this process; hand everything back so audio keeps playing.
    if (m_filter->isRunning() && !m_mixer->sinkName().isEmpty()) {
        if (m_mixer->defaultSink() == pw::EqualizerSinkName)
            QProcess::execute(QStringLiteral("pactl"), {QStringLiteral("set-default-sink"), m_mixer->sinkName()});
        AudioMixer::moveStreams(AudioMixer::StreamKind::Playback, pw::EqualizerSinkName, m_mixer->sinkName());
    }
}

bool EqualizerEngine::available() const
{
    return PipeWireFilter::isAvailable();
}

bool EqualizerEngine::active() const
{
    return m_filter->isRunning() && m_filter->isReady();
}

void EqualizerEngine::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    emit enabledChanged();
    if (enabled && m_mixer->sinkName().isEmpty())
        setError(tr("Headset output not found; the equalizer starts when it appears."));
    changed();
    updateRunning();
}

bool EqualizerEngine::chainNeeded() const
{
    return m_enabled || enhancementsActive();
}

bool EqualizerEngine::enhancementsActive() const
{
    const eq::Enhancements &e = m_settings.enhancements;
    return e.bassEnabled || e.brillianceEnabled || e.clarityEnabled;
}

void EqualizerEngine::updateRunning()
{
    const bool wanted = chainNeeded() && !m_mixer->sinkName().isEmpty();
    if (wanted && !m_filter->isRunning())
        start();
    else if (!wanted && m_filter->isRunning())
        stop();
}

eq::Settings EqualizerEngine::effectiveSettings() const
{
    eq::Settings settings = m_settings;
    if (!m_enabled)
        settings.gains.fill(0.0);
    return settings;
}

void EqualizerEngine::start()
{
    setError({});
    m_targetSink = m_mixer->sinkName();
    m_filter->start(pw::equalizerConfig(effectiveSettings(), m_targetSink));
    emit activeChanged();
}

void EqualizerEngine::stop()
{
    if (m_filter->isRunning() && !m_mixer->sinkName().isEmpty()) {
        if (m_mixer->defaultSink() == pw::EqualizerSinkName)
            m_mixer->setDefaultSink(m_mixer->sinkName());
        AudioMixer::moveStreams(AudioMixer::StreamKind::Playback, pw::EqualizerSinkName, m_mixer->sinkName());
    }
    m_filter->stop();
    m_targetSink.clear();
    emit activeChanged();
}

QStringList EqualizerEngine::bandLabels() const
{
    QStringList labels;
    for (const eq::BandSpec &band : eq::bands())
        labels << QString::fromLatin1(band.label);
    return labels;
}

QVariantList EqualizerEngine::gains() const
{
    QVariantList list;
    for (double gain : m_settings.gains)
        list << gain;
    return list;
}

void EqualizerEngine::setBand(int band, double gainDb)
{
    if (band < 0 || band >= eq::BandCount)
        return;
    gainDb = std::clamp(std::round(gainDb * 2.0) / 2.0, eq::MinGainDb, eq::MaxGainDb);
    if (m_settings.gains[band] == gainDb)
        return;
    m_settings.gains[band] = gainDb;
    m_profileDirty = !m_currentProfile.isEmpty() && !isBuiltInProfile(m_currentProfile);
    changed();
}

void EqualizerEngine::setPreamp(double db)
{
    db = std::clamp(std::round(db * 10.0) / 10.0, -24.0, 6.0);
    if (m_autoPreamp || m_settings.preampDb == db)
        return;
    m_settings.preampDb = db;
    changed();
}

void EqualizerEngine::setAutoPreamp(bool on)
{
    if (m_autoPreamp == on)
        return;
    m_autoPreamp = on;
    changed();
}

#define V9_ENHANCEMENT_SETTER(Name, field, lo, hi)                                                                       \
    void EqualizerEngine::set##Name(int value)                                                                           \
    {                                                                                                                    \
        value = std::clamp(value, lo, hi);                                                                               \
        if (m_settings.enhancements.field == value)                                                                      \
            return;                                                                                                      \
        m_settings.enhancements.field = value;                                                                           \
        changed();                                                                                                       \
    }
V9_ENHANCEMENT_SETTER(BassLevel, bassLevel, 0, 10)
V9_ENHANCEMENT_SETTER(BassCutoff, bassCutoffHz, 50, 400)
V9_ENHANCEMENT_SETTER(BrillianceLevel, brillianceLevel, 0, 10)
V9_ENHANCEMENT_SETTER(ClarityLevel, clarityLevel, 0, 10)
#undef V9_ENHANCEMENT_SETTER

void EqualizerEngine::setBassEnabled(bool on)
{
    if (m_settings.enhancements.bassEnabled == on)
        return;
    m_settings.enhancements.bassEnabled = on;
    changed();
    updateRunning();
}

void EqualizerEngine::setBrillianceEnabled(bool on)
{
    if (m_settings.enhancements.brillianceEnabled == on)
        return;
    m_settings.enhancements.brillianceEnabled = on;
    changed();
    updateRunning();
}

void EqualizerEngine::setClarityEnabled(bool on)
{
    if (m_settings.enhancements.clarityEnabled == on)
        return;
    m_settings.enhancements.clarityEnabled = on;
    changed();
    updateRunning();
}

void EqualizerEngine::reset()
{
    const eq::Profile *profile = findProfile(m_currentProfile);
    if (profile && profile->builtIn) {
        m_settings.gains = profile->gains;
    } else {
        m_settings.gains.fill(0.0);
        m_profileDirty = profile != nullptr;
    }
    changed();
}

void EqualizerEngine::changed()
{
    if (m_autoPreamp)
        m_settings.preampDb = eq::headroomPreampDb(effectiveSettings());
    emit settingsChanged();
    m_saveDebounce->start();
    if (m_filter->isRunning())
        m_paramDebounce->start();
}

void EqualizerEngine::pushParams()
{
    m_filter->setParams(pw::equalizerParams(effectiveSettings()));
}

QString EqualizerEngine::profileDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/v9pro-control/profiles");
}

QStringList EqualizerEngine::builtInProfiles() const
{
    QStringList names;
    for (const eq::Profile &p : m_profiles) {
        if (p.builtIn)
            names << p.name;
    }
    return names;
}

QStringList EqualizerEngine::customProfiles() const
{
    QStringList names;
    for (const eq::Profile &p : m_profiles) {
        if (!p.builtIn)
            names << p.name;
    }
    return names;
}

bool EqualizerEngine::modified() const
{
    const eq::Profile *profile = findProfile(m_currentProfile);
    return profile && profile->builtIn && profile->gains != m_settings.gains;
}

const eq::Profile *EqualizerEngine::findProfile(const QString &name) const
{
    if (name.isEmpty())
        return nullptr;
    for (const eq::Profile &p : m_profiles) {
        if (p.name == name)
            return &p;
    }
    return nullptr;
}

bool EqualizerEngine::isBuiltInProfile(const QString &name) const
{
    return std::any_of(m_profiles.cbegin(), m_profiles.cend(), [&](const eq::Profile &p) { return p.name == name && p.builtIn; });
}

bool EqualizerEngine::applyProfile(const QString &name)
{
    const eq::Profile *profile = findProfile(name);
    if (!profile)
        return false;
    saveSettings(); // flush pending edits of the previous custom profile
    m_currentProfile = profile->name;
    m_settings.gains = profile->gains;
    m_profileDirty = false;
    changed();
    return true;
}

bool EqualizerEngine::writeProfile(const QString &name, const std::array<double, eq::BandCount> &gains)
{
    QDir().mkpath(profileDirectory());
    QFile file(profileDirectory() + QLatin1Char('/') + eq::profileFileName(name));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(tr("Cannot save profile: %1").arg(file.errorString()));
        return false;
    }
    file.write(eq::serializeProfile({name, gains, false}));
    return true;
}

QString EqualizerEngine::createProfile()
{
    QString name;
    for (int n = 1; name.isEmpty() || findProfile(name); ++n)
        name = tr("Custom %1").arg(n);
    if (!writeProfile(name, m_settings.gains))
        return {};
    loadProfiles();
    m_currentProfile = name;
    m_profileDirty = false;
    changed();
    return name;
}

bool EqualizerEngine::renameProfile(const QString &from, const QString &to)
{
    const QString trimmed = to.trimmed();
    const eq::Profile *profile = findProfile(from);
    if (!profile || profile->builtIn || trimmed.isEmpty() || trimmed == from || findProfile(trimmed))
        return false;
    if (!writeProfile(trimmed, profile->gains))
        return false;
    QFile::remove(profileDirectory() + QLatin1Char('/') + eq::profileFileName(from));
    if (m_currentProfile == from)
        m_currentProfile = trimmed;
    loadProfiles();
    changed();
    return true;
}

bool EqualizerEngine::deleteProfile(const QString &name)
{
    const eq::Profile *profile = findProfile(name);
    if (!profile || profile->builtIn)
        return false;
    const bool removed = QFile::remove(profileDirectory() + QLatin1Char('/') + eq::profileFileName(name));
    if (m_currentProfile == name) {
        // The gains stay as they are; they just no longer belong to a profile.
        m_currentProfile.clear();
        m_profileDirty = false;
    }
    loadProfiles();
    changed();
    return removed;
}

bool EqualizerEngine::importProfile(const QUrl &file)
{
    QFile in(file.isLocalFile() ? file.toLocalFile() : file.toString());
    if (!in.open(QIODevice::ReadOnly)) {
        setError(tr("%1: %2").arg(in.fileName(), in.errorString()));
        return false;
    }
    QString error;
    auto profile = eq::parseProfile(in.readAll(), QFileInfo(in).completeBaseName(), &error);
    if (!profile) {
        setError(tr("%1: %2").arg(in.fileName(), error));
        return false;
    }
    QString name = profile->name;
    for (int n = 2; findProfile(name); ++n)
        name = QStringLiteral("%1 (%2)").arg(profile->name).arg(n);
    if (!writeProfile(name, profile->gains))
        return false;
    loadProfiles();
    return applyProfile(name);
}

bool EqualizerEngine::exportProfile(const QUrl &file)
{
    QFile out(file.isLocalFile() ? file.toLocalFile() : file.toString());
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(tr("%1: %2").arg(out.fileName(), out.errorString()));
        return false;
    }
    const QString name = m_currentProfile.isEmpty() ? tr("Custom") : m_currentProfile;
    out.write(eq::serializeProfile({name, m_settings.gains, false}));
    return true;
}

void EqualizerEngine::loadProfiles()
{
    m_profiles = eq::builtInProfiles();
    const QDir dir(profileDirectory());
    for (const QFileInfo &info : dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name)) {
        QFile file(info.filePath());
        if (!file.open(QIODevice::ReadOnly))
            continue;
        // One unreadable file must not hide the rest.
        if (auto profile = eq::parseProfile(file.readAll(), info.completeBaseName()); profile && !isBuiltInProfile(profile->name))
            m_profiles.append(*profile);
    }
    emit profilesChanged();
}

void EqualizerEngine::loadSettings()
{
    QSettings s;
    s.beginGroup(QStringLiteral("equalizer"));
    m_enabled = s.value(QStringLiteral("enabled"), false).toBool();
    m_autoPreamp = s.value(QStringLiteral("autoPreamp"), true).toBool();
    m_settings.preampDb = s.value(QStringLiteral("preamp"), 0.0).toDouble();
    const QVariantList gains = s.value(QStringLiteral("gains")).toList();
    for (int i = 0; i < eq::BandCount && i < gains.size(); ++i)
        m_settings.gains[i] = std::clamp(gains[i].toDouble(), eq::MinGainDb, eq::MaxGainDb);
    eq::Enhancements &e = m_settings.enhancements;
    e.bassEnabled = s.value(QStringLiteral("bassEnabled"), false).toBool();
    e.bassLevel = s.value(QStringLiteral("bassLevel"), 5).toInt();
    e.bassCutoffHz = s.value(QStringLiteral("bassCutoff"), 120).toInt();
    e.brillianceEnabled = s.value(QStringLiteral("brillianceEnabled"), false).toBool();
    e.brillianceLevel = s.value(QStringLiteral("brillianceLevel"), 5).toInt();
    e.clarityEnabled = s.value(QStringLiteral("clarityEnabled"), false).toBool();
    e.clarityLevel = s.value(QStringLiteral("clarityLevel"), 5).toInt();
    m_currentProfile = s.value(QStringLiteral("profile")).toString();
    if (!findProfile(m_currentProfile)) {
        // Settings from before profiles were tracked: adopt a profile with the same gains.
        m_currentProfile.clear();
        for (const eq::Profile &p : m_profiles) {
            if (p.gains == m_settings.gains) {
                m_currentProfile = p.name;
                break;
            }
        }
    }
    if (m_autoPreamp)
        m_settings.preampDb = eq::headroomPreampDb(effectiveSettings());
}

void EqualizerEngine::saveSettings()
{
    if (m_profileDirty && findProfile(m_currentProfile) && writeProfile(m_currentProfile, m_settings.gains)) {
        m_profileDirty = false;
        for (eq::Profile &p : m_profiles) {
            if (p.name == m_currentProfile)
                p.gains = m_settings.gains;
        }
    }
    QSettings s;
    s.beginGroup(QStringLiteral("equalizer"));
    s.setValue(QStringLiteral("enabled"), m_enabled);
    s.setValue(QStringLiteral("autoPreamp"), m_autoPreamp);
    s.setValue(QStringLiteral("preamp"), m_settings.preampDb);
    s.setValue(QStringLiteral("gains"), gains());
    s.setValue(QStringLiteral("profile"), m_currentProfile);
    const eq::Enhancements &e = m_settings.enhancements;
    s.setValue(QStringLiteral("bassEnabled"), e.bassEnabled);
    s.setValue(QStringLiteral("bassLevel"), e.bassLevel);
    s.setValue(QStringLiteral("bassCutoff"), e.bassCutoffHz);
    s.setValue(QStringLiteral("brillianceEnabled"), e.brillianceEnabled);
    s.setValue(QStringLiteral("brillianceLevel"), e.brillianceLevel);
    s.setValue(QStringLiteral("clarityEnabled"), e.clarityEnabled);
    s.setValue(QStringLiteral("clarityLevel"), e.clarityLevel);
}

void EqualizerEngine::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}
