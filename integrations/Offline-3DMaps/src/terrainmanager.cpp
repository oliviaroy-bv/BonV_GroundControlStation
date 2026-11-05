#include "terrainmanager.h"

#include "geomath.h"
#include "imagerytileprovider.h"
#include "srtmdownloader.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

// Each render tile is a full Model+geometry, costlier per-unit than an imagery-only
// tile, so this is tighter than ImageryTileProvider's per-request tile budget.
constexpr int kMaxTerrainTiles = 100;

// Upper bound on total imagery sub-tile requests for one loadRegion call (terrain tile
// count times (2^detailLevels)^2). Without this, a huge region combined with a high
// detail level could fire tens of thousands of requests; instead the detail level is
// uniformly reduced for that load, same pattern as the terrain-tile zoom auto-reduction.
constexpr int kMaxImageryRequestsPerLoad = 3000;

int tileCountForRegion(double south, double west, double north, double east, int zoom)
{
    const int tileMax = (1 << zoom) - 1;
    const int minX = std::clamp(static_cast<int>(std::floor(GeoMath::longitudeToTileX(west, zoom))),
                                0, tileMax);
    const int maxX = std::clamp(static_cast<int>(std::floor(
                                    GeoMath::longitudeToTileX(std::nextafter(east, west), zoom))),
                                0, tileMax);
    const int minY = std::clamp(static_cast<int>(std::floor(GeoMath::latitudeToTileY(north, zoom))),
                                0, tileMax);
    const int maxY = std::clamp(static_cast<int>(std::floor(GeoMath::latitudeToTileY(south, zoom))),
                                0, tileMax);
    return std::max(0, maxX - minX + 1) * std::max(0, maxY - minY + 1);
}

int autoZoomForRegion(double south, double west, double north, double east, int requestedZoom,
                      int maxTiles)
{
    int zoom = std::clamp(requestedZoom, 1, 22);
    while (zoom > 1 && tileCountForRegion(south, west, north, east, zoom) > maxTiles)
        --zoom;
    // The requested zoom is a floor, not a ceiling: if the tile budget has headroom,
    // use it, otherwise a low requested zoom leaves tiles far larger than necessary
    // (and no amount of imagery detail compositing on top of a huge tile looks sharp
    // up close).
    while (zoom < 22 && tileCountForRegion(south, west, north, east, zoom + 1) <= maxTiles)
        ++zoom;
    return zoom;
}

} // namespace

TerrainManager::TerrainManager(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TerrainManager::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_tiles.size();
}

QVariant TerrainManager::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tiles.size())
        return {};
    const Tile &tile = m_tiles.at(index.row());
    switch (role) {
    case ZoomRole: return tile.zoom;
    case TileXRole: return tile.tileX;
    case TileYRole: return tile.tileY;
    case ImageryUrlRole: return tile.imageryUrl;
    default: return {};
    }
}

QHash<int, QByteArray> TerrainManager::roleNames() const
{
    return {
        {ZoomRole, "zoom"},
        {TileXRole, "tileX"},
        {TileYRole, "tileY"},
        {ImageryUrlRole, "imageryUrl"},
    };
}

void TerrainManager::setDemDirectory(const QUrl &value)
{
    if (m_demDirectory == value)
        return;
    m_demDirectory = value;
    m_elevationProvider.setDemDirectory(value.isLocalFile() ? value.toLocalFile() : value.toString());
    emit demDirectoryChanged();
}

void TerrainManager::setGridSize(int value)
{
    value = std::clamp(value, 5, 257);
    if (m_gridSize == value)
        return;
    m_gridSize = value;
    emit gridSizeChanged();
}

void TerrainManager::setVerticalScale(double value)
{
    value = std::clamp(value, 0.1, 20.0);
    if (qFuzzyCompare(m_verticalScale, value))
        return;
    m_verticalScale = value;
    emit verticalScaleChanged();
}

void TerrainManager::setSkirtDepth(double value)
{
    value = std::max(value, 0.0);
    if (qFuzzyCompare(m_skirtDepth, value))
        return;
    m_skirtDepth = value;
    emit skirtDepthChanged();
}

void TerrainManager::setImageryDetailLevels(int value)
{
    value = std::clamp(value, 0, 4);
    if (m_imageryDetailLevels == value)
        return;
    m_imageryDetailLevels = value;
    emit imageryDetailLevelsChanged();
}

void TerrainManager::setSrtmAutoDownload(bool value)
{
    if (m_srtmAutoDownload == value)
        return;
    m_srtmAutoDownload = value;
    emit srtmAutoDownloadChanged();
}

