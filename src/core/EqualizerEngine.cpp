#include "EqualizerEngine.h"

#include "AudioMixer.h"
#include "FilterChainConfig.h"
#include "PipeWireFilter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointF>
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

double logPosition(double frequency)
{
    return (std::log10(frequency) - std::log10(20.0)) / (std::log10(20000.0) - std::log10(20.0));
}

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
    });
    // Follow the headset: start once its sink appears, rebuild if it changes, stop if it goes.
    connect(m_mixer, &AudioMixer::outputChanged, this, [this] {
        if (m_enabled && m_mixer->sinkName() != m_targetSink)
            m_mixer->sinkName().isEmpty() ? stop() : start();
    });

    loadSettings();
    loadProfiles();
    if (m_enabled && !m_mixer->sinkName().isEmpty())
        start();
}

EqualizerEngine::~EqualizerEngine()
{
    saveSettings();
    // The virtual sink vanishes with this process; hand the default back so audio keeps playing.
    if (m_filter->isRunning() && m_mixer->defaultSink() == pw::EqualizerSinkName && !m_mixer->sinkName().isEmpty())
        QProcess::execute(QStringLiteral("pactl"), {QStringLiteral("set-default-sink"), m_mixer->sinkName()});
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
    saveSettings();
    if (enabled) {
        if (m_mixer->sinkName().isEmpty())
            setError(tr("Headset output not found; the equalizer starts when it appears."));
        else
            start();
    } else {
        stop();
    }
}

void EqualizerEngine::start()
{
    setError({});
    m_targetSink = m_mixer->sinkName();
    m_filter->start(pw::equalizerConfig(m_settings, m_targetSink));
    emit activeChanged();
}

void EqualizerEngine::stop()
{
    if (m_mixer->defaultSink() == pw::EqualizerSinkName && !m_mixer->sinkName().isEmpty())
        m_mixer->setDefaultSink(m_mixer->sinkName());
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
}

void EqualizerEngine::setBrillianceEnabled(bool on)
{
    if (m_settings.enhancements.brillianceEnabled == on)
        return;
    m_settings.enhancements.brillianceEnabled = on;
    changed();
}

void EqualizerEngine::setClarityEnabled(bool on)
{
    if (m_settings.enhancements.clarityEnabled == on)
        return;
    m_settings.enhancements.clarityEnabled = on;
    changed();
}

void EqualizerEngine::reset()
{
    m_settings.gains.fill(0.0);
    changed();
}

void EqualizerEngine::changed()
{
    if (m_autoPreamp)
        m_settings.preampDb = eq::headroomPreampDb(m_settings);
    emit settingsChanged();
    m_saveDebounce->start();
    if (m_filter->isRunning())
        m_paramDebounce->start();
}

void EqualizerEngine::pushParams()
{
    m_filter->setParams(pw::equalizerParams(m_settings));
}

QVariantList EqualizerEngine::curve() const
{
    // Drawn without the preamp so the curve passes through the band handles; the preamp is
    // shown as a number next to it.
    eq::Settings shape = m_settings;
    shape.preampDb = 0.0;
    QVariantList points;
    for (const QPointF &p : eq::responseCurve(shape))
        points << QPointF(logPosition(p.x()), p.y());
    return points;
}

double EqualizerEngine::responseAt(double frequency) const
{
    return eq::responseDb(m_settings, frequency);
}

QString EqualizerEngine::profileDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/v9pro-control/profiles");
}

QStringList EqualizerEngine::profiles() const
{
    QStringList names;
    for (const eq::Profile &p : m_profiles)
        names << p.name;
    return names;
}

QString EqualizerEngine::currentProfile() const
{
    for (const eq::Profile &p : m_profiles) {
        if (p.gains == m_settings.gains)
            return p.name;
    }
    return {};
}

bool EqualizerEngine::isBuiltInProfile(const QString &name) const
{
    return std::any_of(m_profiles.cbegin(), m_profiles.cend(), [&](const eq::Profile &p) { return p.name == name && p.builtIn; });
}

bool EqualizerEngine::applyProfile(const QString &name)
{
    for (const eq::Profile &p : m_profiles) {
        if (p.name == name) {
            m_settings.gains = p.gains;
            changed();
            return true;
        }
    }
    return false;
}

bool EqualizerEngine::saveProfile(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || isBuiltInProfile(trimmed))
        return false;
    QDir().mkpath(profileDirectory());
    QFile file(profileDirectory() + QLatin1Char('/') + eq::profileFileName(trimmed));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(eq::serializeProfile({trimmed, m_settings.gains, false}));
    file.close();
    loadProfiles();
    emit settingsChanged();
    return true;
}

bool EqualizerEngine::deleteProfile(const QString &name)
{
    if (isBuiltInProfile(name))
        return false;
    const bool removed = QFile::remove(profileDirectory() + QLatin1Char('/') + eq::profileFileName(name));
    loadProfiles();
    emit settingsChanged();
    return removed;
}

bool EqualizerEngine::importProfile(const QUrl &file)
{
    QFile in(file.isLocalFile() ? file.toLocalFile() : file.toString());
    if (!in.open(QIODevice::ReadOnly))
        return false;
    QString error;
    auto profile = eq::parseProfile(in.readAll(), QFileInfo(in).completeBaseName(), &error);
    if (!profile) {
        setError(tr("%1: %2").arg(in.fileName(), error));
        return false;
    }
    if (isBuiltInProfile(profile->name))
        profile->name += tr(" (imported)");
    m_settings.gains = profile->gains;
    return saveProfile(profile->name);
}

bool EqualizerEngine::exportProfile(const QUrl &file, const QString &name)
{
    QFile out(file.isLocalFile() ? file.toLocalFile() : file.toString());
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    out.write(eq::serializeProfile({name.trimmed().isEmpty() ? tr("Custom") : name.trimmed(), m_settings.gains, false}));
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
    if (m_autoPreamp)
        m_settings.preampDb = eq::headroomPreampDb(m_settings);
}

void EqualizerEngine::saveSettings() const
{
    QSettings s;
    s.beginGroup(QStringLiteral("equalizer"));
    s.setValue(QStringLiteral("enabled"), m_enabled);
    s.setValue(QStringLiteral("autoPreamp"), m_autoPreamp);
    s.setValue(QStringLiteral("preamp"), m_settings.preampDb);
    s.setValue(QStringLiteral("gains"), gains());
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
