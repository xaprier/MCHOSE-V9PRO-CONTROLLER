#pragma once

#include "LinkRules.h"
#include "PipeWireGraphState.h"

#include <QHash>
#include <QLoggingCategory>
#include <QObject>
#include <QPointF>
#include <QSet>
#include <QStringList>
#include <QVariantList>

class QProcess;
class QTimer;

Q_DECLARE_LOGGING_CATEGORY(lcGraph)

// Live audio graph for the patchbay page: `pw-dump --monitor` keeps GraphState current, `pw-link`
// creates and removes links. Optionally re-creates links the user made once their ends come back.
class PipeWireGraph : public QObject
{
    Q_OBJECT

    // {id, label, media, kind ("playback" | "capture" | "device" | "internal"), internal, x, y,
    //  inputs: [{id, name}], outputs: [{id, name}]}
    Q_PROPERTY(QVariantList nodes READ nodes NOTIFY graphChanged)
    // {id, outPort, inPort, outNode, inNode, internal}
    Q_PROPERTY(QVariantList links READ links NOTIFY graphChanged)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool restoreLinks READ restoreLinks WRITE setRestoreLinks NOTIFY restoreLinksChanged)
    // "Zen FL → Discord MONO", in rule order.
    Q_PROPERTY(QStringList rememberedLinks READ rememberedLinks NOTIFY rulesChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit PipeWireGraph(QObject *parent = nullptr);
    ~PipeWireGraph() override;

    QVariantList nodes() const { return m_nodeList; }
    QVariantList links() const { return m_linkList; }
    static bool available();
    bool restoreLinks() const { return m_restore; }
    void setRestoreLinks(bool on);
    QStringList rememberedLinks() const;
    QString error() const { return m_error; }

    // Either argument may be the output; ports of the same direction or node are rejected.
    Q_INVOKABLE void connectPorts(quint32 a, quint32 b);
    Q_INVOKABLE void disconnectLink(quint32 linkId);
    Q_INVOKABLE void forgetRule(int index);
    Q_INVOKABLE void setNodePosition(quint32 nodeId, qreal x, qreal y);
    // Back to the session manager's routing: removes user-made links (never this app's own), forgets
    // remembered links and box positions, and sends every program stream to the default devices.
    Q_INVOKABLE void resetToDefaults();

signals:
    void graphChanged();
    void restoreLinksChanged();
    void rulesChanged();
    void errorChanged();

private:
    void startDump();
    void onDumpOutput();
    void rebuild();
    void applyRules();
    void runLink(const QStringList &arguments);
    void moveStreamsToDefault(const QString &streams, const QString &move, const QString &target);
    QString positionKey(quint32 nodeId) const;
    void saveRules();
    void setError(const QString &error);

    QProcess *m_dump = nullptr;
    QTimer *m_rebuildDebounce;
    QTimer *m_restoreDebounce;
    v9::pw::DumpStreamSplitter m_splitter;
    v9::pw::GraphState m_state;
    QVariantList m_nodeList;
    QVariantList m_linkList;
    QHash<QString, QPointF> m_positions; // boxes the user moved, by position key; others are auto-laid out
    QList<v9::pw::LinkRule> m_rules;
    QSet<QString> m_attempted; // "out:in" port ids the restorer already tried
    bool m_restore = false;
    bool m_quitting = false;
    QString m_error;
};
