#include "TerrainProvider.h"

#include "storage/AppDataPaths.h"

#include <QByteArray>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <QtEndian>
#include <QtMath>

#include <limits>
#include <utility>

#ifdef BONV_HAS_ZLIB
#include <zlib.h>
#endif

namespace {

QString appTerrainDirectory()
{
    return AppDataPaths::terrainDirectory();
}

bool isSquareSampleCount(qint64 sampleCount)
{
    if (sampleCount < 4) {
        return false;
    }

    const int side = static_cast<int>(qSqrt(static_cast<double>(sampleCount)));
    return static_cast<qint64>(side) * side == sampleCount;
}

double metersToLatitudeDegrees(double meters)
{
    constexpr double metersPerDegreeLatitude = 111320.0;
    return meters / metersPerDegreeLatitude;
}

double metersToLongitudeDegrees(double meters, double latitude)
{
    constexpr double metersPerDegreeAtEquator = 111320.0;
    const double scale = qMax(0.01, qCos(qDegreesToRadians(latitude)));
    return meters / (metersPerDegreeAtEquator * scale);
}

double distanceMeters(double firstLat, double firstLon, double secondLat, double secondLon)
{
    constexpr double earthRadiusMeters = 6371000.0;
    const double firstLatRadians = qDegreesToRadians(firstLat);
    const double secondLatRadians = qDegreesToRadians(secondLat);
    const double deltaLatRadians = qDegreesToRadians(secondLat - firstLat);
    const double deltaLonRadians = qDegreesToRadians(secondLon - firstLon);

    const double a = qSin(deltaLatRadians / 2.0) * qSin(deltaLatRadians / 2.0)
            + qCos(firstLatRadians) * qCos(secondLatRadians)
            * qSin(deltaLonRadians / 2.0) * qSin(deltaLonRadians / 2.0);
    const double c = 2.0 * qAtan2(qSqrt(a), qSqrt(1.0 - a));
    return earthRadiusMeters * c;
}

double interpolate(double a, double b, double fraction)
{
    return a + ((b - a) * fraction);
}

quint16 readLittleEndianUint16(const uchar *data)
{
    return static_cast<quint16>(data[0] | (data[1] << 8));
}

quint32 readLittleEndianUint32(const uchar *data)
{
    return static_cast<quint32>(data[0])
            | (static_cast<quint32>(data[1]) << 8)
            | (static_cast<quint32>(data[2]) << 16)
            | (static_cast<quint32>(data[3]) << 24);
}

} // namespace

TerrainProvider::TerrainProvider(QObject *parent)
    : QObject(parent)
    , networkManager(new QNetworkAccessManager(this))
{
    const QByteArray envTerrainDir = qgetenv("BONV_TERRAIN_DIR");
    if (!envTerrainDir.isEmpty()) {
        directories.append(QString::fromLocal8Bit(envTerrainDir));
    }

    const QString defaultTerrainDirectory = appTerrainDirectory();
    AppDataPaths::ensureDirectory(defaultTerrainDirectory);
    directories.append(defaultTerrainDirectory);
    directories.append(QDir::current().filePath(QStringLiteral("terrain")));
}

QStringList TerrainProvider::terrainDirectories() const
{
    return directories;
}

void TerrainProvider::setTerrainDirectories(const QStringList &terrainDirectories)
{
    QStringList uniqueDirectories;
    for (const QString &directory : terrainDirectories) {
        const QString trimmedDirectory = directory.trimmed();
        if (!trimmedDirectory.isEmpty() && !uniqueDirectories.contains(trimmedDirectory)) {
            uniqueDirectories.append(trimmedDirectory);
        }
    }

    if (uniqueDirectories.isEmpty()) {
        return;
    }

    directories = uniqueDirectories;
    tileCache.clear();
    missingTileWarnings.clear();
    failedTileDownloads.clear();
}

