#include "FilterChainConfig.h"
#include "GraphLayout.h"
#include "LinkRules.h"
#include "PipeWireGraphState.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

namespace pw = v9::pw;

namespace {

QJsonArray parse(const char *json)
{
    return QJsonDocument::fromJson(json).array();
}

// One headset sink with two playback ports and a monitor port, one MIDI port, one video node, one
// Zen playback stream with FL/FR outputs and a link from Zen FL to the sink FL.
const char *Fixture = R"([
  {"id":54,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"alsa_output.usb-X","node.description":"V9 PRO","media.class":"Audio/Sink"}}},
  {"id":61,"type":"PipeWire:Interface:Port","info":{"direction":"input","props":{"node.id":54,"port.name":"playback_FL","format.dsp":"32 bit float mono audio"}}},
  {"id":62,"type":"PipeWire:Interface:Port","info":{"direction":"input","props":{"node.id":54,"port.name":"playback_FR","format.dsp":"32 bit float mono audio"}}},
  {"id":63,"type":"PipeWire:Interface:Port","info":{"direction":"output","props":{"node.id":54,"port.name":"monitor_FL","port.monitor":true,"format.dsp":"32 bit float mono audio"}}},
  {"id":45,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"midi-bridge","media.class":"Midi/Bridge"}}},
  {"id":46,"type":"PipeWire:Interface:Port","info":{"direction":"input","props":{"node.id":45,"port.name":"Midi Through Port-0","format.dsp":"8 bit raw midi"}}},
  {"id":73,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"v4l2_input","media.class":"Video/Source"}}},
  {"id":120,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"Zen","application.name":"Zen","media.class":"Stream/Output/Audio"}}},
  {"id":121,"type":"PipeWire:Interface:Port","info":{"direction":"output","props":{"node.id":120,"port.name":"output_FL","format.dsp":"32 bit float mono audio"}}},
  {"id":122,"type":"PipeWire:Interface:Port","info":{"direction":"output","props":{"node.id":120,"port.name":"output_FR","format.dsp":"32 bit float mono audio"}}},
  {"id":130,"type":"PipeWire:Interface:Link","info":{"output-port-id":121,"input-port-id":61}}
])";

// Two Zen playback streams (FL/FR each), Discord recording stream (MONO), and the internal EQ sink
// with its output stream; no links.
const char *RuleFixture = R"([
  {"id":10,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"Zen","media.class":"Stream/Output/Audio"}}},
  {"id":11,"type":"PipeWire:Interface:Port","info":{"direction":"output","props":{"node.id":10,"port.name":"output_FL"}}},
  {"id":12,"type":"PipeWire:Interface:Port","info":{"direction":"output","props":{"node.id":10,"port.name":"output_FR"}}},
  {"id":20,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"Zen","media.class":"Stream/Output/Audio"}}},
  {"id":21,"type":"PipeWire:Interface:Port","info":{"direction":"output","props":{"node.id":20,"port.name":"output_FL"}}},
  {"id":30,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"Discord","media.class":"Stream/Input/Audio"}}},
  {"id":31,"type":"PipeWire:Interface:Port","info":{"direction":"input","props":{"node.id":30,"port.name":"input_MONO"}}},
  {"id":40,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"v9pro_equalizer","media.class":"Audio/Sink"}}},
  {"id":41,"type":"PipeWire:Interface:Port","info":{"direction":"input","props":{"node.id":40,"port.name":"playback_FL"}}},
  {"id":50,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"v9pro_equalizer_output","media.class":"Stream/Output/Audio"}}},
  {"id":51,"type":"PipeWire:Interface:Port","info":{"direction":"output","props":{"node.id":50,"port.name":"output_FL"}}}
])";

