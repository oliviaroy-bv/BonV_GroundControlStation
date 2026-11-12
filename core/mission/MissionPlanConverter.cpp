#include "MissionPlanConverter.h"

#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>

#include <cmath>
#include <limits>

namespace {
bool parseInteger(const QString &text, int minimum, int maximum, int *value)
{
    bool ok = false;
    const qlonglong parsed = text.toLongLong(&ok);
    if (!ok || parsed < minimum || parsed > maximum) return false;
    *value = static_cast<int>(parsed);
    return true;
}

bool parseFloat(const QString &text, float *value)
{
    bool ok = false;
    const float parsed = text.toFloat(&ok);
    if (!ok || !std::isfinite(parsed)) return false;
    *value = parsed;
    return true;
}

bool parseDouble(const QString &text, double *value)
{
    bool ok = false;
    const double parsed = text.toDouble(&ok);
    if (!ok || !std::isfinite(parsed)) return false;
    *value = parsed;
    return true;
}

QString wplNumber(float value)
{
    return QString::number(value, 'g', std::numeric_limits<float>::max_digits10);
}

QString wplNumber(double value)
{
    return QString::number(value, 'g', std::numeric_limits<double>::max_digits10);
}
}

MissionPlanConverter::MissionPlanConverter(QObject *parent)
    : QObject(parent)
{
}

bool MissionPlanConverter::finite(float value) { return std::isfinite(value); }
bool MissionPlanConverter::finite(double value) { return std::isfinite(value); }

namespace {
// QGC WPL and several autopilots encode the home location as the initial,
// current NAV_WAYPOINT item.  It remains a protocol item in the canonical
// model, but is not an operator waypoint and must not receive a map pin.
bool isHomeMissionItem(const MissionPlanItem &item)
{
    return item.sequence == 0 && item.current && item.hasCoordinate
            && item.command == MAV_CMD_NAV_WAYPOINT;
}
}

MissionPlanConverter::Result MissionPlanConverter::makeResultError(const QString &error, int line)
{
    return { false, line > 0 ? QStringLiteral("Line %1: %2").arg(line).arg(error) : error, line };
}

bool MissionPlanConverter::isGlobalFrame(quint8 frame)
{
    switch (frame) {
    case MAV_FRAME_GLOBAL:
    case MAV_FRAME_GLOBAL_INT:
    case MAV_FRAME_GLOBAL_RELATIVE_ALT:
    case MAV_FRAME_GLOBAL_RELATIVE_ALT_INT:
    case MAV_FRAME_GLOBAL_TERRAIN_ALT:
    case MAV_FRAME_GLOBAL_TERRAIN_ALT_INT:
        return true;
    default:
        return false;
    }
}

MissionPlanConverter::Result MissionPlanConverter::parseWpl(const QString &path, QVector<MissionPlanItem> *items)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return makeResultError(QStringLiteral("Could not open mission file: %1").arg(file.errorString()));
    }
    QTextStream stream(&file);
    if (stream.readLine().trimmed() != QStringLiteral("QGC WPL 110")) {
        return makeResultError(QStringLiteral("Expected QGC WPL 110 header."), 1);
    }

    QVector<MissionPlanItem> parsed;
    int lineNumber = 1;
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        ++lineNumber;
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
        const QStringList columns = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (columns.size() != 12) return makeResultError(QStringLiteral("Expected 12 WPL fields."), lineNumber);

        int sequence = 0, current = 0, frame = 0, command = 0, autocontinue = 0;
        MissionPlanItem item;
        if (!parseInteger(columns.at(0), 0, std::numeric_limits<quint16>::max(), &sequence)
                || !parseInteger(columns.at(1), 0, 1, &current)
                || !parseInteger(columns.at(2), 0, std::numeric_limits<quint8>::max(), &frame)
                || !parseInteger(columns.at(3), 0, std::numeric_limits<quint16>::max(), &command)
                || !parseInteger(columns.at(11), 0, 1, &autocontinue)) {
            return makeResultError(QStringLiteral("Invalid integer mission field."), lineNumber);
        }
        if (!parseFloat(columns.at(4), &item.param1) || !parseFloat(columns.at(5), &item.param2)
                || !parseFloat(columns.at(6), &item.param3) || !parseFloat(columns.at(7), &item.param4)
                || !parseDouble(columns.at(8), &item.latitude) || !parseDouble(columns.at(9), &item.longitude)
                || !parseFloat(columns.at(10), &item.altitude)) {
            return makeResultError(QStringLiteral("Invalid finite numeric mission field."), lineNumber);
        }
        Q_UNUSED(sequence)
        item.id = QUuid::createUuid();
        item.command = static_cast<quint16>(command);
        item.frame = static_cast<quint8>(frame);
        item.current = current != 0;
        item.autocontinue = autocontinue != 0;
        // WPL uses x/y for more than geographic coordinates.  Only global
        // frames with valid latitude/longitude receive map-coordinate meaning.
        item.hasCoordinate = isGlobalFrame(item.frame)
                && item.latitude >= -90.0 && item.latitude <= 90.0
                && item.longitude >= -180.0 && item.longitude <= 180.0;
        if (isGlobalFrame(item.frame) && !item.hasCoordinate) {
            return makeResultError(QStringLiteral("Global-frame coordinates are outside latitude/longitude bounds."), lineNumber);
        }
        parsed.append(item);
    }
    if (parsed.isEmpty()) return makeResultError(QStringLiteral("Mission file has no mission items."));
    *items = std::move(parsed);
    return { true, {}, -1 };
}

