#include "TileStitcher.h"
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkRequest>
#include <QPainter>
#include <QtMath>
#include <QDebug>

TileStitcher::TileStitcher(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

// ── Tile coordinate math ──────────────────────────────────────────────────────

void TileStitcher::latLonToTile(double lat, double lon, int zoom,
                                  int& tx, int& ty)
{
    double n   = std::pow(2.0, zoom);
    double latR = lat * M_PI / 180.0;
    tx = static_cast<int>((lon + 180.0) / 360.0 * n);
    ty = static_cast<int>((1.0 - std::log(std::tan(latR) + 1.0 / std::cos(latR)) / M_PI) / 2.0 * n);
    tx = qBound(0, tx, static_cast<int>(n) - 1);
    ty = qBound(0, ty, static_cast<int>(n) - 1);
}

double TileStitcher::tileToLon(int tx, int zoom)
{
    return tx / std::pow(2.0, zoom) * 360.0 - 180.0;
}

double TileStitcher::tileToLat(int ty, int zoom)
{
    double n = M_PI - 2.0 * M_PI * ty / std::pow(2.0, zoom);
    return 180.0 / M_PI * std::atan(0.5 * (std::exp(n) - std::exp(-n)));
}

QString TileStitcher::quadKey(int x, int y, int z) const
{
    QString key;
    for (int i = z; i > 0; i--) {
        char d    = '0';
        int  mask = 1 << (i - 1);
        if (x & mask) d++;
        if (y & mask) d += 2;
        key.append(d);
    }
    return key;
}

QString TileStitcher::tilePath(int x, int y, int z) const
{
    static const char* dirs[] = { "bing_sat","bing_map","google_sat","google_map","osm" };
    static const char* exts[] = { "jpg","jpg","jpg","png","png" };
    int src = qBound(0, m_tileSource, 4);
    return QString("%1/%2/%3/%4/%5.%6")
        .arg(m_cacheDir).arg(dirs[src]).arg(z).arg(x).arg(y).arg(exts[src]);
}

QString TileStitcher::tileUrl(int x, int y, int z) const
{
    switch (m_tileSource) {
        case 0: return QString("https://ecn.t3.tiles.virtualearth.net/tiles/a%1.jpeg?g=13869").arg(quadKey(x,y,z));
        case 1: return QString("https://ecn.t3.tiles.virtualearth.net/tiles/r%1.jpeg?g=13869").arg(quadKey(x,y,z));
        case 2: return QString("https://mt1.google.com/vt/lyrs=s&x=%1&y=%2&z=%3").arg(x).arg(y).arg(z);
        case 3: return QString("https://mt1.google.com/vt/lyrs=m&x=%1&y=%2&z=%3").arg(x).arg(y).arg(z);
        default: return QString("https://tile.openstreetmap.org/%1/%2/%3.png").arg(z).arg(x).arg(y);
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void TileStitcher::fetchForBounds(double minLat, double maxLat,
                                   double minLon, double maxLon,
                                   int zoomLevel)
{
    m_zoom   = zoomLevel;
    m_tiles.clear();
    m_pending.clear();

    // Note: tile Y increases southward, so maxLat → smaller tyMin
    int txMin, tyMin_unused, txMax, tyMax_unused;
    latLonToTile(maxLat, minLon, m_zoom, txMin, tyMin_unused);
    latLonToTile(minLat, maxLon, m_zoom, txMax, tyMax_unused);

    int txMinA, txMaxA, tyMinA, tyMaxA;
    latLonToTile(maxLat, minLon, m_zoom, txMinA, tyMinA);   // top-left
    latLonToTile(minLat, maxLon, m_zoom, txMaxA, tyMaxA);   // bottom-right

    m_txMin = qMin(txMinA, txMaxA);
    m_txMax = qMax(txMinA, txMaxA);
    m_tyMin = qMin(tyMinA, tyMaxA);
    m_tyMax = qMax(tyMinA, tyMaxA);

    // Geographic extent of the full tile grid (slightly larger than requested)
    m_texMinLon = tileToLon(m_txMin,     m_zoom);
    m_texMaxLon = tileToLon(m_txMax + 1, m_zoom);
    m_texMaxLat = tileToLat(m_tyMin,     m_zoom);   // top tile edge = higher lat
    m_texMinLat = tileToLat(m_tyMax + 1, m_zoom);   // bottom tile edge = lower lat

    int cols  = m_txMax - m_txMin + 1;
    int rows  = m_tyMax - m_tyMin + 1;
    m_total   = cols * rows;

    qDebug() << "TileStitcher: fetching" << m_total << "tiles at zoom" << m_zoom
             << "(" << cols << "x" << rows << ")";

    emit statusChanged(QString("Loading %1 map tiles...").arg(m_total));

    // Try loading from disk first, queue downloads for missing ones
    for (int tx = m_txMin; tx <= m_txMax; tx++) {
        for (int ty = m_tyMin; ty <= m_tyMax; ty++) {
            quint32 key = (quint32(tx) << 16) | quint32(ty);
            QImage img;
            QString path = tilePath(tx, ty, m_zoom);
            if (QFile::exists(path) && img.load(path)) {
                m_tiles[key] = img;
            } else {
                m_pending.insert(key);
                downloadTile(tx, ty, m_zoom);
            }
        }
    }

    // If everything was cached, stitch immediately
    if (m_pending.isEmpty()) stitch();
    else {
        emit progress(m_tiles.size(), m_total);
    }
}

// ── Download ──────────────────────────────────────────────────────────────────

void TileStitcher::downloadTile(int x, int y, int z)
{
    QNetworkRequest req(tileUrl(x, y, z));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  m_tileSource == 4 ? "FlightPlanner/2.0" : "Mozilla/5.0");
    req.setTransferTimeout(12000);

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [=]() {
        onTileDownloaded(reply, x, y, z);
    });
}

void TileStitcher::onTileDownloaded(QNetworkReply* reply, int x, int y, int z)
{
    reply->deleteLater();
    quint32 key = (quint32(x) << 16) | quint32(y);
    m_pending.remove(key);

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QImage img;
        if (!data.isEmpty() && img.loadFromData(data)) {
            // Save to disk cache
            QString path = tilePath(x, y, z);
            QDir().mkpath(QFileInfo(path).dir().path());
            bool jpeg = path.endsWith(".jpg", Qt::CaseInsensitive);
            img.save(path, jpeg ? "JPEG" : "PNG", jpeg ? 85 : -1);
            m_tiles[key] = img;
        } else {
            m_tiles[key] = placeholderTile();
        }
    } else {
        qWarning() << "TileStitcher: download failed" << x << y << reply->errorString();
        m_tiles[key] = placeholderTile();
    }

    emit progress(m_tiles.size(), m_total);
    emit statusChanged(QString("Loading tiles: %1 / %2").arg(m_tiles.size()).arg(m_total));

    if (m_pending.isEmpty()) stitch();
}

