#include "MissionPlanModel.h"

#include <cmath>
#include <limits>

namespace {
bool finite(double value)
{
    return std::isfinite(value);
}

bool finite(float value)
{
    return std::isfinite(value);
}

bool number(const QVariantMap &map, const char *key, double *value, QString *error)
{
    const QString name = QString::fromLatin1(key);
    if (!map.contains(name)) {
        return true;
    }
    bool ok = false;
    const double parsed = map.value(name).toDouble(&ok);
    if (!ok || !finite(parsed)) {
        if (error) {
            *error = QStringLiteral("%1 must be a finite number.").arg(name);
        }
        return false;
    }
    *value = parsed;
    return true;
}

bool integer(const QVariantMap &map, const char *key, int minimum, int maximum, int *value, QString *error)
{
    const QString name = QString::fromLatin1(key);
    if (!map.contains(name)) {
        return true;
    }
    bool ok = false;
    const qlonglong parsed = map.value(name).toLongLong(&ok);
    if (!ok || parsed < minimum || parsed > maximum) {
        if (error) {
            *error = QStringLiteral("%1 is outside its MAVLink field range.").arg(name);
        }
        return false;
    }
    *value = static_cast<int>(parsed);
    return true;
}

bool boolValue(const QVariantMap &map, const char *key, bool *value, QString *error)
{
    const QString name = QString::fromLatin1(key);
    if (!map.contains(name)) {
        return true;
    }
    const QVariant raw = map.value(name);
    if (!raw.canConvert<bool>()) {
        if (error) {
            *error = QStringLiteral("%1 must be a boolean.").arg(name);
        }
        return false;
    }
    *value = raw.toBool();
    return true;
}
}

MissionPlanModel::MissionPlanModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MissionPlanModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : items.size();
}

int MissionPlanModel::count() const
{
    return items.size();
}

QVariant MissionPlanModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= items.size()) {
        return {};
    }
    const MissionPlanItem &item = items.at(index.row());
    switch (role) {
    case IdRole: return item.id.toString(QUuid::WithoutBraces);
    case SequenceRole: return item.sequence;
    case CommandRole: return item.command;
    case FrameRole: return item.frame;
    case LatitudeRole: return item.hasCoordinate ? QVariant(item.latitude) : QVariant();
    case LongitudeRole: return item.hasCoordinate ? QVariant(item.longitude) : QVariant();
    case AltitudeRole: return item.altitude;
    case Param1Role: return item.param1;
    case Param2Role: return item.param2;
    case Param3Role: return item.param3;
    case Param4Role: return item.param4;
    case CurrentRole: return item.current;
    case AutocontinueRole: return item.autocontinue;
    case HasCoordinateRole: return item.hasCoordinate;
    case ValidationStatusRole: return Valid;
    case ValidationDiagnosticsRole: return QString();
    case SelectedRole: return item.id == selectedItem;
    default: return {};
    }
}

QHash<int, QByteArray> MissionPlanModel::roleNames() const
{
    return {
        { IdRole, "id" }, { SequenceRole, "sequence" }, { CommandRole, "command" },
        { FrameRole, "frame" }, { LatitudeRole, "latitude" }, { LongitudeRole, "longitude" },
        { AltitudeRole, "altitude" }, { Param1Role, "param1" }, { Param2Role, "param2" },
        { Param3Role, "param3" }, { Param4Role, "param4" }, { CurrentRole, "current" },
        { AutocontinueRole, "autocontinue" }, { HasCoordinateRole, "hasCoordinate" },
        { ValidationStatusRole, "validationStatus" }, { ValidationDiagnosticsRole, "validationDiagnostics" },
        { SelectedRole, "selected" },
    };
}

MissionPlanModel::SourceState MissionPlanModel::sourceState() const { return sourceStateValue; }
void MissionPlanModel::setSourceState(SourceState sourceState) { setSourceStateInternal(sourceState); }
quint64 MissionPlanModel::revision() const { return revisionValue; }
quint64 MissionPlanModel::lastSavedRevision() const { return lastSavedRevisionValue; }
bool MissionPlanModel::dirty() const { return revisionValue != lastSavedRevisionValue; }
QString MissionPlanModel::selectedItemId() const
{
    return selectedItem.isNull() ? QString() : selectedItem.toString(QUuid::WithoutBraces);
}