// Soundboard sink with monitor FL/FR; microphone source; Discord (mono) and OBS (stereo) record the
// microphone; cava records the headset sink monitor; our monitor loopback records the mic too.
const char *FeedFixture = R"([
  {"id":1,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"v9pro_soundboard","media.class":"Audio/Sink"}}},
  {"id":2,"type":"PipeWire:Interface:Port","info":{"direction":"output","props":{"node.id":1,"port.name":"monitor_FL","port.monitor":true}}},
  {"id":3,"type":"PipeWire:Interface:Port","info":{"direction":"output","props":{"node.id":1,"port.name":"monitor_FR","port.monitor":true}}},
  {"id":4,"type":"PipeWire:Interface:Port","info":{"direction":"input","props":{"node.id":1,"port.name":"playback_FL"}}},
  {"id":10,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"alsa_input.usb","media.class":"Audio/Source"}}},
  {"id":11,"type":"PipeWire:Interface:Port","info":{"direction":"output","props":{"node.id":10,"port.name":"capture_MONO"}}},
  {"id":20,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"alsa_output.usb","media.class":"Audio/Sink"}}},
  {"id":21,"type":"PipeWire:Interface:Port","info":{"direction":"output","props":{"node.id":20,"port.name":"monitor_FL","port.monitor":true}}},
  {"id":30,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"Discord","media.class":"Stream/Input/Audio"}}},
  {"id":31,"type":"PipeWire:Interface:Port","info":{"direction":"input","props":{"node.id":30,"port.name":"input_MONO"}}},
  {"id":40,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"OBS","media.class":"Stream/Input/Audio"}}},
  {"id":41,"type":"PipeWire:Interface:Port","info":{"direction":"input","props":{"node.id":40,"port.name":"input_FL"}}},
  {"id":42,"type":"PipeWire:Interface:Port","info":{"direction":"input","props":{"node.id":40,"port.name":"input_FR"}}},
  {"id":50,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"cava","media.class":"Stream/Input/Audio"}}},
  {"id":51,"type":"PipeWire:Interface:Port","info":{"direction":"input","props":{"node.id":50,"port.name":"input_FL"}}},
  {"id":60,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"v9pro_mic_monitor_input","media.class":"Stream/Input/Audio"}}},
  {"id":61,"type":"PipeWire:Interface:Port","info":{"direction":"input","props":{"node.id":60,"port.name":"input_MONO"}}},
  {"id":100,"type":"PipeWire:Interface:Link","info":{"output-port-id":11,"input-port-id":31}},
  {"id":101,"type":"PipeWire:Interface:Link","info":{"output-port-id":11,"input-port-id":41}},
  {"id":102,"type":"PipeWire:Interface:Link","info":{"output-port-id":11,"input-port-id":42}},
  {"id":103,"type":"PipeWire:Interface:Link","info":{"output-port-id":21,"input-port-id":51}},
  {"id":104,"type":"PipeWire:Interface:Link","info":{"output-port-id":11,"input-port-id":61}}
])";

} // namespace

class TestPatchbay : public QObject
{
    Q_OBJECT

private slots:
    void splitterHandlesPartialAndMultipleArrays()
    {
        pw::DumpStreamSplitter splitter;
        QVERIFY(splitter.feed("[{\"a\":\"]\"}").isEmpty());
        const QList<QByteArray> arrays = splitter.feed("]\n[]\n[{\"b\":\"\\\"[\"}");
        QCOMPARE(arrays.size(), 2);
        QCOMPARE(QJsonDocument::fromJson(arrays[0]).array().first().toObject().value("a").toString(), QStringLiteral("]"));
        QVERIFY(QJsonDocument::fromJson(arrays[1]).array().isEmpty());
        const QList<QByteArray> last = splitter.feed("]");
        QCOMPARE(last.size(), 1);
        QCOMPARE(QJsonDocument::fromJson(last[0]).array().first().toObject().value("b").toString(), QStringLiteral("\"["));
    }

    void stateKeepsAudioDropsMidi()
    {
        pw::GraphState state;
        QVERIFY(state.apply(parse(Fixture)));
        QCOMPARE(state.nodes().size(), 2);
        QVERIFY(state.nodes().contains(54));
        QVERIFY(state.nodes().contains(120));
        QCOMPARE(state.nodes().value(54).label, QStringLiteral("V9 PRO"));
        QCOMPARE(state.ports().size(), 5);
        QVERIFY(!state.ports().contains(46));
        QVERIFY(state.ports().value(63).monitor);
        QVERIFY(state.ports().value(63).output);
        QVERIFY(!state.ports().value(61).output);
    }

