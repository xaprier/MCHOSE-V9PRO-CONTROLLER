#pragma once

#include <QByteArray>
#include <QLoggingCategory>
#include <QObject>
#include <QString>

#include <cstdint>
#include <optional>

class QSocketNotifier;

Q_DECLARE_LOGGING_CATEGORY(lcHid)

// Thin wrapper around a Linux /dev/hidraw node: non-blocking reads through QSocketNotifier,
// output reports via write(2), feature reports via HIDIOC{S,G}FEATURE.
class HidRawDevice : public QObject
{
    Q_OBJECT

public:
    explicit HidRawDevice(QObject *parent = nullptr);
    ~HidRawDevice() override;

    // Returns the hidraw node of the given device whose report descriptor declares `reportId`.
    static std::optional<QString> findNode(std::uint16_t vendorId, std::uint16_t productId, std::uint8_t reportId);

    // `watch` = deliver input through reportReceived on this object's thread. Without it the
    // caller reads synchronously with readReport() (used by the flasher's worker thread).
    bool open(const QString &path, bool watch = true);
    void close();
    bool isOpen() const { return m_fd >= 0; }
    QString path() const { return m_path; }
    QString errorString() const { return m_error; }

    bool writeReport(const QByteArray &report);
    bool setFeature(const QByteArray &feature);
    std::optional<QByteArray> getFeature(std::uint8_t reportId, int length);
    // Blocking read with timeout; only valid when opened with watch = false.
    std::optional<QByteArray> readReport(int timeoutMs);

signals:
    void reportReceived(const QByteArray &report);
    // Emitted when the node disappears (dongle unplugged) or reading fails irrecoverably.
    void disconnected();

private:
    void onReadable();
    void fail(const QString &what);

    int m_fd = -1;
    QString m_path;
    QString m_error;
    QSocketNotifier *m_notifier = nullptr;
};
