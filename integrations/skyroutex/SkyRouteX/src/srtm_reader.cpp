// #include "include/srtm_reader.h"

// #include <QNetworkAccessManager>
// #include <QNetworkReply>
// #include <QNetworkRequest>
// #include <QFile>
// #include <QDir>
// #include <QStandardPaths>
// #include <QDebug>
// #include <QtEndian>
// #include <cmath>
// #include <limits>
// #include <zlib.h>

// // ── SrtmTile ──────────────────────────────────────────────────────────────────

// double SrtmTile::elevationAt(double lat, double lon) const
// {
//     double fracLat = lat - std::floor(lat);
//     double fracLon = lon - std::floor(lon);

//     double gx = fracLon * (SIZE - 1);
//     double gy = fracLat * (SIZE - 1);

//     int x0 = static_cast<int>(gx);
//     int y0 = static_cast<int>(gy);
//     int x1 = std::min(x0 + 1, SIZE - 1);
//     int y1 = std::min(y0 + 1, SIZE - 1);

//     double fx = gx - x0;
//     double fy = gy - y0;

//     auto idx = [&](int x, int y) { return y * SIZE + x; };
//     auto fix = [](double v) -> double { return (v <= -32000.0) ? 0.0 : v; };

//     double v00 = fix(data[idx(x0, y0)]);
//     double v10 = fix(data[idx(x1, y0)]);
//     double v01 = fix(data[idx(x0, y1)]);
//     double v11 = fix(data[idx(x1, y1)]);

//     return (v00*(1-fx) + v10*fx)*(1-fy)
//          + (v01*(1-fx) + v11*fx)*fy;
// }

// // ── SrtmReader ────────────────────────────────────────────────────────────────

// SrtmReader::SrtmReader(QObject* parent)
//     : QObject(parent)
//     , m_nam(new QNetworkAccessManager(this))
// {
//     QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
//     m_cacheDir = desktop + "/SkyRootX_Terrain/SRTM1";
//     QDir().mkpath(m_cacheDir);
//     qDebug() << "SRTM cache dir:" << m_cacheDir;
// }

// QString SrtmReader::tilePath(int lat, int lon) const
// {
//     QString ns = (lat >= 0) ? "N" : "S";
//     QString ew = (lon >= 0) ? "E" : "W";
//     return QString("%1/%2%3%4%5.hgt")
//         .arg(m_cacheDir)
//         .arg(ns).arg(std::abs(lat), 2, 10, QChar('0'))
//         .arg(ew).arg(std::abs(lon), 3, 10, QChar('0'));
// }

// QString SrtmReader::tileUrl(int lat, int lon) const
// {
//     QString ns = (lat >= 0) ? "N" : "S";
//     QString ew = (lon >= 0) ? "E" : "W";
//     QString name = QString("%1%2%3%4.hgt.zip")
//         .arg(ns).arg(std::abs(lat), 2, 10, QChar('0'))
//         .arg(ew).arg(std::abs(lon), 3, 10, QChar('0'));
//     return QString("https://terrain.ardupilot.org/SRTM1/%1").arg(name);
// }

// bool SrtmReader::loadTile(int lat, int lon)
// {
//     QString path = tilePath(lat, lon);
//     QFile f(path);
//     if (!f.open(QIODevice::ReadOnly)) return false;

//     QByteArray raw = f.readAll();
//     f.close();

//     const int expected = SrtmTile::SIZE * SrtmTile::SIZE * 2;
//     if (raw.size() < expected) {
//         qWarning() << "SRTM tile too small:" << path << raw.size();
//         return false;
//     }

//     auto* tile = new SrtmTile();
//     tile->lat = lat;
//     tile->lon = lon;

//     const int16_t* src = reinterpret_cast<const int16_t*>(raw.constData());
//     int n = SrtmTile::SIZE * SrtmTile::SIZE;
//     for (int i = 0; i < n; ++i)
//         tile->data[i] = qFromBigEndian(src[i]);

