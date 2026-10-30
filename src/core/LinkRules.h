#pragma once

#include "PipeWireGraphState.h"

#include <QList>
#include <QSet>
#include <QString>

#include <optional>
#include <utility>

// Patchbay links the user wants back whenever both ends exist again. Ends are matched by node key
// and short port name, not by ids, because ids change every time a program reopens its stream.
namespace v9::pw {

// node.name, or label + media class for nodes without one. Several nodes can share a key (one
// stream per browser tab); a rule then applies to all of them.
QString nodeKey(const GraphNode &node);

struct LinkRule {
    QString outKey;
    QString outPort;
    QString inKey;
    QString inPort;

    bool operator==(const LinkRule &) const = default;

    // Tab-separated, for QSettings.
    QString toString() const;
    static std::optional<LinkRule> fromString(const QString &text);
};

// Port id pairs (output, input) that the rules want but the graph does not have. Skips pairs
// between two internal (v9pro_*) nodes and pairs listed in `attempted` as "out:in".
QList<std::pair<quint32, quint32>> missingLinks(const GraphState &state, const QList<LinkRule> &rules, const QSet<QString> &attempted);

// Links that feed the monitor of sink `sinkName` into every program recording a microphone (an
// Audio/Source node), the soundboard path. Recorders of sink monitors (visualisers, desktop audio
// capture) and this app's own streams are left alone. A recorder input named like a sink channel
// (FL, FR) gets that channel; any other input (MONO) gets all of them.
QList<std::pair<quint32, quint32>> feedLinks(const GraphState &state, const QString &sinkName);

} // namespace v9::pw