bool TerrainProvider::heightMeters(double latitude, double longitude, double &heightMeters)
{
    if (!qIsFinite(latitude) || !qIsFinite(longitude)
            || latitude < -90.0 || latitude > 90.0
            || longitude < -180.0 || longitude > 180.0) {
        return false;
    }

    const int baseLatitude = static_cast<int>(qFloor(latitude));
    const int baseLongitude = static_cast<int>(qFloor(longitude));
    const QSharedPointer<HgtTile> tile = loadTile(baseLatitude, baseLongitude);
    if (!tile || !tile->isValid()) {
        requestTileDownload(baseLatitude, baseLongitude);
        return false;
    }

    return tile->heightMeters(latitude, longitude, heightMeters);
}

bool TerrainProvider::allTilesAvailableForBounds(double minLatitude, double minLongitude,
                                                 double maxLatitude, double maxLongitude)
{
    if (!qIsFinite(minLatitude) || !qIsFinite(minLongitude)
            || !qIsFinite(maxLatitude) || !qIsFinite(maxLongitude)) {
        return false;
    }

    const double south = qMax(-90.0, qMin(minLatitude, maxLatitude));
    const double north = qMin(90.0, qMax(minLatitude, maxLatitude));
    const double west = qMax(-180.0, qMin(minLongitude, maxLongitude));
    const double east = qMin(180.0, qMax(minLongitude, maxLongitude));
    const int minTileLatitude = static_cast<int>(qFloor(south));
    const int maxTileLatitude = static_cast<int>(qFloor(qMin(north, 89.999999)));
    const int minTileLongitude = static_cast<int>(qFloor(west));
    const int maxTileLongitude = static_cast<int>(qFloor(qMin(east, 179.999999)));

    for (int latitude = minTileLatitude; latitude <= maxTileLatitude; ++latitude) {
        for (int longitude = minTileLongitude; longitude <= maxTileLongitude; ++longitude) {
            if (!tileFileExists(latitude, longitude)) {
                return false;
            }
        }
    }

    return true;
}

void TerrainProvider::prefetchTilesForBounds(double minLatitude, double minLongitude,
                                             double maxLatitude, double maxLongitude)
{
    requestTilesForBounds(minLatitude, minLongitude, maxLatitude, maxLongitude, true);
}

QVector<TerrainProvider::TerrainSample> TerrainProvider::queryCoordinates(const QVector<Coordinate> &coordinates)
{
    QVector<TerrainSample> samples;
    samples.reserve(coordinates.size());

    for (const Coordinate &coordinate : coordinates) {
        TerrainSample sample;
        sample.latitude = coordinate.latitude;
        sample.longitude = coordinate.longitude;
        sample.hasTerrain = heightMeters(coordinate.latitude,
                                         coordinate.longitude,
                                         sample.terrainHeightMeters);
        samples.append(sample);
    }

    return samples;
}