//     m_tiles[{lat, lon}] = tile;
//     qDebug() << "SRTM tile loaded from disk:" << path;
//     return true;
// }

// void SrtmReader::downloadTile(int tileLat, int tileLon)
// {
//     auto key = QPair<int,int>{tileLat, tileLon};
//     if (m_downloading.value(key, false)) return;
//     m_downloading[key] = true;

//     QString url = tileUrl(tileLat, tileLon);
//     qDebug() << "Downloading SRTM tile:" << url;

//     QUrl qurl(url);
//     QNetworkRequest req(qurl);
//     req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
//                      static_cast<int>(QNetworkRequest::NoLessSafeRedirectPolicy));
//     QNetworkReply* reply = m_nam->get(req);

//     connect(reply, &QNetworkReply::downloadProgress,
//             this, [this](qint64 rx, qint64 total) {
//         if (total > 0)
//             emit downloadProgress(static_cast<int>(rx * 100 / total));
//     });

//     connect(reply, &QNetworkReply::finished, this, [=]() {
//         onDownloadFinished(reply, tileLat, tileLon);
//     });
// }

// void SrtmReader::onDownloadFinished(QNetworkReply* reply, int tileLat, int tileLon)
// {
//     auto key = QPair<int,int>{tileLat, tileLon};
//     m_downloading[key] = false;
//     reply->deleteLater();

//     if (reply->error() != QNetworkReply::NoError) {
//         emit downloadError(
//             QString("Failed to download SRTM tile N%1E%2: %3")
//                 .arg(tileLat).arg(tileLon).arg(reply->errorString()));
//         return;
//     }

//     QByteArray zipData = reply->readAll();
//     QString hgtPath = tilePath(tileLat, tileLon);

//     if (!extractHgtFromZip(zipData, hgtPath)) {
//         emit downloadError(
//             QString("Failed to unzip SRTM tile N%1E%2").arg(tileLat).arg(tileLon));
//         return;
//     }

//     if (loadTile(tileLat, tileLon)) {
//         emit tileReady(tileLat, tileLon);
//     } else {
//         emit downloadError(
//             QString("Failed to load SRTM tile after download N%1E%2")
//                 .arg(tileLat).arg(tileLon));
//     }
// }

// bool SrtmReader::extractHgtFromZip(const QByteArray& zipData, const QString& outPath)
// {
//     // Parse ZIP local file header (PKZIP format)
//     // Signature: 0x04034b50 (little-endian: 50 4B 03 04)
//     const uint8_t* d = reinterpret_cast<const uint8_t*>(zipData.constData());
//     int len = zipData.size();

//     if (len < 30) return false;
//     if (d[0]!=0x50 || d[1]!=0x4B || d[2]!=0x03 || d[3]!=0x04) return false;

//     uint16_t compression = static_cast<uint16_t>(d[8]  | (d[9]  << 8));
//     uint32_t compSize    = static_cast<uint32_t>(d[18] | (d[19]<<8) | (d[20]<<16) | (d[21]<<24));
//     uint32_t uncompSize  = static_cast<uint32_t>(d[22] | (d[23]<<8) | (d[24]<<16) | (d[25]<<24));
//     uint16_t fnLen       = static_cast<uint16_t>(d[26] | (d[27] << 8));
//     uint16_t extraLen    = static_cast<uint16_t>(d[28] | (d[29] << 8));

//     int dataOffset = 30 + fnLen + extraLen;
//     if (dataOffset + static_cast<int>(compSize) > len) return false;

//     QByteArray raw;

