#pragma once

#include "FirmwareFlasher.h"
#include "FirmwareImage.h"

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

#include <optional>

class HeadsetController;
class QNetworkAccessManager;

// Voice prompt silencer: load or download a stock headset image, pick prompt clips, build a
// patched image and flash it. Every flash needs an explicit typed confirmation.
class FirmwareManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool imageLoaded READ imageLoaded NOTIFY imageChanged)
    Q_PROPERTY(QString imageName READ imageName NOTIFY imageChanged)
    Q_PROPERTY(QString imageSummary READ imageSummary NOTIFY imageChanged)
    Q_PROPERTY(QVariantList clips READ clips NOTIFY clipsChanged)
    Q_PROPERTY(bool patchedReady READ patchedReady NOTIFY patchedChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool flashing READ flashing NOTIFY flashStateChanged)
    Q_PROPERTY(FirmwareFlasher::State flashState READ flashState NOTIFY flashStateChanged)
    Q_PROPERTY(double flashProgress READ flashProgress NOTIFY flashProgressChanged)
    Q_PROPERTY(QStringList log READ log NOTIFY logChanged)
    Q_PROPERTY(QString firmwareDirectory READ firmwareDirectory CONSTANT)

public:
    explicit FirmwareManager(HeadsetController *headset, QObject *parent = nullptr);

    bool imageLoaded() const { return m_image.has_value(); }
    QString imageName() const { return m_imageName; }
    QString imageSummary() const;
    QVariantList clips() const;
    bool patchedReady() const { return !m_patched.isEmpty(); }
    bool busy() const { return m_busy; }
    bool flashing() const { return m_flasher->isBusy(); }
    FirmwareFlasher::State flashState() const { return m_flasher->state(); }
    double flashProgress() const { return m_flashProgress; }
    QStringList log() const { return m_log; }
    QString firmwareDirectory() const;

    Q_INVOKABLE bool loadFile(const QUrl &file);
    // Fetches MCHOSE's OTA manifest and the stock headset image (English or Chinese prompts).
    Q_INVOKABLE void downloadStock(bool english);
    Q_INVOKABLE void setClipSelected(int index, bool selected);
    Q_INVOKABLE bool buildPatched();
    Q_INVOKABLE bool savePatched(const QUrl &file);
    // kind: "noop" (stock repacked, byte-identical; validates the path), "patched", "stock".
    Q_INVOKABLE bool flash(const QString &kind, const QString &confirmation);
    Q_INVOKABLE void abortFlash();
    Q_INVOKABLE void clearLog();

signals:
    void imageChanged();
    void clipsChanged();
    void patchedChanged();
    void busyChanged();
    void flashStateChanged();
    void flashProgressChanged();
    void logChanged();

private:
    void addLog(const QString &message);
    void setBusy(bool busy);
    void setImage(const QByteArray &data, const QString &name);

    HeadsetController *m_headset;
    FirmwareFlasher *m_flasher;
    QNetworkAccessManager *m_network = nullptr;
    std::optional<v9::fw::Image> m_image;
    QString m_imageName;
    QList<int> m_selected;
    QByteArray m_patched;
    double m_flashProgress = 0.0;
    QStringList m_log;
    bool m_busy = false;
};