    void linksResolve()
    {
        pw::GraphState state;
        // Link first: its ports are unknown yet, so it waits.
        state.apply(parse(R"([{"id":130,"type":"PipeWire:Interface:Link","info":{"output-port-id":121,"input-port-id":61}}])"));
        QVERIFY(state.links().isEmpty());
        state.apply(parse(Fixture));
        QCOMPARE(state.links().size(), 1);
        QCOMPARE(state.links().value(130).outPort, 121u);
        QCOMPARE(state.links().value(130).inPort, 61u);
    }

    void removalCascades()
    {
        pw::GraphState state;
        state.apply(parse(Fixture));
        QVERIFY(state.apply(parse(R"([{"id":120,"info":null}])")));
        QVERIFY(!state.nodes().contains(120));
        QVERIFY(!state.ports().contains(121));
        QVERIFY(!state.ports().contains(122));
        QVERIFY(state.links().isEmpty());
        QVERIFY(!state.apply(parse(R"([{"id":999,"info":null}])")));
    }

    void portBeforeNodeIsKept()
    {
        pw::GraphState state;
        state.apply(parse(R"([{"id":5,"type":"PipeWire:Interface:Port","info":{"direction":"output","props":{"node.id":4,"port.name":"output_FL"}}}])"));
        QVERIFY(state.ports().isEmpty());
        state.apply(parse(R"([{"id":4,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"late","media.class":"Stream/Output/Audio"}}}])"));
        QCOMPARE(state.ports().size(), 1);
        QCOMPARE(state.ports().value(5).nodeId, 4u);
    }

    void streamsCarryMediaName()
    {
        pw::GraphState state;
        state.apply(parse(R"([{"id":9,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"Zen","application.name":"Zen","media.name":"Song - YouTube","media.class":"Stream/Output/Audio"}}}])"));
        QCOMPARE(state.nodes().value(9).label, QStringLiteral("Zen"));
        QCOMPARE(state.nodes().value(9).media, QStringLiteral("Song - YouTube"));
    }

    void userLinksTellApartFromPolicyLinks()
    {
        pw::GraphState state;
        state.apply(parse(Fixture));
        state.apply(parse(R"([
          {"id":32,"type":"PipeWire:Interface:Client","info":{"props":{"application.name":"WirePlumber"}}},
          {"id":131,"type":"PipeWire:Interface:Link","info":{"output-port-id":122,"input-port-id":62,"props":{"client.id":32}}},
          {"id":132,"type":"PipeWire:Interface:Link","info":{"output-port-id":121,"input-port-id":62,"props":{"object.linger":true}}}
        ])"));
        QVERIFY(!state.links().value(131).userMade);
        QVERIFY(state.links().value(132).userMade);
    }

    void internalNodesFlagged()
    {
        pw::GraphState state;
        state.apply(parse(R"([{"id":7,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"v9pro_equalizer","media.class":"Audio/Sink"}}}])"));
        QVERIFY(state.nodes().value(7).internal);
    }

    void shortPortNames()
    {
        QCOMPARE(pw::shortPortName(QStringLiteral("playback_FL")), QStringLiteral("FL"));
        QCOMPARE(pw::shortPortName(QStringLiteral("capture_MONO")), QStringLiteral("MONO"));
        QCOMPARE(pw::shortPortName(QStringLiteral("monitor_FR")), QStringLiteral("FR"));
        QCOMPARE(pw::shortPortName(QStringLiteral("output_FL")), QStringLiteral("FL"));
        QCOMPARE(pw::shortPortName(QStringLiteral("input_MONO")), QStringLiteral("MONO"));
        QCOMPARE(pw::shortPortName(QStringLiteral("In 1")), QStringLiteral("In 1"));
    }

    void ruleMatchesEveryStreamWithKey()
    {
        pw::GraphState state;
        state.apply(parse(RuleFixture));
        const QList<pw::LinkRule> rules{{QStringLiteral("Zen"), QStringLiteral("FL"), QStringLiteral("Discord"), QStringLiteral("MONO")}};
        auto pairs = pw::missingLinks(state, rules, {});
        std::sort(pairs.begin(), pairs.end());
        const QList<std::pair<quint32, quint32>> expected{{11, 31}, {21, 31}};
        QCOMPARE(pairs, expected);
    }

