#include "terrainmanager.h"

#include "geomath.h"

#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <cmath>
#include <limits>

// ElevationProvider falls back to a recursive QDirIterator scan when a required SRTM
// tile isn't at the expected path. Pointing every manager at an empty temp directory
// keeps that scan cheap instead of walking the real (large) working directory.
namespace {
QUrl emptyDemDirectory(QTemporaryDir &dir)
{
    return QUrl::fromLocalFile(dir.path());
}
} // namespace

class TerrainManagerTests : public QObject
{
    Q_OBJECT

private slots:
    void tileRangeMatchesRequestedZoom();
    void tileCapReducesZoomForLargeRegion();
    void originIsRegionCenterWithElevation();
};

void TerrainManagerTests::tileRangeMatchesRequestedZoom()
{
    QTemporaryDir demDir;
    QVERIFY(demDir.isValid());
    TerrainManager manager;
    manager.setDemDirectory(emptyDemDirectory(demDir));
    manager.setSrtmAutoDownload(false); // keep tests hermetic, no real network calls
    manager.loadRegion(12.9716, 77.5946, 2.0, 14, QString());

    QVERIFY(manager.rowCount() > 0);
    // The manager's tile budget still has plenty of headroom at zoom 14 for a 2 km
    // radius, so the requested zoom is a floor: it should climb finer, not stay put.
    QVERIFY(manager.rowCount() <= 100);

    int minX = std::numeric_limits<int>::max();
    int maxX = std::numeric_limits<int>::min();
    int minY = std::numeric_limits<int>::max();
    int maxY = std::numeric_limits<int>::min();
    int zoom = -1;
    for (int row = 0; row < manager.rowCount(); ++row) {
        const QModelIndex index = manager.index(row);
        zoom = manager.data(index, TerrainManager::ZoomRole).toInt();
        const int x = manager.data(index, TerrainManager::TileXRole).toInt();
        const int y = manager.data(index, TerrainManager::TileYRole).toInt();
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }
    QVERIFY(zoom >= 14);

    // Whatever zoom it settled on should still bracket the center point's own tile.
    const int centerTileX = static_cast<int>(std::floor(GeoMath::longitudeToTileX(77.5946, zoom)));
    const int centerTileY = static_cast<int>(std::floor(GeoMath::latitudeToTileY(12.9716, zoom)));
    QVERIFY(centerTileX >= minX && centerTileX <= maxX);
    QVERIFY(centerTileY >= minY && centerTileY <= maxY);
}

void TerrainManagerTests::tileCapReducesZoomForLargeRegion()
{
    QTemporaryDir demDir;
    QVERIFY(demDir.isValid());
    TerrainManager manager;
    manager.setDemDirectory(emptyDemDirectory(demDir));
    manager.setSrtmAutoDownload(false); // keep tests hermetic, no real network calls
    manager.loadRegion(12.9716, 77.5946, 100.0, 18, QString());

    QVERIFY(manager.rowCount() > 0);
    QVERIFY(manager.rowCount() <= 100);

    const int zoom = manager.data(manager.index(0), TerrainManager::ZoomRole).toInt();
    QVERIFY(zoom < 18);
}

void TerrainManagerTests::originIsRegionCenterWithElevation()
{
    QTemporaryDir demDir;
    QVERIFY(demDir.isValid());
    TerrainManager manager;
    manager.setDemDirectory(emptyDemDirectory(demDir));
    manager.setSrtmAutoDownload(false); // keep tests hermetic, no real network calls
    manager.loadRegion(12.9716, 77.5946, 5.0, 12, QString());

    QVERIFY(std::abs(manager.originLatitude() - 12.9716) < 1.0e-9);
    QVERIFY(std::abs(manager.originLongitude() - 77.5946) < 1.0e-9);
    QCOMPARE(manager.originHeight(),
             manager.elevationAt(manager.originLatitude(), manager.originLongitude()));
}

QTEST_GUILESS_MAIN(TerrainManagerTests)
#include "test_terrainmanager.moc"