MissionPlanConverter::Result MissionPlanConverter::importWplFile(const QString &path, MissionPlanModel *model,
                                                                  MissionPlanModel::SourceState sourceState)
{
    if (!model) return makeResultError(QStringLiteral("Mission plan model is unavailable."));
    QVector<MissionPlanItem> items;
    Result result = parseWpl(path, &items);
    if (!result.success) return result;
    QString error;
    if (!model->replaceMissionItems(std::move(items), sourceState, &error)) return makeResultError(error);
    return { true, {}, -1 };
}

MissionPlanConverter::Result MissionPlanConverter::exportWplFile(const QString &path, const MissionPlanModel &model)
{
    if (path.trimmed().isEmpty()) return makeResultError(QStringLiteral("Select an output mission file."));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return makeResultError(QStringLiteral("Could not write mission file: %1").arg(file.errorString()));
    }
    QTextStream stream(&file);
    stream << "QGC WPL 110\n";
    for (int row = 0; row < model.rowCount(); ++row) {
        const auto value = model.missionItemAt(row);
        if (!value) return makeResultError(QStringLiteral("Model row %1 is unavailable.").arg(row));
        const MissionPlanItem &item = *value;
        if (!finite(item.altitude) || !finite(item.param1) || !finite(item.param2)
                || !finite(item.param3) || !finite(item.param4)
                || !finite(item.latitude) || !finite(item.longitude)) {
            return makeResultError(QStringLiteral("Mission item %1 has values that WPL cannot represent.").arg(row));
        }
        if (item.hasCoordinate && (item.latitude < -90.0 || item.latitude > 90.0
                                   || item.longitude < -180.0 || item.longitude > 180.0)) {
            return makeResultError(QStringLiteral("Mission item %1 has invalid geographic coordinates.").arg(row));
        }
        stream << row << '\t' << (item.current ? 1 : 0) << '\t' << item.frame << '\t' << item.command << '\t'
               << wplNumber(item.param1) << '\t' << wplNumber(item.param2) << '\t'
               << wplNumber(item.param3) << '\t' << wplNumber(item.param4) << '\t'
               << wplNumber(item.latitude) << '\t' << wplNumber(item.longitude) << '\t'
               << wplNumber(item.altitude) << '\t' << (item.autocontinue ? 1 : 0) << '\n';
    }
    if (stream.status() != QTextStream::Ok || !file.commit()) {
        return makeResultError(QStringLiteral("Could not finalize mission file."));
    }
    return { true, {}, -1 };
}

MissionPlanConverter::Result MissionPlanConverter::importVehicleMission(const QVector<MavlinkMissionItem> &rawItems,
                                                                         MissionPlanModel *model, bool replaceDirty)
{
    if (!model) return makeResultError(QStringLiteral("Mission plan model is unavailable."));
    if (model->dirty() && !replaceDirty) {
        return makeResultError(QStringLiteral("A local mission has unsaved changes; replacement requires an explicit decision."));
    }
    if (rawItems.isEmpty()) return makeResultError(QStringLiteral("Vehicle mission has no items."));
    QVector<MissionPlanItem> converted;
    converted.reserve(rawItems.size());
    for (const MavlinkMissionItem &raw : rawItems) {
        if (!finite(raw.x) || !finite(raw.y) || !finite(raw.z) || !finite(raw.param1) || !finite(raw.param2)
                || !finite(raw.param3) || !finite(raw.param4) || raw.current > 1 || raw.autocontinue > 1) {
            return makeResultError(QStringLiteral("Vehicle mission contains an unrepresentable item."));
        }
        MissionPlanItem item;
        item.id = QUuid::createUuid();
        item.command = raw.command;
        item.frame = raw.frame;
        item.latitude = raw.x;
        item.longitude = raw.y;
        item.altitude = raw.z;
        item.param1 = raw.param1; item.param2 = raw.param2; item.param3 = raw.param3; item.param4 = raw.param4;
        item.current = raw.current != 0; item.autocontinue = raw.autocontinue != 0;
        item.hasCoordinate = isGlobalFrame(item.frame) && item.latitude >= -90.0 && item.latitude <= 90.0
                && item.longitude >= -180.0 && item.longitude <= 180.0;
        if (isGlobalFrame(item.frame) && !item.hasCoordinate) {
            return makeResultError(QStringLiteral("Vehicle mission contains invalid global coordinates."));
        }
        converted.append(item);
    }
    QString error;
    if (!model->replaceMissionItems(std::move(converted), MissionPlanModel::DownloadedVehicleMission, &error)) {
        return makeResultError(error);
    }
    return { true, {}, -1 };
}

