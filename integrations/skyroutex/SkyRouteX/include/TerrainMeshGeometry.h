#pragma once

#include <QtQuick3D/QQuick3DGeometry>
#include <QVector3D>
#include <QVariantList>

// TerrainMeshGeometry
// Builds a triangulated terrain mesh from a route profile for Qt Quick 3D.
//
// Input: profile points [{lat, lon, droneAlt, terrainAlt, distKm}, ...]
// Output: triangle strip mesh representing terrain cross-section corridor
//
// Coordinate system:
//   X = distance along route (metres, 0 at takeoff)
//   Y = altitude (metres)
//   Z = lateral offset (for corridor width)
//
// Register in main.cpp:
//   qmlRegisterType<TerrainMeshGeometry>("FlightPlanner", 1, 0, "TerrainMeshGeometry");

class TerrainMeshGeometry : public QQuick3DGeometry
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantList profilePoints READ profilePoints
               WRITE setProfilePoints NOTIFY profilePointsChanged)
    Q_PROPERTY(float corridorWidth READ corridorWidth
               WRITE setCorridorWidth NOTIFY corridorWidthChanged)

public:
    explicit TerrainMeshGeometry(QQuick3DObject* parent = nullptr);

    QVariantList profilePoints() const { return m_profilePoints; }
    float        corridorWidth() const { return m_corridorWidth; }

    Q_INVOKABLE void setProfilePoints(const QVariantList& pts);
    Q_INVOKABLE void setCorridorWidth(float w);

signals:
    void profilePointsChanged();
    void corridorWidthChanged();

    // Emitted after rebuild — QML can read normalised scale info
    void scaleInfoChanged(float xRange, float yRange, float yMin);

private:
    void rebuild();

    QVariantList m_profilePoints;
    float        m_corridorWidth = 500.0f;  // metres either side of route
};
