#include "V9Protocol.h"

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
