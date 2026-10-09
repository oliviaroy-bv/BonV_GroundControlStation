#include "TileCacheManager.h"

#include "storage/AppDataPaths.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSettings>
#include <QSet>
#include <QTimer>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace {
constexpr qint64 kMegabyte = 1024LL * 1024LL;
constexpr int kPrefetchConcurrency = 4;
constexpr int kMaximumPrefetchTiles = 100000;
constexpr double kPi = 3.14159265358979323846;

double clampLatitude(double latitude)
{
    return qBound(-85.05112878, latitude, 85.05112878);
}

double tileX(double longitude, int zoom)
{
    return (longitude + 180.0) / 360.0 * (1 << zoom);
}

double tileY(double latitude, int zoom)
{
    const double radians = qDegreesToRadians(clampLatitude(latitude));
    return (1.0 - std::asinh(std::tan(radians)) / kPi) / 2.0 * (1 << zoom);
}

void collectTileTemplates(const QJsonValue &value, QStringList *templates)
{
    if (value.isObject()) {
        const QJsonObject object = value.toObject();
        const QJsonValue tiles = object.value(QStringLiteral("tiles"));
        if (tiles.isArray()) {
            for (const QJsonValue &tile : tiles.toArray()) {
                if (tile.isString()) {
                    templates->append(tile.toString());
                }
            }
        }
        for (auto it = object.begin(); it != object.end(); ++it) {
            collectTileTemplates(it.value(), templates);
        }
    } else if (value.isArray()) {
        for (const QJsonValue &entry : value.toArray()) {
            collectTileTemplates(entry, templates);
        }
    }
}

void collectTileJsonUrls(const QJsonValue &value, QStringList *urls)
{
    if (!value.isObject()) return;
    const QJsonObject object = value.toObject();
    const QJsonValue url = object.value(QStringLiteral("url"));
    if (url.isString()) urls->append(url.toString());
    for (auto it = object.begin(); it != object.end(); ++it) collectTileJsonUrls(it.value(), urls);
}
}

TileCacheManager::TileCacheManager(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    _offlineMode = settings.value(QStringLiteral("OfflineMaps/offlineMode"), false).toBool();
    _maximumCacheSizeBytes = qMax(64LL * kMegabyte,
        settings.value(QStringLiteral("OfflineMaps/maximumCacheSizeBytes"), _maximumCacheSizeBytes).toLongLong());
    AppDataPaths::ensureDirectory(cacheDirectory());
    refreshCacheSize();
    enforceCacheLimit();
}

bool TileCacheManager::offlineMode() const { return _offlineMode; }
qint64 TileCacheManager::cacheSizeBytes() const { return _cacheSizeBytes; }
qint64 TileCacheManager::maximumCacheSizeBytes() const { return _maximumCacheSizeBytes; }
QString TileCacheManager::cacheDirectory() const { return AppDataPaths::mapTileCacheDirectory(); }
bool TileCacheManager::prefetchActive() const { return _prefetchActive; }
int TileCacheManager::prefetchCompleted() const { return _prefetchCompleted; }
int TileCacheManager::prefetchTotal() const { return _prefetchTotal; }
QString TileCacheManager::prefetchStatus() const { return _prefetchStatus; }

void TileCacheManager::setOfflineMode(bool offline)
{
    if (_offlineMode == offline) {
        return;
    }
    _offlineMode = offline;
    QSettings().setValue(QStringLiteral("OfflineMaps/offlineMode"), offline);
    emit offlineModeChanged();
}

QString TileCacheManager::cacheFilePath(const QUrl &url) const
{
    const QByteArray key = QCryptographicHash::hash(url.toEncoded(), QCryptographicHash::Sha256).toHex();
    return QDir(cacheDirectory()).filePath(QString::fromLatin1(key) + QStringLiteral(".tile"));
}

QString TileCacheManager::mimeFilePath(const QUrl &url) const
{
    return cacheFilePath(url) + QStringLiteral(".mime");
}

bool TileCacheManager::readCached(const QUrl &url, QByteArray *data, QByteArray *mimeType)
{
    QFile file(cacheFilePath(url));
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray contents = file.readAll();
    if (contents.isEmpty()) {
        return false;
    }
    file.setFileTime(QDateTime::currentDateTimeUtc(), QFileDevice::FileModificationTime);

    QFile mimeFile(mimeFilePath(url));
    QByteArray cachedMime = QByteArrayLiteral("application/octet-stream");
    if (mimeFile.open(QIODevice::ReadOnly)) {
        cachedMime = mimeFile.readAll().trimmed();
    }
    if (cachedMime.isEmpty()) {
        cachedMime = QMimeDatabase().mimeTypeForData(contents).name().toLatin1();
    }

    if (data) *data = contents;
    if (mimeType) *mimeType = cachedMime;
    return true;
}

