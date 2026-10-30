#include "PipeWireGraph.h"

#include "GraphLayout.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <algorithm>

Q_LOGGING_CATEGORY(lcGraph, "v9.graph")

namespace pw = v9::pw;

namespace {

constexpr int RebuildDebounceMs = 50;
constexpr int RestoreDebounceMs = 300;
constexpr int DumpRestartMs = 2000;

const QString RestoreKey = QStringLiteral("patchbay/restore");
const QString RulesKey = QStringLiteral("patchbay/rules");
const QString PositionsGroup = QStringLiteral("patchbay/positions");

QString kindOf(const pw::GraphNode &node)
{
    if (node.internal)
        return QStringLiteral("internal");
    if (node.mediaClass.startsWith(QLatin1String("Stream/Output")))
        return QStringLiteral("playback");
    if (node.mediaClass.startsWith(QLatin1String("Stream/Input")))
        return QStringLiteral("capture");
    return QStringLiteral("device");
}

} // namespace

PipeWireGraph::PipeWireGraph(QObject *parent)
    : QObject(parent)
    , m_rebuildDebounce(new QTimer(this))
    , m_restoreDebounce(new QTimer(this))
{
    m_rebuildDebounce->setSingleShot(true);
    m_rebuildDebounce->setInterval(RebuildDebounceMs);
    connect(m_rebuildDebounce, &QTimer::timeout, this, &PipeWireGraph::rebuild);
    m_restoreDebounce->setSingleShot(true);
    m_restoreDebounce->setInterval(RestoreDebounceMs);
    connect(m_restoreDebounce, &QTimer::timeout, this, &PipeWireGraph::applyRules);

    QSettings settings;
    m_restore = settings.value(RestoreKey, false).toBool();
    for (const QString &text : settings.value(RulesKey).toStringList()) {
        if (const auto rule = pw::LinkRule::fromString(text))
            m_rules.append(*rule);
    }
    settings.beginGroup(PositionsGroup);
    for (const QString &key : settings.childKeys())
        m_positions.insert(QString::fromUtf8(QByteArray::fromPercentEncoding(key.toUtf8())), settings.value(key).toPointF());
    settings.endGroup();

    if (available())
        startDump();
}

PipeWireGraph::~PipeWireGraph()
{
    m_quitting = true;
    if (m_dump) {
        m_dump->disconnect(this);
        m_dump->kill();
        m_dump->waitForFinished(500);
    }
}

bool PipeWireGraph::available()
{
    return !QStandardPaths::findExecutable(QStringLiteral("pw-dump")).isEmpty()
        && !QStandardPaths::findExecutable(QStringLiteral("pw-link")).isEmpty();
}

void PipeWireGraph::startDump()
{
    m_splitter = {};
    m_dump = new QProcess(this);
    m_dump->setProgram(QStringLiteral("pw-dump"));
    m_dump->setArguments({QStringLiteral("--monitor"), QStringLiteral("--no-colors")});
    connect(m_dump, &QProcess::readyReadStandardOutput, this, &PipeWireGraph::onDumpOutput);
    connect(m_dump, &QProcess::finished, this, [this] {
        qCWarning(lcGraph) << "pw-dump exited, restarting";
        m_dump->deleteLater();
        m_dump = nullptr;
        // Nothing is known until the new dump arrives; stale boxes would be worse than none.
        m_state.clear();
        m_attempted.clear();
        rebuild();
        if (!m_quitting)
            QTimer::singleShot(DumpRestartMs, this, &PipeWireGraph::startDump);
    });
    m_dump->start();
}

void PipeWireGraph::onDumpOutput()
{
    bool changed = false;
    for (const QByteArray &array : m_splitter.feed(m_dump->readAllStandardOutput())) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(array, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            qCWarning(lcGraph) << "unparsable pw-dump update:" << parseError.errorString();
            continue;
        }
        changed |= m_state.apply(document.array());
    }
    if (changed)
        m_rebuildDebounce->start();
}

QString PipeWireGraph::positionKey(quint32 nodeId) const
{
    // Streams of one program share a key; the occurrence (ordered by id) keeps their boxes apart.
    const pw::GraphNode &node = m_state.nodes()[nodeId];
    const QString key = pw::nodeKey(node);
    int occurrence = 0;
    for (const pw::GraphNode &other : m_state.nodes()) {
        if (other.id < nodeId && pw::nodeKey(other) == key)
            ++occurrence;
    }
    return key + QLatin1Char('#') + QString::number(occurrence);
}

