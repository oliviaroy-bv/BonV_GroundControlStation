#include "MissionFileWriter.h"

#include "drone_models.h"
#include "path_planner.h"
#include "storage/AppDataPaths.h"
#include "waypoint_writer.h"

#include <algorithm>
#include <cmath>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

#include <limits>

namespace {
DroneModel droneForName(const QString &name)
{
    const QString normalized = name.trimmed().toLower();
    for (const DroneModel &drone : allDroneModels()) {
        if (drone.displayName.toLower() == normalized || drone.displayName.toLower().replace("-", " ") == normalized) {
            return drone;
        }
    }

    if (normalized == QStringLiteral("air orca")) {
        return { "AIR_ORCA", "Air-Orca", 7500.0, 2, 30, 60, 120, 5, 5.0 };
    }
    if (normalized == QStringLiteral("air prahar max")) {
        return { "AIR_PRAHAR_MAX", "Air-Prahar-Max", 20000.0, 3, 40, 80, 150, 5, 7.0 };
    }
    if (normalized == QStringLiteral("air hans")) {
        return { "AIR_HANS", "Air-Hans", 10000.0, 3, 35, 70, 130, 5, 6.0 };
    }
    if (normalized == QStringLiteral("air hex")) {
        return { "AIR_HEX", "Air-Hex", 8000.0, 2, 30, 60, 120, 5, 5.0 };
    }

    DroneModel fallback = allDroneModels().isEmpty()
            ? DroneModel { "AIR_ORCA", "Air-Orca", 7500.0, 2, 30, 60, 120, 5, 5.0 }
            : allDroneModels().first();
    fallback.displayName = name.trimmed().isEmpty() ? fallback.displayName : name.trimmed().replace(" ", "-");
    return fallback;
}

bool validCoordinate(double value, double limit)
{
    return std::isfinite(value) && std::abs(value) <= limit;
}
}

MissionFileWriter::MissionFileWriter(QObject *parent)
    : QObject(parent)
{
}

QString MissionFileWriter::lastError() const
{
    return lastErrorText;
}

QString MissionFileWriter::saveMission(const QUrl &folderUrl, const QString &fileName, const QString &content)
{
    setLastError(QString());

    QString folderPath = folderUrl.isLocalFile() ? folderUrl.toLocalFile() : folderUrl.toString();
    if (folderPath.trimmed().isEmpty()) {
        folderPath = AppDataPaths::missionsDirectory();
    }

    QDir outputDir(folderPath);
    if (!outputDir.exists() && !outputDir.mkpath(QStringLiteral("."))) {
        setLastError(QStringLiteral("Could not create the selected output folder."));
        return QString();
    }

    QString safeName = fileName.trimmed();
    safeName.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|])")), QStringLiteral("_"));
    if (safeName.isEmpty()) {
        safeName = QStringLiteral("mission.waypoints");
    }
    if (!safeName.endsWith(QStringLiteral(".waypoints"), Qt::CaseInsensitive)) {
        safeName += QStringLiteral(".waypoints");
    }

    const QString outputPath = outputDir.filePath(safeName);
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        setLastError(QStringLiteral("Could not write mission file: %1").arg(file.errorString()));
        return QString();
    }

    QTextStream stream(&file);
    stream << content;
    if (stream.status() != QTextStream::Ok) {
        setLastError(QStringLiteral("Could not write mission content."));
        return QString();
    }

    return QFileInfo(file).absoluteFilePath();
}

