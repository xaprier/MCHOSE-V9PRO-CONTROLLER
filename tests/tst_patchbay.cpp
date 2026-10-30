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
};

QTEST_GUILESS_MAIN(TestPatchbay)
#include "tst_patchbay.moc"
