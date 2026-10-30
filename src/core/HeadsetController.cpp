#include "HeadsetController.h"

#include "HidRawDevice.h"
#include "V9Protocol.h"

#include <QTimer>

#include <iterator>

namespace proto = v9::protocol;

namespace {

constexpr int ScanIntervalMs = 2000;
constexpr int PollIntervalMs = 15000;
constexpr int ReplyTimeoutMs = 800;
constexpr int RetryDelayMs = 250;
constexpr int InterRequestGapMs = 30;

HeadsetController::ChargeState toChargeState(proto::ChargeState state)
{
    switch (state) {
    case proto::ChargeState::Discharging:
        return HeadsetController::ChargeState::Discharging;
    case proto::ChargeState::Charging:
        return HeadsetController::ChargeState::Charging;
    case proto::ChargeState::Full:
        return HeadsetController::ChargeState::Full;
    case proto::ChargeState::Asleep:
        return HeadsetController::ChargeState::Asleep;
    case proto::ChargeState::Unknown:
        break;
    }
    return HeadsetController::ChargeState::Unknown;
}

} // namespace

HeadsetController::HeadsetController(QObject *parent)
    : QObject(parent)
    , m_device(new HidRawDevice(this))
    , m_scanTimer(new QTimer(this))
    , m_pollTimer(new QTimer(this))
    , m_replyTimer(new QTimer(this))
{
    m_scanTimer->setInterval(ScanIntervalMs);
    m_pollTimer->setInterval(PollIntervalMs);
    m_replyTimer->setSingleShot(true);
    m_replyTimer->setInterval(ReplyTimeoutMs);

    connect(m_scanTimer, &QTimer::timeout, this, &HeadsetController::scan);
    connect(m_pollTimer, &QTimer::timeout, this, &HeadsetController::refresh);
    connect(m_replyTimer, &QTimer::timeout, this, &HeadsetController::onTimeout);
    connect(m_device, &HidRawDevice::reportReceived, this, &HeadsetController::onReport);
    connect(m_device, &HidRawDevice::disconnected, this, &HeadsetController::onDisconnected);

    m_scanTimer->start();
    QTimer::singleShot(0, this, &HeadsetController::scan);
}

HeadsetController::~HeadsetController() = default;

bool HeadsetController::dongleConnected() const
{
    return m_device->isOpen();
}

QString HeadsetController::devicePath() const
{
    return m_device->path();
}

void HeadsetController::scan()
{
    if (m_device->isOpen())
        return;

    const auto node = HidRawDevice::findNode(proto::VendorId, proto::ProductId, proto::ControlReportId);
    if (!node)
        return;

    if (!m_device->open(*node)) {
        setLastError(tr("Cannot open %1. Install the udev rule (packaging/70-mchose-v9pro.rules).").arg(*node));
        return;
    }

    setLastError({});
    m_scanTimer->stop();
    m_pollTimer->start();
    emit connectionChanged();
    readFirmware();
    refresh();
}

void HeadsetController::onDisconnected()
{
    m_replyTimer->stop();
    m_queue.clear();
    m_inFlight = false;
    m_pollTimer->stop();
    resetState();
    emit connectionChanged();
    m_scanTimer->start();
}

void HeadsetController::refresh()
{
    if (!m_device->isOpen())
        return;

    enqueue({proto::batteryRequest(), proto::CmdBattery, 1, [this](bool answered) {
                 // No battery reply means the dongle is there but the headset is off or out of range.
                 if (!answered)
                     setHeadsetOnline(false);
             }});
    enqueue({proto::modeQueryRequest(), proto::CmdModeQuery, 1, {}});
}

void HeadsetController::setSoundMode(int mode)
{
    if (mode < GameOne || mode > Music || !m_device->isOpen())
        return;

    if (m_soundMode != mode) {
        m_soundMode = mode;
        emit soundModeChanged();
    }
    enqueue({proto::modeSetRequest(static_cast<proto::SoundMode>(mode)), -1, 0, {}});
    // The set is unacknowledged; read back so the UI reflects what the device actually took.
    enqueue({proto::modeQueryRequest(), proto::CmdModeQuery, 1, {}});
}

void HeadsetController::readFirmware()
{
    // A second SET_FEATURE while an answer is pending makes the dongle stall both.
    if (m_firmwareReading)
        return;
    m_firmwareReading = true;
    readFirmwareStep(static_cast<int>(proto::FirmwareTarget::Dongle));
}

void HeadsetController::readFirmwareStep(int target)
{
    const auto firmwareTarget = static_cast<proto::FirmwareTarget>(target);
    if (!m_device->isOpen() || !m_device->setFeature(proto::firmwareSelectFeature(firmwareTarget))) {
        m_firmwareReading = false;
        return;
    }
    collectFirmware(target, 0);
}

