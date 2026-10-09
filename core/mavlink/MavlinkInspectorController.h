#pragma once

#include "MavlinkFrame.h"
#include "MavlinkInspectorModel.h"
#include "MavlinkInspectorFilterModel.h"
#include "MavlinkInspectorGraphModel.h"

#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <QVector>

#include <optional>

class Mavlink;

class MavlinkInspectorController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *model READ model CONSTANT)
    Q_PROPERTY(QAbstractItemModel *filteredModel READ filteredModel CONSTANT)
    Q_PROPERTY(int filteredStreamCount READ filteredStreamCount NOTIFY filterStateChanged)
    Q_PROPERTY(QVariantList vehicleFilterOptions READ vehicleFilterOptions NOTIFY filterStateChanged)
    Q_PROPERTY(QVariantList componentFilterOptions READ componentFilterOptions NOTIFY filterStateChanged)
    Q_PROPERTY(QVariantList messageFilterOptions READ messageFilterOptions NOTIFY filterStateChanged)
    Q_PROPERTY(QVariantList linkFilterOptions READ linkFilterOptions NOTIFY filterStateChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(int streamCount READ streamCount NOTIFY statisticsChanged)
    Q_PROPERTY(double totalRxRate READ totalRxRate NOTIFY statisticsChanged)
    Q_PROPERTY(double totalRxBytesPerSecond READ totalRxBytesPerSecond NOTIFY statisticsChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedDetails READ selectedDetails NOTIFY selectionChanged)
    Q_PROPERTY(bool hasRawPacket READ hasRawPacket NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedRawPacket READ selectedRawPacket NOTIFY selectionChanged)
    Q_PROPERTY(int rawPacketCount READ rawPacketCount NOTIFY selectionChanged)
    Q_PROPERTY(MavlinkInspectorGraphModel *graphModel READ graphModel CONSTANT)
    Q_PROPERTY(QVariantList availableGraphFields READ availableGraphFields NOTIFY graphStateChanged)
    Q_PROPERTY(QString selectedGraphField READ selectedGraphField NOTIFY graphStateChanged)

public:
    struct StreamKey {
        QString link;
        quint8 systemId = 0;
        quint8 componentId = 0;
        quint32 messageId = 0;
        bool operator==(const StreamKey &other) const = default;
        friend size_t qHash(const StreamKey &key, size_t seed) noexcept
        {
            return ::qHash(key.link, seed)
                    ^ (::qHash(key.systemId, seed) << 1)
                    ^ (::qHash(key.componentId, seed) << 2)
                    ^ (::qHash(key.messageId, seed) << 3);
        }
    };

    explicit MavlinkInspectorController(QObject *parent = nullptr);
    explicit MavlinkInspectorController(Mavlink *mavlink, QObject *parent = nullptr);

    QAbstractItemModel *model() const;
    QAbstractItemModel *filteredModel() const;
    int filteredStreamCount() const;
    QVariantList vehicleFilterOptions() const;
    QVariantList componentFilterOptions() const;
    QVariantList messageFilterOptions() const;
    QVariantList linkFilterOptions() const;
    bool paused() const { return pausedValue; }
    int streamCount() const;
    double totalRxRate() const;
    double totalRxBytesPerSecond() const;
    bool hasSelection() const { return selectedKey.has_value(); }
    QVariantMap selectedDetails() const;
    bool hasRawPacket() const;
    QVariantMap selectedRawPacket() const;
    int rawPacketCount() const { return rawPackets.size(); }
    MavlinkInspectorGraphModel *graphModel() const { return inspectorGraphModel; }
    QVariantList availableGraphFields() const;
    QString selectedGraphField() const;

public slots:
    void observeFrame(const MavlinkFrame &frame);
    void setPaused(bool paused);
    void clear();

    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE bool selectStream(const QString &link, int systemId, int componentId, quint32 messageId);
    Q_INVOKABLE bool selectRow(int row);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void setVehicleFilter(int systemId);
    Q_INVOKABLE void setComponentFilter(int componentId);
    Q_INVOKABLE void setMessageFilter(qint64 messageId);
    Q_INVOKABLE void setLinkFilter(const QString &link);
    Q_INVOKABLE void setSearchText(const QString &searchText);
    Q_INVOKABLE void clearFilters();
    Q_INVOKABLE bool copySelectedRawHex() const;
    Q_INVOKABLE bool selectGraphField(const QString &fieldName);
    Q_INVOKABLE void clearGraph();

signals:
    void pausedChanged();
    void statisticsChanged();
    void selectionChanged();
    void filterStateChanged();
    void graphStateChanged();

private slots:
    void refreshModel();

private:
    struct StreamState {
        MavlinkInspectorEntry entry;
        QDateTime firstTimestamp;
        quint64 totalBytes = 0;
        mavlink_message_t latestMessage = {};
        QVariantList latestFields;
        bool fieldsDirty = false;
    };

    struct RawPacketRecord {
        QDateTime timestamp;
        StreamKey key;
        QString messageName;
        int packetLength = 0;
        int mavlinkVersion = 0;
        quint8 sequence = 0;
        QByteArray rawPacket;
    };

    static QString messageNameFor(quint32 messageId);
    static QString hexBytes(const QByteArray &packet);
    static QString asciiBytes(const QByteArray &packet);
    static QString formattedPacket(const QByteArray &packet);
    static QVariantList graphableFields(const QVariantList &fields);
    static std::optional<double> graphValue(const QVariantList &fields, const QString &fieldName);
    void appendGraphSample();
    void refreshFields(StreamState &state);
    void attach(Mavlink *mavlink);
    const RawPacketRecord *latestRawPacketFor(const StreamKey &key) const;

    QHash<StreamKey, StreamState> streams;
    QVector<RawPacketRecord> rawPackets;
    MavlinkInspectorModel *inspectorModel = nullptr;
    MavlinkInspectorFilterModel *filterModel = nullptr;
    MavlinkInspectorGraphModel *inspectorGraphModel = nullptr;
    QTimer refreshTimer;
    bool pausedValue = false;
    bool dirty = false;
    std::optional<StreamKey> selectedKey;
    std::optional<StreamKey> graphKey;
    QString graphFieldName;
    quint64 graphLastObservedCount = 0;
    int streamCountValue = 0;
    double totalRxRateValue = 0.0;
    double totalRxBytesPerSecondValue = 0.0;
};
