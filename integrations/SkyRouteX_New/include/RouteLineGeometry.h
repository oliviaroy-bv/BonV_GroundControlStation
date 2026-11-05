#pragma once

#include <QtQuick3D/QQuick3DGeometry>
#include <QVariantList>

// RouteLineGeometry
// Builds a thick tube/ribbon geometry for the drone flight path in 3D.
// Uses the same coordinate system as TerrainMeshGeometry.
//
// Register in main.cpp:
//   qmlRegisterType<RouteLineGeometry>("FlightPlanner", 1, 0, "RouteLineGeometry");

class RouteLineGeometry : public QQuick3DGeometry
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantList profilePoints READ profilePoints
               WRITE setProfilePoints NOTIFY profilePointsChanged)
    Q_PROPERTY(float tubeRadius READ tubeRadius
               WRITE setTubeRadius NOTIFY tubeRadiusChanged)

public:
    explicit RouteLineGeometry(QQuick3DObject* parent = nullptr);

    QVariantList profilePoints() const { return m_profilePoints; }
    float        tubeRadius()    const { return m_tubeRadius; }

    Q_INVOKABLE void setProfilePoints(const QVariantList& pts);
    Q_INVOKABLE void setTubeRadius(float r);

signals:
    void profilePointsChanged();
    void tubeRadiusChanged();

private:
    void rebuild();

    QVariantList m_profilePoints;
    float        m_tubeRadius = 25.0f;  // metres
};
