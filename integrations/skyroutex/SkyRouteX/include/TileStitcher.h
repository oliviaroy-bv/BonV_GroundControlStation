#pragma once

#include <QObject>
#include <QImage>
#include <QString>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>

// TileStitcher
// Fetches satellite map tiles for a geographic bounding box,
// stitches them into a single QImage, and emits textureReady().
//
// Uses the same on-disk tile cache as MapWidget
// (terrain/tiles/bing_sat/zoom/x/y.jpg) so any tile browsed
// in the 2D map is already available here — no re-download needed.
//
// Fully offline after first fetch. Downloads only if tile missing.

class TileStitcher : public QObject
{
    Q_OBJECT

public:
    explicit TileStitcher(QObject* parent = nullptr);

    // Set cache directory (same as MapWidget::m_cacheDir = exe/terrain/tiles)
    void setCacheDir(const QString& dir) { m_cacheDir = dir; }

    // Tile source: 0=Bing Sat, 1=Bing Map, 2=Google Sat, 3=Google Map, 4=OSM
    void setTileSource(int src) { m_tileSource = qBound(0, src, 4); }

    // Start fetching tiles for the given bounding box.
    // zoomLevel 14 gives ~2.4km/tile, good for 10km corridor.
    // Call this after mission generation.
    void fetchForBounds(double minLat, double maxLat,
                        double minLon, double maxLon,
                        int zoomLevel = 14);

    // Geographic extent of the last stitched texture (set after textureReady)
    double texMinLat() const { return m_texMinLat; }
    double texMaxLat() const { return m_texMaxLat; }
    double texMinLon() const { return m_texMinLon; }
    double texMaxLon() const { return m_texMaxLon; }

signals:
    // Emits path to stitched JPEG saved in temp location
    void textureReady(const QString& imagePath,
                      double minLat, double maxLat,
                      double minLon, double maxLon);

    void progress(int fetched, int total);
    void statusChanged(const QString& msg);

private slots:
    void onTileDownloaded(QNetworkReply* reply,
                          int x, int y, int z);

private:
    // Tile coordinate math (same as MapWidget)
    static void latLonToTile(double lat, double lon, int zoom,
                              int& tx, int& ty);
    static double tileToLon(int tx, int zoom);
    static double tileToLat(int ty, int zoom);
    QString tilePath(int x, int y, int z) const;
    QString tileUrl(int x, int y, int z)  const;
    QString quadKey(int x, int y, int z)  const;
    void    downloadTile(int x, int y, int z);
    void    checkAndStitch();
    void    stitch();
    QImage  placeholderTile();

    static constexpr int TILE_SIZE = 256;

    QString               m_cacheDir;
    int                   m_tileSource = 0;

    // Current job
    int                   m_zoom  = 14;
    int                   m_txMin = 0, m_txMax = 0;
    int                   m_tyMin = 0, m_tyMax = 0;

    // Texture geographic extent
    double m_texMinLat = 0, m_texMaxLat = 0;
    double m_texMinLon = 0, m_texMaxLon = 0;

    // Loaded tiles: key = (x<<16)|y
    QMap<quint32, QImage>      m_tiles;
    QSet<quint32>              m_pending;
    int                        m_total = 0;

    QNetworkAccessManager*     m_nam;
};