QVector<TerrainProvider::TerrainSample> TerrainProvider::queryPath(const QVector<Coordinate> &coordinates,
                                                                   double sampleSpacingMeters)
{
    QVector<TerrainSample> samples;
    if (coordinates.isEmpty()) {
        return samples;
    }

    double minLatitude = coordinates.first().latitude;
    double maxLatitude = coordinates.first().latitude;
    double minLongitude = coordinates.first().longitude;
    double maxLongitude = coordinates.first().longitude;
    double totalDistance = 0.0;

    QVector<double> cumulativeDistances;
    cumulativeDistances.reserve(coordinates.size());
    cumulativeDistances.append(0.0);

    for (int index = 0; index < coordinates.size(); ++index) {
        const Coordinate coordinate = coordinates.at(index);
        minLatitude = qMin(minLatitude, coordinate.latitude);
        maxLatitude = qMax(maxLatitude, coordinate.latitude);
        minLongitude = qMin(minLongitude, coordinate.longitude);
        maxLongitude = qMax(maxLongitude, coordinate.longitude);

        if (index == 0) {
            continue;
        }

        const Coordinate previous = coordinates.at(index - 1);
        totalDistance += distanceMeters(previous.latitude, previous.longitude,
                                        coordinate.latitude, coordinate.longitude);
        cumulativeDistances.append(totalDistance);
    }

    requestTilesForBounds(minLatitude, minLongitude, maxLatitude, maxLongitude, false);

    if (coordinates.size() == 1 || qFuzzyIsNull(totalDistance)) {
        TerrainSample sample;
        sample.latitude = coordinates.first().latitude;
        sample.longitude = coordinates.first().longitude;
        sample.distanceMeters = 0.0;
        sample.hasTerrain = heightMeters(sample.latitude, sample.longitude, sample.terrainHeightMeters);
        samples.append(sample);
        return samples;
    }

    const double safeSpacing = qBound(1.0, sampleSpacingMeters, 5000.0);
    const int sampleCount = qBound(2, static_cast<int>(qCeil(totalDistance / safeSpacing)) + 1, 5000);
    samples.reserve(sampleCount);

    int segmentIndex = 0;
    for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
        const double targetDistance = totalDistance * sampleIndex / (sampleCount - 1);
        while (segmentIndex < cumulativeDistances.size() - 2
                && cumulativeDistances.at(segmentIndex + 1) < targetDistance) {
            ++segmentIndex;
        }

        const Coordinate start = coordinates.at(segmentIndex);
        const Coordinate end = coordinates.at(qMin(segmentIndex + 1, coordinates.size() - 1));
        const double segmentStart = cumulativeDistances.at(segmentIndex);
        const double segmentEnd = cumulativeDistances.at(qMin(segmentIndex + 1, cumulativeDistances.size() - 1));
        const double segmentDistance = qMax(0.0, segmentEnd - segmentStart);
        const double fraction = segmentDistance > 0.0
                ? qBound(0.0, (targetDistance - segmentStart) / segmentDistance, 1.0)
                : 0.0;

        TerrainSample sample;
        sample.latitude = interpolate(start.latitude, end.latitude, fraction);
        sample.longitude = interpolate(start.longitude, end.longitude, fraction);
        sample.distanceMeters = targetDistance;
        sample.hasTerrain = heightMeters(sample.latitude, sample.longitude, sample.terrainHeightMeters);
        samples.append(sample);
    }

    return samples;
}

QVector<TerrainProvider::TerrainSample> TerrainProvider::queryArea(double minLatitude, double minLongitude,
                                                                   double maxLatitude, double maxLongitude,
                                                                   double spacingMeters,
                                                                   int maxSamples)
{
    QVector<TerrainSample> samples;
    if (!qIsFinite(minLatitude) || !qIsFinite(minLongitude)
            || !qIsFinite(maxLatitude) || !qIsFinite(maxLongitude)) {
        return samples;
    }

    const double south = qMax(-90.0, qMin(minLatitude, maxLatitude));
    const double north = qMin(90.0, qMax(minLatitude, maxLatitude));
    const double west = qMax(-180.0, qMin(minLongitude, maxLongitude));
    const double east = qMin(180.0, qMax(minLongitude, maxLongitude));
    requestTilesForBounds(south, west, north, east, false);

    const double safeSpacing = qBound(1.0, spacingMeters, 10000.0);
    const double midLatitude = (south + north) / 2.0;
    const double latitudeStep = qMax(0.000001, metersToLatitudeDegrees(safeSpacing));
    const double longitudeStep = qMax(0.000001, metersToLongitudeDegrees(safeSpacing, midLatitude));
    const int rowCount = qMax(1, static_cast<int>(qFloor((north - south) / latitudeStep)) + 1);
    const int columnCount = qMax(1, static_cast<int>(qFloor((east - west) / longitudeStep)) + 1);
    const int sampleLimit = qBound(1, maxSamples, 10000);
    const int stride = qMax(1, static_cast<int>(qCeil(qSqrt(static_cast<double>(rowCount * columnCount) / sampleLimit))));

    samples.reserve(qMin(rowCount * columnCount, sampleLimit));
    for (int row = 0; row < rowCount; ++row) {
        if ((row % stride) != 0 && rowCount * columnCount > sampleLimit) {
            continue;
        }

        const double latitude = qMin(north, south + (row * latitudeStep));
        for (int column = 0; column < columnCount; ++column) {
            if ((column % stride) != 0 && rowCount * columnCount > sampleLimit) {
                continue;
            }

            TerrainSample sample;
            sample.latitude = latitude;
            sample.longitude = qMin(east, west + (column * longitudeStep));
            sample.hasTerrain = heightMeters(sample.latitude, sample.longitude, sample.terrainHeightMeters);
            samples.append(sample);
            if (samples.size() >= sampleLimit) {
                return samples;
            }
        }
    }

    return samples;
}

