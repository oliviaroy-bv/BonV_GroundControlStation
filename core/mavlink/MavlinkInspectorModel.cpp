#include "MavlinkInspectorModel.h"

MavlinkInspectorModel::MavlinkInspectorModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MavlinkInspectorModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : rows.size();
}

QVariant MavlinkInspectorModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rows.size()) {
        return {};
    }
    const MavlinkInspectorEntry &entry = rows.at(index.row());
    switch (role) {
    case TimestampRole: return entry.timestamp;
    case LinkRole: return entry.link;
    case SystemIdRole: return entry.systemId;
    case ComponentIdRole: return entry.componentId;
    case MessageNameRole: return entry.messageName;
    case MessageIdRole: return entry.messageId;
    case PacketSizeRole: return entry.packetSize;
    case RateHzRole: return entry.rateHz;
    case CountRole: return entry.count;
    case BytesPerSecondRole: return entry.bytesPerSecond;
    case SequenceRole: return entry.sequence;
    case MavlinkVersionRole: return entry.mavlinkVersion;
    case SearchTextRole: return entry.searchText;
    default: return {};
    }
}

QHash<int, QByteArray> MavlinkInspectorModel::roleNames() const
{
    return {
        { TimestampRole, "timestamp" }, { LinkRole, "link" },
        { SystemIdRole, "sysid" }, { ComponentIdRole, "compid" },
        { MessageNameRole, "messageName" }, { MessageIdRole, "messageId" },
        { PacketSizeRole, "packetSize" }, { RateHzRole, "rateHz" },
        { CountRole, "count" }, { BytesPerSecondRole, "bytesPerSecond" },
        { SequenceRole, "sequence" }, { MavlinkVersionRole, "mavlinkVersion" },
        { SearchTextRole, "searchText" },
    };
}

void MavlinkInspectorModel::synchronize(const QVector<MavlinkInspectorEntry> &entries)
{
    // Refreshes are intentionally batched by the controller (10 Hz), never by
    // the MAVLink receive path.  Most refreshes only update values for an
    // unchanged set of aggregate streams; advertise those as data changes so
    // proxy models and QML do not rebuild their complete view unnecessarily.
    bool sameLayout = rows.size() == entries.size();
    if (sameLayout) {
        for (qsizetype row = 0; row < rows.size(); ++row) {
            const MavlinkInspectorEntry &oldEntry = rows.at(row);
            const MavlinkInspectorEntry &newEntry = entries.at(row);
            if (oldEntry.link != newEntry.link || oldEntry.systemId != newEntry.systemId
                    || oldEntry.componentId != newEntry.componentId || oldEntry.messageId != newEntry.messageId) {
                sameLayout = false;
                break;
            }
        }
    }

    if (!sameLayout) {
        beginResetModel();
        rows = entries;
        endResetModel();
        return;
    }

    rows = entries;
    if (!rows.isEmpty()) {
        emit dataChanged(index(0, 0), index(rows.size() - 1, 0));
    }
}

void MavlinkInspectorModel::clear()
{
    if (rows.isEmpty()) {
        return;
    }
    beginResetModel();
    rows.clear();
    endResetModel();
}