MissionPlanConverter::Result MissionPlanConverter::exportVehicleMission(const MissionPlanModel &model,
                                                                         QVector<MavlinkMissionItem> *items)
{
    if (!items) return makeResultError(QStringLiteral("Mission output is unavailable."));
    QVector<MavlinkMissionItem> converted;
    converted.reserve(model.rowCount());
    for (int row = 0; row < model.rowCount(); ++row) {
        const auto value = model.missionItemAt(row);
        if (!value) return makeResultError(QStringLiteral("Mission item %1 is unavailable.").arg(row));
        const MissionPlanItem &item = *value;
        if (!finite(item.altitude) || !finite(item.param1) || !finite(item.param2)
                || !finite(item.param3) || !finite(item.param4)
                || !finite(item.latitude) || !finite(item.longitude)) {
            return makeResultError(QStringLiteral("Mission item %1 contains non-finite values.").arg(row));
        }
        if (item.hasCoordinate && (item.latitude < -90.0 || item.latitude > 90.0
                                   || item.longitude < -180.0 || item.longitude > 180.0)) {
            return makeResultError(QStringLiteral("Mission item %1 has invalid geographic coordinates.").arg(row));
        }
        MavlinkMissionItem raw;
        raw.seq = static_cast<quint16>(row);
        raw.current = item.current ? 1 : 0;
        raw.frame = item.frame;
        raw.command = item.command;
        raw.param1 = item.param1; raw.param2 = item.param2; raw.param3 = item.param3; raw.param4 = item.param4;
        raw.x = item.latitude; raw.y = item.longitude; raw.z = item.altitude;
        raw.autocontinue = item.autocontinue ? 1 : 0;
        converted.append(raw);
    }
    *items = std::move(converted);
    return { true, {}, -1 };
}

QVariantList MissionPlanConverter::mapProjection(const MissionPlanModel &model)
{
    QVariantList projection;
    int displaySequence = 1;
    for (int row = 0; row < model.rowCount(); ++row) {
        const auto value = model.missionItemAt(row);
        if (!value || !value->hasCoordinate || !isGlobalFrame(value->frame)
                || isHomeMissionItem(*value)) continue;
        const MissionPlanItem &item = *value;
        if (!finite(item.latitude) || !finite(item.longitude)
                || item.latitude < -90.0 || item.latitude > 90.0
                || item.longitude < -180.0 || item.longitude > 180.0) continue;
        projection.append(QVariantMap {
            { QStringLiteral("id"), item.id.toString(QUuid::WithoutBraces) },
            { QStringLiteral("seq"), item.sequence },
            // Kept separate from the MAVLink sequence so a protocol home
            // item does not make the first operator waypoint appear as #2.
            { QStringLiteral("displaySequence"), displaySequence++ },
            { QStringLiteral("command"), item.command },
            { QStringLiteral("frame"), item.frame },
            { QStringLiteral("lat"), item.latitude },
            { QStringLiteral("lon"), item.longitude },
            { QStringLiteral("alt"), item.altitude },
            { QStringLiteral("selected"), item.id.toString(QUuid::WithoutBraces) == model.selectedItemId() },
        });
    }
    return projection;
}

QVariantMap MissionPlanConverter::importWplFile(const QUrl &url, QObject *object, int source)
{
    auto *model = qobject_cast<MissionPlanModel *>(object);
    if (source < MissionPlanModel::Empty || source > MissionPlanModel::AutomaticPlannerResult) {
        setLastError(QStringLiteral("Invalid mission source state."));
        return {{ QStringLiteral("success"), false }, { QStringLiteral("error"), lastErrorValue }};
    }
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    const Result result = importWplFile(path, model, static_cast<MissionPlanModel::SourceState>(source));
    setLastError(result.error);
    return {{ QStringLiteral("success"), result.success }, { QStringLiteral("error"), result.error },
            { QStringLiteral("line"), result.line }};
}

QString MissionPlanConverter::exportWplFile(const QUrl &url, QObject *object)
{
    auto *model = qobject_cast<MissionPlanModel *>(object);
    if (!model) { setLastError(QStringLiteral("Mission plan model is unavailable.")); return {}; }
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    const Result result = exportWplFile(path, *model);
    setLastError(result.error);
    return result.success ? path : QString();
}

QVariantList MissionPlanConverter::mapProjection(QObject *object) const
{
    const auto *model = qobject_cast<const MissionPlanModel *>(object);
    return model ? mapProjection(*model) : QVariantList {};
}

QString MissionPlanConverter::lastError() const { return lastErrorValue; }

void MissionPlanConverter::setLastError(const QString &error) { lastErrorValue = error; }
