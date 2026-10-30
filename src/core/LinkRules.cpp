#include "LinkRules.h"

#include <QStringList>

#include <algorithm>

namespace v9::pw {

QString nodeKey(const GraphNode &node)
{
    if (!node.name.isEmpty())
        return node.name;
    return node.label + QLatin1Char('|') + node.mediaClass;
}

QString LinkRule::toString() const
{
    return QStringList{outKey, outPort, inKey, inPort}.join(QLatin1Char('\t'));
}

std::optional<LinkRule> LinkRule::fromString(const QString &text)
{
    const QStringList parts = text.split(QLatin1Char('\t'));
    if (parts.size() != 4 || parts.contains(QString()))
        return std::nullopt;
    return LinkRule{parts[0], parts[1], parts[2], parts[3]};
}

QList<std::pair<quint32, quint32>> missingLinks(const GraphState &state, const QList<LinkRule> &rules, const QSet<QString> &attempted)
{
    QSet<std::pair<quint32, quint32>> existing;
    for (const GraphLink &link : state.links())
        existing.insert({link.outPort, link.inPort});

    // Ports of every node, grouped by node key.
    QHash<QString, QList<const GraphPort *>> portsByKey;
    for (const GraphPort &port : state.ports())
        portsByKey[nodeKey(state.nodes().value(port.nodeId))].append(&port);

    QList<std::pair<quint32, quint32>> missing;
    for (const LinkRule &rule : rules) {
        for (const GraphPort *out : portsByKey.value(rule.outKey)) {
            if (!out->output || out->name != rule.outPort)
                continue;
            for (const GraphPort *in : portsByKey.value(rule.inKey)) {
                if (in->output || in->name != rule.inPort)
                    continue;
                const std::pair pair{out->id, in->id};
                if (existing.contains(pair) || missing.contains(pair))
                    continue;
                if (state.nodes().value(out->nodeId).internal && state.nodes().value(in->nodeId).internal)
                    continue;
                if (attempted.contains(QStringLiteral("%1:%2").arg(out->id).arg(in->id)))
                    continue;
                missing.append(pair);
            }
        }
    }
    return missing;
}

QList<std::pair<quint32, quint32>> feedLinks(const GraphState &state, const QString &sinkName)
{
    QList<const GraphPort *> monitors;
    for (const GraphPort &port : state.ports()) {
        if (port.output && port.monitor && state.nodes().value(port.nodeId).name == sinkName)
            monitors.append(&port);
    }
    if (monitors.isEmpty())
        return {};

    QSet<std::pair<quint32, quint32>> existing;
    QSet<quint32> recordsMicrophone;
    for (const GraphLink &link : state.links()) {
        existing.insert({link.outPort, link.inPort});
        const GraphNode &from = state.nodes().value(state.ports().value(link.outPort).nodeId);
        if (from.mediaClass.startsWith(QLatin1String("Audio/Source")))
            recordsMicrophone.insert(state.ports().value(link.inPort).nodeId);
    }

    QList<std::pair<quint32, quint32>> links;
    for (const GraphPort &in : state.ports()) {
        const GraphNode &node = state.nodes().value(in.nodeId);
        if (in.output || node.internal || !node.mediaClass.startsWith(QLatin1String("Stream/Input"))
            || !recordsMicrophone.contains(node.id))
            continue;
        const bool namedChannel = std::any_of(monitors.cbegin(), monitors.cend(), [&](const GraphPort *m) { return m->name == in.name; });
        for (const GraphPort *monitor : std::as_const(monitors)) {
            const std::pair pair{monitor->id, in.id};
            if ((!namedChannel || monitor->name == in.name) && !existing.contains(pair))
                links.append(pair);
        }
    }
    return links;
}

} // namespace v9::pw
