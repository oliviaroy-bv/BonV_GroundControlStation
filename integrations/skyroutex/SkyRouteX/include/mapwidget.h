// #pragma once

// #include <QQuickPaintedItem>
// #include <QNetworkAccessManager>
// #include <QNetworkReply>
// #include <QPixmap>
// #include <QMap>
// #include <QPoint>
// #include <QVariantList>

// // ── TileKey ───────────────────────────────────────────────────────────────────
// struct TileKey {
//     int x, y, z;
//     bool operator<(const TileKey& o) const {
//         if (z != o.z) return z < o.z;
//         if (x != o.x) return x < o.x;
//         return y < o.y;
//     }
// };

// // ── MapWidget ─────────────────────────────────────────────────────────────────
// // QQuickPaintedItem subclass — registered as QML type "MapWidget".
// // Ported from GeoDataLogger's MapViewerWidget (QWidget → QQuickPaintedItem).
// // Tile fetch/cache logic is identical; only the paint entry point changes.
// //
// // Register in main.cpp:
// //   qmlRegisterType<MapWidget>("FlightPlanner", 1, 0, "MapWidget");

// class MapWidget : public QQuickPaintedItem
// {
//     Q_OBJECT
//     QML_ELEMENT

//     // ── Properties exposed to QML ─────────────────────────────────────────────
//     Q_PROPERTY(double      centerLat   READ centerLat   WRITE setCenterLat   NOTIFY centerChanged)
//     Q_PROPERTY(double      centerLon   READ centerLon   WRITE setCenterLon   NOTIFY centerChanged)
//     Q_PROPERTY(int         zoomLevel   READ zoomLevel   WRITE setZoomLevel   NOTIFY zoomChanged)
//     Q_PROPERTY(int         tileSource  READ tileSource  WRITE setTileSource  NOTIFY tileSourceChanged)

//     // Markers
//     Q_PROPERTY(double      takeoffLat  READ takeoffLat  WRITE setTakeoffLat  NOTIFY markersChanged)
//     Q_PROPERTY(double      takeoffLon  READ takeoffLon  WRITE setTakeoffLon  NOTIFY markersChanged)
//     Q_PROPERTY(double      landLat     READ landLat     WRITE setLandLat     NOTIFY markersChanged)
//     Q_PROPERTY(double      landLon     READ landLon     WRITE setLandLon     NOTIFY markersChanged)
//     Q_PROPERTY(bool        hasTakeoff  READ hasTakeoff  WRITE setHasTakeoff  NOTIFY markersChanged)
//     Q_PROPERTY(bool        hasLand     READ hasLand     WRITE setHasLand     NOTIFY markersChanged)

//     // Route polyline — list of {lat, lon, alt} QVariantMaps
//     Q_PROPERTY(QVariantList routePoints READ routePoints WRITE setRoutePoints NOTIFY routeChanged)

//     // Click mode: 0 = place Takeoff, 1 = place Landing
//     Q_PROPERTY(int         clickMode   READ clickMode   WRITE setClickMode   NOTIFY clickModeChanged)

// public:
//     explicit MapWidget(QQuickItem* parent = nullptr);

//     // ── Getters ───────────────────────────────────────────────────────────────
//     double       centerLat()   const { return m_centerLat; }
//     double       centerLon()   const { return m_centerLon; }
//     int          zoomLevel()   const { return m_zoom; }
//     int          tileSource()  const { return m_tileSource; }
//     double       takeoffLat()  const { return m_takeoffLat; }
//     double       takeoffLon()  const { return m_takeoffLon; }
//     double       landLat()     const { return m_landLat; }
//     double       landLon()     const { return m_landLon; }
//     bool         hasTakeoff()  const { return m_hasTakeoff; }
//     bool         hasLand()     const { return m_hasLand; }
//     int          clickMode()   const { return m_clickMode; }
//     QVariantList routePoints() const { return m_routePoints; }

//     // ── Setters ───────────────────────────────────────────────────────────────
//     void setCenterLat(double v);
//     void setCenterLon(double v);
//     void setZoomLevel(int v);
//     void setTileSource(int v);

