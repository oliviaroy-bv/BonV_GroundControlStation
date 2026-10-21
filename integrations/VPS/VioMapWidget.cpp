#include "VioMapWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QLineF>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QWheelEvent>
#include <cmath>
#include <QtMath>

namespace {
constexpr double EarthRadiusM = 6371000.0;
constexpr double Pi = 3.14159265358979323846;
constexpr int MaxSamples = 2000;
}

VioMapWidget::VioMapWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(380);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_tiles = new QNetworkAccessManager(this);
}

void VioMapWidget::requestTile(int x, int y, int zoom)
{
    const int n = 1 << zoom;
    x = (x % n + n) % n;
    if (y < 0 || y >= n) return;
    const QString key = QString("%1/%2/%3").arg(zoom).arg(x).arg(y);
    if (m_tileCache.contains(key) || m_pendingTiles.contains(key)) return;
    m_pendingTiles.insert(key);
    // Same World_Imagery tile endpoint used by QGroundControl's Esri provider.
    const QUrl url(QString("https://services.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/%1/%2/%3").arg(zoom).arg(y).arg(x));
    QNetworkReply *reply = m_tiles->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, key] {
        m_pendingTiles.remove(key);
        QPixmap tile; tile.loadFromData(reply->readAll());
        if (!tile.isNull()) m_tileCache.insert(key, tile);
        reply->deleteLater(); update();
    });
}

QPointF VioMapWidget::toLocal(double lat, double lon) const
{
    const double north = qDegreesToRadians(lat - m_originLat) * EarthRadiusM;
    const double east = qDegreesToRadians(lon - m_originLon) * EarthRadiusM * qCos(qDegreesToRadians(m_originLat));
    return {east, north};
}

void VioMapWidget::append(QVector<QPointF> &track, const QPointF &point)
{
    track.append(point);
    if (track.size() > MaxSamples) track.remove(0, track.size() - MaxSamples);
}

void VioMapWidget::addSample(double vioLat, double vioLon, double vprLat, double vprLon,
                             double fusedLat, double fusedLon)
{
    if (!m_hasOrigin) {
        m_hasOrigin = true;
        m_originLat = fusedLat;
        m_originLon = fusedLon;
    }
    const QPointF vio = toLocal(vioLat, vioLon);
    if (!m_vio.isEmpty()) m_vioDistance += QLineF(m_vio.last(), vio).length();
    append(m_vio, vio);
    append(m_vpr, toLocal(vprLat, vprLon));
    append(m_fused, toLocal(fusedLat, fusedLon));
    update();
}

void VioMapWidget::clearTracks()
{
    m_hasOrigin = false;
    m_vio.clear(); m_vpr.clear(); m_fused.clear(); m_gps.clear(); m_hasGps = false;
    m_vioDistance = 0;
    update();
}

void VioMapWidget::wheelEvent(QWheelEvent *event)
{
    const int next = m_zoom + (event->angleDelta().y() > 0 ? 1 : -1);
    m_zoom = qBound(2, next, 20);
    event->accept();
    update();
}

void VioMapWidget::addGpsSample(double lat, double lon)
{
    if (!m_hasOrigin) { m_hasOrigin = true; m_originLat = lat; m_originLon = lon; }
    append(m_gps, toLocal(lat, lon)); m_hasGps = true; update();
}

double VioMapWidget::vioFusedErrorMetres() const
{
    return m_vio.isEmpty() || m_fused.isEmpty() ? 0 : QLineF(m_vio.last(), m_fused.last()).length();
}

double VioMapWidget::vioGpsErrorMetres() const
{
    return m_vio.isEmpty() || m_gps.isEmpty() ? 0 : QLineF(m_vio.last(), m_gps.last()).length();
}

void VioMapWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor("#152027"));
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF area = rect().adjusted(18, 18, -18, -18);
    if (!m_hasOrigin || m_vio.isEmpty()) {
        p.setPen(QColor("#b9c6cc"));
        p.drawText(area, Qt::AlignCenter, "Waiting for VPS position data…");
        return;
    }

    // QGC's Esri World Satellite provider uses this XYZ tile scheme. The map
    // follows the latest fused VPS point, while track samples are overlaid.
    constexpr int tileSize = 256;
    const int zoom = m_zoom;
    const QPointF center = m_fused.isEmpty() ? m_vio.last() : m_fused.last();
    const double lat = m_originLat + qRadiansToDegrees(center.y() / EarthRadiusM);
    const double lon = m_originLon + qRadiansToDegrees(center.x() / (EarthRadiusM * qCos(qDegreesToRadians(m_originLat))));
    const double n = double(1 << zoom);
    const double tx = (lon + 180.0) / 360.0 * n;
    const double ty = (1.0 - std::asinh(qTan(qDegreesToRadians(lat))) / Pi) / 2.0 * n;
    const int baseX = qFloor(tx), baseY = qFloor(ty);
    const QPointF tileOrigin(area.center().x() - (tx - baseX) * tileSize, area.center().y() - (ty - baseY) * tileSize);
    for (int iy = -2; iy <= 2; ++iy) for (int ix = -2; ix <= 2; ++ix) {
        const QString key = QString("%1/%2/%3").arg(zoom).arg(baseX + ix).arg(baseY + iy);
        const QRectF target(tileOrigin + QPointF(ix * tileSize, iy * tileSize), QSizeF(tileSize, tileSize));
        if (const auto it = m_tileCache.constFind(key); it != m_tileCache.cend()) p.drawPixmap(target, *it, it->rect());
        else { p.fillRect(target, QColor("#20313a")); requestTile(baseX + ix, baseY + iy, zoom); }
    }
    const double pixelsPerM = (tileSize * n / (2.0 * Pi * EarthRadiusM)) / qCos(qDegreesToRadians(lat));
    auto screen = [&](const QPointF &point) { return area.center() + QPointF((point.x() - center.x()) * pixelsPerM, (center.y() - point.y()) * pixelsPerM); };
    /*
    QRectF bounds;
    for (const auto *track : {&m_vio, &m_vpr, &m_fused, &m_gps})
        for (const QPointF &point : *track) bounds |= QRectF(point, QSizeF(0, 0));
    if (bounds.width() < 20) bounds.adjust(-10, 0, 10, 0);
    if (bounds.height() < 20) bounds.adjust(0, -10, 0, 10);
    bounds.adjust(-bounds.width() * .12, -bounds.height() * .12,
                  bounds.width() * .12, bounds.height() * .12);
    const double scale = qMin(area.width() / bounds.width(), area.height() / bounds.height());
    auto oldscreen = [&](const QPointF &point) {
        return QPointF(area.center().x() + (point.x() - bounds.center().x()) * scale,
                       area.center().y() - (point.y() - bounds.center().y()) * scale);
    }; */

    auto drawTrack = [&](const QVector<QPointF> &track, const QColor &colour) {
        if (track.size() < 2) return;
        QPainterPath path(screen(track.first()));
        for (int i = 1; i < track.size(); ++i) path.lineTo(screen(track[i]));
        p.setPen(QPen(colour, 2.5)); p.drawPath(path);
        p.setBrush(colour); p.drawEllipse(screen(track.last()), 5, 5);
    };
    drawTrack(m_fused, QColor("#ff9d19"));
    drawTrack(m_vpr, QColor("#c067ff"));
    drawTrack(m_vio, QColor("#00d9e8"));
    drawTrack(m_gps, QColor("#4dff6a"));
    p.setPen(Qt::white);
    p.drawText(area.left() + 8, area.top() + 18, "VIO"); p.setPen(QColor("#00d9e8")); p.drawLine(area.left() + 40, area.top() + 14, area.left() + 70, area.top() + 14);
    p.setPen(Qt::white); p.drawText(area.left() + 82, area.top() + 18, "VPR"); p.setPen(QColor("#c067ff")); p.drawLine(area.left() + 120, area.top() + 14, area.left() + 150, area.top() + 14);
    p.setPen(Qt::white); p.drawText(area.left() + 162, area.top() + 18, "Fused VPS"); p.setPen(QColor("#ff9d19")); p.drawLine(area.left() + 235, area.top() + 14, area.left() + 265, area.top() + 14);
    p.setPen(Qt::white); p.drawText(area.left() + 277, area.top() + 18, "FC GPS"); p.setPen(QColor("#4dff6a")); p.drawLine(area.left() + 325, area.top() + 14, area.left() + 355, area.top() + 14);
    p.setPen(QColor("#ffffff"));
    p.drawText(area.left() + 8, area.bottom() - 8, QString("Esri World Imagery · N ↑ · zoom %1 · mouse wheel to zoom").arg(zoom));
}
