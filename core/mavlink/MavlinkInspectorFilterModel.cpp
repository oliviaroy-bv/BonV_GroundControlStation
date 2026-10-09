#include "MavlinkInspectorFilterModel.h"

#include "MavlinkInspectorModel.h"

#include <QSet>

namespace {
QVariantMap option(const QString &label, const QVariant &value)
{
    return {{QStringLiteral("label"), label}, {QStringLiteral("value"), value}};
}
}

MavlinkInspectorFilterModel::MavlinkInspectorFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
}

void MavlinkInspectorFilterModel::setSourceModel(QAbstractItemModel *newSourceModel)
{
    if (sourceModel() == newSourceModel) {
        return;
    }
    if (sourceModel()) {
        disconnect(sourceModel(), nullptr, this, nullptr);
    }
    QSortFilterProxyModel::setSourceModel(newSourceModel);
    if (newSourceModel) {
        connect(newSourceModel, &QAbstractItemModel::modelReset, this, &MavlinkInspectorFilterModel::rebuildOptions);
        connect(newSourceModel, &QAbstractItemModel::rowsInserted, this, &MavlinkInspectorFilterModel::rebuildOptions);
        connect(newSourceModel, &QAbstractItemModel::rowsRemoved, this, &MavlinkInspectorFilterModel::rebuildOptions);
    }
    rebuildOptions();
}

void MavlinkInspectorFilterModel::setSystemIdFilter(int systemId)
{
    const int normalized = systemId < 0 ? -1 : systemId;
    if (systemIdValue == normalized) return;
    systemIdValue = normalized;
    invalidateFilter();
    emit filtersChanged();
}

void MavlinkInspectorFilterModel::setComponentIdFilter(int componentId)
{
    const int normalized = componentId < 0 ? -1 : componentId;
    if (componentIdValue == normalized) return;
    componentIdValue = normalized;
    invalidateFilter();
    emit filtersChanged();
}

void MavlinkInspectorFilterModel::setMessageIdFilter(qint64 messageId)
{
    const qint64 normalized = messageId < 0 ? -1 : messageId;
    if (messageIdValue == normalized) return;
    messageIdValue = normalized;
    invalidateFilter();
    emit filtersChanged();
}

void MavlinkInspectorFilterModel::setLinkFilter(const QString &link)
{
    const QString normalized = link.trimmed();
    if (linkValue == normalized) return;
    linkValue = normalized;
    invalidateFilter();
    emit filtersChanged();
}

void MavlinkInspectorFilterModel::setSearchText(const QString &searchText)
{
    const QString normalized = searchText.trimmed();
    if (searchValue == normalized) return;
    searchValue = normalized;
    invalidateFilter();
    emit filtersChanged();
}

void MavlinkInspectorFilterModel::clearFilters()
{
    const bool changed = systemIdValue != -1 || componentIdValue != -1 || messageIdValue != -1
            || !linkValue.isEmpty() || !searchValue.isEmpty();
    systemIdValue = -1;
    componentIdValue = -1;
    messageIdValue = -1;
    linkValue.clear();
    searchValue.clear();
    if (changed) {
        invalidateFilter();
        emit filtersChanged();
    }
}

bool MavlinkInspectorFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    const int sysid = index.data(MavlinkInspectorModel::SystemIdRole).toInt();
    const int compid = index.data(MavlinkInspectorModel::ComponentIdRole).toInt();
    const quint32 messageId = index.data(MavlinkInspectorModel::MessageIdRole).toUInt();
    const QString link = index.data(MavlinkInspectorModel::LinkRole).toString();
    if ((systemIdValue >= 0 && sysid != systemIdValue)
            || (componentIdValue >= 0 && compid != componentIdValue)
            || (messageIdValue >= 0 && messageId != static_cast<quint32>(messageIdValue))
            || (!linkValue.isEmpty() && link != linkValue)) {
        return false;
    }
    if (searchValue.isEmpty()) {
        return true;
    }
    return index.data(MavlinkInspectorModel::SearchTextRole)
            .toString().contains(searchValue, Qt::CaseInsensitive);
}

void MavlinkInspectorFilterModel::rebuildOptions()
{
    QSet<int> systemIds;
    QSet<int> componentIds;
    QMap<quint32, QString> messages;
    QSet<QString> links;
    for (int row = 0; sourceModel() && row < sourceModel()->rowCount(); ++row) {
        const QModelIndex index = sourceModel()->index(row, 0);
        systemIds.insert(index.data(MavlinkInspectorModel::SystemIdRole).toInt());
        componentIds.insert(index.data(MavlinkInspectorModel::ComponentIdRole).toInt());
        messages.insert(index.data(MavlinkInspectorModel::MessageIdRole).toUInt(), index.data(MavlinkInspectorModel::MessageNameRole).toString());
        links.insert(index.data(MavlinkInspectorModel::LinkRole).toString());
    }
    QList<int> sortedSystemIds = systemIds.values(); std::sort(sortedSystemIds.begin(), sortedSystemIds.end());
    QList<int> sortedComponentIds = componentIds.values(); std::sort(sortedComponentIds.begin(), sortedComponentIds.end());
    QStringList sortedLinks = links.values(); std::sort(sortedLinks.begin(), sortedLinks.end());

    QVariantList vehicles { option(QStringLiteral("ALL VEHICLES"), -1) };
    for (int id : sortedSystemIds) vehicles.append(option(QStringLiteral("Vehicle %1 / SYSID %1").arg(id), id));
    QVariantList components { option(QStringLiteral("ALL COMPONENTS"), -1) };
    for (int id : sortedComponentIds) components.append(option(QStringLiteral("COMP %1").arg(id), id));
    QVariantList messageOptions { option(QStringLiteral("ALL MESSAGES"), -1) };
    for (auto it = messages.cbegin(); it != messages.cend(); ++it) messageOptions.append(option(QStringLiteral("%1 (%2)").arg(it.value()).arg(it.key()), it.key()));
    QVariantList linkOptions { option(QStringLiteral("ALL LINKS"), QString()) };
    for (const QString &link : sortedLinks) linkOptions.append(option(link, link));

    if (vehicleOptionValues != vehicles || componentOptionValues != components || messageOptionValues != messageOptions || linkOptionValues != linkOptions) {
        vehicleOptionValues = vehicles;
        componentOptionValues = components;
        messageOptionValues = messageOptions;
        linkOptionValues = linkOptions;
        emit filterOptionsChanged();
    }
}
