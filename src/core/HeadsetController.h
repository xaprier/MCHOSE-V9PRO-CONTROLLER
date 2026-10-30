#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>

#include <deque>
#include <functional>

class HidRawDevice;
class QTimer;

// Device-side features of the V9 PRO over the 0x55 vendor report: battery, sound mode, firmware.
// The dongle answers one exchange at a time, so every request goes through a single queue.
class HeadsetController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool dongleConnected READ dongleConnected NOTIFY connectionChanged)
    Q_PROPERTY(bool headsetOnline READ headsetOnline NOTIFY connectionChanged)
    Q_PROPERTY(QString devicePath READ devicePath NOTIFY connectionChanged)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY batteryChanged)
    Q_PROPERTY(ChargeState chargeState READ chargeState NOTIFY batteryChanged)
    Q_PROPERTY(QDateTime lastBatteryUpdate READ lastBatteryUpdate NOTIFY batteryChanged)
    Q_PROPERTY(int soundMode READ soundMode WRITE setSoundMode NOTIFY soundModeChanged)
    Q_PROPERTY(QString dongleFirmware READ dongleFirmware NOTIFY firmwareChanged)
    Q_PROPERTY(QString headsetFirmware READ headsetFirmware NOTIFY firmwareChanged)
    Q_PROPERTY(QString dongleFirmwareRaw READ dongleFirmwareRaw NOTIFY firmwareChanged)
    Q_PROPERTY(QString headsetFirmwareRaw READ headsetFirmwareRaw NOTIFY firmwareChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    enum class ChargeState {
        Unknown,
        Discharging,
        Charging,
        Full,
        Asleep,
    };
    Q_ENUM(ChargeState)

    enum SoundMode {
        GameOne = 0,
        GameTwo = 1,
        Music = 2,
    };
    Q_ENUM(SoundMode)

    explicit HeadsetController(QObject *parent = nullptr);
    ~HeadsetController() override;

    bool dongleConnected() const;
    bool headsetOnline() const { return m_headsetOnline; }
    QString devicePath() const;
    int batteryPercent() const { return m_batteryPercent; }
    ChargeState chargeState() const { return m_chargeState; }
    QDateTime lastBatteryUpdate() const { return m_lastBatteryUpdate; }
    int soundMode() const { return m_soundMode; }
    QString dongleFirmware() const { return m_dongleFirmware; }
    QString headsetFirmware() const { return m_headsetFirmware; }
    QString dongleFirmwareRaw() const { return m_dongleFirmwareRaw; }
    QString headsetFirmwareRaw() const { return m_headsetFirmwareRaw; }
    QString lastError() const { return m_lastError; }

    void setSoundMode(int mode);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void readFirmware();

signals:
    void connectionChanged();
    void batteryChanged();
    void soundModeChanged();
    void firmwareChanged();
    void lastErrorChanged();

private:
    struct Request {
        QByteArray data;
        int replyCommand = -1; // -1: fire-and-forget
        int retriesLeft = 1;
        std::function<void(bool answered)> done;
    };

    void scan();
    void onDisconnected();
    void onReport(const QByteArray &report);
    void enqueue(Request request);
    void processNext();
    void finishCurrent(bool answered);
    void onTimeout();
    void readFirmwareStep(int target);
    void collectFirmware(int target, int attempt);

    void setHeadsetOnline(bool online);
    void setLastError(const QString &error);
    void resetState();

    HidRawDevice *m_device = nullptr;
    QTimer *m_scanTimer = nullptr;
    QTimer *m_pollTimer = nullptr;
    QTimer *m_replyTimer = nullptr;

    std::deque<Request> m_queue;
    bool m_inFlight = false;
    bool m_firmwareReading = false;

    bool m_headsetOnline = false;
    int m_batteryPercent = -1;
    ChargeState m_chargeState = ChargeState::Unknown;
    QDateTime m_lastBatteryUpdate;
    int m_soundMode = -1;
    QString m_dongleFirmware;
    QString m_headsetFirmware;
    QString m_dongleFirmwareRaw;
    QString m_headsetFirmwareRaw;
    QString m_lastError;
};