//     if (compression == 0) {
//         // Stored — no compression
//         raw = QByteArray(reinterpret_cast<const char*>(d + dataOffset),
//                          static_cast<int>(uncompSize));
//     } else if (compression == 8) {
//         // Deflate — use Qt's zlib wrapper.
//         // qUncompress() expects a 4-byte big-endian uncompressed length prefix
//         // followed by a zlib stream (with 0x78 0x9C header).
//         // Raw deflate (what ZIP stores) needs the zlib header prepended.
//         QByteArray payload;
//         payload.reserve(static_cast<int>(compSize) + 6);
//         // 4-byte uncompressed size prefix for qUncompress
//         payload.append(static_cast<char>((uncompSize >> 24) & 0xFF));
//         payload.append(static_cast<char>((uncompSize >> 16) & 0xFF));
//         payload.append(static_cast<char>((uncompSize >>  8) & 0xFF));
//         payload.append(static_cast<char>((uncompSize >>  0) & 0xFF));
//         // zlib header (default compression)
//         payload.append(static_cast<char>(0x78));
//         payload.append(static_cast<char>(0x9C));
//         // raw deflate data
//         payload.append(reinterpret_cast<const char*>(d + dataOffset),
//                        static_cast<int>(compSize));

//         raw = qUncompress(payload);
//         if (raw.isEmpty()) {
//             qWarning() << "extractHgtFromZip: qUncompress failed";
//             return false;
//         }
//     } else {
//         qWarning() << "extractHgtFromZip: unsupported compression" << compression;
//         return false;
//     }

//     QFile out(outPath);
//     if (!out.open(QIODevice::WriteOnly)) {
//         qWarning() << "extractHgtFromZip: cannot write" << outPath;
//         return false;
//     }
//     out.write(raw);
//     out.close();
//     qDebug() << "Extracted HGT:" << outPath << "size:" << raw.size();
//     return true;
// }

// // ── Public API ────────────────────────────────────────────────────────────────

// double SrtmReader::elevation(double lat, double lon)
// {
//     int tLat = static_cast<int>(std::floor(lat));
//     int tLon = static_cast<int>(std::floor(lon));
//     auto key = QPair<int,int>{tLat, tLon};

//     if (!m_tiles.contains(key)) {
//         if (!loadTile(tLat, tLon)) {
//             downloadTile(tLat, tLon);
//             return std::numeric_limits<double>::quiet_NaN();
//         }
//     }
//     return m_tiles[key]->elevationAt(lat, lon);
// }

// bool SrtmReader::allTilesCached(double lat1, double lon1,
//                                  double lat2, double lon2)
// {
//     int latMin = static_cast<int>(std::floor(std::min(lat1, lat2)));
//     int latMax = static_cast<int>(std::floor(std::max(lat1, lat2)));
//     int lonMin = static_cast<int>(std::floor(std::min(lon1, lon2)));
//     int lonMax = static_cast<int>(std::floor(std::max(lon1, lon2)));

//     for (int la = latMin; la <= latMax; ++la)
//         for (int lo = lonMin; lo <= lonMax; ++lo)
//             if (!QFile::exists(tilePath(la, lo))) return false;
//     return true;
// }

// void SrtmReader::prefetchTiles(double lat1, double lon1,
//                                 double lat2, double lon2)
// {
//     int latMin = static_cast<int>(std::floor(std::min(lat1, lat2)));
//     int latMax = static_cast<int>(std::floor(std::max(lat1, lat2)));
//     int lonMin = static_cast<int>(std::floor(std::min(lon1, lon2)));
//     int lonMax = static_cast<int>(std::floor(std::max(lon1, lon2)));

//     for (int la = latMin; la <= latMax; ++la) {
//         for (int lo = lonMin; lo <= lonMax; ++lo) {
//             if (QFile::exists(tilePath(la, lo)))
//                 loadTile(la, lo);
//             else
//                 downloadTile(la, lo);
//         }
//     }
// }


#include "include/srtm_reader.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QSslSocket>
#include <QtEndian>
#include <cmath>
#include <limits>
#include <zlib.h>

// ── SrtmTile ──────────────────────────────────────────────────────────────────

