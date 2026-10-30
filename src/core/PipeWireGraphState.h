#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QHash>
#include <QJsonArray>
#include <QList>
#include <QString>

// Audio part of the PipeWire graph as reported by `pw-dump --monitor`. Pure state, no I/O, so the
// patchbay logic can be unit tested.
namespace v9::pw {

// `pw-dump -m` writes one JSON array per update, back to back. Collects bytes until a top-level
// array is complete (brackets inside strings ignored) and returns every finished one in order.
class DumpStreamSplitter
{
public:
    QList<QByteArray> feed(QByteArrayView chunk);

private:
    QByteArray m_buffer;
    int m_depth = 0;
    bool m_inString = false;
    bool m_escaped = false;
};

struct GraphNode {
    quint32 id = 0;
    QString name;       // node.name
    QString label;      // what to show
    QString mediaClass; // Audio/Sink, Stream/Output/Audio, ...
    bool internal = false; // one of this app's v9pro_* nodes
};

struct GraphPort {
    quint32 id = 0;
    quint32 nodeId = 0;
    QString name; // short form, see shortPortName()
    bool output = false;
    bool monitor = false;
};

struct GraphLink {
    quint32 id = 0;
    quint32 outPort = 0;
    quint32 inPort = 0;
};

// "playback_FL" -> "FL", "capture_MONO" -> "MONO"; names without a known prefix are kept.
QString shortPortName(const QString &portName);

class GraphState
{
public:
    // Applies one pw-dump update. Returns true when nodes, ports or links changed.
    bool apply(const QJsonArray &objects);
    void clear();

    const QHash<quint32, GraphNode> &nodes() const { return m_nodes; }
    const QHash<quint32, GraphPort> &ports() const { return m_ports; }
    const QHash<quint32, GraphLink> &links() const { return m_links; }

private:
    bool remove(quint32 id);
    bool resolvePending();

    QHash<quint32, GraphNode> m_nodes;
    QHash<quint32, GraphPort> m_ports;
    QHash<quint32, GraphLink> m_links;
    // Links whose ports are not known (yet, or because they are MIDI/video); kept for later updates.
    QHash<quint32, GraphLink> m_pendingLinks;
};

} // namespace v9::pw