QVariantList TerrainProvider::queryCoordinates(const QVariantList &coordinates)
{
    QVector<Coordinate> parsedCoordinates;
    parsedCoordinates.reserve(coordinates.size());
    for (const QVariant &value : coordinates) {
        Coordinate coordinate;
        if (coordinateFromVariant(value, coordinate)) {
            parsedCoordinates.append(coordinate);
        }
    }

    QVariantList result;
    const QVector<TerrainSample> samples = queryCoordinates(parsedCoordinates);
    result.reserve(samples.size());
    for (const TerrainSample &sample : samples) {
        result.append(sampleToVariant(sample));
    }
    return result;
}

QVariantList TerrainProvider::queryPath(const QVariantList &coordinates, double sampleSpacingMeters)
{
    QVector<Coordinate> parsedCoordinates;
    parsedCoordinates.reserve(coordinates.size());
    for (const QVariant &value : coordinates) {
        Coordinate coordinate;
        if (coordinateFromVariant(value, coordinate)) {
            parsedCoordinates.append(coordinate);
        }
    }

    QVariantList result;
    const QVector<TerrainSample> samples = queryPath(parsedCoordinates, sampleSpacingMeters);
    result.reserve(samples.size());
    for (const TerrainSample &sample : samples) {
        result.append(sampleToVariant(sample));
    }
    return result;
}

QVariantList TerrainProvider::queryAreaVariant(double minLatitude, double minLongitude,
                                               double maxLatitude, double maxLongitude,
                                               double spacingMeters,
                                               int maxSamples)
{
    QVariantList result;
    const QVector<TerrainSample> samples = queryArea(minLatitude, minLongitude,
                                                     maxLatitude, maxLongitude,
                                                     spacingMeters, maxSamples);
    result.reserve(samples.size());
    for (const TerrainSample &sample : samples) {
        result.append(sampleToVariant(sample));
    }
    return result;
}

QString TerrainProvider::tileName(int baseLatitude, int baseLongitude) const
{
    const QChar latHemisphere = baseLatitude >= 0 ? QLatin1Char('N') : QLatin1Char('S');
    const QChar lonHemisphere = baseLongitude >= 0 ? QLatin1Char('E') : QLatin1Char('W');

    return QStringLiteral("%1%2%3%4.hgt")
            .arg(latHemisphere)
            .arg(qAbs(baseLatitude), 2, 10, QLatin1Char('0'))
            .arg(lonHemisphere)
            .arg(qAbs(baseLongitude), 3, 10, QLatin1Char('0'));
}

QString TerrainProvider::tileUrl(int baseLatitude, int baseLongitude) const
{
    return QStringLiteral("https://terrain.ardupilot.org/SRTM1/%1.zip")
            .arg(tileName(baseLatitude, baseLongitude));
}

QString TerrainProvider::tilePath(int baseLatitude, int baseLongitude) const
{
    return QDir(appTerrainDirectory()).filePath(tileName(baseLatitude, baseLongitude));
}

QString TerrainProvider::tileCacheKey(int baseLatitude, int baseLongitude) const
{
    return QStringLiteral("%1:%2").arg(baseLatitude).arg(baseLongitude);
}

