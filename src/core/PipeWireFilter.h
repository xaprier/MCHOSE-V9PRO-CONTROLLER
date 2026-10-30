#pragma once

#include <QLoggingCategory>
#include <QObject>
#include <QString>

class QProcess;
class QTimer;

Q_DECLARE_LOGGING_CATEGORY(lcPipeWire)

// Runs one PipeWire module graph in a child `pipewire -c <file>` process (the documented way to
// host filter-chain / echo-cancel outside the main daemon config) and changes its controls live
// with `pw-cli set-param <node> Props`. Stopping the process removes the virtual nodes again.
class PipeWireFilter : public QObject
{
    Q_OBJECT

public:
    // `controlNodeName` is the node whose Props carry the graph controls (the filter's capture
    // node for filter-chain). Empty when the graph has no live controls.
    PipeWireFilter(QString id, QString controlNodeName, QObject *parent = nullptr);
    ~PipeWireFilter() override;

    static bool isAvailable();

    bool isRunning() const;
    bool isReady() const { return m_nodeId >= 0 || (isRunning() && m_controlNodeName.isEmpty()); }
    QString errorString() const { return m_error; }

    void start(const QString &config);
    void stop();
    // Queued until the control node is found; only the latest payload is applied.
    void setParams(const QString &props);

signals:
    void runningChanged();
    void ready();
    void errorOccurred(const QString &message);

private:
    void lookupNode();
    void applyPending();

    QString m_id;
    QString m_controlNodeName;
    QString m_configPath;
    QProcess *m_process = nullptr;
    QTimer *m_lookupTimer = nullptr;
    int m_lookupAttempts = 0;
    int m_nodeId = -1;
    QString m_pendingParams;
    bool m_applying = false;
    QString m_error;
};
