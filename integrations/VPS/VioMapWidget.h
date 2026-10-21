#pragma once

#include <QWidget>
#include <QPointF>
#include <QHash>
#include <QPixmap>
#include <QSet>

class QNetworkAccessManager;

// Lightweight, dependency-free geographic track view. Coordinates are drawn in
// a local metre frame so it also works without an internet map-tile service.
class VioMapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit VioMapWidget(QWidget *parent = nullptr);
    void addSample(double vioLat, double vioLon, double vprLat, double vprLon,
                   double fusedLat, double fusedLon);
    void clearTracks();
    void addGpsSample(double lat, double lon);
    double vioFusedErrorMetres() const;
    double vioGpsErrorMetres() const;
    double vioDistanceMetres() const { return m_vioDistance; }

protected:
    void paintEvent(QPaintEvent *) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QPointF toLocal(double lat, double lon) const;
    void append(QVector<QPointF> &track, const QPointF &point);
    void requestTile(int x, int y, int zoom);

    bool m_hasOrigin = false;
    double m_originLat = 0, m_originLon = 0;
    QVector<QPointF> m_vio, m_vpr, m_fused;
    QVector<QPointF> m_gps;
    bool m_hasGps = false;
    double m_vioDistance = 0;
    QNetworkAccessManager *m_tiles;
    QHash<QString, QPixmap> m_tileCache;
    QSet<QString> m_pendingTiles;
    int m_zoom = 17;
};