bool MissionPlanModel::validateItem(const MissionPlanItem &item, QString *diagnostics)
{
    QStringList failures;
    if (item.hasCoordinate && (!finite(item.latitude) || item.latitude < -90.0 || item.latitude > 90.0)) {
        failures << QStringLiteral("Latitude must be between -90 and 90 when coordinates are present.");
    }
    if (item.hasCoordinate && (!finite(item.longitude) || item.longitude < -180.0 || item.longitude > 180.0)) {
        failures << QStringLiteral("Longitude must be between -180 and 180 when coordinates are present.");
    }
    if (!finite(item.altitude)) failures << QStringLiteral("Altitude must be finite.");
    if (!finite(item.param1) || !finite(item.param2) || !finite(item.param3) || !finite(item.param4)) {
        failures << QStringLiteral("Mission parameters must be finite.");
    }
    if (diagnostics) *diagnostics = failures.join(QLatin1Char(' '));
    return failures.isEmpty();
}

bool MissionPlanModel::validateCollection(const QVector<MissionPlanItem> &candidate, QString *error) const
{
    if (candidate.size() > std::numeric_limits<quint16>::max() + 1LL) {
        if (error) *error = QStringLiteral("A MAVLink mission cannot contain more than 65536 items.");
        return false;
    }
    QSet<QUuid> ids;
    for (const MissionPlanItem &item : candidate) {
        if (item.id.isNull()) {
            if (error) *error = QStringLiteral("Mission items require a stable ID.");
            return false;
        }
        if (ids.contains(item.id)) {
            if (error) *error = QStringLiteral("Mission item IDs must be unique.");
            return false;
        }
        ids.insert(item.id);
        QString diagnostics;
        if (!validateItem(item, &diagnostics)) {
            if (error) *error = diagnostics;
            return false;
        }
    }
    return true;
}

void MissionPlanModel::normalizeSequences()
{
    for (qsizetype row = 0; row < items.size(); ++row) {
        items[row].sequence = static_cast<quint16>(row);
    }
}

void MissionPlanModel::contentChanged()
{
    const bool wasDirty = dirty();
    ++revisionValue;
    emit revisionChanged();
    if (wasDirty != dirty()) emit dirtyChanged();
}

void MissionPlanModel::setLastOperationError(const QString &error)
{
    if (lastOperationErrorValue == error) return;
    lastOperationErrorValue = error;
    emit lastOperationErrorChanged();
}

void MissionPlanModel::setSourceStateInternal(SourceState sourceState)
{
    if (sourceStateValue == sourceState) return;
    sourceStateValue = sourceState;
    emit sourceStateChanged();
}

bool MissionPlanModel::insertMissionItem(MissionPlanItem item, int row, QString *error)
{
    if (item.id.isNull()) item.id = QUuid::createUuid();
    if (row < 0) row = items.size();
    if (row < 0 || row > items.size()) {
        if (error) *error = QStringLiteral("Insert row is outside the mission plan.");
        setLastOperationError(error ? *error : QStringLiteral("Invalid insert row."));
        return false;
    }
    QVector<MissionPlanItem> candidate = items;
    candidate.insert(row, item);
    if (!validateCollection(candidate, error)) {
        setLastOperationError(error ? *error : QStringLiteral("Invalid mission item."));
        return false;
    }
    beginInsertRows({}, row, row);
    items.insert(row, item);
    normalizeSequences();
    endInsertRows();
    emit countChanged();
    if (sourceStateValue == Empty) setSourceStateInternal(ManualDraft);
    contentChanged();
    setLastOperationError({});
    return true;
}

