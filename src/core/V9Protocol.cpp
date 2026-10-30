#include "V9Protocol.h"

#include <algorithm>

namespace v9::protocol {

namespace {

QByteArray report(std::initializer_list<std::uint8_t> bytes)
{
    QByteArray buffer(ReportSize, '\0');
    int i = 0;
    for (std::uint8_t b : bytes)
        buffer[i++] = static_cast<char>(b);
    return buffer;
}

std::uint8_t byteAt(const QByteArray &data, int index)
{
    return static_cast<std::uint8_t>(data.at(index));
}

} // namespace

QByteArray batteryRequest()
{
    return report({ControlReportId, CmdBattery, 0x01});
}

QByteArray modeQueryRequest()
{
    return report({ControlReportId, CmdModeQuery});
}

QByteArray modeSetRequest(SoundMode mode)
{
    return report({ControlReportId, CmdModeSet, 0x01, static_cast<std::uint8_t>(mode)});
}

QByteArray firmwareSelectFeature(FirmwareTarget target)
{
    return report({FirmwareReportId, 0x01, static_cast<std::uint8_t>(target)});
}

QByteArray legacyRequest(LegacyCommand command, std::uint8_t value)
{
    return report({ControlReportId, CmdLegacy, static_cast<std::uint8_t>(command), value});
}

std::optional<int> parseLegacyReply(const QByteArray &report, LegacyCommand command)
{
    if (report.size() < 4 || !isReply(report, CmdLegacy) || byteAt(report, 2) != static_cast<std::uint8_t>(command))
        return std::nullopt;
    return byteAt(report, 3);
}

QByteArray otaEnterRequest(FirmwareTarget target)
{
    return report({ControlReportId, CmdOtaEnter, target == FirmwareTarget::Headset ? std::uint8_t(1) : std::uint8_t(0)});
}

QByteArray otaDataReport(QByteArrayView chunk)
{
    QByteArray buffer(ReportSize, '\0');
    buffer[0] = static_cast<char>(OtaDataReportId);
    const qsizetype n = qMin<qsizetype>(chunk.size(), OtaChunkSize);
    std::copy_n(chunk.data(), n, buffer.data() + 1);
    return buffer;
}

std::optional<OtaEvent> parseOtaEvent(const QByteArray &report)
{
    if (report.size() < 4 || byteAt(report, 0) != ControlReportId)
        return std::nullopt;
    const std::uint8_t a = byteAt(report, 1);
    const std::uint8_t b = byteAt(report, 2);
    const std::uint8_t c = byteAt(report, 3);
    if (a == 0x02 && b == 0x01 && c == 0x00)
        return OtaEvent::Ready;
    if (a == 0x60 && b == 0xFF)
        return OtaEvent::Pause;
    if (a == 0x60 && b == 0x00)
        return OtaEvent::Resume;
    if (a == 0x01 && c == 0x00 && (b == 0x01 || b == 0x00))
        return b == 0x01 ? OtaEvent::Success : OtaEvent::Failure;
    return std::nullopt;
}

QString describeReport(const QByteArray &report)
{
    if (report.isEmpty())
        return {};
    if (const auto battery = parseBattery(report))
        return QStringLiteral("battery %1%, state %2").arg(battery->percent).arg(battery->rawState);
    if (const auto mode = parseMode(report))
        return QStringLiteral("sound mode %1").arg(static_cast<int>(*mode));
    if (isReply(report, CmdLegacy) && report.size() >= 4)
        return QStringLiteral("legacy 0x22 sub %1 value %2").arg(byteAt(report, 2)).arg(byteAt(report, 3));
    if (const auto ota = parseOtaEvent(report)) {
        static const char *names[] = {"ota ready", "ota pause", "ota resume", "ota success", "ota failure"};
        return QString::fromLatin1(names[static_cast<int>(*ota)]);
    }
    switch (byteAt(report, 0)) {
    case ConsumerReportId:
        return QStringLiteral("consumer keys 0x%1").arg(report.size() > 1 ? byteAt(report, 1) : 0, 2, 16, QLatin1Char('0'));
    case TelephonyReportId:
        return QStringLiteral("TELEPHONY (hook/mute) 0x%1").arg(report.size() > 1 ? byteAt(report, 1) : 0, 2, 16, QLatin1Char('0'));
    case 0xED:
        return QStringLiteral("vendor 0xED");
    case ControlReportId:
        return QStringLiteral("unknown 0x55 command 0x%1").arg(report.size() > 1 ? byteAt(report, 1) : 0, 2, 16, QLatin1Char('0'));
    default:
        return QStringLiteral("report 0x%1").arg(byteAt(report, 0), 2, 16, QLatin1Char('0'));
    }
}

bool isReply(const QByteArray &report, std::uint8_t command)
{
    return report.size() >= 2 && byteAt(report, 0) == ControlReportId && byteAt(report, 1) == command;
}

std::optional<BatteryInfo> parseBattery(const QByteArray &report)
{
    if (report.size() < 4 || !isReply(report, CmdBattery))
        return std::nullopt;

    const int percent = byteAt(report, 2);
    if (percent > 100)
        return std::nullopt;

    BatteryInfo info;
    info.percent = percent;
    info.rawState = byteAt(report, 3);
    switch (info.rawState) {
    case 2:
    case 3:
    case 4:
    case 26:
        info.state = static_cast<ChargeState>(info.rawState);
        break;
    default:
        info.state = ChargeState::Unknown;
        break;
    }
    return info;
}

std::optional<SoundMode> parseMode(const QByteArray &report)
{
    if (report.size() < 3 || !isReply(report, CmdModeQuery))
        return std::nullopt;

    const std::uint8_t mode = byteAt(report, 2);
    if (mode > static_cast<std::uint8_t>(SoundMode::Music))
        return std::nullopt;
    return static_cast<SoundMode>(mode);
}

std::optional<FirmwareInfo> parseFirmware(const QByteArray &feature)
{
    if (feature.size() < 6 || byteAt(feature, 0) != FirmwareReportId || byteAt(feature, 1) != 0x01)
        return std::nullopt;

    FirmwareInfo info;
    for (int i = 2; i < 6; ++i)
        info.version += QString::number(byteAt(feature, i));
    info.rawHex = QString::fromLatin1(feature.first(qMin<qsizetype>(16, feature.size())).toHex(' '));
    return info;
}

bool descriptorHasReportId(const QByteArray &descriptor, std::uint8_t reportId)
{
    // Short items: prefix byte = tag(4) | type(2) | size(2). Report ID is global item tag 8 → 0x85.
    qsizetype i = 0;
    while (i < descriptor.size()) {
        const std::uint8_t prefix = byteAt(descriptor, i);
        if (prefix == 0xFE) { // long item: [FE, dataSize, longTag, data...]
            if (i + 1 >= descriptor.size())
                return false;
            i += 3 + byteAt(descriptor, i + 1);
            continue;
        }
        static constexpr int sizes[] = {0, 1, 2, 4};
        const int size = sizes[prefix & 0x03];
        if ((prefix & 0xFC) == 0x84 && size == 1 && i + 1 < descriptor.size() && byteAt(descriptor, i + 1) == reportId)
            return true;
        i += 1 + size;
    }
    return false;
}

} // namespace v9::protocol
