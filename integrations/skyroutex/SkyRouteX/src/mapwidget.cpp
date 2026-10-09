#include "mapwidget.h"
#include <QMetaObject>

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QNetworkRequest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QStandardPaths>
#include <cmath>

// ── Constructor ───────────────────────────────────────────────────────────────

MapWidget::MapWidget(QQuickItem* parent)
    : QQuickPaintedItem(parent)
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
        baseDir = QCoreApplication::applicationDirPath();

    m_cacheDir = baseDir + "/SkyRouteX/MapTiles";
    if (!QDir().mkpath(m_cacheDir)) {
        QString fallback = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (fallback.isEmpty())
            fallback = QCoreApplication::applicationDirPath();
        m_cacheDir = fallback + "/SkyRouteX/MapTiles";
        QDir().mkpath(m_cacheDir);
    }

    setAcceptedMouseButtons(Qt::AllButtons);
    setFlag(QQuickItem::ItemAcceptsInputMethod, true);
}

// ── Property setters ──────────────────────────────────────────────────────────

void MapWidget::setCenterLat(double v)
{
    m_centerLat = v;
    emit centerChanged();
    update();
}

void MapWidget::setCenterLon(double v)
{
    m_centerLon = v;
    emit centerChanged();
    update();
}

void MapWidget::setZoomLevel(int v)
{
    m_zoom = qBound(3, v, 21);
    m_tileCache.clear();   // clear cache on zoom change — tiles are zoom-specific
    emit zoomChanged();
    update();
}

void MapWidget::setTileSource(int v)
{
    m_tileSource = qBound(0, v, 4);
    m_tileCache.clear();   // clear cache on source change
    emit tileSourceChanged();
    update();
}

// ── Invokables ────────────────────────────────────────────────────────────────

void MapWidget::panTo(double lat, double lon)
{
    m_centerLat = lat;
    m_centerLon = lon;
    emit centerChanged();
    update();
}

void MapWidget::clearRoute()
{
    m_routePoints.clear();
    emit routeChanged();
    update();
}

void MapWidget::setCacheDir(const QString& dir)
{
    m_cacheDir = dir;
    QDir().mkpath(m_cacheDir);
    m_tileCache.clear();
    update();
}

// ── Coordinate math (same as GeoDataLogger MapViewerWidget) ───────────────────

QPoint MapWidget::latLonToPixel(double lat, double lon, int zoom) const
{
    double n      = pow(2.0, zoom);
    double latRad = lat * M_PI / 180.0;
    double x      = (lon + 180.0) / 360.0 * n * TILE_SIZE;
    double y      = (1.0 - log(tan(latRad) + 1.0 / cos(latRad)) / M_PI)
                    / 2.0 * n * TILE_SIZE;
    return QPoint(static_cast<int>(x), static_cast<int>(y));
}

QPair<double, double> MapWidget::pixelToLatLon(int px, int py, int zoom) const
{
    double n   = pow(2.0, zoom) * TILE_SIZE;
    double lon = px / n * 360.0 - 180.0;
    double lat = atan(sinh(M_PI * (1.0 - 2.0 * py / n))) * 180.0 / M_PI;
    return { lat, lon };
}

QString MapWidget::quadKey(int x, int y, int z) const
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

// ── Tile cache (same logic as GeoDataLogger) ──────────────────────────────────

QString MapWidget::getTilePath(int x, int y, int z) const
{
    static const char* dirs[] = {
        "bing_sat", "bing_map", "google_sat", "google_map", "osm"
    };
    static const char* exts[] = { "jpg", "jpg", "jpg", "png", "png" };

    int src = qBound(0, m_tileSource, 4);
    return QString("%1/%2/%3/%4/%5.%6")
        .arg(m_cacheDir)
        .arg(dirs[src])
        .arg(z).arg(x).arg(y)
        .arg(exts[src]);
}

bool MapWidget::loadTileFromDisk(int x, int y, int z, QPixmap& tile)
{
    QString path = getTilePath(x, y, z);
    return QFile::exists(path) && tile.load(path);
}

void MapWidget::saveTileToDisk(int x, int y, int z, const QPixmap& tile)
{
    QString  path = getTilePath(x, y, z);
    QDir().mkpath(QFileInfo(path).dir().path());
    bool jpeg = path.endsWith(".jpg", Qt::CaseInsensitive);
    tile.save(path, jpeg ? "JPEG" : "PNG", jpeg ? 85 : -1);
}

