#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <atomic>

class QThread;

// Streams an AOTA image to the headset through the 2.4G dongle (protocol: docs/ANALYSIS.md §2.4).
// Runs on its own thread with its own hidraw handle because the transfer is thousands of
// synchronous writes paced by the device's flow control.
class FirmwareFlasher : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Idle,
        Entering,
        Streaming,
        Finishing,
        Succeeded,
        Failed,
    };
    Q_ENUM(State)

    explicit FirmwareFlasher(QObject *parent = nullptr);
    ~FirmwareFlasher() override;

    State state() const { return m_state; }
    bool isBusy() const;

    // `toHeadset` false targets the dongle (not used by the UI; kept for completeness).
    void start(const QString &hidrawNode, const QByteArray &image, bool toHeadset = true);
    // Stops sending. The headset discards an incomplete image and keeps its current firmware.
    void abort();

signals:
    void stateChanged(FirmwareFlasher::State state);
    void progressChanged(double progress);
    void logMessage(const QString &message);
    void finished(bool success, const QString &message);

private:
    void run(QString node, QByteArray image, bool toHeadset);
    void post(State state);
    void postLog(const QString &message);

    State m_state = State::Idle;
    QThread *m_thread = nullptr;
    std::atomic<bool> m_abort{false};
};