void TileCacheManager::storeCached(const QUrl &url, const QByteArray &data, const QByteArray &mimeType)
{
    if (data.isEmpty() || !AppDataPaths::ensureDirectory(cacheDirectory())) {
        return;
    }

    QSaveFile file(cacheFilePath(url));
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        return;
    }
    QSaveFile mimeFile(mimeFilePath(url));
    if (mimeFile.open(QIODevice::WriteOnly)) {
        mimeFile.write(mimeType.isEmpty() ? QByteArrayLiteral("application/octet-stream") : mimeType);
        mimeFile.commit();
    }
    refreshCacheSize();
    enforceCacheLimit();
}

void TileCacheManager::fetch(const QUrl &url, FetchCallback callback)
{
    QByteArray data;
    QByteArray mimeType;
    if (readCached(url, &data, &mimeType)) {
        QTimer::singleShot(0, this, [callback = std::move(callback), data, mimeType]() mutable {
            callback(data, mimeType, true);
        });
        return;
    }
    if (_offlineMode) {
        callback({}, {}, false);
        return;
    }

    auto *reply = _network.get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, url, callback = std::move(callback)]() mutable {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool success = reply->error() == QNetworkReply::NoError
            && (statusCode == 0 || (statusCode >= 200 && statusCode < 300));
        const QByteArray data = success ? reply->readAll() : QByteArray();
        QByteArray mimeType = reply->header(QNetworkRequest::ContentTypeHeader).toByteArray();
        const int separator = mimeType.indexOf(';');
        if (separator >= 0) mimeType.truncate(separator);
        const QString error = reply->errorString();
        reply->deleteLater();
        // MapTiler uses HTTP 204 for a valid tile with no features. Do not cache
        // it, but let MapLibre receive the empty response instead of reporting a
        // failed resource and leaving the map in an error state.
        const bool emptyTile = success && statusCode == 204;
        if (!success || (data.isEmpty() && !emptyTile)) {
            QUrl logUrl(url);
            logUrl.setQuery(QString());
            qWarning().noquote() << QStringLiteral("Map request failed: %1 (HTTP %2, %3)")
                                      .arg(logUrl.toString(), QString::number(statusCode), error);
            callback({}, {}, false);
            return;
        }
        if (emptyTile) {
            callback({}, mimeType, true);
            return;
        }
        if (mimeType.isEmpty()) mimeType = QMimeDatabase().mimeTypeForData(data).name().toLatin1();
        storeCached(url, data, mimeType);
        callback(data, mimeType, true);
    });
}

void TileCacheManager::refreshCacheSize()
{
    qint64 size = 0;
    QDirIterator it(cacheDirectory(), {QStringLiteral("*.tile")}, QDir::Files);
    while (it.hasNext()) {
        it.next();
        size += it.fileInfo().size();
    }
    if (_cacheSizeBytes != size) {
        _cacheSizeBytes = size;
        emit cacheStatsChanged();
    }
}

void TileCacheManager::enforceCacheLimit()
{
    if (_cacheSizeBytes <= _maximumCacheSizeBytes) return;
    QList<QFileInfo> files;
    QDirIterator it(cacheDirectory(), {QStringLiteral("*.tile")}, QDir::Files);
    while (it.hasNext()) { it.next(); files.append(it.fileInfo()); }
    std::sort(files.begin(), files.end(), [](const QFileInfo &a, const QFileInfo &b) {
        return a.lastModified() < b.lastModified();
    });
    for (const QFileInfo &file : files) {
        if (_cacheSizeBytes <= _maximumCacheSizeBytes) break;
        const qint64 size = file.size();
        QFile::remove(file.filePath());
        QFile::remove(file.filePath() + QStringLiteral(".mime"));
        _cacheSizeBytes -= size;
    }
    emit cacheStatsChanged();
}

void TileCacheManager::clearCache()
{
    QDir directory(cacheDirectory());
    directory.removeRecursively();
    AppDataPaths::ensureDirectory(cacheDirectory());
    _cacheSizeBytes = 0;
    emit cacheStatsChanged();
}