QSharedPointer<TerrainProvider::HgtTile> TerrainProvider::loadTile(int baseLatitude, int baseLongitude)
{
    const QString name = tileName(baseLatitude, baseLongitude);
    const QString cacheKey = tileCacheKey(baseLatitude, baseLongitude);
    if (tileCache.contains(cacheKey)) {
        return tileCache.value(cacheKey);
    }

    for (const QString &directory : directories) {
        const QString path = QDir(directory).filePath(name);
        QFile file(path);
        if (!file.exists()) {
            continue;
        }

        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Terrain tile exists but cannot be opened:" << path << file.errorString();
            continue;
        }

        const QByteArray bytes = file.readAll();
        const qint64 sampleCount = bytes.size() / static_cast<qint64>(sizeof(qint16));
        if ((bytes.size() % 2) != 0 || !isSquareSampleCount(sampleCount)) {
            qWarning() << "Ignoring invalid HGT terrain tile:" << path << "bytes" << bytes.size();
            continue;
        }

        QSharedPointer<HgtTile> tile(new HgtTile);
        tile->baseLatitude = baseLatitude;
        tile->baseLongitude = baseLongitude;
        tile->sampleCount = static_cast<int>(qSqrt(static_cast<double>(sampleCount)));
        tile->samples.reserve(static_cast<int>(sampleCount));

        const uchar *raw = reinterpret_cast<const uchar*>(bytes.constData());
        for (qint64 i = 0; i < sampleCount; ++i) {
            tile->samples.append(qFromBigEndian<qint16>(raw + (i * 2)));
        }

        if (tile->isValid()) {
            tileCache.insert(cacheKey, tile);
            missingTileWarnings.remove(cacheKey);
            qDebug() << "Loaded terrain tile" << QFileInfo(path).fileName()
                     << "samples" << tile->sampleCount << "x" << tile->sampleCount;
            return tile;
        }
    }

    if (!missingTileWarnings.contains(cacheKey)) {
        missingTileWarnings.insert(cacheKey);
        qWarning() << "No terrain tile found for" << name << "searched" << directories;
    }
    return {};
}

void TerrainProvider::requestTileDownload(int baseLatitude, int baseLongitude)
{
    const QString cacheKey = tileCacheKey(baseLatitude, baseLongitude);
    if (downloadingTiles.contains(cacheKey) || failedTileDownloads.contains(cacheKey)) {
        return;
    }

#ifndef BONV_HAS_ZLIB
    failedTileDownloads.insert(cacheKey);
    const QString reason = QStringLiteral("zlib support is not available; cannot extract downloaded SRTM zip tiles.");
    qWarning() << reason;
    emit tileDownloadFailed(baseLatitude, baseLongitude, reason);
    return;
#else
    const QString destinationDirectory = appTerrainDirectory();
    if (!AppDataPaths::ensureDirectory(destinationDirectory)) {
        failedTileDownloads.insert(cacheKey);
        const QString reason = QStringLiteral("Cannot create terrain cache directory: %1").arg(destinationDirectory);
        qWarning() << reason;
        emit tileDownloadFailed(baseLatitude, baseLongitude, reason);
        return;
    }

    downloadingTiles.insert(cacheKey);

    QNetworkRequest request(QUrl(tileUrl(baseLatitude, baseLongitude)));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("BonV-GCS terrain downloader"));

    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, baseLatitude, baseLongitude]() {
        handleTileDownloadFinished(reply, baseLatitude, baseLongitude);
    });
    QTimer::singleShot(10000, reply, [reply]() {
        if (reply->isRunning())
            reply->abort();
    });

    qDebug() << "Downloading missing terrain tile" << tileName(baseLatitude, baseLongitude)
             << "from" << request.url();
#endif
}

void TerrainProvider::handleTileDownloadFinished(QNetworkReply *reply, int baseLatitude, int baseLongitude)
{
    const QString cacheKey = tileCacheKey(baseLatitude, baseLongitude);
    downloadingTiles.remove(cacheKey);
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        // Only a 404 is permanent. Transient failures (timeout, network
        // error) stay retryable; downloadingTiles guards against concurrent
        // duplicate downloads in the meantime.
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404)
            failedTileDownloads.insert(cacheKey);
        const QString reason = reply->errorString();
        qWarning() << "Terrain tile download failed for" << tileName(baseLatitude, baseLongitude)
                   << reason;
        emit tileDownloadFailed(baseLatitude, baseLongitude, reason);
        return;
    }

    const QString outputPath = tilePath(baseLatitude, baseLongitude);
    if (!extractHgtFromZip(reply->readAll(), outputPath)) {
        failedTileDownloads.insert(cacheKey);
        const QString reason = QStringLiteral("Downloaded terrain tile could not be extracted.");
        qWarning() << "Terrain tile extraction failed for" << outputPath;
        emit tileDownloadFailed(baseLatitude, baseLongitude, reason);
        return;
    }

    tileCache.remove(cacheKey);
    const QSharedPointer<HgtTile> tile = loadTile(baseLatitude, baseLongitude);
    if (!tile || !tile->isValid()) {
        failedTileDownloads.insert(cacheKey);
        const QString reason = QStringLiteral("Downloaded terrain tile could not be loaded.");
        qWarning() << reason << outputPath;
        emit tileDownloadFailed(baseLatitude, baseLongitude, reason);
        return;
    }

    failedTileDownloads.remove(cacheKey);
    qDebug() << "Downloaded terrain tile ready:" << outputPath;
    emit tileAvailable(baseLatitude, baseLongitude);
}