QString MissionFileWriter::generateMission(const QUrl &folderUrl,
                                           const QString &droneName,
                                           int missionTypeIndex,
                                           int tripModeIndex,
                                           double takeoffLat,
                                           double takeoffLon,
                                           const QString &takeoffName,
                                           double landingLat,
                                           double landingLon,
                                           const QString &landingName,
                                           double takeoffAlt,
                                           double nearHomeAlt,
                                           double terrainClearance,
                                           double droppingAlt,
                                           double landAlt,
                                           double cruiseSpeed,
                                           double delayTime,
                                           double hoverTime)
{
    setLastError(QString());

    QString folderPath = folderUrl.isLocalFile() ? folderUrl.toLocalFile() : folderUrl.toString();
    if (folderPath.trimmed().isEmpty()) {
        folderPath = AppDataPaths::missionsDirectory();
    }

    QDir outputDir(folderPath);
    if (!outputDir.exists() && !outputDir.mkpath(QStringLiteral("."))) {
        setLastError(QStringLiteral("Could not create the selected output folder."));
        return QString();
    }

    if (!validCoordinate(takeoffLat, 90.0) || !validCoordinate(landingLat, 90.0)) {
        setLastError(QStringLiteral("Latitude must be between -90 and 90."));
        return QString();
    }

    if (!validCoordinate(takeoffLon, 180.0) || !validCoordinate(landingLon, 180.0)) {
        setLastError(QStringLiteral("Longitude must be between -180 and 180."));
        return QString();
    }

    if (takeoffName.trimmed().isEmpty() || landingName.trimmed().isEmpty()) {
        setLastError(QStringLiteral("Enter both takeoff and landing names."));
        return QString();
    }

    if (takeoffAlt < 0 || nearHomeAlt < 0 || terrainClearance < 0 || droppingAlt < 0 || landAlt < 0 || cruiseSpeed <= 0) {
        setLastError(QStringLiteral("Flight parameters must be positive numbers."));
        return QString();
    }

    const MissionType missionType = static_cast<MissionType>(std::clamp(missionTypeIndex, 0, 2));
    const TripMode tripMode = tripModeIndex == 1 ? TripMode::Return : TripMode::OneWay;
    if (missionType != MissionType::Delivery && tripMode != TripMode::Return) {
        setLastError(QStringLiteral("Drop and Paradrop missions require Return trip mode."));
        return QString();
    }

    DroneModel drone = droneForName(droneName);

    MissionParams params;
    params.takeoffLat = takeoffLat;
    params.takeoffLon = takeoffLon;
    params.landLat = landingLat;
    params.landLon = landingLon;
    params.takeoffAlt = takeoffAlt;
    params.nearHomeAlt = nearHomeAlt;
    params.cruiseAlt = terrainClearance;
    params.dropAlt = droppingAlt;
    params.landAlt = landAlt;
    params.cruiseSpeedMs = cruiseSpeed;
    params.waitDelayS = missionType == MissionType::Delivery ? delayTime : hoverTime;
    params.rcTriggerCount = 1;
    params.fromName = takeoffName.trimmed();
    params.toName = landingName.trimmed();

    // The planner and writer use absolute (AMSL) altitudes.  Supplying zero
    // here used to turn every mission into a sea-level mission, which is
    // unsafe and bypasses terrain-obstacle planning entirely.
    const double minLatitude = std::min(takeoffLat, landingLat);
    const double maxLatitude = std::max(takeoffLat, landingLat);
    const double minLongitude = std::min(takeoffLon, landingLon);
    const double maxLongitude = std::max(takeoffLon, landingLon);
    if (!terrainProvider.allTilesAvailableForBounds(minLatitude, minLongitude,
                                                     maxLatitude, maxLongitude)) {
        terrainProvider.prefetchTilesForBounds(minLatitude, minLongitude,
                                               maxLatitude, maxLongitude);
        setLastError(QStringLiteral("Terrain data is not ready for this route. "
                                    "The required SRTM tile is downloading; retry when it is available."));
        return QString();
    }

    PathPlanner planner;
    auto elevationFn = [this](double latitude, double longitude) {
        double terrainHeight = 0.0;
        return terrainProvider.heightMeters(latitude, longitude, terrainHeight)
                ? terrainHeight
                : std::numeric_limits<double>::quiet_NaN();
    };

    QVector<GeoPoint> outboundAnchors = planner.plan(params, elevationFn);
    if (outboundAnchors.size() < 2) {
        setLastError(QStringLiteral("Could not plan outbound path."));
        return QString();
    }
    outboundAnchors = PathPlanner::withVerticalRateLimitedCruiseEnds(outboundAnchors, params.cruiseSpeedMs);

    QVector<GeoPoint> returnAnchors;
    if (tripMode == TripMode::Return) {
        MissionParams returnParams = params;
        std::swap(returnParams.takeoffLat, returnParams.landLat);
        std::swap(returnParams.takeoffLon, returnParams.landLon);

        returnAnchors = planner.plan(returnParams, elevationFn);
        if (returnAnchors.size() < 2) {
            setLastError(QStringLiteral("Could not plan return path."));
            return QString();
        }
        returnAnchors = PathPlanner::withVerticalRateLimitedCruiseEnds(returnAnchors, params.cruiseSpeedMs);
    }

    const double distanceM = PathPlanner::haversineM(params.takeoffLat, params.takeoffLon,
                                                     params.landLat, params.landLon);
    const QString outputPath = WaypointWriter::buildFilename(folderPath, params, drone, missionType, tripMode, distanceM);

    WaypointWriter writer;
    if (!writer.write(outputPath, params, outboundAnchors, returnAnchors, tripMode, missionType, drone)) {
        setLastError(QStringLiteral("Could not write waypoint file."));
        return QString();
    }

    return QFileInfo(outputPath).absoluteFilePath();
}

void MissionFileWriter::setLastError(const QString &error)
{
    if (lastErrorText == error) {
        return;
    }

    lastErrorText = error;
    emit lastErrorChanged();
}
