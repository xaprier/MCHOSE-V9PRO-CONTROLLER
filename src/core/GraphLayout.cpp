#include "GraphLayout.h"

#include <QMap>
#include <QSet>

#include <algorithm>

namespace v9::pw {

namespace {

constexpr qreal HeaderHeight = 40;
constexpr qreal RowHeight = 22;
constexpr qreal Padding = 8;

int roleColumn(const GraphNode &node)
{
    if (node.internal)
        return 1;
    if (node.mediaClass.startsWith(QLatin1String("Stream/Output")) || node.mediaClass == QLatin1String("Audio/Source"))
        return 0;
    if (node.mediaClass.startsWith(QLatin1String("Stream/Input")))
        return 3;
    return 2;
}

} // namespace

int channelOrder(const QString &portName)
{
    static const QStringList speakers{QStringLiteral("FL"), QStringLiteral("FR"), QStringLiteral("FC"), QStringLiteral("LFE"),
                                      QStringLiteral("RL"), QStringLiteral("RR"), QStringLiteral("SL"), QStringLiteral("SR"),
                                      QStringLiteral("MONO")};
    const bool monitor = portName.startsWith(QLatin1String("mon "));
    const qsizetype index = speakers.indexOf(monitor ? portName.mid(4) : portName);
    return (monitor ? 100 : 0) + (index >= 0 ? int(index) : 50);
}

qreal nodeHeight(int rows)
{
    return HeaderHeight + rows * RowHeight + Padding;
}

QHash<quint32, QPointF> autoLayout(const GraphState &state)
{
    QHash<quint32, int> inputs;
    QHash<quint32, int> outputs;
    for (const GraphPort &port : state.ports())
        ++(port.output ? outputs : inputs)[port.nodeId];

    QHash<quint32, QSet<quint32>> neighbours;
    for (const GraphLink &link : state.links()) {
        const quint32 from = state.ports().value(link.outPort).nodeId;
        const quint32 to = state.ports().value(link.inPort).nodeId;
        if (from != to) {
            neighbours[from].insert(to);
            neighbours[to].insert(from);
        }
    }

    QMap<int, QList<quint32>> columns; // role column -> nodes, connected only
    QList<quint32> idle;
    QList<quint32> ids = state.nodes().keys();
    std::sort(ids.begin(), ids.end());
    for (quint32 id : std::as_const(ids)) {
        if (!inputs.contains(id) && !outputs.contains(id))
            continue;
        if (neighbours.contains(id))
            columns[roleColumn(state.nodes().value(id))].append(id);
        else
            idle.append(id);
    }

    // Barycentre ordering: a forward pass places each column by the ranks of its neighbours in the
    // columns before it, then the first column is re-placed by its neighbours to the right.
    QHash<quint32, qreal> rank;
    const auto rankColumn = [&](QList<quint32> &column, bool lookLeft, int columnKey) {
        QHash<quint32, qreal> key;
        for (int i = 0; i < column.size(); ++i) {
            qreal sum = 0;
            int count = 0;
            for (quint32 other : neighbours.value(column[i])) {
                const int otherColumn = roleColumn(state.nodes().value(other));
                if (rank.contains(other) && (lookLeft ? otherColumn < columnKey : otherColumn > columnKey)) {
                    sum += rank.value(other);
                    ++count;
                }
            }
            key.insert(column[i], count ? sum / count : i);
        }
        std::stable_sort(column.begin(), column.end(), [&](quint32 a, quint32 b) { return key.value(a) < key.value(b); });
        for (int i = 0; i < column.size(); ++i)
            rank.insert(column[i], i);
    };
    for (auto it = columns.begin(); it != columns.end(); ++it)
        rankColumn(*it, true, it.key());
    if (columns.size() > 1)
        rankColumn(columns.begin().value(), false, columns.begin().key());

    QHash<quint32, QPointF> positions;
    const auto stack = [&](const QList<quint32> &column, qreal x) {
        qreal y = 0;
        for (quint32 id : column) {
            positions.insert(id, QPointF(x, y));
            y += nodeHeight(std::max(inputs.value(id), outputs.value(id))) + LayoutGap;
        }
    };
    int index = 0;
    for (const QList<quint32> &column : std::as_const(columns))
        stack(column, index++ * LayoutColumnWidth);
    stack(idle, index * LayoutColumnWidth);
    return positions;
}

} // namespace v9::pw
