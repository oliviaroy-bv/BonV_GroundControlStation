#pragma once

#include <QSortFilterProxyModel>
#include <QVariantList>

// A view-only filter over the inspector's aggregated stream model. It never
// receives MAVLink frames and therefore cannot alter collection or statistics.
class MavlinkInspectorFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(int systemIdFilter READ systemIdFilter WRITE setSystemIdFilter NOTIFY filtersChanged)
    Q_PROPERTY(int componentIdFilter READ componentIdFilter WRITE setComponentIdFilter NOTIFY filtersChanged)
    Q_PROPERTY(qint64 messageIdFilter READ messageIdFilter WRITE setMessageIdFilter NOTIFY filtersChanged)
    Q_PROPERTY(QString linkFilter READ linkFilter WRITE setLinkFilter NOTIFY filtersChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY filtersChanged)
    Q_PROPERTY(QVariantList vehicleOptions READ vehicleOptions NOTIFY filterOptionsChanged)
    Q_PROPERTY(QVariantList componentOptions READ componentOptions NOTIFY filterOptionsChanged)
    Q_PROPERTY(QVariantList messageOptions READ messageOptions NOTIFY filterOptionsChanged)
    Q_PROPERTY(QVariantList linkOptions READ linkOptions NOTIFY filterOptionsChanged)

public:
    explicit MavlinkInspectorFilterModel(QObject *parent = nullptr);

    int systemIdFilter() const { return systemIdValue; }
    int componentIdFilter() const { return componentIdValue; }
    qint64 messageIdFilter() const { return messageIdValue; }
    QString linkFilter() const { return linkValue; }
    QString searchText() const { return searchValue; }
    QVariantList vehicleOptions() const { return vehicleOptionValues; }
    QVariantList componentOptions() const { return componentOptionValues; }
    QVariantList messageOptions() const { return messageOptionValues; }
    QVariantList linkOptions() const { return linkOptionValues; }

    void setSourceModel(QAbstractItemModel *sourceModel) override;

public slots:
    void setSystemIdFilter(int systemId);
    void setComponentIdFilter(int componentId);
    void setMessageIdFilter(qint64 messageId);
    void setLinkFilter(const QString &link);
    void setSearchText(const QString &searchText);
    Q_INVOKABLE void clearFilters();

signals:
    void filtersChanged();
    void filterOptionsChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private slots:
    void rebuildOptions();

private:
    int systemIdValue = -1;
    int componentIdValue = -1;
    qint64 messageIdValue = -1;
    QString linkValue;
    QString searchValue;
    QVariantList vehicleOptionValues;
    QVariantList componentOptionValues;
    QVariantList messageOptionValues;
    QVariantList linkOptionValues;
};