void PipeWireGraph::rebuild()
{
    QList<pw::GraphNode> nodes = m_state.nodes().values();
    std::sort(nodes.begin(), nodes.end(), [](const pw::GraphNode &a, const pw::GraphNode &b) { return a.id < b.id; });
    QList<pw::GraphPort> ports = m_state.ports().values();
    std::sort(ports.begin(), ports.end(), [](const pw::GraphPort &a, const pw::GraphPort &b) { return a.id < b.id; });

    const QHash<quint32, QPointF> layout = pw::autoLayout(m_state);
    m_nodeList.clear();
    for (const pw::GraphNode &node : std::as_const(nodes)) {
        QVariantList inputs;
        QVariantList outputs;
        for (const pw::GraphPort &port : std::as_const(ports)) {
            if (port.nodeId != node.id)
                continue;
            // Sink monitor outputs carry the same channel names as the inputs; tell them apart.
            const QString name = port.monitor ? QStringLiteral("mon ") + port.name : port.name;
            const QVariantMap entry{{QStringLiteral("id"), port.id}, {QStringLiteral("name"), name}};
            (port.output ? outputs : inputs).append(entry);
        }
        // Speaker order keeps FL above FR, so stereo cables run parallel instead of crossing.
        const auto byChannel = [](const QVariant &a, const QVariant &b) {
            return pw::channelOrder(a.toMap().value(QStringLiteral("name")).toString())
                 < pw::channelOrder(b.toMap().value(QStringLiteral("name")).toString());
        };
        std::stable_sort(inputs.begin(), inputs.end(), byChannel);
        std::stable_sort(outputs.begin(), outputs.end(), byChannel);
        if (inputs.isEmpty() && outputs.isEmpty())
            continue; // nodes without audio ports (e.g. a suspended device profile) have nothing to patch

        const QString key = positionKey(node.id);
        const QPointF position = m_positions.value(key, layout.value(node.id));
        m_nodeList.append(QVariantMap{
            {QStringLiteral("id"), node.id},
            {QStringLiteral("label"), node.label},
            {QStringLiteral("media"), node.media},
            {QStringLiteral("kind"), kindOf(node)},
            {QStringLiteral("internal"), node.internal},
            {QStringLiteral("x"), position.x()},
            {QStringLiteral("y"), position.y()},
            {QStringLiteral("inputs"), inputs},
            {QStringLiteral("outputs"), outputs},
        });
    }

    m_linkList.clear();
    for (const pw::GraphLink &link : m_state.links()) {
        const pw::GraphPort out = m_state.ports().value(link.outPort);
        const pw::GraphPort in = m_state.ports().value(link.inPort);
        const bool internal = m_state.nodes().value(out.nodeId).internal || m_state.nodes().value(in.nodeId).internal;
        m_linkList.append(QVariantMap{
            {QStringLiteral("id"), link.id},
            {QStringLiteral("outPort"), link.outPort},
            {QStringLiteral("inPort"), link.inPort},
            {QStringLiteral("outNode"), out.nodeId},
            {QStringLiteral("inNode"), in.nodeId},
            {QStringLiteral("internal"), internal},
        });
    }

    // Forget attempts whose ports are gone, so a returning stream is tried again.
    for (auto it = m_attempted.begin(); it != m_attempted.end();) {
        const QStringList ids = it->split(QLatin1Char(':'));
        const bool alive = ids.size() == 2 && m_state.ports().contains(ids[0].toUInt()) && m_state.ports().contains(ids[1].toUInt());
        it = alive ? std::next(it) : m_attempted.erase(it);
    }

    qCDebug(lcGraph) << "graph:" << m_nodeList.size() << "nodes," << m_linkList.size() << "links";
    emit graphChanged();
    if (m_restore && !m_rules.isEmpty())
        m_restoreDebounce->start();
}

void PipeWireGraph::applyRules()
{
    if (!m_restore)
        return;
    for (const auto &[out, in] : pw::missingLinks(m_state, m_rules, m_attempted)) {
        m_attempted.insert(QStringLiteral("%1:%2").arg(out).arg(in));
        qCInfo(lcGraph) << "restoring link" << out << "->" << in;
        runLink({QString::number(out), QString::number(in)});
    }
}

void PipeWireGraph::setRestoreLinks(bool on)
{
    if (m_restore == on)
        return;
    m_restore = on;
    QSettings().setValue(RestoreKey, on);
    emit restoreLinksChanged();
    if (on)
        m_restoreDebounce->start();
}

QStringList PipeWireGraph::rememberedLinks() const
{
    QStringList texts;
    for (const pw::LinkRule &rule : m_rules)
        texts << tr("%1 %2 → %3 %4").arg(rule.outKey, rule.outPort, rule.inKey, rule.inPort);
    return texts;
}

void PipeWireGraph::connectPorts(quint32 a, quint32 b)
{
    if (!m_state.ports().contains(a) || !m_state.ports().contains(b))
        return;
    pw::GraphPort out = m_state.ports().value(a);
    pw::GraphPort in = m_state.ports().value(b);
    if (!out.output)
        std::swap(out, in);
    if (!out.output || in.output || out.nodeId == in.nodeId)
        return;
    runLink({QString::number(out.id), QString::number(in.id)});

    // Rules are only collected while restoring is on (it is opt-in).
    if (!m_restore)
        return;
    const pw::LinkRule rule{pw::nodeKey(m_state.nodes().value(out.nodeId)), out.name, pw::nodeKey(m_state.nodes().value(in.nodeId)), in.name};
    if (!m_rules.contains(rule)) {
        m_rules.append(rule);
        saveRules();
    }
}

