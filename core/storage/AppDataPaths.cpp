#include "AppDataPaths.h"

#include <QDir>
#include <QStandardPaths>

namespace {

QString documentsRoot()
{
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty()) {
        documents = QDir::homePath();
    }
    if (documents.isEmpty()) {
        documents = QDir::currentPath();
    }

    return documents;
}

QString rootChild(const QString &name)
{
    return QDir(AppDataPaths::rootDirectory()).filePath(name);
}

} // namespace

namespace AppDataPaths
{

QString rootDirectory()
{
    return QDir(documentsRoot()).filePath(QStringLiteral("BonV GCS"));
}

QString logsDirectory()
{
    return rootChild(QStringLiteral("Logs"));
}

QString missionsDirectory()
{
    return rootChild(QStringLiteral("Missions"));
}

QString telemetryLogsDirectory()
{
    return QDir(logsDirectory()).filePath(QStringLiteral("TLogs"));
}

QString ardupilotLogsDirectory()
{
    return QDir(logsDirectory()).filePath(QStringLiteral("ArduPilot Logs"));
}

QString videoRecordingsDirectory()
{
    return rootChild(QStringLiteral("Video Recordings"));
}

QString screenRecordingsDirectory()
{
    return rootChild(QStringLiteral("Screen Recordings"));
}

QString terrainDirectory()
{
    return rootChild(QStringLiteral("Terrain"));
}

QString rasterOverlaysDirectory()
{
    return rootChild(QStringLiteral("Raster Overlays"));
}

QString mapTileCacheDirectory()
{
    return rootChild(QStringLiteral("Map Tile Cache"));
}

QString directoryUrl(const QString &path)
{
    return QUrl::fromLocalFile(path).toString();
}

QStringList allDirectories()
{
    return {
        rootDirectory(),
        logsDirectory(),
        telemetryLogsDirectory(),
        ardupilotLogsDirectory(),
        missionsDirectory(),
        videoRecordingsDirectory(),
        screenRecordingsDirectory(),
        terrainDirectory(),
        rasterOverlaysDirectory(),
        mapTileCacheDirectory(),
    };
}

bool ensureDirectory(const QString &path)
{
    return !path.trimmed().isEmpty() && QDir().mkpath(path);
}

bool ensureAllDirectories()
{
    bool ok = true;
    for (const QString &directory : allDirectories()) {
        ok = ensureDirectory(directory) && ok;
    }
    return ok;
}

} // namespace AppDataPaths
