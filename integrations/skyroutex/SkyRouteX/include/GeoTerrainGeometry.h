#pragma once

#include <QtQuick3D/QQuick3DGeometry>
#include <QVariantList>
#include <QObject>

// GeoTerrainGeometry
// Builds a geographic terrain mesh suitable for draping a satellite tile texture.
//
// The mesh covers the geographic bounding box sampled at SRTM resolution.
// UV coordinates map directly to (lon-minLon)/(maxLon-minLon) and
// (lat-minLat)/(maxLat-minLat) so the stitched tile texture aligns perfectly.
//
// Coordinate system (metres, relative to route midpoint):
//   X = (lon - midLon) * metersPerDegreeLon(midLat)
//   Y = elevation (metres ASL, shifted so min = 0)
//   Z = (lat - midLat) * metersPerDegreeLat   (Z increases northward)
//
// Register in main.cpp:
//   qmlRegisterType<GeoTerrainGeometry>("FlightPlanner", 1, 0, "GeoTerrainGeometry");

class SrtmReader;   // forward declare so we can call elevation()

class GeoTerrainGeometry : public QQuick3DGeometry
{
    Q_OBJECT

    // Geographic bounds of the terrain mesh
    Q_PROPERTY(double minLat READ minLat WRITE setMinLat NOTIFY boundsChanged)
    Q_PROPERTY(double maxLat READ maxLat WRITE setMaxLat NOTIFY boundsChanged)
    Q_PROPERTY(double minLon READ minLon WRITE setMinLon NOTIFY boundsChanged)
    Q_PROPERTY(double maxLon READ maxLon WRITE setMaxLon NOTIFY boundsChanged)

    // Texture geographic extent (from TileStitcher)
    // May differ slightly from mesh bounds — UVs are remapped accordingly
    Q_PROPERTY(double texMinLat READ texMinLat WRITE setTexMinLat NOTIFY texBoundsChanged)
    Q_PROPERTY(double texMaxLat READ texMaxLat WRITE setTexMaxLat NOTIFY texBoundsChanged)
    Q_PROPERTY(double texMinLon READ texMinLon WRITE setTexMinLon NOTIFY texBoundsChanged)
    Q_PROPERTY(double texMaxLon READ texMaxLon WRITE setTexMaxLon NOTIFY texBoundsChanged)

    // Grid step in degrees (~90m at mid-latitudes = 3× SRTM sample for speed)
    Q_PROPERTY(double stepDeg READ stepDeg WRITE setStepDeg NOTIFY stepChanged)

    // World-space centre (route midpoint, output after rebuild)
    Q_PROPERTY(double midLat  READ midLat  NOTIFY midChanged)
    Q_PROPERTY(double midLon  READ midLon  NOTIFY midChanged)
    Q_PROPERTY(double yMin    READ yMin    NOTIFY midChanged)

public:
    explicit GeoTerrainGeometry(QQuick3DObject* parent = nullptr);

    // Must be set before calling rebuild()
    void setSrtmReader(SrtmReader* srtm) { m_srtm = srtm; }
    Q_INVOKABLE void setSrtmReaderObject(QObject* obj);

    double minLat()   const { return m_minLat; }
    double maxLat()   const { return m_maxLat; }
    double minLon()   const { return m_minLon; }
    double maxLon()   const { return m_maxLon; }
    double texMinLat()const { return m_texMinLat; }
    double texMaxLat()const { return m_texMaxLat; }
    double texMinLon()const { return m_texMinLon; }
    double texMaxLon()const { return m_texMaxLon; }
    double stepDeg()  const { return m_stepDeg; }
    double midLat()   const { return m_midLat; }
    double midLon()   const { return m_midLon; }
    double yMin()     const { return m_yMin; }

    void setMinLat(double v)    { m_minLat    = v; emit boundsChanged(); }
    void setMaxLat(double v)    { m_maxLat    = v; emit boundsChanged(); }
    void setMinLon(double v)    { m_minLon    = v; emit boundsChanged(); }
    void setMaxLon(double v)    { m_maxLon    = v; emit boundsChanged(); }
    void setTexMinLat(double v) { m_texMinLat = v; emit texBoundsChanged(); }
    void setTexMaxLat(double v) { m_texMaxLat = v; emit texBoundsChanged(); }
    void setTexMinLon(double v) { m_texMinLon = v; emit texBoundsChanged(); }
    void setTexMaxLon(double v) { m_texMaxLon = v; emit texBoundsChanged(); }
    void setStepDeg(double v)   { m_stepDeg   = v; emit stepChanged(); }

    // Call after setting all bounds + srtmReader
    Q_INVOKABLE void rebuild();

signals:
    void boundsChanged();
    void texBoundsChanged();
    void stepChanged();
    void midChanged();
    void rebuildDone();

private:
    SrtmReader* m_srtm    = nullptr;

    double m_minLat    = 0, m_maxLat = 0;
    double m_minLon    = 0, m_maxLon = 0;
    double m_texMinLat = 0, m_texMaxLat = 0;
    double m_texMinLon = 0, m_texMaxLon = 0;
    double m_stepDeg   = 0.0009;   // ~100m at mid-latitudes

    double m_midLat = 0, m_midLon = 0;
    double m_yMin   = 0;
};
