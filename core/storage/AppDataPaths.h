#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

namespace AppDataPaths
{
QString rootDirectory();
QString logsDirectory();
QString missionsDirectory();
QString telemetryLogsDirectory();
QString ardupilotLogsDirectory();
QString videoRecordingsDirectory();
QString screenRecordingsDirectory();
QString terrainDirectory();
QString rasterOverlaysDirectory();
QString mapTileCacheDirectory();
QString directoryUrl(const QString &path);
QStringList allDirectories();
bool ensureDirectory(const QString &path);
bool ensureAllDirectories();
}