void TileCacheManager::setMaximumCacheSizeMB(int megabytes)
{
    const qint64 bytes = qMax(64LL * kMegabyte, static_cast<qint64>(megabytes) * kMegabyte);
    if (_maximumCacheSizeBytes == bytes) return;
    _maximumCacheSizeBytes = bytes;
    QSettings().setValue(QStringLiteral("OfflineMaps/maximumCacheSizeBytes"), bytes);
    enforceCacheLimit();
    emit cacheStatsChanged();
}

void TileCacheManager::prefetchMissionArea(const QVariantList &waypoints, int minimumZoom,
                                           int maximumZoom, double bufferMeters,
                                           const QString &styleUrl)
{
    if (_offlineMode || waypoints.isEmpty() || !QUrl(styleUrl).isValid()) {
        _prefetchStatus = _offlineMode ? QStringLiteral("Disable offline mode to download maps") : QStringLiteral("A mission area and map style are required");
        emit prefetchChanged();
        return;
    }
    _prefetchActive = true;
    _prefetchCompleted = 0;
    _prefetchFailed = 0;
    _prefetchTotal = 0;
    _prefetchNextIndex = 0;
    _prefetchInFlight = 0;
    _prefetchUrls.clear();
    _prefetchStatus = QStringLiteral("Preparing mission area");
    emit prefetchChanged();
    fetchStyleForPrefetch(QUrl(styleUrl), waypoints, qBound(0, minimumZoom, 22),
                          qBound(qBound(0, minimumZoom, 22), maximumZoom, 22), qMax(0.0, bufferMeters));
}

void TileCacheManager::fetchStyleForPrefetch(const QUrl &styleUrl, const QVariantList &waypoints,
                                              int minimumZoom, int maximumZoom, double bufferMeters)
{
    fetch(styleUrl, [this, styleUrl, waypoints, minimumZoom, maximumZoom, bufferMeters](const QByteArray &data, const QByteArray &, bool ok) {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
        if (!ok || !document.isObject()) {
            _prefetchStatus = QStringLiteral("Could not load the selected map style");
            finishPrefetch();
            return;
        }
        QStringList templates;
        collectTileTemplates(document.object().value(QStringLiteral("sources")), &templates);
        if (!templates.isEmpty()) {
            beginPrefetchTiles(templates, styleUrl, waypoints, minimumZoom, maximumZoom, bufferMeters);
            return;
        }

        QStringList tileJsonUrls;
        collectTileJsonUrls(document.object().value(QStringLiteral("sources")), &tileJsonUrls);
        if (tileJsonUrls.isEmpty()) {
            _prefetchStatus = QStringLiteral("The selected map style has no downloadable tile source");
            finishPrefetch();
            return;
        }

        auto remaining = std::make_shared<int>(tileJsonUrls.size());
        auto tileTemplates = std::make_shared<QStringList>();
        for (const QString &tileJsonUrl : tileJsonUrls) {
            const QUrl resolvedUrl = styleUrl.resolved(QUrl(tileJsonUrl));
            fetch(resolvedUrl, [this, remaining, tileTemplates, resolvedUrl, waypoints, minimumZoom, maximumZoom, bufferMeters](const QByteArray &tileJson, const QByteArray &, bool tileJsonOk) {
                if (tileJsonOk) {
                    const QJsonDocument tileJsonDocument = QJsonDocument::fromJson(tileJson);
                    if (tileJsonDocument.isObject()) collectTileTemplates(tileJsonDocument.object(), tileTemplates.get());
                }
                if (--*remaining == 0) {
                    if (tileTemplates->isEmpty()) {
                        _prefetchStatus = QStringLiteral("The selected map source did not provide tile templates");
                        finishPrefetch();
                    } else {
                        beginPrefetchTiles(*tileTemplates, resolvedUrl, waypoints, minimumZoom, maximumZoom, bufferMeters);
                    }
                }
            });
        }
    });
}

