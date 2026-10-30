#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QUrl>

class HeadsetController;

// Live list of HID reports for the "HID monitor" (used e.g. to find out whether the headset's
// mic button produces any USB traffic). Newest entry first.
class ReportLogModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(bool capturing READ capturing WRITE setCapturing NOTIFY capturingChanged)
    Q_PROPERTY(bool hidePolling READ hidePolling WRITE setHidePolling NOTIFY hidePollingChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        TimeRole = Qt::UserRole + 1,
        DirectionRole,
        HexRole,
        DescriptionRole,
        InterestingRole,
    };

    explicit ReportLogModel(HeadsetController *headset, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool capturing() const { return m_capturing; }
    void setCapturing(bool capturing);
    bool hidePolling() const { return m_hidePolling; }
    void setHidePolling(bool hide);

    Q_INVOKABLE void clear();
    Q_INVOKABLE bool saveTo(const QUrl &file) const;

signals:
    void capturingChanged();
    void hidePollingChanged();
    void countChanged();

private:
    struct Entry {
        QDateTime time;
        bool outgoing = false;
        QByteArray report;
        QString description;
        bool interesting = false;
    };

    void append(const QByteArray &report, bool outgoing);

    QList<Entry> m_entries;
    bool m_capturing = false;
    bool m_hidePolling = true;
};