    void existingLinkNotRepeated()
    {
        pw::GraphState state;
        state.apply(parse(RuleFixture));
        state.apply(parse(R"([{"id":99,"type":"PipeWire:Interface:Link","info":{"output-port-id":11,"input-port-id":31}}])"));
        const QList<pw::LinkRule> rules{{QStringLiteral("Zen"), QStringLiteral("FL"), QStringLiteral("Discord"), QStringLiteral("MONO")}};
        const QList<std::pair<quint32, quint32>> expected{{21, 31}};
        QCOMPARE(pw::missingLinks(state, rules, {}), expected);
    }

    void internalPairsIgnored()
    {
        pw::GraphState state;
        state.apply(parse(RuleFixture));
        const QList<pw::LinkRule> rules{
            {QStringLiteral("v9pro_equalizer_output"), QStringLiteral("FL"), QStringLiteral("v9pro_equalizer"), QStringLiteral("FL")}};
        QVERIFY(pw::missingLinks(state, rules, {}).isEmpty());
    }

    void attemptedSkipped()
    {
        pw::GraphState state;
        state.apply(parse(RuleFixture));
        const QList<pw::LinkRule> rules{{QStringLiteral("Zen"), QStringLiteral("FL"), QStringLiteral("Discord"), QStringLiteral("MONO")}};
        const QList<std::pair<quint32, quint32>> expected{{21, 31}};
        QCOMPARE(pw::missingLinks(state, rules, {QStringLiteral("11:31")}), expected);
    }

    void ruleRoundTripsString()
    {
        const pw::LinkRule rule{QStringLiteral("Zen"), QStringLiteral("FL"), QStringLiteral("Discord"), QStringLiteral("MONO")};
        QCOMPARE(pw::LinkRule::fromString(rule.toString()), rule);
        QVERIFY(!pw::LinkRule::fromString(QStringLiteral("broken")));
    }

    void nodeKeyFallsBackToLabel()
    {
        pw::GraphNode named;
        named.name = QStringLiteral("Zen");
        QCOMPARE(pw::nodeKey(named), QStringLiteral("Zen"));
        pw::GraphNode unnamed;
        unnamed.label = QStringLiteral("Speaker");
        unnamed.mediaClass = QStringLiteral("Audio/Sink");
        QCOMPARE(pw::nodeKey(unnamed), QStringLiteral("Speaker|Audio/Sink"));
    }

    void layoutPutsIdleNodesTopRight()
    {
        pw::GraphState state;
        state.apply(parse(RuleFixture));
        state.apply(parse(R"([{"id":99,"type":"PipeWire:Interface:Link","info":{"output-port-id":11,"input-port-id":31}}])"));
        const QHash<quint32, QPointF> positions = pw::autoLayout(state);
        // Zen 10 feeds Discord 30: first and second used column. Everything else is idle, stacked in
        // the column right of them from the top.
        QCOMPARE(positions.value(10).x(), 0.0);
        QCOMPARE(positions.value(30).x(), pw::LayoutColumnWidth);
        const qreal idleX = 2 * pw::LayoutColumnWidth;
        for (quint32 idle : {20u, 40u, 50u})
            QCOMPARE(positions.value(idle).x(), idleX);
        QCOMPARE(positions.value(20).y(), 0.0);
        QVERIFY(positions.value(40).y() > positions.value(20).y());
        QVERIFY(positions.value(50).y() > positions.value(40).y());
    }

