#include "AudioMixer.h"
#include "EqualizerEngine.h"
#include "EqualizerMath.h"
#include "EqualizerProfile.h"
#include "FilterChainConfig.h"
#include "V9Protocol.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTest>

namespace eq = v9::eq;
namespace pw = v9::pw;
namespace proto = v9::protocol;

class TestEngines : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    // Hub-style profile handling: built-ins get "modified", custom profiles save edits into themselves.
    void equalizerProfileFlow()
    {
        QSettings().clear();
        AudioMixer mixer;
        EqualizerEngine engine(&mixer);
        QDir(engine.profileDirectory()).removeRecursively();
        engine.deleteProfile(QStringLiteral("none")); // reloads the (now empty) custom list
        QVERIFY(engine.customProfiles().isEmpty());

        const QString builtIn = engine.builtInProfiles().at(1);
        QVERIFY(engine.applyProfile(builtIn));
        QCOMPARE(engine.currentProfile(), builtIn);
        QVERIFY(!engine.modified());
        engine.setBand(5, 7.5);
        QVERIFY(engine.modified());
        engine.reset();
        QVERIFY(!engine.modified());

        engine.setBand(5, 7.5);
        const QString custom = engine.createProfile();
        QCOMPARE(custom, QStringLiteral("Custom 1"));
        QCOMPARE(engine.currentProfile(), custom);
        QVERIFY(!engine.modified());

        engine.setBand(0, -3.0); // saved into the custom profile once the debounce flushes
        QVERIFY(engine.applyProfile(builtIn));
        QVERIFY(engine.applyProfile(custom));
        QCOMPARE(engine.gains().at(0).toDouble(), -3.0);
        QCOMPARE(engine.gains().at(5).toDouble(), 7.5);

        QVERIFY(engine.renameProfile(custom, QStringLiteral("Mine")));
        QCOMPARE(engine.customProfiles(), QStringList{QStringLiteral("Mine")});
        QCOMPARE(engine.currentProfile(), QStringLiteral("Mine"));
        QVERIFY(!engine.renameProfile(QStringLiteral("Mine"), builtIn));

        QVERIFY(engine.deleteProfile(QStringLiteral("Mine")));
        QVERIFY(engine.currentProfile().isEmpty());
        QVERIFY(engine.customProfiles().isEmpty());
        QVERIFY(!engine.deleteProfile(builtIn));
    }

    void flatEqualizerIsTransparent()
    {
        const eq::Settings flat;
        for (double f : {20.0, 100.0, 1000.0, 10000.0, 20000.0})
            QVERIFY(qAbs(eq::responseDb(flat, f)) < 1e-9);
        QCOMPARE(eq::headroomPreampDb(flat), 0.0);
    }

    void peakingBandHitsItsGainAtCentre()
    {
        eq::Settings settings;
        settings.gains[5] = 6.0; // 1 kHz
        QVERIFY(qAbs(eq::responseDb(settings, 1000.0) - 6.0) < 0.05);
        QVERIFY(eq::responseDb(settings, 100.0) < 0.5);
        QVERIFY(eq::headroomPreampDb(settings) <= -5.9);
    }

    void overlappingBandsStillHitTheirSliders()
    {
        eq::Settings settings;
        settings.gains = {13.0, 5.0, 20.0, 0.0, 0.0, -6.0, 0.0, 0.0, 0.0, 0.0};
        for (int i = 0; i < eq::BandCount; ++i)
            QVERIFY2(qAbs(eq::responseDb(settings, eq::bands()[i].frequency) - settings.gains[i]) < 0.1, eq::bands()[i].label);
        QVERIFY(eq::headroomPreampDb(settings) >= -20.5);
    }

    void enhancementsOnlyActWhenEnabled()
    {
        eq::Settings settings;
        settings.enhancements.bassLevel = 10;
        QCOMPARE(eq::responseDb(settings, 40.0), 0.0);
        settings.enhancements.bassEnabled = true;
        QVERIFY(eq::responseDb(settings, 40.0) > 8.0);
    }

    void equalizerConfigTargetsHeadset()
    {
        eq::Settings settings;
        settings.gains[0] = 3.5;
        const QString config = pw::equalizerConfig(settings, QStringLiteral("alsa_output.usb-X_MCHOSE_V9_PRO"));
        QVERIFY(config.contains(QStringLiteral("libpipewire-module-filter-chain")));
        QVERIFY(config.contains(QStringLiteral("target.object = \"alsa_output.usb-X_MCHOSE_V9_PRO\"")));
        QVERIFY(config.contains(QStringLiteral("node.name = \"v9pro_equalizer\"")));
        QVERIFY(config.contains(QStringLiteral("name = eq_1 label = bq_lowshelf")));
        // Band gains are written after overlap compensation, not as the raw slider value.
        const QString gain = QString::number(eq::filters(settings).first().gainDb, 'f', 4);
        QVERIFY(config.contains(QStringLiteral("\"Gain\" = ") + gain));
        QCOMPARE(config.count(QStringLiteral("label = bq_")), eq::BandCount + 3);
        QVERIFY(config.contains(QStringLiteral("outputs = [ \"clarity:Out\" ]")));

        const QString params = pw::equalizerParams(settings);
        QVERIFY(params.startsWith(QStringLiteral("{ params = [ \"preamp:Gain 1\"")));
        QVERIFY(params.contains(QStringLiteral("\"eq_1:Gain\" ") + gain));
    }

    void noiseAndMonitorConfigs()
    {
        const QString webrtc = pw::noiseSuppressionConfig(pw::NoiseEngine::WebRtc, QStringLiteral("mic"), 50);
        QVERIFY(webrtc.contains(QStringLiteral("libpipewire-module-echo-cancel")));
        QVERIFY(webrtc.contains(QStringLiteral("webrtc.noise_suppression = true")));
        const QString rnnoise = pw::noiseSuppressionConfig(pw::NoiseEngine::RNNoise, QStringLiteral("mic"), 70);
        QVERIFY(rnnoise.contains(QStringLiteral("\"VAD Threshold (%)\" = 70.0000")));
        QVERIFY(rnnoise.contains(QStringLiteral("media.class = Audio/Source")));
        const QString monitor = pw::micMonitorConfig(QStringLiteral("mic"), QStringLiteral("sink"), 0.25);
        QVERIFY(monitor.contains(QStringLiteral("target.object = \"mic\"")));
        QVERIFY(monitor.contains(QStringLiteral("target.object = \"sink\"")));
        QCOMPARE(pw::quoted(QStringLiteral("a\"b")), QStringLiteral("\"a\\\"b\""));
    }

    void profilesAreSanitised()
    {
        const auto profile = eq::parseProfile(R"({"name":" Bass ","data":[-30, 2.6, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]})", QStringLiteral("file"));
        QVERIFY(profile);
        QCOMPARE(profile->name, QStringLiteral("Bass"));
        QCOMPARE(profile->gains[0], -20.0);
        QCOMPARE(profile->gains[1], 3.0);
        QCOMPARE(profile->gains[9], 10.0);

        const auto shortProfile = eq::parseProfile(R"({"data":[1,2]})", QStringLiteral("fallback"));
        QCOMPARE(shortProfile->name, QStringLiteral("fallback"));
        QCOMPARE(shortProfile->gains[2], 0.0);

        QString error;
        QVERIFY(!eq::parseProfile("{\"name\":\"x\"}", QStringLiteral("f"), &error));
        QVERIFY(!error.isEmpty());

        const auto roundTrip = eq::parseProfile(eq::serializeProfile(*profile), QString());
        QCOMPARE(roundTrip->gains, profile->gains);
        QCOMPARE(eq::profileFileName(QStringLiteral("a/b:c")), QStringLiteral("a_b_c.json"));
    }

    // Opt-in: write the generated PipeWire configs for manual `pipewire -c` checks.
    void dumpConfigs()
    {
        const QString dir = qEnvironmentVariable("V9_DUMP_CONFIGS");
        if (dir.isEmpty())
            QSKIP("V9_DUMP_CONFIGS not set");
        const QString sink = qEnvironmentVariable("V9_SINK");
        const QString source = qEnvironmentVariable("V9_SOURCE");
        const QList<std::pair<QString, QString>> files{
            {QStringLiteral("eq.conf"), pw::equalizerConfig(eq::Settings{}, sink)},
            {QStringLiteral("ns-webrtc.conf"), pw::noiseSuppressionConfig(pw::NoiseEngine::WebRtc, source, 50)},
            {QStringLiteral("monitor.conf"), pw::micMonitorConfig(source, sink, 0.0)},
        };
        for (const auto &[name, text] : files) {
            QFile out(dir + QLatin1Char('/') + name);
            QVERIFY(out.open(QIODevice::WriteOnly | QIODevice::Truncate));
            out.write(text.toUtf8());
        }
    }

    void legacyProtocol()
    {
        QCOMPARE(proto::legacyRequest(proto::LegacyCommand::GetPromptVolume).first(4), QByteArray::fromHex("55220400"));
        QCOMPARE(*proto::parseLegacyReply(QByteArray::fromHex("55220407"), proto::LegacyCommand::GetPromptVolume), 7);
        QVERIFY(!proto::parseLegacyReply(QByteArray::fromHex("55220207"), proto::LegacyCommand::GetPromptVolume));

        QVERIFY(proto::describeReport(QByteArray::fromHex("0701")).startsWith(QStringLiteral("TELEPHONY")));
    }
};

QTEST_GUILESS_MAIN(TestEngines)
#include "tst_engines.moc"