bool MissionPlanModel::updateMissionItem(const QUuid &id, const MissionPlanItem &item, QString *error)
{
    const int row = indexOfId(id);
    if (row < 0 || item.id != id) {
        if (error) *error = QStringLiteral("Mission item was not found or its stable ID changed.");
        setLastOperationError(error ? *error : QStringLiteral("Invalid mission item update."));
        return false;
    }
    if (!validateItem(item, error)) {
        setLastOperationError(error ? *error : QStringLiteral("Invalid mission item."));
        return false;
    }
    MissionPlanItem normalized = item;
    normalized.sequence = items.at(row).sequence;
    if (itemEquals(items.at(row), normalized)) {
        setLastOperationError({});
        return true;
    }
    items[row] = normalized;
    emit dataChanged(index(row), index(row));
    contentChanged();
    setLastOperationError({});
    return true;
}

bool MissionPlanModel::removeMissionItem(const QUuid &id, QString *error)
{
    const int row = indexOfId(id);
    if (row < 0) {
        if (error) *error = QStringLiteral("Mission item was not found.");
        setLastOperationError(error ? *error : QStringLiteral("Mission item was not found."));
        return false;
    }
    const bool selected = selectedItem == id;
    beginRemoveRows({}, row, row);
    items.removeAt(row);
    normalizeSequences();
    endRemoveRows();
    emit countChanged();
    if (selected) setSelectedItemId({});
    if (items.isEmpty()) setSourceStateInternal(Empty);
    contentChanged();
    setLastOperationError({});
    return true;
}

bool MissionPlanModel::moveMissionItem(int from, int to, QString *error)
{
    if (from < 0 || from >= items.size() || to < 0 || to >= items.size()) {
        if (error) *error = QStringLiteral("Move rows are outside the mission plan.");
        setLastOperationError(error ? *error : QStringLiteral("Invalid move."));
        return false;
    }
    if (from == to) {
        setLastOperationError({});
        return true;
    }
    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
    items.move(from, to);
    normalizeSequences();
    endMoveRows();
    emit dataChanged(index(0), index(items.size() - 1), { SequenceRole });
    contentChanged();
    setLastOperationError({});
    return true;
}

bool MissionPlanModel::clearMission(QString *error)
{
    Q_UNUSED(error)
    if (items.isEmpty()) {
        setLastOperationError({});
        return true;
    }
    beginResetModel();
    items.clear();
    selectedItem = {};
    endResetModel();
    emit countChanged();
    setSourceStateInternal(Empty);
    emit selectedItemIdChanged();
    contentChanged();
    setLastOperationError({});
    return true;
}

bool MissionPlanModel::replaceMissionItems(QVector<MissionPlanItem> replacement, SourceState replacementSource, QString *error)
{
    for (MissionPlanItem &item : replacement) if (item.id.isNull()) item.id = QUuid::createUuid();
    if (!validateCollection(replacement, error)) {
        setLastOperationError(error ? *error : QStringLiteral("Invalid mission plan."));
        return false;
    }
    for (qsizetype row = 0; row < replacement.size(); ++row) replacement[row].sequence = static_cast<quint16>(row);
    bool same = items.size() == replacement.size();
    if (same) {
        for (qsizetype row = 0; row < items.size(); ++row) {
            if (!itemEquals(items.at(row), replacement.at(row))) { same = false; break; }
        }
    }
    if (!same) {
        beginResetModel();
        items = std::move(replacement);
        if (!selectedItem.isNull() && indexOfId(selectedItem) < 0) selectedItem = {};
        endResetModel();
        emit countChanged();
        contentChanged();
        emit selectedItemIdChanged();
    }
    setSourceStateInternal(items.isEmpty() ? Empty : replacementSource);
    if (replacementSource == ImportedMission || replacementSource == DownloadedVehicleMission) markCurrentRevisionSaved();
    setLastOperationError({});
    return true;
}

std::optional<MissionPlanItem> MissionPlanModel::missionItemAt(int row) const
{
    if (row < 0 || row >= items.size()) return std::nullopt;
    return items.at(row);
}

std::optional<MissionPlanItem> MissionPlanModel::missionItemById(const QUuid &id) const
{
    const int row = indexOfId(id);
    return row < 0 ? std::nullopt : std::optional<MissionPlanItem>(items.at(row));
}

int MissionPlanModel::indexOfId(const QUuid &id) const
{
    for (qsizetype row = 0; row < items.size(); ++row) if (items.at(row).id == id) return static_cast<int>(row);
    return -1;
}