void HeadsetController::collectFirmware(int target, int attempt)
{
    // The answer is not ready immediately; GET_FEATURE stalls (EPIPE) until it is. Same back-off
    // as MCHOSE HUB.
    static constexpr int delaysMs[] = {300, 500, 900};
    if (attempt >= int(std::size(delaysMs))) {
        if (target == static_cast<int>(proto::FirmwareTarget::Dongle))
            readFirmwareStep(static_cast<int>(proto::FirmwareTarget::Headset));
        else
            m_firmwareReading = false;
        return;
    }

    QTimer::singleShot(delaysMs[attempt], this, [this, target, attempt] {
        if (!m_device->isOpen()) {
            m_firmwareReading = false;
            return;
        }
        const auto feature = m_device->getFeature(proto::FirmwareReportId, proto::ReportSize);
        const auto info = feature ? proto::parseFirmware(*feature) : std::nullopt;
        if (!info) {
            collectFirmware(target, attempt + 1);
            return;
        }
        if (target == static_cast<int>(proto::FirmwareTarget::Dongle)) {
            m_dongleFirmware = info->version;
            m_dongleFirmwareRaw = info->rawHex;
            emit firmwareChanged();
            readFirmwareStep(static_cast<int>(proto::FirmwareTarget::Headset));
        } else {
            m_headsetFirmware = info->version;
            m_headsetFirmwareRaw = info->rawHex;
            m_firmwareReading = false;
            emit firmwareChanged();
        }
    });
}

void HeadsetController::onReport(const QByteArray &report)
{
    // Replies and unsolicited pushes share one format, so state is updated from any report.
    if (const auto battery = proto::parseBattery(report)) {
        const ChargeState state = toChargeState(battery->state);
        m_batteryPercent = battery->percent;
        m_chargeState = state;
        m_lastBatteryUpdate = QDateTime::currentDateTime();
        setHeadsetOnline(state != ChargeState::Asleep);
        emit batteryChanged();
    } else if (const auto mode = proto::parseMode(report)) {
        const int value = static_cast<int>(*mode);
        if (m_soundMode != value) {
            m_soundMode = value;
            emit soundModeChanged();
        }
    }

    if (m_inFlight && !m_queue.empty() && m_queue.front().replyCommand >= 0
        && proto::isReply(report, static_cast<std::uint8_t>(m_queue.front().replyCommand))) {
        m_replyTimer->stop();
        finishCurrent(true);
    }
}

void HeadsetController::enqueue(Request request)
{
    m_queue.push_back(std::move(request));
    processNext();
}

void HeadsetController::processNext()
{
    if (m_inFlight || m_queue.empty() || !m_device->isOpen())
        return;

    Request &request = m_queue.front();
    m_inFlight = true;
    if (!m_device->writeReport(request.data)) {
        setLastError(m_device->errorString());
        finishCurrent(false);
        return;
    }

    if (request.replyCommand < 0) {
        QTimer::singleShot(InterRequestGapMs, this, [this] { finishCurrent(true); });
        return;
    }
    m_replyTimer->start();
}

void HeadsetController::finishCurrent(bool answered)
{
    if (!m_inFlight || m_queue.empty())
        return;

    Request request = std::move(m_queue.front());
    m_queue.pop_front();
    m_inFlight = false;
    if (request.done)
        request.done(answered);
    QTimer::singleShot(InterRequestGapMs, this, &HeadsetController::processNext);
}

void HeadsetController::onTimeout()
{
    if (!m_inFlight || m_queue.empty())
        return;

    // The first exchange after an idle period is occasionally dropped by the dongle.
    Request &request = m_queue.front();
    if (request.retriesLeft > 0) {
        --request.retriesLeft;
        m_inFlight = false;
        QTimer::singleShot(RetryDelayMs, this, &HeadsetController::processNext);
        return;
    }
    finishCurrent(false);
}

void HeadsetController::setHeadsetOnline(bool online)
{
    if (m_headsetOnline == online)
        return;
    m_headsetOnline = online;
    emit connectionChanged();
    if (online && m_headsetFirmware.isEmpty())
        readFirmware();
}

void HeadsetController::setLastError(const QString &error)
{
    if (m_lastError == error)
        return;
    m_lastError = error;
    emit lastErrorChanged();
}

void HeadsetController::resetState()
{
    m_firmwareReading = false;
    m_headsetOnline = false;
    m_batteryPercent = -1;
    m_chargeState = ChargeState::Unknown;
    m_soundMode = -1;
    m_dongleFirmware.clear();
    m_headsetFirmware.clear();
    m_dongleFirmwareRaw.clear();
    m_headsetFirmwareRaw.clear();
    emit batteryChanged();
    emit soundModeChanged();
    emit firmwareChanged();
}