//     void setTakeoffLat(double v) { m_takeoffLat = v; emit markersChanged(); update(); }
//     void setTakeoffLon(double v) { m_takeoffLon = v; emit markersChanged(); update(); }
//     void setLandLat(double v)    { m_landLat    = v; emit markersChanged(); update(); }
//     void setLandLon(double v)    { m_landLon    = v; emit markersChanged(); update(); }
//     void setHasTakeoff(bool v)   { m_hasTakeoff = v; emit markersChanged(); update(); }
//     void setHasLand(bool v)      { m_hasLand    = v; emit markersChanged(); update(); }
//     void setClickMode(int v)     { m_clickMode  = v; emit clickModeChanged(); }
//     void setRoutePoints(const QVariantList& pts) { m_routePoints = pts; emit routeChanged(); update(); }

//     // ── Invokable from QML ────────────────────────────────────────────────────
//     Q_INVOKABLE void panTo(double lat, double lon);
//     Q_INVOKABLE void clearRoute();
//     Q_INVOKABLE void setCacheDir(const QString& dir);

//     // QQuickPaintedItem override
//     void paint(QPainter* painter) override;

// signals:
//     void centerChanged();
//     void zoomChanged();
//     void tileSourceChanged();
//     void markersChanged();
//     void routeChanged();
//     void clickModeChanged();

//     // Emitted on map click — QML wires these to form fields
//     void takeoffPlaced(double lat, double lon);
//     void landPlaced(double lat, double lon);
//     void tileStatusChanged(const QString& status);

// protected:
//     void mousePressEvent(QMouseEvent* e)   override;
//     void mouseMoveEvent(QMouseEvent* e)    override;
//     void mouseReleaseEvent(QMouseEvent* e) override;
//     void wheelEvent(QWheelEvent* e)        override;

// private:
//     // Tile helpers — same logic as GeoDataLogger MapViewerWidget
//     QPixmap              getTile(int x, int y, int z);
//     QString              getTilePath(int x, int y, int z) const;
//     bool                 loadTileFromDisk(int x, int y, int z, QPixmap& tile);
//     void                 saveTileToDisk(int x, int y, int z, const QPixmap& tile);
//     void                 downloadTile(int x, int y, int z);
//     QString              quadKey(int x, int y, int z) const;

//     // Coordinate conversion
//     QPoint               latLonToPixel(double lat, double lon, int zoom) const;
//     QPair<double,double> pixelToLatLon(int px, int py, int zoom) const;

//     // Drawing helper
//     void drawMarker(QPainter* p, int sx, int sy,
//                     const QColor& fill, const QString& label);

//     static constexpr int TILE_SIZE = 256;

//     // Map state
//     double m_centerLat  = 20.5;
//     double m_centerLon  = 78.9;   // Centre of India as default
//     int    m_zoom       = 6;
//     int    m_tileSource = 0;      // 0=Bing Sat, 1=Bing Map, 2=Google Sat, 3=Google Map, 4=OSM

//     // Markers
//     double m_takeoffLat = 0.0,  m_takeoffLon = 0.0;
//     double m_landLat    = 0.0,  m_landLon    = 0.0;
//     bool   m_hasTakeoff = false, m_hasLand   = false;
//     int    m_clickMode  = 0;

//     // Route
//     QVariantList m_routePoints;

//     // Tile infrastructure
//     QMap<TileKey, QPixmap>        m_tileCache;
//     QMap<QString, QNetworkReply*> m_pending;
//     QNetworkAccessManager*        m_nam;
//     QString                       m_cacheDir;

//     // Pan/drag state
//     bool   m_dragging = false;
//     QPoint m_dragStart;
// };


// include/mapwidget.h

#pragma once
#include <QQuickPaintedItem>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPixmap>
#include <QMap>
#include <QPoint>
#include <QVariantList>

struct TileKey {
    int x, y, z;
    bool operator<(const TileKey& o) const {
        if (z != o.z) return z < o.z;
        if (x != o.x) return x < o.x;
        return y < o.y;
    }
};

class MapWidget : public QQuickPaintedItem
{
    Q_OBJECT
    // QML_ELEMENT  ← remove this line