double SrtmTile::elevationAt(double lat, double lon) const
{
    double fracLat = lat - std::floor(lat);
    double fracLon = lon - std::floor(lon);

    double gx = fracLon * (SIZE - 1);

    // FIX 1: SRTM .hgt stores rows top-to-bottom (north→south),
    // so row 0 = northern edge. Flip fracLat so that higher latitudes
    // map to lower row indices.
    double gy = (1.0 - fracLat) * (SIZE - 1);

    int x0 = static_cast<int>(gx);
    int y0 = static_cast<int>(gy);
    int x1 = std::min(x0 + 1, SIZE - 1);
    int y1 = std::min(y0 + 1, SIZE - 1);

    double fx = gx - x0;
    double fy = gy - y0;

    auto idx = [&](int x, int y) { return y * SIZE + x; };

    // Treat any value <= -32000 as void/no-data → use 0.0 as fallback
    auto fix = [](double v) -> double { return (v <= -32000.0) ? 0.0 : v; };

    double v00 = fix(data[idx(x0, y0)]);
    double v10 = fix(data[idx(x1, y0)]);
    double v01 = fix(data[idx(x0, y1)]);
    double v11 = fix(data[idx(x1, y1)]);

    return (v00*(1-fx) + v10*fx)*(1-fy)
           + (v01*(1-fx) + v11*fx)*fy;
}

// ── SrtmReader ────────────────────────────────────────────────────────────────

SrtmReader::SrtmReader(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
#ifdef Q_OS_ANDROID
    QString baseDir = "/storage/emulated/0/Documents";
#else
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif
    if (baseDir.isEmpty())
        baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (baseDir.isEmpty())
        baseDir = QDir::homePath();

    m_cacheDir = baseDir + "/SkyRouteX/SRTM";
    if (!QDir().mkpath(m_cacheDir)) {
        QString fallback = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (fallback.isEmpty())
            fallback = QDir::homePath();
        m_cacheDir = fallback + "/SkyRouteX/SRTM";
        QDir().mkpath(m_cacheDir);
    }
    qDebug() << "SRTM cache dir:" << m_cacheDir;
    qDebug() << "SRTM TLS:" << QSslSocket::supportsSsl()
             << "backend:" << QSslSocket::activeBackend()
             << "available:" << QSslSocket::availableBackends()
             << "runtime:" << QSslSocket::sslLibraryVersionString();
}

QString SrtmReader::tilePath(int lat, int lon) const
{
    QString ns = (lat >= 0) ? "N" : "S";
    QString ew = (lon >= 0) ? "E" : "W";
    return QString("%1/%2%3%4%5.hgt")
        .arg(m_cacheDir)
        .arg(ns).arg(std::abs(lat), 2, 10, QChar('0'))
        .arg(ew).arg(std::abs(lon), 3, 10, QChar('0'));
}

QString SrtmReader::tileUrl(int lat, int lon) const
{
    QString ns = (lat >= 0) ? "N" : "S";
    QString ew = (lon >= 0) ? "E" : "W";
    QString name = QString("%1%2%3%4.hgt.zip")
                       .arg(ns).arg(std::abs(lat), 2, 10, QChar('0'))
                       .arg(ew).arg(std::abs(lon), 3, 10, QChar('0'));
    return QString("https://terrain.ardupilot.org/SRTM1/%1").arg(name);
}

bool SrtmReader::loadTile(int lat, int lon)
{
    QString path = tilePath(lat, lon);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;

    QByteArray raw = f.readAll();
    f.close();

    const int expected = SrtmTile::SIZE * SrtmTile::SIZE * 2;
    if (raw.size() < expected) {
        qWarning() << "SRTM tile too small:" << path << raw.size();
        return false;
    }

    auto* tile = new SrtmTile();
    tile->lat = lat;
    tile->lon = lon;

    const int16_t* src = reinterpret_cast<const int16_t*>(raw.constData());
    int n = SrtmTile::SIZE * SrtmTile::SIZE;
    for (int i = 0; i < n; ++i)
        tile->data[i] = qFromBigEndian(src[i]);

    m_tiles[{lat, lon}] = tile;
    qDebug() << "SRTM tile loaded from disk:" << path;
    return true;
}