void PipeWireGraph::disconnectLink(quint32 linkId)
{
    if (!m_state.links().contains(linkId))
        return;
    const pw::GraphLink link = m_state.links().value(linkId);
    const pw::GraphPort out = m_state.ports().value(link.outPort);
    const pw::GraphPort in = m_state.ports().value(link.inPort);
    const pw::LinkRule rule{pw::nodeKey(m_state.nodes().value(out.nodeId)), out.name, pw::nodeKey(m_state.nodes().value(in.nodeId)), in.name};
    if (m_rules.removeAll(rule) > 0)
        saveRules();
    runLink({QStringLiteral("--disconnect"), QString::number(linkId)});
}

void PipeWireGraph::forgetRule(int index)
{
    if (index < 0 || index >= m_rules.size())
        return;
    m_rules.removeAt(index);
    saveRules();
}

void PipeWireGraph::setNodePosition(quint32 nodeId, qreal x, qreal y)
{
    if (!m_state.nodes().contains(nodeId))
        return;
    const QString key = positionKey(nodeId);
    m_positions.insert(key, QPointF(x, y));
    QSettings settings;
    settings.beginGroup(PositionsGroup);
    // Keys contain '/' (media classes); QSettings would read those as groups.
    settings.setValue(QString::fromLatin1(key.toUtf8().toPercentEncoding()), QPointF(x, y));
    for (QVariant &value : m_nodeList) {
        QVariantMap node = value.toMap();
        if (node.value(QStringLiteral("id")).toUInt() == nodeId) {
            node.insert(QStringLiteral("x"), x);
            node.insert(QStringLiteral("y"), y);
            value = node;
        }
    }
}

void PipeWireGraph::resetToDefaults()
{
    for (const pw::GraphLink &link : m_state.links()) {
        const bool internal = m_state.nodes().value(m_state.ports().value(link.outPort).nodeId).internal
                           && m_state.nodes().value(m_state.ports().value(link.inPort).nodeId).internal;
        if (link.userMade && !internal)
            runLink({QStringLiteral("--disconnect"), QString::number(link.id)});
    }
    m_rules.clear();
    saveRules();
    m_attempted.clear();
    m_positions.clear();
    QSettings settings;
    settings.remove(PositionsGroup);
    moveStreamsToDefault(QStringLiteral("sink-inputs"), QStringLiteral("move-sink-input"), QStringLiteral("@DEFAULT_SINK@"));
    moveStreamsToDefault(QStringLiteral("source-outputs"), QStringLiteral("move-source-output"), QStringLiteral("@DEFAULT_SOURCE@"));
    rebuild();
}

void PipeWireGraph::moveStreamsToDefault(const QString &streams, const QString &move, const QString &target)
{
    // A stream whose cable the user pulled has no link at all; moving it to the default makes the
    // session manager link it again. Our own v9pro_* streams target their devices on purpose.
    auto *list = new QProcess(this);
    connect(list, &QProcess::finished, this, [this, list, move, target] {
        list->deleteLater();
        const QJsonArray entries = QJsonDocument::fromJson(list->readAllStandardOutput()).array();
        for (const QJsonValue &entry : entries) {
            if (entry[QStringLiteral("properties")][QStringLiteral("node.name")].toString().startsWith(QLatin1String("v9pro_")))
                continue;
            auto *process = new QProcess(this);
            connect(process, &QProcess::finished, process, &QObject::deleteLater);
            process->start(QStringLiteral("pactl"), {move, QString::number(entry[QStringLiteral("index")].toInteger()), target});
        }
    });
    list->start(QStringLiteral("pactl"), {QStringLiteral("-f"), QStringLiteral("json"), QStringLiteral("list"), streams});
}

void PipeWireGraph::saveRules()
{
    QStringList texts;
    for (const pw::LinkRule &rule : m_rules)
        texts << rule.toString();
    QSettings().setValue(RulesKey, texts);
    emit rulesChanged();
}

void PipeWireGraph::runLink(const QStringList &arguments)
{
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process, arguments](int exitCode, QProcess::ExitStatus status) {
        process->deleteLater();
        if (status == QProcess::NormalExit && exitCode == 0) {
            setError({});
            return;
        }
        const QString message = QString::fromUtf8(process->readAllStandardError()).trimmed();
        qCWarning(lcGraph) << "pw-link" << arguments << "failed:" << message;
        setError(message.isEmpty() ? tr("pw-link failed") : message);
    });
    process->start(QStringLiteral("pw-link"), arguments);
}

void PipeWireGraph::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}
