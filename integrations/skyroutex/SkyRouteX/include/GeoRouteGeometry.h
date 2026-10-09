#pragma once

#include <QtQuick3D/QQuick3DGeometry>
#include <QVariantList>

// GeoRouteGeometry
// Builds the drone flight path as a 3D ribbon in the same geographic
// coordinate system as GeoTerrainGeometry.
//
// Input: waypoint list [{lat, lon, droneAlt, terrainAlt, distKm}, ...]
// Uses midLat/midLon/yMin from GeoTerrainGeometry for consistent coords.

class GeoRouteGeometry : public QQuick3DGeometry
{
    Q_OBJECT

    Q_PROPERTY(QVariantList waypoints READ waypoints
               WRITE setWaypoints NOTIFY waypointsChanged)
    Q_PROPERTY(double midLat READ midLat WRITE setMidLat NOTIFY originChanged)
    Q_PROPERTY(double midLon READ midLon WRITE setMidLon NOTIFY originChanged)
    Q_PROPERTY(double yMin   READ yMin   WRITE setYMin   NOTIFY originChanged)
    Q_PROPERTY(float  tubeRadius READ tubeRadius WRITE setTubeRadius NOTIFY tubeRadiusChanged)

public:
    explicit GeoRouteGeometry(QQuick3DObject* parent = nullptr);

    QVariantList waypoints()  const { return m_waypoints; }
    double       midLat()     const { return m_midLat; }
    double       midLon()     const { return m_midLon; }
    double       yMin()       const { return m_yMin; }
    float        tubeRadius() const { return m_tubeRadius; }

    void setWaypoints(const QVariantList& pts) { m_waypoints = pts; emit waypointsChanged(); rebuild(); }
    void setMidLat(double v)    { m_midLat    = v; emit originChanged(); rebuild(); }
    void setMidLon(double v)    { m_midLon    = v; emit originChanged(); rebuild(); }
    void setYMin(double v)      { m_yMin      = v; emit originChanged(); rebuild(); }
    void setTubeRadius(float v) { m_tubeRadius = v; emit tubeRadiusChanged(); rebuild(); }

    Q_INVOKABLE void rebuild();

signals:
    void waypointsChanged();
    void originChanged();
    void tubeRadiusChanged();

private:
    QVariantList m_waypoints;
    double       m_midLat    = 0;
    double       m_midLon    = 0;
    double       m_yMin      = 0;
    float        m_tubeRadius = 20.0f;
};