void SrtmReader::downloadTile(int tileLat, int tileLon)
{
    auto key = QPair<int,int>{tileLat, tileLon};
    if (m_downloading.value(key, false)) return;
    m_downloading[key] = true;

    QString url = tileUrl(tileLat, tileLon);
    qDebug() << "Downloading SRTM tile:" << url;

    QUrl qurl(url);
    QNetworkRequest req(qurl);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     static_cast<int>(QNetworkRequest::NoLessSafeRedirectPolicy));
    QNetworkReply* reply = m_nam->get(req);

    connect(reply, &QNetworkReply::downloadProgress,
            this, [this](qint64 rx, qint64 total) {
                if (total > 0)
                    emit downloadProgress(static_cast<int>(rx * 100 / total));
            });

    connect(reply, &QNetworkReply::finished, this, [=]() {
        onDownloadFinished(reply, tileLat, tileLon);
    });
}

void SrtmReader::onDownloadFinished(QNetworkReply* reply, int tileLat, int tileLon)
{
    auto key = QPair<int,int>{tileLat, tileLon};
    m_downloading[key] = false;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit downloadError(
            QString("Failed to download SRTM tile N%1E%2: %3. TLS supported=%4 backend=%5")
                .arg(tileLat).arg(tileLon).arg(reply->errorString())
                .arg(QSslSocket::supportsSsl() ? "true" : "false")
                .arg(QSslSocket::activeBackend()));
        return;
    }

    QByteArray zipData = reply->readAll();
    QString hgtPath = tilePath(tileLat, tileLon);

    if (!extractHgtFromZip(zipData, hgtPath)) {
        emit downloadError(
            QString("Failed to unzip SRTM tile N%1E%2").arg(tileLat).arg(tileLon));
        return;
    }

    if (loadTile(tileLat, tileLon)) {
        emit tileReady(tileLat, tileLon);
    } else {
        emit downloadError(
            QString("Failed to load SRTM tile after download N%1E%2")
                .arg(tileLat).arg(tileLon));
    }
}

bool SrtmReader::extractHgtFromZip(const QByteArray& zipData, const QString& outPath)
{
    // Parse ZIP local file header (PKZIP format)
    // Signature: 0x04034b50 (little-endian: 50 4B 03 04)
    const uint8_t* d = reinterpret_cast<const uint8_t*>(zipData.constData());
    int len = zipData.size();

    if (len < 30) return false;
    if (d[0]!=0x50 || d[1]!=0x4B || d[2]!=0x03 || d[3]!=0x04) return false;

    uint16_t compression = static_cast<uint16_t>(d[8]  | (d[9]  << 8));
    uint32_t compSize    = static_cast<uint32_t>(d[18] | (d[19]<<8) | (d[20]<<16) | (d[21]<<24));
    uint32_t uncompSize  = static_cast<uint32_t>(d[22] | (d[23]<<8) | (d[24]<<16) | (d[25]<<24));
    uint16_t fnLen       = static_cast<uint16_t>(d[26] | (d[27] << 8));
    uint16_t extraLen    = static_cast<uint16_t>(d[28] | (d[29] << 8));

    int dataOffset = 30 + fnLen + extraLen;
    if (dataOffset + static_cast<int>(compSize) > len) return false;

    QByteArray raw;

    if (compression == 0) {
        // Stored — no compression
        raw = QByteArray(reinterpret_cast<const char*>(d + dataOffset),
                         static_cast<int>(uncompSize));

    } else if (compression == 8) {
        // FIX 2: Use raw inflate (windowBits = -15) instead of the
        // qUncompress() hack. qUncompress() requires a zlib wrapper header
        // (0x78 0x9C / 0x78 0xDA / 0x78 0x01) whose exact byte depends on
        // the compression level used by the ZIP writer — guessing 0x78 0x9C
        // silently produces garbage or fails for many real-world ZIP files.
        // Raw deflate (windowBits < 0) works unconditionally.
        raw.resize(static_cast<int>(uncompSize));

        z_stream zs{};
        zs.next_in   = const_cast<Bytef*>(d + dataOffset);
        zs.avail_in  = static_cast<uInt>(compSize);
        zs.next_out  = reinterpret_cast<Bytef*>(raw.data());
        zs.avail_out = static_cast<uInt>(uncompSize);

        // windowBits = -15 → raw deflate stream, no zlib/gzip wrapper
        if (inflateInit2(&zs, -15) != Z_OK) {
            qWarning() << "extractHgtFromZip: inflateInit2 failed";
            return false;
        }

        int ret = inflate(&zs, Z_FINISH);
        inflateEnd(&zs);

        if (ret != Z_STREAM_END) {
            qWarning() << "extractHgtFromZip: inflate failed, ret=" << ret
                       << "msg:" << (zs.msg ? zs.msg : "null");
            return false;
        }

    } else {
        qWarning() << "extractHgtFromZip: unsupported compression method" << compression;
        return false;
    }

    QFile out(outPath);
    if (!out.open(QIODevice::WriteOnly)) {
        qWarning() << "extractHgtFromZip: cannot write" << outPath;
        return false;
    }
    out.write(raw);
    out.close();
    qDebug() << "Extracted HGT:" << outPath << "size:" << raw.size();
    return true;
}