bool TerrainProvider::tileFileExists(int baseLatitude, int baseLongitude) const
{
    const QString name = tileName(baseLatitude, baseLongitude);
    for (const QString &directory : directories) {
        if (QFileInfo::exists(QDir(directory).filePath(name))) {
            return true;
        }
    }
    return false;
}

void TerrainProvider::requestTilesForBounds(double minLatitude, double minLongitude,
                                            double maxLatitude, double maxLongitude,
                                            bool loadIfAvailable)
{
    if (!qIsFinite(minLatitude) || !qIsFinite(minLongitude)
            || !qIsFinite(maxLatitude) || !qIsFinite(maxLongitude)) {
        return;
    }

    const double south = qMax(-90.0, qMin(minLatitude, maxLatitude));
    const double north = qMin(90.0, qMax(minLatitude, maxLatitude));
    const double west = qMax(-180.0, qMin(minLongitude, maxLongitude));
    const double east = qMin(180.0, qMax(minLongitude, maxLongitude));
    const int minTileLatitude = static_cast<int>(qFloor(south));
    const int maxTileLatitude = static_cast<int>(qFloor(qMin(north, 89.999999)));
    const int minTileLongitude = static_cast<int>(qFloor(west));
    const int maxTileLongitude = static_cast<int>(qFloor(qMin(east, 179.999999)));

    for (int latitude = minTileLatitude; latitude <= maxTileLatitude; ++latitude) {
        for (int longitude = minTileLongitude; longitude <= maxTileLongitude; ++longitude) {
            if (loadIfAvailable && tileFileExists(latitude, longitude)) {
                loadTile(latitude, longitude);
            } else if (!tileFileExists(latitude, longitude)) {
                requestTileDownload(latitude, longitude);
            }
        }
    }
}

bool TerrainProvider::coordinateFromVariant(const QVariant &value, Coordinate &coordinate)
{
    const QVariantMap map = value.toMap();
    if (map.isEmpty()) {
        return false;
    }

    const QVariant latValue = map.contains(QStringLiteral("lat"))
            ? map.value(QStringLiteral("lat"))
            : map.value(QStringLiteral("latitude"));
    const QVariant lonValue = map.contains(QStringLiteral("lon"))
            ? map.value(QStringLiteral("lon"))
            : map.value(QStringLiteral("longitude"));

    bool latOk = false;
    bool lonOk = false;
    coordinate.latitude = latValue.toDouble(&latOk);
    coordinate.longitude = lonValue.toDouble(&lonOk);
    return latOk && lonOk && qIsFinite(coordinate.latitude) && qIsFinite(coordinate.longitude);
}

QVariantMap TerrainProvider::sampleToVariant(const TerrainSample &sample)
{
    QVariantMap map;
    map[QStringLiteral("lat")] = sample.latitude;
    map[QStringLiteral("lon")] = sample.longitude;
    map[QStringLiteral("distance")] = sample.distanceMeters;
    map[QStringLiteral("terrain")] = sample.terrainHeightMeters;
    map[QStringLiteral("hasTerrain")] = sample.hasTerrain;
    return map;
}

