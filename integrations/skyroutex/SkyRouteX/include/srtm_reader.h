#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QPair>
#include <QByteArray>
#include <QDir>
#include <cstdint>
#include <limits>

class QNetworkAccessManager;
class QNetworkReply;

struct SrtmTile {
    int lat;
    int lon;
    static const int SIZE = 3601;
    int16_t data[SIZE * SIZE];
    double elevationAt(double lat, double lon) const;
};

class SrtmReader : public QObject
{
    Q_OBJECT
public:
    explicit SrtmReader(QObject* parent = nullptr);

    double elevation(double lat, double lon);
    bool   allTilesCached(double lat1, double lon1, double lat2, double lon2);
    void   prefetchTiles(double lat1, double lon1, double lat2, double lon2);

    QString cacheDir() const { return m_cacheDir; }

    // Set a new cache directory at runtime (called from Settings page)
    void setCacheDir(const QString& dir) {
        m_cacheDir = dir;
        QDir().mkpath(dir);
        // Clear in-memory tile cache so tiles reload from new path
        qDeleteAll(m_tiles);
        m_tiles.clear();
        m_downloading.clear();
    }

signals:
    void tileReady(int lat, int lon);
    void downloadProgress(int percent);
    void downloadError(const QString& msg);

private slots:
    void onDownloadFinished(QNetworkReply* reply, int tileLat, int tileLon);

private:
    QString tilePath(int lat, int lon) const;
    QString tileUrl(int lat, int lon)  const;
    bool    loadTile(int lat, int lon);
    void    downloadTile(int lat, int lon);
    bool    extractHgtFromZip(const QByteArray& zipData, const QString& outPath);

    QString m_cacheDir;
    QMap<QPair<int,int>, SrtmTile*> m_tiles;
    QNetworkAccessManager* m_nam;
    QMap<QPair<int,int>, bool> m_downloading;
};