QImage TileStitcher::placeholderTile()
{
    QImage img(TILE_SIZE, TILE_SIZE, QImage::Format_RGB888);
    img.fill(QColor(200, 195, 185));
    return img;
}

// ── Stitch ────────────────────────────────────────────────────────────────────

void TileStitcher::stitch()
{
    int cols = m_txMax - m_txMin + 1;
    int rows = m_tyMax - m_tyMin + 1;
    int W    = cols * TILE_SIZE;
    int H    = rows * TILE_SIZE;

    QImage result(W, H, QImage::Format_RGB888);
    result.fill(QColor(200, 195, 185));

    QPainter p(&result);
    for (int tx = m_txMin; tx <= m_txMax; tx++) {
        for (int ty = m_tyMin; ty <= m_tyMax; ty++) {
            quint32 key = (quint32(tx) << 16) | quint32(ty);
            int px = (tx - m_txMin) * TILE_SIZE;
            int py = (ty - m_tyMin) * TILE_SIZE;
            if (m_tiles.contains(key))
                p.drawImage(px, py, m_tiles[key]);
        }
    }
    p.end();

    qDebug() << "TileStitcher: stitched" << W << "x" << H << "px texture"
             << "covering" << m_texMinLat << m_texMaxLat << m_texMinLon << m_texMaxLon;

    // Save stitched image to app cache dir as JPEG, emit the file path.
    // Qt Quick 3D Texture uses source: "file://..." which requires a file URL.
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                       + "/FlightPlanner";
    QDir().mkpath(cacheDir);
    QString outPath  = cacheDir + "/terrain_texture.jpg";
    result.save(outPath, "JPEG", 90);

    qDebug() << "TileStitcher: saved texture to" << outPath;
    emit statusChanged(QString("Texture ready (%1×%2 px, %3 tiles)")
                           .arg(W).arg(H).arg(cols * rows));
    emit textureReady(outPath, m_texMinLat, m_texMaxLat, m_texMinLon, m_texMaxLon);
}