void MapWidget::downloadTile(int x, int y, int z)
{
    int maxTile = (1 << z) - 1;
    if (x < 0 || x > maxTile || y < 0 || y > maxTile) return;

    QString tileId = QString("%1/%2/%3/%4")
                         .arg(m_tileSource).arg(z).arg(x).arg(y);
    if (m_pending.contains(tileId)) return;

    // URL templates — same sources as GeoDataLogger
    QString url;
    switch (m_tileSource) {
        case 0:  // Bing Satellite
            url = QString("https://ecn.t3.tiles.virtualearth.net/tiles/a%1.jpeg?g=13869")
                      .arg(quadKey(x, y, z));
            break;
        case 1:  // Bing Maps (roads)
            url = QString("https://ecn.t3.tiles.virtualearth.net/tiles/r%1.jpeg?g=13869")
                      .arg(quadKey(x, y, z));
            break;
        case 2:  // Google Satellite
            url = QString("https://mt1.google.com/vt/lyrs=s&x=%1&y=%2&z=%3")
                      .arg(x).arg(y).arg(z);
            break;
        case 3:  // Google Maps
            url = QString("https://mt1.google.com/vt/lyrs=m&x=%1&y=%2&z=%3")
                      .arg(x).arg(y).arg(z);
            break;
        case 4:  // OpenStreetMap
        default:
            url = QString("https://tile.openstreetmap.org/%1/%2/%3.png")
                      .arg(z).arg(x).arg(y);
            break;
    }

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  m_tileSource == 4 ? "FlightPlanner/2.0" : "Mozilla/5.0");
    req.setTransferTimeout(10000);

    QNetworkReply* reply = m_nam->get(req);
    m_pending[tileId]    = reply;

    connect(reply, &QNetworkReply::finished, this, [=]() {
        m_pending.remove(tileId);

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QPixmap    tile;
            if (!data.isEmpty() && tile.loadFromData(data)) {
                saveTileToDisk(x, y, z, tile);
                m_tileCache[{x, y, z}] = tile;
                emit tileStatusChanged(
                    QString("%1 tiles cached").arg(m_tileCache.size()));
                update();
            }
        } else {
            emit tileStatusChanged(
                QString("Error: %1").arg(reply->errorString()));
        }
        reply->deleteLater();
    });

    emit tileStatusChanged(
        QString("Downloading... (%1 pending)").arg(m_pending.size()));
}

