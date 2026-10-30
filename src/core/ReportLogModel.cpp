#include "ReportLogModel.h"

#include "HeadsetController.h"
#include "V9Protocol.h"

#include <QFile>
#include <QTextStream>

namespace proto = v9::protocol;

namespace {

constexpr qsizetype MaxEntries = 2000;

// Battery and mode traffic is what the app itself polls; anything else is worth highlighting.
bool isPolling(const QByteArray &report)
{
    return proto::isReply(report, proto::CmdBattery) || proto::isReply(report, proto::CmdModeQuery);
}

} // namespace

ReportLogModel::ReportLogModel(HeadsetController *headset, QObject *parent)
    : QAbstractListModel(parent)
{
    connect(headset, &HeadsetController::rawReport, this, &ReportLogModel::append);
}

int ReportLogModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant ReportLogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case TimeRole:
        return entry.time.toString(QStringLiteral("HH:mm:ss.zzz"));
    case DirectionRole:
        return entry.outgoing ? QStringLiteral("TX") : QStringLiteral("RX");
    case HexRole:
        return QString::fromLatin1(entry.report.first(qMin<qsizetype>(16, entry.report.size())).toHex(' '));
    case DescriptionRole:
        return entry.description;
    case InterestingRole:
        return entry.interesting;
    default:
        return {};
    }
}

QHash<int, QByteArray> ReportLogModel::roleNames() const
{
    return {{TimeRole, "time"}, {DirectionRole, "direction"}, {HexRole, "hex"}, {DescriptionRole, "description"}, {InterestingRole, "interesting"}};
}

void ReportLogModel::setCapturing(bool capturing)
{
    if (m_capturing == capturing)
        return;
    m_capturing = capturing;
    emit capturingChanged();
}

void ReportLogModel::setHidePolling(bool hide)
{
    if (m_hidePolling == hide)
        return;
    m_hidePolling = hide;
    emit hidePollingChanged();
}

void ReportLogModel::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
    emit countChanged();
}

bool ReportLogModel::saveTo(const QUrl &file) const
{
    QFile out(file.isLocalFile() ? file.toLocalFile() : file.toString());
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    QTextStream stream(&out);
    for (auto it = m_entries.crbegin(); it != m_entries.crend(); ++it)
        stream << it->time.toString(Qt::ISODateWithMs) << ' ' << (it->outgoing ? "TX " : "RX ") << it->report.toHex(' ') << "  # "
               << it->description << '\n';
    return true;
}

void ReportLogModel::append(const QByteArray &report, bool outgoing)
{
    if (!m_capturing || (m_hidePolling && isPolling(report)))
        return;
    if (m_hidePolling && outgoing && (report.startsWith(proto::batteryRequest().first(3)) || report.startsWith(proto::modeQueryRequest().first(2))))
        return;

    Entry entry{QDateTime::currentDateTime(), outgoing, report, outgoing ? QStringLiteral("sent") : proto::describeReport(report),
                !outgoing && !isPolling(report)};
    beginInsertRows({}, 0, 0);
    m_entries.prepend(std::move(entry));
    endInsertRows();
    if (m_entries.size() > MaxEntries) {
        beginRemoveRows({}, int(MaxEntries), int(m_entries.size() - 1));
        m_entries.resize(MaxEntries);
        endRemoveRows();
    }
    emit countChanged();
}