void TerrainManager::setImageryProvider(ImageryTileProvider *provider)
{
    if (m_imageryProvider == provider)
        return;
    if (m_imageryProvider)
        m_imageryProvider->disconnect(this);
    m_imageryProvider = provider;
    if (m_imageryProvider) {
        connect(m_imageryProvider, &ImageryTileProvider::tileReady, this,
                [this](int zoom, int x, int y, const QUrl &path) {
            for (int row = 0; row < m_tiles.size(); ++row) {
                if (m_tiles.at(row).zoom != zoom || m_tiles.at(row).tileX != x
                    || m_tiles.at(row).tileY != y) {
                    continue;
                }
                m_tiles[row].imageryUrl = path;
                const QModelIndex changed = index(row);
                emit dataChanged(changed, changed, {ImageryUrlRole});

                ++m_completedTileRequests;
                setProgress(m_totalTileRequests > 0
                                ? static_cast<double>(m_completedTileRequests) / m_totalTileRequests
                                : 1.0);
                if (m_completedTileRequests >= m_totalTileRequests) {
                    setBusy(false);
                    setStatus(QStringLiteral("Region ready"));
                }
                break;
            }
        });
    }
    emit imageryProviderChanged();
}

bool TerrainManager::hasDemCoverage() const
{
    return m_demTileCount > 0 && m_loadedDemTileCount == m_demTileCount;
}

double TerrainManager::elevationAt(double latitude, double longitude) const
{
    return m_elevationProvider.elevationAt(latitude, longitude, true);
}

