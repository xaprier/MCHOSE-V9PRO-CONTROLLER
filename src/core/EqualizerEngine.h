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
// ("V9 PRO Equalizer") in front of the headset. While enabled that sink is made the default
// output; disabling hands the default back to the headset.
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
    // Points {x: 0..1 log-frequency position, y: dB} of the band shape (preamp excluded).
    Q_PROPERTY(QVariantList curve READ curve NOTIFY settingsChanged)
    Q_PROPERTY(QStringList profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(QString currentProfile READ currentProfile NOTIFY settingsChanged)

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

    QVariantList curve() const;
    QStringList profiles() const;
    QString currentProfile() const;

    Q_INVOKABLE void setBand(int band, double gainDb);
    Q_INVOKABLE void reset();
    Q_INVOKABLE bool applyProfile(const QString &name);
    Q_INVOKABLE bool saveProfile(const QString &name);
    Q_INVOKABLE bool deleteProfile(const QString &name);
    Q_INVOKABLE bool isBuiltInProfile(const QString &name) const;
    Q_INVOKABLE bool importProfile(const QUrl &file);
    Q_INVOKABLE bool exportProfile(const QUrl &file, const QString &name);
    // Response in dB at a frequency, for the graph's hover read-out.
    Q_INVOKABLE double responseAt(double frequency) const;

    QString profileDirectory() const;

signals:
    void enabledChanged();
    void activeChanged();
    void errorChanged();
    void settingsChanged();
    void profilesChanged();

private:
    void changed();
    void start();
    void stop();
    void pushParams();
    void loadProfiles();
    void loadSettings();
    void saveSettings() const;
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
    QString m_error;
};