bool TerrainProvider::extractHgtFromZip(const QByteArray &zipData, const QString &outPath) const
{
#ifndef BONV_HAS_ZLIB
    Q_UNUSED(zipData)
    Q_UNUSED(outPath)
    return false;
#else
    const uchar *data = reinterpret_cast<const uchar*>(zipData.constData());
    const int size = zipData.size();
    if (size < 30 || data[0] != 0x50 || data[1] != 0x4b || data[2] != 0x03 || data[3] != 0x04) {
        return false;
    }

    const quint16 compression = readLittleEndianUint16(data + 8);
    const quint32 compressedSize = readLittleEndianUint32(data + 18);
    const quint32 uncompressedSize = readLittleEndianUint32(data + 22);
    const quint16 fileNameLength = readLittleEndianUint16(data + 26);
    const quint16 extraLength = readLittleEndianUint16(data + 28);
    const quint64 dataOffset = 30ULL + fileNameLength + extraLength;
    if (dataOffset > static_cast<quint64>(size)
            || compressedSize > static_cast<quint32>(std::numeric_limits<int>::max())
            || uncompressedSize > static_cast<quint32>(std::numeric_limits<int>::max())
            || dataOffset + compressedSize > static_cast<quint64>(size)) {
        return false;
    }

    QByteArray raw;
    if (compression == 0) {
        raw = QByteArray(reinterpret_cast<const char*>(data + dataOffset),
                         static_cast<int>(uncompressedSize));
    } else if (compression == 8) {
        raw.resize(static_cast<int>(uncompressedSize));

        z_stream stream = {};
        stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data + dataOffset));
        stream.avail_in = static_cast<uInt>(compressedSize);
        stream.next_out = reinterpret_cast<Bytef*>(raw.data());
        stream.avail_out = static_cast<uInt>(uncompressedSize);

        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
            return false;
        }

        const int result = inflate(&stream, Z_FINISH);
        inflateEnd(&stream);
        if (result != Z_STREAM_END) {
            return false;
        }
    } else {
        qWarning() << "Unsupported terrain zip compression method:" << compression;
        return false;
    }

    const qint64 sampleCount = raw.size() / static_cast<qint64>(sizeof(qint16));
    if ((raw.size() % 2) != 0 || !isSquareSampleCount(sampleCount)) {
        qWarning() << "Downloaded terrain tile has invalid HGT payload size:" << raw.size();
        return false;
    }

    QSaveFile output(outPath);
    if (!output.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot write terrain tile:" << outPath << output.errorString();
        return false;
    }

    if (output.write(raw) != raw.size()) {
        qWarning() << "Failed writing terrain tile:" << outPath << output.errorString();
        return false;
    }

    return output.commit();
#endif
}

bool TerrainProvider::HgtTile::isValid() const
{
    return sampleCount >= 2 && samples.size() == sampleCount * sampleCount;
}

bool TerrainProvider::HgtTile::heightMeters(double latitude, double longitude, double &heightMeters) const
{
    if (!isValid()) {
        return false;
    }

    const double x = qBound(0.0, (longitude - baseLongitude) * (sampleCount - 1), static_cast<double>(sampleCount - 1));
    const double y = qBound(0.0, (1.0 - (latitude - baseLatitude)) * (sampleCount - 1), static_cast<double>(sampleCount - 1));

    const int x0 = qBound(0, static_cast<int>(qFloor(x)), sampleCount - 1);
    const int y0 = qBound(0, static_cast<int>(qFloor(y)), sampleCount - 1);
    const int x1 = qBound(0, x0 + 1, sampleCount - 1);
    const int y1 = qBound(0, y0 + 1, sampleCount - 1);
    const double fx = x - x0;
    const double fy = y - y0;

    const auto sample = [this](int row, int column) -> qint16 {
        return samples.at((row * sampleCount) + column);
    };

    const qint16 h00 = sample(y0, x0);
    const qint16 h10 = sample(y0, x1);
    const qint16 h01 = sample(y1, x0);
    const qint16 h11 = sample(y1, x1);
    if (h00 <= -32768 || h10 <= -32768 || h01 <= -32768 || h11 <= -32768) {
        return false;
    }

    const double top = interpolate(h00, h10, fx);
    const double bottom = interpolate(h01, h11, fx);
    heightMeters = interpolate(top, bottom, fy);
    return true;
}