    Q_PROPERTY(double centerLat  READ centerLat  WRITE setCenterLat  NOTIFY centerChanged)
    Q_PROPERTY(double centerLon  READ centerLon  WRITE setCenterLon  NOTIFY centerChanged)
    Q_PROPERTY(int    zoomLevel  READ zoomLevel  WRITE setZoomLevel  NOTIFY zoomChanged)
    Q_PROPERTY(int    tileSource READ tileSource WRITE setTileSource NOTIFY tileSourceChanged)
    Q_PROPERTY(double takeoffLat READ takeoffLat WRITE setTakeoffLat NOTIFY markersChanged)
    Q_PROPERTY(double takeoffLon READ takeoffLon WRITE setTakeoffLon NOTIFY markersChanged)
    Q_PROPERTY(double landLat    READ landLat    WRITE setLandLat    NOTIFY markersChanged)
    Q_PROPERTY(double landLon    READ landLon    WRITE setLandLon    NOTIFY markersChanged)
    Q_PROPERTY(bool   hasTakeoff READ hasTakeoff WRITE setHasTakeoff NOTIFY markersChanged)
    Q_PROPERTY(bool   hasLand    READ hasLand    WRITE setHasLand    NOTIFY markersChanged)
    Q_PROPERTY(QVariantList routePoints READ routePoints WRITE setRoutePoints NOTIFY routeChanged)
    Q_PROPERTY(int    clickMode  READ clickMode  WRITE setClickMode  NOTIFY clickModeChanged)

public:
    explicit MapWidget(QQuickItem* parent = nullptr);

    double       centerLat()   const { return m_centerLat; }
    double       centerLon()   const { return m_centerLon; }
    int          zoomLevel()   const { return m_zoom; }
    int          tileSource()  const { return m_tileSource; }
    double       takeoffLat()  const { return m_takeoffLat; }
    double       takeoffLon()  const { return m_takeoffLon; }
    double       landLat()     const { return m_landLat; }
    double       landLon()     const { return m_landLon; }
    bool         hasTakeoff()  const { return m_hasTakeoff; }
    bool         hasLand()     const { return m_hasLand; }
    int          clickMode()   const { return m_clickMode; }
    QVariantList routePoints() const { return m_routePoints; }

    void setCenterLat(double v);
    void setCenterLon(double v);
    void setZoomLevel(int v);
    void setTileSource(int v);
    void setTakeoffLat(double v) { m_takeoffLat = v; emit markersChanged(); update(); }
    void setTakeoffLon(double v) { m_takeoffLon = v; emit markersChanged(); update(); }
    void setLandLat(double v)    { m_landLat    = v; emit markersChanged(); update(); }
    void setLandLon(double v)    { m_landLon    = v; emit markersChanged(); update(); }
    void setHasTakeoff(bool v)   { m_hasTakeoff = v; emit markersChanged(); update(); }
    void setHasLand(bool v)      { m_hasLand    = v; emit markersChanged(); update(); }
    void setClickMode(int v)     { m_clickMode  = v; emit clickModeChanged(); }
    void setRoutePoints(const QVariantList& pts) { m_routePoints = pts; emit routeChanged(); update(); }

    Q_INVOKABLE void panTo(double lat, double lon);
    Q_INVOKABLE void clearRoute();
    Q_INVOKABLE void setCacheDir(const QString& dir);

    void paint(QPainter* painter) override;

signals:
    void centerChanged();
    void zoomChanged();
    void tileSourceChanged();
    void markersChanged();
    void routeChanged();
    void clickModeChanged();
    void takeoffPlaced(double lat, double lon);
    void landPlaced(double lat, double lon);
    void tileStatusChanged(const QString& status);

protected:
    void mousePressEvent(QMouseEvent* e)   override;
    void mouseMoveEvent(QMouseEvent* e)    override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e)        override;

private:
    QPixmap              getTile(int x, int y, int z);
    QString              getTilePath(int x, int y, int z) const;
    bool                 loadTileFromDisk(int x, int y, int z, QPixmap& tile);
    void                 saveTileToDisk(int x, int y, int z, const QPixmap& tile);
    void                 downloadTile(int x, int y, int z);
    QString              quadKey(int x, int y, int z) const;
    QPoint               latLonToPixel(double lat, double lon, int zoom) const;
    QPair<double,double> pixelToLatLon(int px, int py, int zoom) const;
    void                 drawMarker(QPainter* p, int sx, int sy,
                    const QColor& fill, const QString& label);

    static constexpr int TILE_SIZE = 256;

    double m_centerLat  = 20.5;
    double m_centerLon  = 78.9;
    int    m_zoom       = 6;
    int    m_tileSource = 0;

    double m_takeoffLat = 0.0, m_takeoffLon = 0.0;
    double m_landLat    = 0.0, m_landLon    = 0.0;
    bool   m_hasTakeoff = false, m_hasLand  = false;
    int    m_clickMode  = 0;

    QVariantList m_routePoints;

    QMap<TileKey, QPixmap>        m_tileCache;
    QMap<QString, QNetworkReply*> m_pending;
    QNetworkAccessManager*        m_nam;
    QString                       m_cacheDir;

    bool   m_dragging = false;
    QPoint m_dragStart;
};
