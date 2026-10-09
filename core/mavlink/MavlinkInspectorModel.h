#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QVector>

struct MavlinkInspectorEntry {
    QDateTime timestamp;
    QString link;
    quint8 systemId = 0;
    quint8 componentId = 0;
    QString messageName;
    quint32 messageId = 0;
    int packetSize = 0;
    double rateHz = 0.0;
    quint64 count = 0;
    double bytesPerSecond = 0.0;
    quint8 sequence = 0;
    int mavlinkVersion = 0;
    QString searchText;
};

class MavlinkInspectorModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        TimestampRole = Qt::UserRole + 1,
        LinkRole,
        SystemIdRole,
        ComponentIdRole,
        MessageNameRole,
        MessageIdRole,
        PacketSizeRole,
        RateHzRole,
        CountRole,
        BytesPerSecondRole,
        SequenceRole,
        MavlinkVersionRole,
        SearchTextRole,
    };
    Q_ENUM(Role)

    explicit MavlinkInspectorModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void synchronize(const QVector<MavlinkInspectorEntry> &entries);
    void clear();

private:
    QVector<MavlinkInspectorEntry> rows;
};