QVariantMap MissionPlanModel::itemToVariantMap(const MissionPlanItem &item, bool selected)
{
    return {
        { QStringLiteral("id"), item.id.toString(QUuid::WithoutBraces) },
        { QStringLiteral("sequence"), item.sequence }, { QStringLiteral("command"), item.command },
        { QStringLiteral("frame"), item.frame }, { QStringLiteral("latitude"), item.hasCoordinate ? QVariant(item.latitude) : QVariant() },
        { QStringLiteral("longitude"), item.hasCoordinate ? QVariant(item.longitude) : QVariant() },
        { QStringLiteral("altitude"), item.altitude }, { QStringLiteral("param1"), item.param1 },
        { QStringLiteral("param2"), item.param2 }, { QStringLiteral("param3"), item.param3 },
        { QStringLiteral("param4"), item.param4 }, { QStringLiteral("current"), item.current },
        { QStringLiteral("autocontinue"), item.autocontinue }, { QStringLiteral("hasCoordinate"), item.hasCoordinate },
        { QStringLiteral("validationStatus"), Valid }, { QStringLiteral("validationDiagnostics"), QString() },
        { QStringLiteral("selected"), selected },
    };
}

bool MissionPlanModel::itemEquals(const MissionPlanItem &a, const MissionPlanItem &b)
{
    return a.id == b.id && a.sequence == b.sequence && a.command == b.command && a.frame == b.frame
            && a.latitude == b.latitude && a.longitude == b.longitude && a.altitude == b.altitude
            && a.param1 == b.param1 && a.param2 == b.param2 && a.param3 == b.param3 && a.param4 == b.param4
            && a.current == b.current && a.autocontinue == b.autocontinue && a.hasCoordinate == b.hasCoordinate;
}

bool MissionPlanModel::parseId(const QString &text, QUuid *id)
{
    const QUuid parsed(text);
    if (parsed.isNull()) return false;
    *id = parsed;
    return true;
}

bool MissionPlanModel::sourceStateFromInt(int value, SourceState *sourceState)
{
    if (value < Empty || value > AutomaticPlannerResult) return false;
    *sourceState = static_cast<SourceState>(value);
    return true;
}

bool MissionPlanModel::itemFromVariantMap(const QVariantMap &map, MissionPlanItem *item, bool partial, QString *error) const
{
    MissionPlanItem parsed = partial ? *item : MissionPlanItem {};
    if (map.contains(QStringLiteral("id"))) {
        QUuid id;
        if (!parseId(map.value(QStringLiteral("id")).toString(), &id)) {
            if (error) *error = QStringLiteral("id must be a UUID.");
            return false;
        }
        parsed.id = id;
    }
    int command = parsed.command, frame = parsed.frame;
    if (!integer(map, "command", 0, std::numeric_limits<quint16>::max(), &command, error)
            || !integer(map, "frame", 0, std::numeric_limits<quint8>::max(), &frame, error)) return false;
    parsed.command = static_cast<quint16>(command); parsed.frame = static_cast<quint8>(frame);
    double latitude = parsed.latitude, longitude = parsed.longitude, altitude = parsed.altitude;
    double param1 = parsed.param1, param2 = parsed.param2, param3 = parsed.param3, param4 = parsed.param4;
    if (!number(map, "latitude", &latitude, error) || !number(map, "longitude", &longitude, error)
            || !number(map, "altitude", &altitude, error) || !number(map, "param1", &param1, error)
            || !number(map, "param2", &param2, error) || !number(map, "param3", &param3, error)
            || !number(map, "param4", &param4, error)) return false;
    if (std::abs(altitude) > std::numeric_limits<float>::max() || std::abs(param1) > std::numeric_limits<float>::max()
            || std::abs(param2) > std::numeric_limits<float>::max() || std::abs(param3) > std::numeric_limits<float>::max()
            || std::abs(param4) > std::numeric_limits<float>::max()) {
        if (error) *error = QStringLiteral("Altitude and parameters exceed MAVLink float precision.");
        return false;
    }
    parsed.latitude = latitude; parsed.longitude = longitude; parsed.altitude = static_cast<float>(altitude);
    parsed.param1 = static_cast<float>(param1); parsed.param2 = static_cast<float>(param2);
    parsed.param3 = static_cast<float>(param3); parsed.param4 = static_cast<float>(param4);
    if (!boolValue(map, "current", &parsed.current, error) || !boolValue(map, "autocontinue", &parsed.autocontinue, error)
            || !boolValue(map, "hasCoordinate", &parsed.hasCoordinate, error)) return false;
    *item = parsed;
    return true;
}

