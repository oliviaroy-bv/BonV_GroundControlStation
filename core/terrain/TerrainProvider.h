#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QSharedPointer>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

class QNetworkAccessManager;
class QNetworkReply;

class TerrainProvider : public QObject
{
    Q_OBJECT

public:
    struct Coordinate {
        double latitude = 0.0;
        double longitude = 0.0;
    };

    struct TerrainSample {
        double latitude = 0.0;
        double longitude = 0.0;
        double distanceMeters = 0.0;
        double terrainHeightMeters = 0.0;
        bool hasTerrain = false;
    };

    explicit TerrainProvider(QObject *parent = nullptr);

    QStringList terrainDirectories() const;
    void setTerrainDirectories(const QStringList &terrainDirectories);
    bool heightMeters(double latitude, double longitude, double &heightMeters);
    bool allTilesAvailableForBounds(double minLatitude, double minLongitude,
                                    double maxLatitude, double maxLongitude);
    void prefetchTilesForBounds(double minLatitude, double minLongitude,
                                double maxLatitude, double maxLongitude);
    QVector<TerrainSample> queryCoordinates(const QVector<Coordinate> &coordinates);
    QVector<TerrainSample> queryPath(const QVector<Coordinate> &coordinates,
                                     double sampleSpacingMeters = 60.0);
    QVector<TerrainSample> queryArea(double minLatitude, double minLongitude,
                                     double maxLatitude, double maxLongitude,
                                     double spacingMeters = 90.0,
                                     int maxSamples = 2500);

    QVariantList queryCoordinates(const QVariantList &coordinates);
    QVariantList queryPath(const QVariantList &coordinates, double sampleSpacingMeters = 60.0);
    QVariantList queryAreaVariant(double minLatitude, double minLongitude,
                                  double maxLatitude, double maxLongitude,
                                  double spacingMeters = 90.0,
                                  int maxSamples = 2500);

signals:
    void tileAvailable(int baseLatitude, int baseLongitude);
    void tileDownloadFailed(int baseLatitude, int baseLongitude, const QString &reason);

private:
    struct HgtTile {
        int baseLatitude = 0;
        int baseLongitude = 0;
        int sampleCount = 0;
        QVector<qint16> samples;

        bool isValid() const;
        bool heightMeters(double latitude, double longitude, double &heightMeters) const;
    };

    QString tileName(int baseLatitude, int baseLongitude) const;
    QString tileUrl(int baseLatitude, int baseLongitude) const;
    QString tilePath(int baseLatitude, int baseLongitude) const;
    QString tileCacheKey(int baseLatitude, int baseLongitude) const;
    QSharedPointer<HgtTile> loadTile(int baseLatitude, int baseLongitude);
    void requestTileDownload(int baseLatitude, int baseLongitude);
    void handleTileDownloadFinished(QNetworkReply *reply, int baseLatitude, int baseLongitude);
    bool extractHgtFromZip(const QByteArray &zipData, const QString &outPath) const;
    bool tileFileExists(int baseLatitude, int baseLongitude) const;
    void requestTilesForBounds(double minLatitude, double minLongitude,
                               double maxLatitude, double maxLongitude,
                               bool loadIfAvailable);

    static bool coordinateFromVariant(const QVariant &value, Coordinate &coordinate);
    static QVariantMap sampleToVariant(const TerrainSample &sample);

    QStringList directories;
    QHash<QString, QSharedPointer<HgtTile>> tileCache;
    QSet<QString> missingTileWarnings;
    QSet<QString> downloadingTiles;
    QSet<QString> failedTileDownloads;
    QNetworkAccessManager *networkManager = nullptr;
};