// ── Public API ────────────────────────────────────────────────────────────────

double SrtmReader::elevation(double lat, double lon)
{
    int tLat = static_cast<int>(std::floor(lat));
    int tLon = static_cast<int>(std::floor(lon));
    auto key = QPair<int,int>{tLat, tLon};

    // FIX 3: Check m_tiles first to avoid redundant loadTile() calls after
    // a tile has already been loaded (e.g. after tileReady fires and the
    // caller retries). Previously the in-memory cache was only checked via
    // m_tiles.contains() but loadTile() could still be attempted unnecessarily.
    if (m_tiles.contains(key))
        return m_tiles[key]->elevationAt(lat, lon);

    // Try loading from disk
    if (loadTile(tLat, tLon))
        return m_tiles[key]->elevationAt(lat, lon);

    // Tile not on disk — enqueue a download if not already in-flight.
    // Return NaN now; caller should listen to tileReady() and re-query.
    if (!m_downloading.value(key, false))
        downloadTile(tLat, tLon);

    return std::numeric_limits<double>::quiet_NaN();
}

bool SrtmReader::allTilesCached(double lat1, double lon1,
                                double lat2, double lon2)
{
    int latMin = static_cast<int>(std::floor(std::min(lat1, lat2)));
    int latMax = static_cast<int>(std::floor(std::max(lat1, lat2)));
    int lonMin = static_cast<int>(std::floor(std::min(lon1, lon2)));
    int lonMax = static_cast<int>(std::floor(std::max(lon1, lon2)));

    for (int la = latMin; la <= latMax; ++la)
        for (int lo = lonMin; lo <= lonMax; ++lo)
            if (!QFile::exists(tilePath(la, lo))) return false;
    return true;
}

void SrtmReader::prefetchTiles(double lat1, double lon1,
                               double lat2, double lon2)
{
    int latMin = static_cast<int>(std::floor(std::min(lat1, lat2)));
    int latMax = static_cast<int>(std::floor(std::max(lat1, lat2)));
    int lonMin = static_cast<int>(std::floor(std::min(lon1, lon2)));
    int lonMax = static_cast<int>(std::floor(std::max(lon1, lon2)));

    for (int la = latMin; la <= latMax; ++la) {
        for (int lo = lonMin; lo <= lonMax; ++lo) {
            auto key = QPair<int,int>{la, lo};
            if (m_tiles.contains(key)) continue;          // already in memory
            if (QFile::exists(tilePath(la, lo)))
                loadTile(la, lo);
            else
                downloadTile(la, lo);
        }
    }
}
