#include "PipeWireGraphState.h"

#include <QJsonObject>

namespace v9::pw {

QList<QByteArray> DumpStreamSplitter::feed(QByteArrayView chunk)
{
    QList<QByteArray> complete;
    for (const char c : chunk) {
        if (m_depth == 0 && c != '[')
            continue; // whitespace between arrays
        m_buffer.append(c);
        if (m_inString) {
            if (m_escaped)
                m_escaped = false;
            else if (c == '\\')
                m_escaped = true;
            else if (c == '"')
                m_inString = false;
            continue;
        }
        if (c == '"') {
            m_inString = true;
        } else if (c == '[' || c == '{') {
            ++m_depth;
        } else if (c == ']' || c == '}') {
            if (--m_depth == 0) {
                complete.append(m_buffer);
                m_buffer.clear();
            }
        }
    }
    return complete;
}

QString shortPortName(const QString &portName)
{
    static const QStringList prefixes{QStringLiteral("playback_"), QStringLiteral("capture_"), QStringLiteral("monitor_"),
                                      QStringLiteral("output_"), QStringLiteral("input_")};
    for (const QString &prefix : prefixes) {
        if (portName.startsWith(prefix) && portName.size() > prefix.size())
            return portName.mid(prefix.size());
    }
    return portName;
}

namespace {

QString firstNonEmpty(const QJsonObject &props, std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        const QString value = props.value(QLatin1String(key)).toString();
        if (!value.isEmpty())
            return value;
    }
    return {};
}

} // namespace

bool GraphState::apply(const QJsonArray &objects)
{
    bool changed = false;
    for (const QJsonValue &value : objects) {
        const QJsonObject object = value.toObject();
        const auto id = static_cast<quint32>(object.value(QStringLiteral("id")).toInteger());
        const QJsonValue infoValue = object.value(QStringLiteral("info"));
        if (infoValue.isNull()) {
            changed |= remove(id);
            continue;
        }
        const QString type = object.value(QStringLiteral("type")).toString();
        const QJsonObject info = infoValue.toObject();
        const QJsonObject props = info.value(QStringLiteral("props")).toObject();

        // Partial updates (only "state" etc.) carry no type; the object is already known.
        if (type.endsWith(QLatin1String(":Node"))) {
            const QString mediaClass = props.value(QStringLiteral("media.class")).toString();
            if (!mediaClass.contains(QLatin1String("Audio")))
                continue;
            GraphNode node;
            node.id = id;
            node.name = props.value(QStringLiteral("node.name")).toString();
            node.label = firstNonEmpty(props, {"node.description", "node.nick", "application.name", "node.name"});
            node.mediaClass = mediaClass;
            node.internal = node.name.startsWith(QLatin1String("v9pro_"));
            if (!m_nodes.contains(id) || m_nodes[id].label != node.label || m_nodes[id].name != node.name) {
                m_nodes.insert(id, node);
                changed = true;
            }
        } else if (type.endsWith(QLatin1String(":Port"))) {
            if (props.value(QStringLiteral("format.dsp")).toString().contains(QLatin1String("midi"), Qt::CaseInsensitive))
                continue;
            const auto nodeId = static_cast<quint32>(props.value(QStringLiteral("node.id")).toInteger());
            if (!m_nodes.contains(nodeId))
                continue;
            GraphPort port;
            port.id = id;
            port.nodeId = nodeId;
            port.name = shortPortName(props.value(QStringLiteral("port.name")).toString());
            port.output = info.value(QStringLiteral("direction")).toString() == QLatin1String("output");
            port.monitor = props.value(QStringLiteral("port.monitor")).toBool();
            if (!m_ports.contains(id)) {
                m_ports.insert(id, port);
                changed = true;
            }
        } else if (type.endsWith(QLatin1String(":Link"))) {
            GraphLink link;
            link.id = id;
            link.outPort = static_cast<quint32>(info.value(QStringLiteral("output-port-id")).toInteger());
            link.inPort = static_cast<quint32>(info.value(QStringLiteral("input-port-id")).toInteger());
            if (!m_links.contains(id))
                m_pendingLinks.insert(id, link);
        }
    }
    changed |= resolvePending();
    return changed;
}

bool GraphState::resolvePending()
{
    bool changed = false;
    for (auto it = m_pendingLinks.begin(); it != m_pendingLinks.end();) {
        if (m_ports.contains(it->outPort) && m_ports.contains(it->inPort)) {
            m_links.insert(it.key(), *it);
            it = m_pendingLinks.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    return changed;
}

bool GraphState::remove(quint32 id)
{
    m_pendingLinks.remove(id);
    if (m_links.remove(id))
        return true;
    if (m_ports.contains(id)) {
        m_ports.remove(id);
        for (auto it = m_links.begin(); it != m_links.end();)
            it = (it->outPort == id || it->inPort == id) ? m_links.erase(it) : std::next(it);
        return true;
    }
    if (m_nodes.remove(id)) {
        QList<quint32> ports;
        for (const GraphPort &port : std::as_const(m_ports)) {
            if (port.nodeId == id)
                ports.append(port.id);
        }
        for (quint32 port : ports)
            remove(port);
        return true;
    }
    return false;
}

void GraphState::clear()
{
    m_nodes.clear();
    m_ports.clear();
    m_links.clear();
    m_pendingLinks.clear();
}

} // namespace v9::pw
