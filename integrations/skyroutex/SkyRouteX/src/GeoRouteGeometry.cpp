#include "GeoRouteGeometry.h"
#include <QVector3D>
#include <QtMath>
#include <cmath>

GeoRouteGeometry::GeoRouteGeometry(QQuick3DObject* parent)
    : QQuick3DGeometry(parent)
{
}

void GeoRouteGeometry::rebuild()
{
    if (m_waypoints.size() < 2) {
        clear(); update(); return;
    }

    const double mPerDegLat = 111320.0;
    const double mPerDegLon = 111320.0 * std::cos(m_midLat * M_PI / 180.0);

    // Convert waypoints to world-space positions
    struct WP { float x, y, z; };
    QVector<WP> wps;
    wps.reserve(m_waypoints.size());

    for (const QVariant& v : m_waypoints) {
        QVariantMap m = v.toMap();
        double lat = m["lat"].toDouble();
        double lon = m["lon"].toDouble();
        double alt = m["droneAlt"].toDouble();
        float x = static_cast<float>((lon - m_midLon) * mPerDegLon);
        float y = static_cast<float>(alt - m_yMin);
        float z = static_cast<float>((lat - m_midLat) * mPerDegLat);
        wps.append({x, y, z});
    }

    int n = wps.size();
    float r = m_tubeRadius;

    // Simple flat ribbon perpendicular to XZ plane (vertical ribbon)
    // Two vertices per waypoint: top and bottom
    const int STRIDE = 8;
    QByteArray vbuf;
    vbuf.resize(n * 2 * STRIDE * sizeof(float));
    float* vd = reinterpret_cast<float*>(vbuf.data());
    int vi = 0;

    auto addV = [&](float x, float y, float z,
                    float nx, float ny, float nz, float u, float fv) {
        vd[vi++]=x; vd[vi++]=y; vd[vi++]=z;
        vd[vi++]=nx; vd[vi++]=ny; vd[vi++]=nz;
        vd[vi++]=u; vd[vi++]=fv;
    };

    for (int i = 0; i < n; i++) {
        float u = static_cast<float>(i) / (n - 1);
        addV(wps[i].x, wps[i].y + r, wps[i].z,  0, 1, 0,  u, 0.0f);
        addV(wps[i].x, wps[i].y - r, wps[i].z,  0, -1, 0, u, 1.0f);
    }

    // Quad strip indices
    int numTris = (n - 1) * 2;
    QByteArray ibuf;
    ibuf.resize(numTris * 3 * sizeof(uint32_t));
    uint32_t* id = reinterpret_cast<uint32_t*>(ibuf.data());
    int ii = 0;

    for (int i = 0; i < n - 1; i++) {
        uint32_t b = i * 2;
        id[ii++]=b;   id[ii++]=b+2; id[ii++]=b+1;
        id[ii++]=b+1; id[ii++]=b+2; id[ii++]=b+3;
    }

    clear();
    setVertexData(vbuf);
    setStride(STRIDE * sizeof(float));
    setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);
    addAttribute(QQuick3DGeometry::Attribute::PositionSemantic,  0,              QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::NormalSemantic,    3*sizeof(float),QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::TexCoordSemantic,  6*sizeof(float),QQuick3DGeometry::Attribute::F32Type);
    setIndexData(ibuf);
    addAttribute(QQuick3DGeometry::Attribute::IndexSemantic, 0, QQuick3DGeometry::Attribute::U32Type);

    update();
}