void TerrainManager::setStatus(const QString &status)
{
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

void TerrainManager::setProgress(double progress)
{
    progress = std::clamp(progress, 0.0, 1.0);
    if (qFuzzyCompare(m_progress, progress))
        return;
    m_progress = progress;
    emit progressChanged();
}

void TerrainManager::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

void TerrainManager::cancel()
{
    if (m_imageryProvider)
        m_imageryProvider->cancelAll();
    setBusy(false);
    setStatus(QStringLiteral("Cancelled"));
}

void TerrainManager::requestImagery(int row)
{
    const Tile &tile = m_tiles.at(row);
    m_imageryProvider->requestTile(tile.zoom, tile.tileX, tile.tileY, m_imageryUrlTemplate,
                                   tile.detailLevel);
}

namespace {

// Cesium-style behavior: imagery gets sharper the closer the camera gets to a tile,
// independent of every other loaded tile, instead of one blanket setting for the
// whole region.
int detailLevelForDistance(double distanceMeters)
{
    // Detail level N mosaics a tile from (2^N)^2 finer sub-tiles, so this is capped at
    // 4 (256 sub-tiles) to keep a single close-range tile refresh from firing an
    // impractical number of requests. Real per-meter clarity at extreme close range
    // needs the terrain mesh itself to subdivide into smaller tiles near the camera,
    // not just a bigger mosaic stretched over one large tile (see autoZoomForRegion,
    // which now also picks the finest mesh zoom the tile budget allows).
    if (distanceMeters < 350.0)
        return 4;
    if (distanceMeters < 800.0)
        return 3;
    if (distanceMeters < 1800.0)
        return 2;
    if (distanceMeters < 4000.0)
        return 1;
    return 0;
}

} // namespace

void TerrainManager::updateCameraPosition(double x, double /*y*/, double z)
{
    if (!m_imageryProvider)
        return;
    for (int row = 0; row < m_tiles.size(); ++row) {
        Tile &tile = m_tiles[row];
        const double dx = tile.centerX - x;
        const double dz = tile.centerZ - z;
        const int target = std::max(detailLevelForDistance(std::sqrt(dx * dx + dz * dz)),
                                    m_baseImageryDetailLevel);
        if (target == tile.detailLevel)
            continue;
        tile.detailLevel = target;
        requestImagery(row);
    }
}

void TerrainManager::loadRegion(double centerLatitude, double centerLongitude, double radiusKm,
                                int zoom, const QString &imageryUrlTemplate)
{
    cancel();
    m_completedTileRequests = 0;
    m_totalTileRequests = 0;

    const double centerLat = std::clamp(centerLatitude, -84.5, 84.5);
    const double centerLon = std::clamp(centerLongitude, -180.0, 180.0);
    const double radiusMeters = std::clamp(radiusKm, 0.1, 1000.0) * 1000.0;
    const double latDelta = radiusMeters / 111320.0;
    const double lonDelta = radiusMeters
        / std::max(1.0, 111320.0 * std::cos(GeoMath::degreesToRadians(centerLat)));
    const double south = std::max(-84.5, centerLat - latDelta);
    const double west = std::max(-180.0, centerLon - lonDelta);
    const double north = std::min(84.5, centerLat + latDelta);
    const double east = std::min(180.0, centerLon + lonDelta);
    if (!(south < north) || !(west < east)) {
        emit errorOccurred(QStringLiteral("Enter valid non-crossing latitude/longitude bounds."));
        return;
    }

    const int resolvedZoom = autoZoomForRegion(south, west, north, east, zoom, kMaxTerrainTiles);

    // Required SRTM tiles must be on disk before any elevationAt() call below: once
    // ElevationProvider fails to find a tile it caches that as a permanent miss for its
    // lifetime, so a tile downloaded only after being queried once would never be picked
    // up (see ElevationProvider::loadTile's m_missingTiles short-circuit).
    const QStringList requiredDemTiles = m_elevationProvider.tileNamesForRegion(south, west,
                                                                                 north, east);
    if (m_srtmAutoDownload) {
        const QString demDirectoryLocal = m_demDirectory.isLocalFile()
            ? m_demDirectory.toLocalFile() : m_demDirectory.toString();
        if (!demDirectoryLocal.isEmpty() && !requiredDemTiles.isEmpty()) {
            setBusy(true);
            setStatus(QStringLiteral("Checking SRTM coverage"));
            for (const QString &tileName : requiredDemTiles) {
                if (tileName.size() > 4)
                    static_cast<void>(SrtmDownloader::ensureTile(demDirectoryLocal, tileName.chopped(4)));
            }
        }
    }

    m_originLatitude = (south + north) * 0.5;
    m_originLongitude = (west + east) * 0.5;
    m_originHeight = elevationAt(m_originLatitude, m_originLongitude);
    m_regionRadiusMeters = radiusMeters;
    emit originChanged();

    QStringList missingDemTiles;
    int loadedDemTiles = 0;
    for (const QString &tileName : requiredDemTiles) {
        const double lat = (tileName.at(0) == QLatin1Char('N') ? 1.0 : -1.0)
            * tileName.mid(1, 2).toDouble() + 0.5;
        const double lon = (tileName.at(3) == QLatin1Char('E') ? 1.0 : -1.0)
            * tileName.mid(4, 3).toDouble() + 0.5;
        if (m_elevationProvider.hasCoverage(lat, lon))
            ++loadedDemTiles;
        else
            missingDemTiles.append(tileName);
    }
    m_demTileCount = requiredDemTiles.size();
    m_loadedDemTileCount = loadedDemTiles;
    m_missingDemTiles = missingDemTiles.join(QStringLiteral(", "));

    double elevationMinimum = std::numeric_limits<double>::max();
    double elevationMaximum = std::numeric_limits<double>::lowest();
    constexpr int sampleSteps = 32;
    for (int row = 0; row <= sampleSteps; ++row) {
        const double latitude = std::lerp(north, south, static_cast<double>(row) / sampleSteps);
        for (int column = 0; column <= sampleSteps; ++column) {
            const double longitude = std::lerp(west, east, static_cast<double>(column) / sampleSteps);
            const double elevation = elevationAt(latitude, longitude);
            elevationMinimum = std::min(elevationMinimum, elevation);
            elevationMaximum = std::max(elevationMaximum, elevation);
        }
    }
    m_elevationMinimum = elevationMinimum == std::numeric_limits<double>::max() ? 0.0 : elevationMinimum;
    m_elevationMaximum = elevationMaximum == std::numeric_limits<double>::lowest() ? 0.0 : elevationMaximum;

    const int tileMax = (1 << resolvedZoom) - 1;
    const int minX = std::clamp(static_cast<int>(std::floor(
                                    GeoMath::longitudeToTileX(west, resolvedZoom))), 0, tileMax);
    const int maxX = std::clamp(static_cast<int>(std::floor(
                                    GeoMath::longitudeToTileX(std::nextafter(east, west), resolvedZoom))),
                                0, tileMax);
    const int minY = std::clamp(static_cast<int>(std::floor(
                                    GeoMath::latitudeToTileY(north, resolvedZoom))), 0, tileMax);
    const int maxY = std::clamp(static_cast<int>(std::floor(
                                    GeoMath::latitudeToTileY(south, resolvedZoom))), 0, tileMax);

    beginResetModel();
    m_tiles.clear();
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            Tile tile;
            tile.zoom = resolvedZoom;
            tile.tileX = x;
            tile.tileY = y;
            const double tileCenterLat = GeoMath::tileYToLatitude(y + 0.5, resolvedZoom);
            const double tileCenterLon = GeoMath::tileXToLongitude(x + 0.5, resolvedZoom);
            const GeoMath::Vec3d enu = GeoMath::latLonHeightToEnu(
                tileCenterLat, tileCenterLon, m_originHeight, m_originLatitude,
                m_originLongitude, m_originHeight);
            tile.centerX = enu.x;
            tile.centerZ = -enu.y;
            m_tiles.append(tile);
        }
    }
    endResetModel();

    m_imageryUrlTemplate = imageryUrlTemplate.trimmed();
    startImageryRequests();

    emit regionLoaded();
}

void TerrainManager::startImageryRequests()
{
    m_completedTileRequests = 0;
    m_totalTileRequests = m_tiles.size();
    int baseDetailLevel = m_imageryDetailLevels;
    while (baseDetailLevel > 0
           && m_totalTileRequests * (1 << baseDetailLevel) * (1 << baseDetailLevel)
                  > kMaxImageryRequestsPerLoad) {
        --baseDetailLevel;
    }
    for (Tile &tile : m_tiles)
        tile.detailLevel = baseDetailLevel;
    m_baseImageryDetailLevel = baseDetailLevel;
    setProgress(0.0);
    if (m_totalTileRequests == 0) {
        setBusy(false);
        setStatus(QStringLiteral("No tiles in region"));
    } else if (!m_imageryProvider) {
        setBusy(false);
        setStatus(QStringLiteral("No imagery provider configured"));
    } else {
        setBusy(true);
        setStatus(QStringLiteral("Loading %1 tile(s)").arg(m_totalTileRequests));
        for (int row = 0; row < m_tiles.size(); ++row)
            requestImagery(row);
    }
}
