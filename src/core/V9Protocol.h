#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <optional>

// Byte layouts of the MCHOSE V9 PRO vendor HID protocol. Everything here is pure so it can be unit
// tested.
namespace v9::protocol {

inline constexpr std::uint16_t VendorId = 0x291D;
inline constexpr std::uint16_t ProductId = 0x385D;

inline constexpr std::uint8_t ControlReportId = 0x55;
inline constexpr std::uint8_t FirmwareReportId = 0xAA;
inline constexpr std::uint8_t ConsumerReportId = 0x06;
inline constexpr std::uint8_t TelephonyReportId = 0x07;

// hidraw output/input reports are the report id followed by 63 payload bytes.
inline constexpr int ReportSize = 64;

inline constexpr std::uint8_t CmdBattery = 0x65;
inline constexpr std::uint8_t CmdModeQuery = 0x11;
inline constexpr std::uint8_t CmdModeSet = 0x21;
// Legacy settings family. Official for V9 TURBO; UNTESTED on V9 PRO.
inline constexpr std::uint8_t CmdLegacy = 0x22;

enum class LegacyCommand : std::uint8_t {
    SetAutoShutdown = 0,
    FactoryReset = 1,
    GetAutoShutdown = 2,
    SetPromptVolume = 3,
    GetPromptVolume = 4,
};

enum class ChargeState : std::uint8_t {
    Unknown = 0,
    Discharging = 2,
    Charging = 3,
    Full = 4,
    Asleep = 26,
};

// On-device DSP presets. The device clamps anything above Music.
enum class SoundMode : std::uint8_t {
    GameOne = 0,
    GameTwo = 1,
    Music = 2,
};

enum class FirmwareTarget : std::uint8_t {
    Headset = 0,
    Dongle = 1,
};

struct BatteryInfo {
    int percent = 0;
    ChargeState state = ChargeState::Unknown;
    std::uint8_t rawState = 0;

    bool operator==(const BatteryInfo &) const = default;
};

struct FirmwareInfo {
    QString version; // Hub interpretation: decimal concatenation of bytes 2..5
    QString rawHex;  // first 16 bytes, kept until the version layout is confirmed

    bool operator==(const FirmwareInfo &) const = default;
};

QByteArray batteryRequest();
QByteArray modeQueryRequest();
QByteArray modeSetRequest(SoundMode mode);

// Feature report buffer that selects which firmware version the next GET_FEATURE returns.
QByteArray firmwareSelectFeature(FirmwareTarget target);

QByteArray legacyRequest(LegacyCommand command, std::uint8_t value = 0);
// Value byte of a legacy reply (55 22 <sub> <value>) answering `command`.
std::optional<int> parseLegacyReply(const QByteArray &report, LegacyCommand command);

// Human readable decoding of any input report from the device, for the HID monitor.
QString describeReport(const QByteArray &report);

std::optional<BatteryInfo> parseBattery(const QByteArray &report);
std::optional<SoundMode> parseMode(const QByteArray &report);
std::optional<FirmwareInfo> parseFirmware(const QByteArray &feature);

// True when `report` is an input report answering `command` (report id 0x55, byte 1 == command).
bool isReply(const QByteArray &report, std::uint8_t command);

// Scans a raw HID report descriptor for a Report ID item with the given id.
bool descriptorHasReportId(const QByteArray &descriptor, std::uint8_t reportId);

} // namespace v9::protocol
