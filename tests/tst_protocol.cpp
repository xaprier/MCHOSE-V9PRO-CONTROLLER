#include "AudioMixer.h"
#include "V9Protocol.h"

#include <QTest>

namespace proto = v9::protocol;

namespace {

QByteArray bytes(std::initializer_list<int> values)
{
    QByteArray data;
    for (int v : values)
        data.append(static_cast<char>(v));
    return data;
}

} // namespace

class TestProtocol : public QObject
{
    Q_OBJECT

private slots:
    void requestsArePaddedReports()
    {
        const QByteArray battery = proto::batteryRequest();
        QCOMPARE(battery.size(), proto::ReportSize);
        QCOMPARE(battery.first(3), bytes({0x55, 0x65, 0x01}));
        QCOMPARE(battery.at(3), '\0');

        QCOMPARE(proto::modeQueryRequest().first(3), bytes({0x55, 0x11, 0x00}));
        QCOMPARE(proto::modeSetRequest(proto::SoundMode::GameTwo).first(4), bytes({0x55, 0x21, 0x01, 0x01}));
        QCOMPARE(proto::firmwareSelectFeature(proto::FirmwareTarget::Dongle).first(3), bytes({0xAA, 0x01, 0x01}));
    }

    void parsesBatteryCapturedOnDevice()
    {
        // Captured 2026-10-30 from a real V9 PRO: 100 %, discharging.
        const auto info = proto::parseBattery(bytes({0x55, 0x65, 0x64, 0x02, 0, 0, 0, 0}));
        QVERIFY(info);
        QCOMPARE(info->percent, 100);
        QCOMPARE(info->state, proto::ChargeState::Discharging);
    }

    void batteryStates()
    {
        QCOMPARE(proto::parseBattery(bytes({0x55, 0x65, 40, 3}))->state, proto::ChargeState::Charging);
        QCOMPARE(proto::parseBattery(bytes({0x55, 0x65, 100, 4}))->state, proto::ChargeState::Full);
        QCOMPARE(proto::parseBattery(bytes({0x55, 0x65, 70, 26}))->state, proto::ChargeState::Asleep);
        QCOMPARE(proto::parseBattery(bytes({0x55, 0x65, 70, 9}))->state, proto::ChargeState::Unknown);
    }

    void rejectsForeignOrInvalidBattery()
    {
        QVERIFY(!proto::parseBattery(bytes({0x55, 0x11, 0x02, 0x00})));
        QVERIFY(!proto::parseBattery(bytes({0x55, 0x65, 0xC8, 0x02})));
        QVERIFY(!proto::parseBattery(bytes({0x55, 0x65})));
    }

    void parsesMode()
    {
        QCOMPARE(*proto::parseMode(bytes({0x55, 0x11, 0x02})), proto::SoundMode::Music);
        QCOMPARE(*proto::parseMode(bytes({0x55, 0x11, 0x00})), proto::SoundMode::GameOne);
        QVERIFY(!proto::parseMode(bytes({0x55, 0x11, 0x05})));
        QVERIFY(!proto::parseMode(bytes({0x55, 0x65, 0x02})));
    }

    void parsesFirmwareCapturedOnDevice()
    {
        const auto info = proto::parseFirmware(
            bytes({0xaa, 0x01, 0x00, 0x00, 0x01, 0x02, 0xff, 0x25, 0x07, 0x08, 0x18, 0x08, 0x36, 0x34, 0x46, 0x30, 0x00}));
        QVERIFY(info);
        QCOMPARE(info->version, QStringLiteral("0012"));
        QCOMPARE(info->rawHex, QStringLiteral("aa 01 00 00 01 02 ff 25 07 08 18 08 36 34 46 30"));
        QVERIFY(!proto::parseFirmware(bytes({0x55, 0x01, 0, 0, 0, 0})));
    }

    void findsReportIdInRealDescriptor()
    {
        // Tail of the V9 PRO descriptor: vendor page 0xFF90 collection with Report ID 0x55.
        const QByteArray descriptor = bytes({0x06, 0x90, 0xff, 0x09, 0x01, 0xa1, 0x01, 0x09, 0x03, 0x85, 0x55,
                                             0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x3f, 0x82, 0x02, 0x01, 0xc0});
        QVERIFY(proto::descriptorHasReportId(descriptor, 0x55));
        QVERIFY(!proto::descriptorHasReportId(descriptor, 0x41));
        // 0x55 appearing as data of another item (Logical Maximum) must not match.
        QVERIFY(!proto::descriptorHasReportId(bytes({0x25, 0x55, 0x85, 0x06}), 0x55));
    }

    void findsHeadsetInPactlJson()
    {
        const QByteArray json = R"([
            {"name":"alsa_output.pci-0000_00_1f.3.analog-stereo","mute":false,
             "volume":{"front-left":{"value_percent":"30%"}},"properties":{"alsa.card":"0"}},
            {"name":"alsa_output.usb-C-Media_Electronics_Inc_MCHOSE_V9_PRO_0123456789AB-01.analog-stereo","mute":true,
             "volume":{"front-left":{"value_percent":"80%"},"front-right":{"value_percent":"81%"}},
             "properties":{"alsa.card":"2"}}
        ])";
        const auto endpoint = AudioMixer::findHeadsetEndpoint(json);
        QVERIFY(endpoint);
        QVERIFY(endpoint->name.contains(QStringLiteral("MCHOSE_V9_PRO")));
        QCOMPARE(endpoint->volume, 81);
        QVERIFY(endpoint->muted);

        QVERIFY(!AudioMixer::findHeadsetEndpoint(R"([{"name":"alsa_output.usb-..._MCHOSE_V9_PRO_x.monitor"}])"));
    }
};

QTEST_GUILESS_MAIN(TestProtocol)
#include "tst_protocol.moc"
