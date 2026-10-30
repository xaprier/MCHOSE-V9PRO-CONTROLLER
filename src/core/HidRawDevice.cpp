#include "HidRawDevice.h"

#include "V9Protocol.h"

#include <QDir>
#include <QFile>
#include <QSocketNotifier>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcHid, "v9.hid")

HidRawDevice::HidRawDevice(QObject *parent)
    : QObject(parent)
{
}

HidRawDevice::~HidRawDevice()
{
    close();
}

std::optional<QString> HidRawDevice::findNode(std::uint16_t vendorId, std::uint16_t productId, std::uint8_t reportId)
{
    // uevent carries "HID_ID=0003:0000291D:0000385D"; bus type varies, so match the ids only.
    const QString wanted = QStringLiteral(":%1:%2")
                               .arg(vendorId, 8, 16, QLatin1Char('0'))
                               .arg(productId, 8, 16, QLatin1Char('0'))
                               .toUpper();

    const QDir classDir(QStringLiteral("/sys/class/hidraw"));
    const QStringList nodes = classDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &node : nodes) {
        QFile uevent(classDir.filePath(node + QStringLiteral("/device/uevent")));
        if (!uevent.open(QIODevice::ReadOnly))
            continue;
        const QString text = QString::fromLatin1(uevent.readAll()).toUpper();
        if (!text.contains(QStringLiteral("HID_ID=")) || !text.contains(wanted))
            continue;

        QFile descriptor(classDir.filePath(node + QStringLiteral("/device/report_descriptor")));
        if (!descriptor.open(QIODevice::ReadOnly))
            continue;
        if (v9::protocol::descriptorHasReportId(descriptor.readAll(), reportId))
            return QStringLiteral("/dev/") + node;
    }
    return std::nullopt;
}

bool HidRawDevice::open(const QString &path, bool watch)
{
    close();

    const QByteArray native = QFile::encodeName(path);
    m_fd = ::open(native.constData(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (m_fd < 0) {
        m_error = QStringLiteral("%1: %2").arg(path, QString::fromLocal8Bit(std::strerror(errno)));
        qCWarning(lcHid) << "open failed" << m_error;
        return false;
    }

    m_path = path;
    m_error.clear();
    if (watch) {
        m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
        connect(m_notifier, &QSocketNotifier::activated, this, &HidRawDevice::onReadable);
    }
    qCInfo(lcHid) << "opened" << path;
    return true;
}

void HidRawDevice::close()
{
    if (m_notifier) {
        m_notifier->setEnabled(false);
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
    m_path.clear();
}

bool HidRawDevice::writeReport(const QByteArray &report)
{
    if (m_fd < 0)
        return false;

    const ssize_t written = ::write(m_fd, report.constData(), static_cast<size_t>(report.size()));
    if (written != report.size()) {
        fail(QStringLiteral("write"));
        return false;
    }
    qCDebug(lcHid) << "tx" << report.first(8).toHex(' ');
    return true;
}

bool HidRawDevice::setFeature(const QByteArray &feature)
{
    if (m_fd < 0)
        return false;

    QByteArray buffer = feature;
    if (::ioctl(m_fd, HIDIOCSFEATURE(buffer.size()), buffer.data()) < 0) {
        fail(QStringLiteral("set feature"));
        return false;
    }
    return true;
}

std::optional<QByteArray> HidRawDevice::getFeature(std::uint8_t reportId, int length)
{
    if (m_fd < 0)
        return std::nullopt;

    QByteArray buffer(length, '\0');
    buffer[0] = static_cast<char>(reportId);
    const int received = ::ioctl(m_fd, HIDIOCGFEATURE(buffer.size()), buffer.data());
    if (received < 0) {
        fail(QStringLiteral("get feature"));
        return std::nullopt;
    }
    buffer.truncate(received);
    return buffer;
}

std::optional<QByteArray> HidRawDevice::readReport(int timeoutMs)
{
    if (m_fd < 0)
        return std::nullopt;

    pollfd pfd{m_fd, POLLIN, 0};
    const int ready = ::poll(&pfd, 1, timeoutMs);
    if (ready <= 0)
        return std::nullopt;

    char buffer[v9::protocol::ReportSize];
    const ssize_t n = ::read(m_fd, buffer, sizeof buffer);
    if (n <= 0) {
        if (n < 0 && (errno == EAGAIN || errno == EINTR))
            return std::nullopt;
        fail(QStringLiteral("read"));
        return std::nullopt;
    }
    return QByteArray(buffer, static_cast<qsizetype>(n));
}

void HidRawDevice::onReadable()
{
    char buffer[v9::protocol::ReportSize];
    for (;;) {
        const ssize_t n = ::read(m_fd, buffer, sizeof buffer);
        if (n > 0) {
            const QByteArray report(buffer, static_cast<qsizetype>(n));
            qCDebug(lcHid) << "rx" << report.first(qMin<qsizetype>(8, report.size())).toHex(' ');
            emit reportReceived(report);
            if (m_fd < 0) // a slot may have closed us
                return;
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
            return;
        fail(QStringLiteral("read"));
        return;
    }
}

void HidRawDevice::fail(const QString &what)
{
    const int error = errno;
    m_error = QStringLiteral("%1 failed: %2").arg(what, QString::fromLocal8Bit(std::strerror(error)));
    qCWarning(lcHid) << m_error;

    // ENODEV/EIO mean the dongle went away; anything else (e.g. EPIPE on a stalled request) is
    // reported but keeps the node open.
    if (error == ENODEV || error == EIO || error == EBADF || what == QLatin1String("read")) {
        close();
        emit disconnected();
    }
}
