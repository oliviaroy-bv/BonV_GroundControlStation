#include <QtTest/QtTest>

#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>

#include "terrain/TerrainProvider.h"

namespace {

bool writeTinyHgtTile(const QString &directoryPath)
{
    QDir directory(directoryPath);
    if (!directory.mkpath(QStringLiteral("."))) {
        return false;
    }

    QFile file(directory.filePath(QStringLiteral("N10E020.hgt")));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    const QVector<qint16> samples = {
        100, 200,
        300, 400,
    };

    for (const qint16 sample : samples) {
        char bytes[2] = {};
        qToBigEndian(sample, bytes);
        if (file.write(bytes, sizeof(bytes)) != sizeof(bytes)) {
            return false;
        }
    }

    return true;
}

} // namespace

class TerrainProviderTest : public QObject
{
    Q_OBJECT

private slots:
    void readsSrtmHgtTilesWithNorthSouthOrientation();
    void samplesPathFromSrtmTerrain();
};

void TerrainProviderTest::readsSrtmHgtTilesWithNorthSouthOrientation()
{
    QTemporaryDir terrainDirectory;
    QVERIFY(terrainDirectory.isValid());
    QVERIFY(writeTinyHgtTile(terrainDirectory.path()));

    TerrainProvider terrainProvider;
    terrainProvider.setTerrainDirectories({ terrainDirectory.path() });
    double height = 0.0;

    QVERIFY(terrainProvider.heightMeters(10.99, 20.01, height));
    QVERIFY(qAbs(height - 103.0) < 0.001);

    QVERIFY(terrainProvider.heightMeters(10.01, 20.99, height));
    QVERIFY(qAbs(height - 397.0) < 0.001);

    QVERIFY(terrainProvider.heightMeters(10.5, 20.5, height));
    QCOMPARE(height, 250.0);
}

void TerrainProviderTest::samplesPathFromSrtmTerrain()
{
    QTemporaryDir terrainDirectory;
    QVERIFY(terrainDirectory.isValid());
    QVERIFY(writeTinyHgtTile(terrainDirectory.path()));

    TerrainProvider terrainProvider;
    terrainProvider.setTerrainDirectories({ terrainDirectory.path() });
    QVector<TerrainProvider::Coordinate> path;
    path.append({10.99, 20.01});
    path.append({10.01, 20.99});

    const QVector<TerrainProvider::TerrainSample> samples = terrainProvider.queryPath(path, 200000.0);
    QVERIFY(samples.size() >= 2);
    QVERIFY(samples.first().hasTerrain);
    QVERIFY(samples.last().hasTerrain);
    QVERIFY(qAbs(samples.first().terrainHeightMeters - 103.0) < 0.001);
    QVERIFY(qAbs(samples.last().terrainHeightMeters - 397.0) < 0.001);
    QCOMPARE(samples.first().distanceMeters, 0.0);
    QVERIFY(samples.last().distanceMeters > 0.0);
}

QTEST_MAIN(TerrainProviderTest)

#include "TerrainProviderTest.moc"