void TileCacheManager::beginPrefetchTiles(const QStringList &templates, const QUrl &baseUrl,
                                          const QVariantList &waypoints, int minimumZoom,
                                          int maximumZoom, double bufferMeters)
{
        double minimumLatitude = 90.0, maximumLatitude = -90.0, minimumLongitude = 180.0, maximumLongitude = -180.0;
        for (const QVariant &entry : waypoints) {
            const QVariantMap point = entry.toMap();
            const double latitude = point.value(QStringLiteral("lat")).toDouble();
            const double longitude = point.value(QStringLiteral("lon")).toDouble();
            if (!qIsFinite(latitude) || !qIsFinite(longitude) || (latitude == 0.0 && longitude == 0.0)) continue;
            minimumLatitude = qMin(minimumLatitude, latitude); maximumLatitude = qMax(maximumLatitude, latitude);
            minimumLongitude = qMin(minimumLongitude, longitude); maximumLongitude = qMax(maximumLongitude, longitude);
        }
        if (minimumLatitude > maximumLatitude) {
            _prefetchStatus = QStringLiteral("Mission has no valid coordinates");
            finishPrefetch();
            return;
        }
        const double latitudeBuffer = bufferMeters / 111320.0;
        const double longitudeBuffer = bufferMeters / qMax(1000.0, 111320.0 * std::cos(qDegreesToRadians((minimumLatitude + maximumLatitude) / 2.0)));
        minimumLatitude = clampLatitude(minimumLatitude - latitudeBuffer);
        maximumLatitude = clampLatitude(maximumLatitude + latitudeBuffer);
        minimumLongitude = qMax(-180.0, minimumLongitude - longitudeBuffer);
        maximumLongitude = qMin(180.0, maximumLongitude + longitudeBuffer);

        QSet<QString> urls;
        for (int zoom = minimumZoom; zoom <= maximumZoom; ++zoom) {
            const int tileCount = 1 << zoom;
            const int left = qBound(0, static_cast<int>(std::floor(tileX(minimumLongitude, zoom))), tileCount - 1);
            const int right = qBound(0, static_cast<int>(std::floor(tileX(maximumLongitude, zoom))), tileCount - 1);
            const int top = qBound(0, static_cast<int>(std::floor(tileY(maximumLatitude, zoom))), tileCount - 1);
            const int bottom = qBound(0, static_cast<int>(std::floor(tileY(minimumLatitude, zoom))), tileCount - 1);
            for (int x = left; x <= right; ++x) for (int y = top; y <= bottom; ++y) for (const QString &templateUrl : templates) {
                const QString expanded = templateUrl.arg(QString::number(zoom), QString::number(x), QString::number(y))
                    .replace(QStringLiteral("{z}"), QString::number(zoom)).replace(QStringLiteral("{x}"), QString::number(x)).replace(QStringLiteral("{y}"), QString::number(y));
                urls.insert(baseUrl.resolved(QUrl(expanded)).toString());
                if (urls.size() > kMaximumPrefetchTiles) {
                    _prefetchStatus = QStringLiteral("Selected region exceeds 100,000 tiles; reduce zoom range or buffer");
                    finishPrefetch();
                    return;
                }
            }
        }
        _prefetchUrls = urls.values();
        _prefetchTotal = _prefetchUrls.size();
        if (_prefetchTotal == 0) {
            _prefetchStatus = QStringLiteral("No tiles are needed for the selected area");
            finishPrefetch();
            return;
        }
        _prefetchStatus = QStringLiteral("Downloading %1 tiles").arg(_prefetchTotal);
        emit prefetchChanged();
        startPrefetchRequests();
}

void TileCacheManager::startPrefetchRequests()
{
    while (_prefetchActive && _prefetchInFlight < kPrefetchConcurrency && _prefetchNextIndex < _prefetchUrls.size()) {
        const QUrl url(_prefetchUrls.at(_prefetchNextIndex++));
        ++_prefetchInFlight;
        fetch(url, [this](const QByteArray &, const QByteArray &, bool ok) {
            --_prefetchInFlight;
            ++_prefetchCompleted;
            if (!ok) ++_prefetchFailed;
            _prefetchStatus = QStringLiteral("Downloading %1 of %2 tiles").arg(_prefetchCompleted).arg(_prefetchTotal);
            emit prefetchChanged();
            if (_prefetchCompleted >= _prefetchTotal) finishPrefetch(); else startPrefetchRequests();
        });
    }
}

void TileCacheManager::finishPrefetch()
{
    _prefetchActive = false;
    _prefetchInFlight = 0;
    if (_prefetchTotal > 0 && _prefetchCompleted >= _prefetchTotal) {
        _prefetchStatus = _prefetchFailed == 0
            ? QStringLiteral("Mission area cached: %1 tiles").arg(_prefetchTotal)
            : QStringLiteral("Mission cache completed with %1 unavailable tiles").arg(_prefetchFailed);
    }
    emit prefetchChanged();
}
