#include "FirmwareFlasher.h"

#include "HidRawDevice.h"
#include "V9Protocol.h"

#include <QElapsedTimer>
#include <QThread>

#include <optional>

namespace proto = v9::protocol;

namespace {

// Official tool waits 5 s for READY; the wireless relay to the headset can take longer.
constexpr int ReadyTimeoutMs = 20000;
constexpr int ResumeTimeoutMs = 15000;
constexpr int ResultTimeoutMs = 90000;
constexpr int WriteRetries = 3;
constexpr int WriteBackoffMs = 15;
constexpr int ProgressEveryChunks = 64;

} // namespace

FirmwareFlasher::FirmwareFlasher(QObject *parent)
    : QObject(parent)
{
}

FirmwareFlasher::~FirmwareFlasher()
{
    abort();
    if (m_thread) {
        m_thread->wait();
        delete m_thread;
    }
}

bool FirmwareFlasher::isBusy() const
{
    return m_thread && m_thread->isRunning();
}

void FirmwareFlasher::start(const QString &hidrawNode, const QByteArray &image, bool toHeadset)
{
    if (isBusy())
        return;
    if (m_thread) {
        m_thread->wait();
        delete m_thread;
    }
    m_abort = false;
    m_thread = QThread::create([this, hidrawNode, image, toHeadset] { run(hidrawNode, image, toHeadset); });
    m_thread->start();
}

void FirmwareFlasher::abort()
{
    m_abort = true;
}

void FirmwareFlasher::post(State state)
{
    QMetaObject::invokeMethod(this, [this, state] {
        m_state = state;
        emit stateChanged(state);
    }, Qt::QueuedConnection);
}

void FirmwareFlasher::postLog(const QString &message)
{
    QMetaObject::invokeMethod(this, [this, message] { emit logMessage(message); }, Qt::QueuedConnection);
}

void FirmwareFlasher::run(QString node, QByteArray image, bool toHeadset)
{
    auto finish = [this](bool success, const QString &message) {
        postLog(message);
        post(success ? State::Succeeded : State::Failed);
        QMetaObject::invokeMethod(this, [this, success, message] { emit finished(success, message); }, Qt::QueuedConnection);
    };

    HidRawDevice device;
    if (!device.open(node, false)) {
        finish(false, tr("Cannot open %1: %2").arg(node, device.errorString()));
        return;
    }
    while (device.readReport(0)) {
    }

    // Waits for a specific OTA event; failure events end the wait early.
    auto waitFor = [&device, this](proto::OtaEvent wanted, int timeoutMs) -> std::optional<proto::OtaEvent> {
        QElapsedTimer timer;
        timer.start();
        while (!m_abort && timer.elapsed() < timeoutMs) {
            const auto report = device.readReport(int(qMin<qint64>(250, timeoutMs - timer.elapsed())));
            if (!report)
                continue;
            const auto event = proto::parseOtaEvent(*report);
            if (event && (*event == wanted || *event == proto::OtaEvent::Failure))
                return event;
        }
        return std::nullopt;
    };

    post(State::Entering);
    postLog(tr("Entering update mode (%1 bytes)…").arg(image.size()));
    const auto target = toHeadset ? proto::FirmwareTarget::Headset : proto::FirmwareTarget::Dongle;
    if (!device.writeReport(proto::otaEnterRequest(target))) {
        finish(false, tr("Enter update failed: %1").arg(device.errorString()));
        return;
    }
    const auto ready = waitFor(proto::OtaEvent::Ready, ReadyTimeoutMs);
    if (ready != proto::OtaEvent::Ready) {
        finish(false, m_abort ? tr("Aborted before streaming.") : tr("Headset did not report READY. Is it on and near the dongle?"));
        return;
    }

    post(State::Streaming);
    postLog(tr("Device ready, streaming…"));
    const qsizetype chunks = (image.size() + proto::OtaChunkSize - 1) / proto::OtaChunkSize;
    bool paused = false;
    for (qsizetype i = 0; i < chunks; ++i) {
        if (m_abort) {
            finish(false, tr("Aborted at chunk %1 of %2. The headset keeps its current firmware.").arg(i).arg(chunks));
            return;
        }

        // Drain flow control without blocking; block only while the device asked us to pause.
        while (const auto report = device.readReport(0)) {
            const auto event = proto::parseOtaEvent(*report);
            if (event == proto::OtaEvent::Pause)
                paused = true;
            else if (event == proto::OtaEvent::Resume)
                paused = false;
            else if (event == proto::OtaEvent::Failure) {
                finish(false, tr("Headset rejected the image at chunk %1.").arg(i));
                return;
            }
        }
        if (paused) {
            const auto resumed = waitFor(proto::OtaEvent::Resume, ResumeTimeoutMs);
            if (resumed != proto::OtaEvent::Resume) {
                finish(false, tr("Transfer stalled at chunk %1 (no resume).").arg(i));
                return;
            }
            paused = false;
        }

        const QByteArray report = proto::otaDataReport(QByteArrayView(image).sliced(i * proto::OtaChunkSize).first(
            qMin<qsizetype>(proto::OtaChunkSize, image.size() - i * proto::OtaChunkSize)));
        bool written = false;
        for (int attempt = 0; attempt < WriteRetries && !written; ++attempt) {
            written = device.writeReport(report);
            if (!written)
                QThread::msleep(WriteBackoffMs);
        }
        if (!written) {
            finish(false, tr("Write failed at chunk %1: %2").arg(i).arg(device.errorString()));
            return;
        }
        if (i % ProgressEveryChunks == 0 || i == chunks - 1) {
            const double progress = double(i + 1) / double(chunks);
            QMetaObject::invokeMethod(this, [this, progress] { emit progressChanged(progress); }, Qt::QueuedConnection);
        }
    }

    post(State::Finishing);
    postLog(tr("All %1 chunks sent, waiting for the headset to verify…").arg(chunks));
    const auto result = waitFor(proto::OtaEvent::Success, ResultTimeoutMs);
    if (result == proto::OtaEvent::Success)
        finish(true, tr("Firmware accepted. The headset restarts by itself."));
    else if (result == proto::OtaEvent::Failure)
        finish(false, tr("Headset reported failure; it keeps its previous firmware."));
    else
        finish(false, tr("No result from the headset within %1 s.").arg(ResultTimeoutMs / 1000));
}