QPixmap MapWidget::getTile(int x, int y, int z)
{
    TileKey key{ x, y, z };

    // 1. Memory cache
    if (m_tileCache.contains(key))
        return m_tileCache[key];

    // 2. Disk cache
    QPixmap tile;
    if (loadTileFromDisk(x, y, z, tile)) {
        m_tileCache[key] = tile;
        return tile;
    }

    // 3. Download (async) — return placeholder meanwhile
    // Schedule download on main thread — paint() runs on render thread
    // and QNetworkAccessManager must not be touched from there
    QMetaObject::invokeMethod(this, [this, x, y, z]() {
        downloadTile(x, y, z);
    }, Qt::QueuedConnection);

    QPixmap ph(TILE_SIZE, TILE_SIZE);
    ph.fill(QColor(220, 228, 236));
    return ph;
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void MapWidget::drawMarker(QPainter* p, int sx, int sy,
                            const QColor& fill, const QString& label)
{
    // Pin: filled circle + downward triangle
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setBrush(fill);
    p->setPen(QPen(fill.darker(160), 1.5));

    // Circle head
    p->drawEllipse(QPoint(sx, sy - 16), 11, 11);

    // Triangle tip
    QPolygon pin;
    pin << QPoint(sx - 6, sy - 9)
        << QPoint(sx + 6, sy - 9)
        << QPoint(sx,     sy + 1);
    p->drawPolygon(pin);

    // Label inside circle
    p->setPen(Qt::white);
    p->setFont(QFont("Segoe UI", 7, QFont::Bold));
    p->drawText(QRect(sx - 11, sy - 27, 22, 14), Qt::AlignCenter, label);
    p->restore();
}

void MapWidget::paint(QPainter* painter)
{
    painter->setRenderHint(QPainter::Antialiasing);

    int w = static_cast<int>(width());
    int h = static_cast<int>(height());

    // Background (shown while tiles load)
    painter->fillRect(0, 0, w, h, QColor(220, 228, 236));

    QPoint cp = latLonToPixel(m_centerLat, m_centerLon, m_zoom);

    // ── Tiles ─────────────────────────────────────────────────────────────────
    int cx = cp.x() / TILE_SIZE;
    int cy = cp.y() / TILE_SIZE;
    int tw = w / TILE_SIZE + 3;
    int th = h / TILE_SIZE + 3;

    for (int tx = cx - tw / 2; tx <= cx + tw / 2; tx++) {
        for (int ty = cy - th / 2; ty <= cy + th / 2; ty++) {
            if (tx < 0 || ty < 0) continue;
            QPixmap tile = getTile(tx, ty, m_zoom);
            int sx = w / 2 + tx * TILE_SIZE - cp.x();
            int sy = h / 2 + ty * TILE_SIZE - cp.y();
            painter->drawPixmap(sx, sy, tile);
        }
    }

    // ── Route polyline ────────────────────────────────────────────────────────
    if (m_routePoints.size() >= 2) {
        painter->setPen(QPen(QColor(24, 95, 165), 2.5,
                             Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPoint prev;
        bool   first = true;

        for (const QVariant& v : m_routePoints) {
            QVariantMap pt  = v.toMap();
            double      lat = pt["lat"].toDouble();
            double      lon = pt["lon"].toDouble();
            QPoint      wp  = latLonToPixel(lat, lon, m_zoom);
            int         sx  = w / 2 + wp.x() - cp.x();
            int         sy  = h / 2 + wp.y() - cp.y();

            if (!first) {
                painter->drawLine(prev, QPoint(sx, sy));
            }
            prev  = { sx, sy };
            first = false;

            // Waypoint dot
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(24, 95, 165));
            painter->drawEllipse(QPoint(sx, sy), 4, 4);
            painter->setPen(QPen(QColor(24, 95, 165), 2.5,
                                 Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        }
    }

    // ── Markers ───────────────────────────────────────────────────────────────
    if (m_hasTakeoff) {
        QPoint tp = latLonToPixel(m_takeoffLat, m_takeoffLon, m_zoom);
        drawMarker(painter,
                   w / 2 + tp.x() - cp.x(),
                   h / 2 + tp.y() - cp.y(),
                   QColor(55, 138, 221), "T");
    }
    if (m_hasLand) {
        QPoint lp = latLonToPixel(m_landLat, m_landLon, m_zoom);
        drawMarker(painter,
                   w / 2 + lp.x() - cp.x(),
                   h / 2 + lp.y() - cp.y(),
                   QColor(99, 153, 34), "L");
    }

    // ── Crosshair ─────────────────────────────────────────────────────────────
    painter->setPen(QPen(QColor(80, 80, 80, 120), 1));
    painter->drawLine(w / 2 - 12, h / 2, w / 2 + 12, h / 2);
    painter->drawLine(w / 2, h / 2 - 12, w / 2, h / 2 + 12);

    // ── Info strip ────────────────────────────────────────────────────────────
    painter->fillRect(0, h - 22, w, 22, QColor(255, 255, 255, 190));
    painter->setPen(QColor(90, 90, 90));
    painter->setFont(QFont("Segoe UI", 8));
    painter->drawText(8, h - 6,
        QString("Center: %1, %2  |  Zoom: %3")
            .arg(m_centerLat, 0, 'f', 5)
            .arg(m_centerLon, 0, 'f', 5)
            .arg(m_zoom));
}

// ── Mouse events ──────────────────────────────────────────────────────────────

void MapWidget::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragStart = e->pos();
        m_dragging  = false;
    }
}

void MapWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (e->buttons() & Qt::LeftButton) {
        QPoint delta = e->pos() - m_dragStart;
        if (delta.manhattanLength() > 4) {
            m_dragging  = true;
            QPoint cp   = latLonToPixel(m_centerLat, m_centerLon, m_zoom);
            auto [lat, lon] = pixelToLatLon(cp.x() - delta.x(),
                                            cp.y() - delta.y(), m_zoom);
            m_centerLat = lat;
            m_centerLon = lon;
            m_dragStart = e->pos();
            emit centerChanged();
            update();
        }
    }
}

void MapWidget::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && !m_dragging) {
        int    w  = static_cast<int>(width());
        int    h  = static_cast<int>(height());
        QPoint cp = latLonToPixel(m_centerLat, m_centerLon, m_zoom);

        int px = cp.x() + (e->pos().x() - w / 2);
        int py = cp.y() + (e->pos().y() - h / 2);
        auto [lat, lon] = pixelToLatLon(px, py, m_zoom);

        if (m_clickMode == 0) {
            m_hasTakeoff = true;
            m_takeoffLat = lat;
            m_takeoffLon = lon;
            emit markersChanged();
            emit takeoffPlaced(lat, lon);
        } else {
            m_hasLand = true;
            m_landLat = lat;
            m_landLon = lon;
            emit markersChanged();
            emit landPlaced(lat, lon);
        }
        update();
    }
}

void MapWidget::wheelEvent(QWheelEvent* e)
{
    setZoomLevel(m_zoom + (e->angleDelta().y() > 0 ? 1 : -1));
}
