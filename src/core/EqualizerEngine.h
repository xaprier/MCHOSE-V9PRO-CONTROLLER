#pragma once

#include "EqualizerMath.h"
#include "EqualizerProfile.h"

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

class AudioMixer;
class PipeWireFilter;
class QTimer;

// 10-band equalizer plus Hub-style enhancements, running as a PipeWire filter-chain virtual sink
// ("V9 PRO Equalizer") in front of the headset. The chain runs while the equalizer or any
// enhancement is on (the bands are flat when only enhancements are); while it runs its sink is the
// default output, and stopping hands the default back to the headset.
//
// Profiles work like MCHOSE HUB's: selecting one loads it, editing a custom profile saves into it,
// editing a built-in one marks it modified until it is saved as a new custom profile.
class EqualizerEngine : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QStringList bandLabels READ bandLabels CONSTANT)
    Q_PROPERTY(QVariantList gains READ gains NOTIFY settingsChanged)
    Q_PROPERTY(double preamp READ preamp WRITE setPreamp NOTIFY settingsChanged)
    Q_PROPERTY(bool autoPreamp READ autoPreamp WRITE setAutoPreamp NOTIFY settingsChanged)
    Q_PROPERTY(bool bassEnabled READ bassEnabled WRITE setBassEnabled NOTIFY settingsChanged)
    Q_PROPERTY(int bassLevel READ bassLevel WRITE setBassLevel NOTIFY settingsChanged)
    Q_PROPERTY(int bassCutoff READ bassCutoff WRITE setBassCutoff NOTIFY settingsChanged)
    Q_PROPERTY(bool brillianceEnabled READ brillianceEnabled WRITE setBrillianceEnabled NOTIFY settingsChanged)
    Q_PROPERTY(int brillianceLevel READ brillianceLevel WRITE setBrillianceLevel NOTIFY settingsChanged)
    Q_PROPERTY(bool clarityEnabled READ clarityEnabled WRITE setClarityEnabled NOTIFY settingsChanged)
    Q_PROPERTY(int clarityLevel READ clarityLevel WRITE setClarityLevel NOTIFY settingsChanged)
    Q_PROPERTY(bool enhancementsActive READ enhancementsActive NOTIFY settingsChanged)
    Q_PROPERTY(QStringList builtInProfiles READ builtInProfiles NOTIFY profilesChanged)
    Q_PROPERTY(QStringList customProfiles READ customProfiles NOTIFY profilesChanged)
    // Selected profile; empty when the gains belong to no profile (e.g. after deleting it).
    Q_PROPERTY(QString currentProfile READ currentProfile NOTIFY settingsChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY settingsChanged)

public:
    explicit EqualizerEngine(AudioMixer *mixer, QObject *parent = nullptr);
    ~EqualizerEngine() override;

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    bool available() const;
    bool active() const;
    QString error() const { return m_error; }

    QStringList bandLabels() const;
    QVariantList gains() const;
    double preamp() const { return m_settings.preampDb; }
    void setPreamp(double db);
    bool autoPreamp() const { return m_autoPreamp; }
    void setAutoPreamp(bool on);

    bool bassEnabled() const { return m_settings.enhancements.bassEnabled; }
    void setBassEnabled(bool on);
    int bassLevel() const { return m_settings.enhancements.bassLevel; }
    void setBassLevel(int level);
    int bassCutoff() const { return m_settings.enhancements.bassCutoffHz; }
    void setBassCutoff(int hz);
    bool brillianceEnabled() const { return m_settings.enhancements.brillianceEnabled; }
    void setBrillianceEnabled(bool on);
    int brillianceLevel() const { return m_settings.enhancements.brillianceLevel; }
    void setBrillianceLevel(int level);
    bool clarityEnabled() const { return m_settings.enhancements.clarityEnabled; }
    void setClarityEnabled(bool on);
    int clarityLevel() const { return m_settings.enhancements.clarityLevel; }
    void setClarityLevel(int level);

    bool enhancementsActive() const;
    QStringList builtInProfiles() const;
    QStringList customProfiles() const;
    QString currentProfile() const { return m_currentProfile; }
    bool modified() const;

    Q_INVOKABLE void setBand(int band, double gainDb);
    // Built-in profile: back to its stored gains. Custom profile or none: all bands flat.
    Q_INVOKABLE void reset();
    Q_INVOKABLE bool applyProfile(const QString &name);
    // Saves the current gains as a new custom profile ("Custom N") and selects it.
    Q_INVOKABLE QString createProfile();
    Q_INVOKABLE bool renameProfile(const QString &from, const QString &to);
    Q_INVOKABLE bool deleteProfile(const QString &name);
    Q_INVOKABLE bool isBuiltInProfile(const QString &name) const;
    Q_INVOKABLE bool importProfile(const QUrl &file);
    Q_INVOKABLE bool exportProfile(const QUrl &file);

    QString profileDirectory() const;

    // First half of quitting: point the default output and current streams back at the headset
    // while the virtual sink still exists. Apps such as browsers reopen their streams when the
    // default changes and pause if the sink vanishes mid-way, so the caller waits a moment before
    // destroying the engine.
    void handOver();

signals:
    void enabledChanged();
    void activeChanged();
    void errorChanged();
    void settingsChanged();
    void profilesChanged();

private:
    void changed();
    void updateRunning();
    bool chainNeeded() const;
    // What actually runs: the bands are flat while the equalizer itself is off.
    v9::eq::Settings effectiveSettings() const;
    const v9::eq::Profile *findProfile(const QString &name) const;
    bool writeProfile(const QString &name, const std::array<double, v9::eq::BandCount> &gains);
    void start();
    void stop();
    void pushParams();
    void loadProfiles();
    void loadSettings();
    void saveSettings();
    void setError(const QString &error);

    AudioMixer *m_mixer;
    PipeWireFilter *m_filter;
    QTimer *m_paramDebounce;
    QTimer *m_saveDebounce;
    v9::eq::Settings m_settings;
    bool m_enabled = false;
    bool m_autoPreamp = true;
    QString m_targetSink;
    QList<v9::eq::Profile> m_profiles;
    QString m_currentProfile;
    bool m_profileDirty = false;
    QString m_error;
};
