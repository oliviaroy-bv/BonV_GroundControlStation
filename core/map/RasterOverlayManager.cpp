#include "RasterOverlayManager.h"

#include "RasterOverlayStorage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>

#include <QCoreApplication>
#include <QDebug>
#include <QProcess>

#include <algorithm>
#include <cmath>
#include <memory>


namespace {

constexpr int MaxOverlayDimension = 2048;

QString rasterCacheDirectory()
{
    return RasterOverlayStorage::overlayDirectory();
}


} // namespace

RasterOverlayManager::RasterOverlayManager(QObject *parent)
    : QObject(parent)
    , _settings(QStringLiteral("BonV"), QStringLiteral("BonV GCS"))
{
    _enabled = false;
    _settings.setValue(QStringLiteral("RasterMaps/enabled"), false);
}

bool RasterOverlayManager::enabled() const
{
    return _enabled;
}

void RasterOverlayManager::setEnabled(bool enabled)
{
    if (_enabled == enabled) {
        return;
    }

    _enabled = enabled;
    _settings.setValue(QStringLiteral("RasterMaps/enabled"), _enabled);
    emit enabledChanged();
}

QVariantList RasterOverlayManager::overlays() const
{
    return _overlays;
}

QString RasterOverlayManager::statusMessage() const
{
    return _statusMessage;
}

void RasterOverlayManager::importGeoTiff(const QUrl &fileUrl)
{
    if (!_enabled) {
        setStatusMessage(QStringLiteral("Enable raster maps in settings before importing."));
        return;
    }

    const QString filePath = fileUrl.toLocalFile();
    if (filePath.isEmpty()) {
        setStatusMessage(QStringLiteral("Select a local GeoTIFF file."));
        return;
    }

#ifndef HAVE_GDAL
    Q_UNUSED(filePath)
    setStatusMessage(QStringLiteral("GeoTIFF import requires GDAL. Configure the project with GDAL to enable this feature."));
#else
    QString workerPath = QCoreApplication::applicationDirPath() + QStringLiteral("/raster_import_worker/RasterImportWorker.exe");
    if (!QFileInfo::exists(workerPath)) {
        workerPath = QCoreApplication::applicationDirPath() + QStringLiteral("/RasterImportWorker.exe");
    }
    if (!QFileInfo::exists(workerPath)) {
        setStatusMessage(QStringLiteral("Raster import worker is missing. Rebuild the application."));
        return;
    }

    QProcess worker;
    worker.setProgram(workerPath);
    worker.setWorkingDirectory(QFileInfo(workerPath).absolutePath());
    worker.setArguments({ filePath });
    worker.setProcessChannelMode(QProcess::SeparateChannels);
    worker.start();

    if (!worker.waitForStarted(5000)) {
        setStatusMessage(QStringLiteral("Unable to start raster import worker."));
        return;
    }

    if (!worker.waitForFinished(120000)) {
        worker.kill();
        worker.waitForFinished(3000);
        setStatusMessage(QStringLiteral("GeoTIFF import timed out."));
        return;
    }

    const QByteArray output = worker.readAllStandardOutput();
    const QByteArray diagnostics = worker.readAllStandardError();

    const QString logPath = rasterCacheDirectory() + QStringLiteral("/last-raster-import.log");
    QFile logFile(logPath);
    if (logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        logFile.write("exitCode: ");
        logFile.write(QByteArray::number(worker.exitCode()));
        logFile.write("\nexitStatus: ");
        logFile.write(worker.exitStatus() == QProcess::NormalExit ? "normal" : "crashed");
        logFile.write("\nstdout:\n");
        logFile.write(output);
        logFile.write("\nstderr:\n");
        logFile.write(diagnostics);
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(output, &parseError);
    const QJsonObject importResult = document.object();
    qDebug() << "Returned image URL:"
         << importResult.value(QStringLiteral("image")).toString();
         
    if (worker.exitStatus() != QProcess::NormalExit || worker.exitCode() != 0 || !importResult.value(QStringLiteral("ok")).toBool()) {
        const QString error = importResult.value(QStringLiteral("error")).toString();
        QString detail = !error.isEmpty() ? error : QString::fromLocal8Bit(diagnostics).trimmed();
        if (detail.isEmpty() && parseError.error != QJsonParseError::NoError) {
            detail = QStringLiteral("Worker output was not valid JSON: %1").arg(parseError.errorString());
        }
        if (detail.isEmpty()) {
            detail = QStringLiteral("Worker exit code %1.").arg(worker.exitCode());
        }
        const QString message = QStringLiteral("GeoTIFF import failed: %1 Log: %2").arg(detail, QDir::toNativeSeparators(logPath));
        qWarning().noquote() << message;
        setStatusMessage(message);
        return;
    }

    QVariantMap overlay;
    overlay.insert(QStringLiteral("id"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    overlay.insert(QStringLiteral("name"), importResult.value(QStringLiteral("name")).toString());
    overlay.insert(QStringLiteral("image"), importResult.value(QStringLiteral("image")).toString());
    overlay.insert(QStringLiteral("coordinates"), importResult.value(QStringLiteral("coordinates")).toArray().toVariantList());
    overlay.insert(QStringLiteral("opacity"), 0.85);
    overlay.insert(QStringLiteral("visible"), true);
    overlay.insert(QStringLiteral("width"), importResult.value(QStringLiteral("width")).toInt());
    overlay.insert(QStringLiteral("height"), importResult.value(QStringLiteral("height")).toInt());

    _overlays.append(overlay);
    emit overlaysChanged();
    setStatusMessage(QStringLiteral("Raster map imported: %1").arg(overlay.value(QStringLiteral("name")).toString()));
#endif
}

void RasterOverlayManager::removeOverlay(const QString &id)
{
    const int index = overlayIndex(id);
    if (index < 0) {
        return;
    }

    _overlays.removeAt(index);
    emit overlaysChanged();
}

void RasterOverlayManager::setOverlayVisible(const QString &id, bool visible)
{
    const int index = overlayIndex(id);
    if (index < 0) {
        return;
    }

    QVariantMap overlay = _overlays.at(index).toMap();
    overlay.insert(QStringLiteral("visible"), visible);
    _overlays[index] = overlay;
    emit overlaysChanged();
}

void RasterOverlayManager::setOverlayOpacity(const QString &id, double opacity)
{
    const int index = overlayIndex(id);
    if (index < 0) {
        return;
    }

    const double nextOpacity = std::clamp(opacity, 0.0, 1.0);
    QVariantMap overlay = _overlays.at(index).toMap();
    if (qFuzzyCompare(overlay.value(QStringLiteral("opacity")).toDouble() + 1.0, nextOpacity + 1.0)) {
        return;
    }

    overlay.insert(QStringLiteral("opacity"), nextOpacity);
    _overlays[index] = overlay;
    emit overlayOpacityChanged(id, nextOpacity);
}
void RasterOverlayManager::setStatusMessage(const QString &message)
{
    if (_statusMessage == message) {
        return;
    }

    _statusMessage = message;
    emit statusMessageChanged();
}

int RasterOverlayManager::overlayIndex(const QString &id) const
{
    for (int i = 0; i < _overlays.size(); ++i) {
        if (_overlays.at(i).toMap().value(QStringLiteral("id")).toString() == id) {
            return i;
        }
    }

    return -1;
}

