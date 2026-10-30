#include "LinkRules.h"

#include <QStringList>

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

} // namespace v9::pw