bool MissionPlanModel::insertItem(const QVariantMap &map, int row)
{
    MissionPlanItem item;
    QString error;
    if (!itemFromVariantMap(map, &item, false, &error)) { setLastOperationError(error); return false; }
    return insertMissionItem(item, row, &error);
}

bool MissionPlanModel::updateItem(const QString &idText, const QVariantMap &map)
{
    QUuid id; QString error;
    if (!parseId(idText, &id)) { setLastOperationError(QStringLiteral("id must be a UUID.")); return false; }
    const auto existing = missionItemById(id);
    if (!existing) { setLastOperationError(QStringLiteral("Mission item was not found.")); return false; }
    MissionPlanItem item = *existing;
    if (!itemFromVariantMap(map, &item, true, &error)) { setLastOperationError(error); return false; }
    return updateMissionItem(id, item, &error);
}

bool MissionPlanModel::removeItem(const QString &idText)
{
    QUuid id; QString error;
    if (!parseId(idText, &id)) { setLastOperationError(QStringLiteral("id must be a UUID.")); return false; }
    return removeMissionItem(id, &error);
}

bool MissionPlanModel::moveItem(int from, int to)
{
    QString error;
    return moveMissionItem(from, to, &error);
}

bool MissionPlanModel::clear()
{
    QString error;
    return clearMission(&error);
}

bool MissionPlanModel::replaceAll(const QVariantList &maps, int source)
{
    SourceState parsedSource;
    if (!sourceStateFromInt(source, &parsedSource)) { setLastOperationError(QStringLiteral("Invalid mission source state.")); return false; }
    QVector<MissionPlanItem> replacement;
    replacement.reserve(maps.size());
    for (const QVariant &entry : maps) {
        MissionPlanItem item; QString error;
        if (!entry.canConvert<QVariantMap>() || !itemFromVariantMap(entry.toMap(), &item, false, &error)) {
            setLastOperationError(error.isEmpty() ? QStringLiteral("Mission items must be maps.") : error);
            return false;
        }
        replacement.append(item);
    }
    QString error;
    return replaceMissionItems(std::move(replacement), parsedSource, &error);
}

QVariantMap MissionPlanModel::itemAt(int row) const
{
    const auto item = missionItemAt(row);
    return item ? itemToVariantMap(*item, item->id == selectedItem) : QVariantMap {};
}

QVariantMap MissionPlanModel::itemById(const QString &idText) const
{
    QUuid id;
    if (!parseId(idText, &id)) return {};
    const auto item = missionItemById(id);
    return item ? itemToVariantMap(*item, item->id == selectedItem) : QVariantMap {};
}

int MissionPlanModel::indexOfId(const QString &idText) const
{
    QUuid id;
    return parseId(idText, &id) ? indexOfId(id) : -1;
}

void MissionPlanModel::setSelectedItemId(const QString &idText)
{
    QUuid next;
    if (!idText.isEmpty() && (!parseId(idText, &next) || indexOfId(next) < 0)) return;
    if (selectedItem == next) return;
    const int oldRow = indexOfId(selectedItem);
    selectedItem = next;
    const int newRow = indexOfId(selectedItem);
    if (oldRow >= 0) emit dataChanged(index(oldRow), index(oldRow), { SelectedRole });
    if (newRow >= 0) emit dataChanged(index(newRow), index(newRow), { SelectedRole });
    emit selectedItemIdChanged();
}

void MissionPlanModel::markCurrentRevisionSaved()
{
    if (lastSavedRevisionValue == revisionValue) return;
    const bool wasDirty = dirty();
    lastSavedRevisionValue = revisionValue;
    emit lastSavedRevisionChanged();
    if (wasDirty != dirty()) emit dirtyChanged();
}

QString MissionPlanModel::lastOperationError() const { return lastOperationErrorValue; }