    void layoutOrdersByNeighbours()
    {
        // Two players feed two recorders crosswise by id; the recorders are reordered so cables do
        // not cross.
        pw::GraphState state;
        state.apply(parse(R"([
          {"id":1,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"a","media.class":"Stream/Output/Audio"}}},
          {"id":2,"type":"PipeWire:Interface:Port","info":{"direction":"output","props":{"node.id":1,"port.name":"output_FL"}}},
          {"id":3,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"b","media.class":"Stream/Output/Audio"}}},
          {"id":4,"type":"PipeWire:Interface:Port","info":{"direction":"output","props":{"node.id":3,"port.name":"output_FL"}}},
          {"id":5,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"x","media.class":"Stream/Input/Audio"}}},
          {"id":6,"type":"PipeWire:Interface:Port","info":{"direction":"input","props":{"node.id":5,"port.name":"input_MONO"}}},
          {"id":7,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"y","media.class":"Stream/Input/Audio"}}},
          {"id":8,"type":"PipeWire:Interface:Port","info":{"direction":"input","props":{"node.id":7,"port.name":"input_MONO"}}},
          {"id":10,"type":"PipeWire:Interface:Link","info":{"output-port-id":2,"input-port-id":8}},
          {"id":11,"type":"PipeWire:Interface:Link","info":{"output-port-id":4,"input-port-id":6}}
        ])"));
        const QHash<quint32, QPointF> positions = pw::autoLayout(state);
        QVERIFY(positions.value(1).y() < positions.value(3).y());
        QVERIFY(positions.value(7).y() < positions.value(5).y());
    }

    void channelsSortInSpeakerOrder()
    {
        QStringList names{QStringLiteral("FR"), QStringLiteral("AUX0"), QStringLiteral("FL"), QStringLiteral("mon FR"), QStringLiteral("mon FL")};
        std::stable_sort(names.begin(), names.end(), [](const QString &a, const QString &b) { return pw::channelOrder(a) < pw::channelOrder(b); });
        const QStringList expected{QStringLiteral("FL"), QStringLiteral("FR"), QStringLiteral("AUX0"), QStringLiteral("mon FL"), QStringLiteral("mon FR")};
        QCOMPARE(names, expected);
    }

    void soundboardFeedsMicrophoneRecorders()
    {
        pw::GraphState state;
        state.apply(parse(FeedFixture));
        auto pairs = pw::feedLinks(state, QStringLiteral("v9pro_soundboard"));
        std::sort(pairs.begin(), pairs.end());
        // Discord's mono input gets both channels, OBS gets FL/FR; cava (records a sink monitor) and
        // our own monitor loopback get nothing.
        const QList<std::pair<quint32, quint32>> expected{{2, 31}, {2, 41}, {3, 31}, {3, 42}};
        QCOMPARE(pairs, expected);
    }

    void soundboardFeedSkipsExistingLinks()
    {
        pw::GraphState state;
        state.apply(parse(FeedFixture));
        state.apply(parse(R"([{"id":105,"type":"PipeWire:Interface:Link","info":{"output-port-id":2,"input-port-id":31}}])"));
        auto pairs = pw::feedLinks(state, QStringLiteral("v9pro_soundboard"));
        std::sort(pairs.begin(), pairs.end());
        const QList<std::pair<quint32, quint32>> expected{{2, 41}, {3, 31}, {3, 42}};
        QCOMPARE(pairs, expected);
        QVERIFY(pw::feedLinks(state, QStringLiteral("missing")).isEmpty());
    }

    void soundboardConfig()
    {
        // A loopback whose capture side is the virtual sink: a client process cannot export a plain
        // null sink, and the playback side doubles as "hear it myself".
        const QString config = pw::soundboardConfig(QStringLiteral("alsa_output.usb-X"));
        QVERIFY(config.contains(QStringLiteral("libpipewire-module-loopback")));
        QVERIFY(config.contains(QStringLiteral("node.name = \"v9pro_soundboard\"")));
        QVERIFY(config.contains(QStringLiteral("media.class = Audio/Sink")));
        QVERIFY(config.contains(QStringLiteral("audio.position = [ MONO ]")));
        QVERIFY(config.contains(QStringLiteral("node.name = \"v9pro_soundboard_listen\"")));
        QVERIFY(config.contains(QStringLiteral("target.object = \"alsa_output.usb-X\"")));
        QVERIFY(!config.contains(QStringLiteral("context.objects")));
        QCOMPARE(pw::soundboardListenParams(false), QStringLiteral("{ mute: true }"));
        QCOMPARE(pw::soundboardListenParams(true), QStringLiteral("{ mute: false }"));
    }
};

QTEST_GUILESS_MAIN(TestPatchbay)
#include "tst_patchbay.moc"
