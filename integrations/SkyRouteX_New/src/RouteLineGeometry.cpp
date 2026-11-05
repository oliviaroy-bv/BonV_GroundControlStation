#include "RouteLineGeometry.h"
#include <QVector3D>
#include <cmath>

RouteLineGeometry::RouteLineGeometry(QQuick3DObject* parent)
    : QQuick3DGeometry(parent)
{
}

void RouteLineGeometry::setProfilePoints(const QVariantList& pts)
{
    m_profilePoints = pts;
    emit profilePointsChanged();
    rebuild();
}

void RouteLineGeometry::setTubeRadius(float r)
{
    m_tubeRadius = r;
    emit tubeRadiusChanged();
    rebuild();
}

void RouteLineGeometry::rebuild()
{
    if (m_profilePoints.size() < 2) {
        clear();
        update();
        return;
    }

    // Collect drone path points
    struct Pt { float x, y; };
    QVector<Pt> pts;
    pts.reserve(m_profilePoints.size());

    float xMin = 0.0f, xMax = 0.0f;
    float yMin = 1e9f;

    for (int i = 0; i < m_profilePoints.size(); i++) {
        QVariantMap m = m_profilePoints[i].toMap();
        float x = static_cast<float>(m["distKm"].toDouble() * 1000.0);
        float y = static_cast<float>(m["droneAlt"].toDouble());
        pts.append({x, y});
        if (x < xMin) xMin = x;
        if (x > xMax) xMax = x;
        if (y < yMin) yMin = y;
    }

    // Find yBase from terrain (same as TerrainMeshGeometry)
    float yBase = 1e9f;
    for (int i = 0; i < m_profilePoints.size(); i++) {
        QVariantMap m = m_profilePoints[i].toMap();
        float ty = static_cast<float>(m["terrainAlt"].toDouble());
        if (ty < yBase) yBase = ty;
    }

    float xMid = (xMin + xMax) / 2.0f;
    float half  = m_tubeRadius;
    int   n     = pts.size();

    // Simple flat ribbon: 2 verts per point (top and bottom of tube)
    const int STRIDE = 8;
    QByteArray vertData;
    vertData.resize(n * 2 * STRIDE * sizeof(float));
    float* vd = reinterpret_cast<float*>(vertData.data());
    int vi = 0;

    float xRange = xMax - xMin;
    if (xRange < 1.0f) xRange = 1.0f;

    auto addV = [&](float x, float y, float z,
                     float nx, float ny, float nz, float u, float v) {
        vd[vi++] = x;  vd[vi++] = y;  vd[vi++] = z;
        vd[vi++] = nx; vd[vi++] = ny; vd[vi++] = nz;
        vd[vi++] = u;  vd[vi++] = v;
    };

    for (int i = 0; i < n; i++) {
        float x  = pts[i].x - xMid;
        float y  = pts[i].y - yBase;
        float u  = (pts[i].x - xMin) / xRange;

        addV(x, y + half, 0,   0, 1, 0,  u, 0.0f);
        addV(x, y - half, 0,   0, -1, 0, u, 1.0f);
    }

    // Index buffer — quad strip
    int numTris = (n - 1) * 2;
    QByteArray idxData;
    idxData.resize(numTris * 3 * sizeof(uint32_t));
    uint32_t* id = reinterpret_cast<uint32_t*>(idxData.data());
    int ii = 0;

    for (int i = 0; i < n - 1; i++) {
        uint32_t b = i * 2;
        id[ii++] = b;   id[ii++] = b+2; id[ii++] = b+1;
        id[ii++] = b+1; id[ii++] = b+2; id[ii++] = b+3;
    }

    clear();
    setVertexData(vertData);
    setStride(STRIDE * sizeof(float));
    setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);

    addAttribute(QQuick3DGeometry::Attribute::PositionSemantic, 0,
                 QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::NormalSemantic, 3 * sizeof(float),
                 QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::TexCoordSemantic, 6 * sizeof(float),
                 QQuick3DGeometry::Attribute::F32Type);

    setIndexData(idxData);
    addAttribute(QQuick3DGeometry::Attribute::IndexSemantic, 0,
                 QQuick3DGeometry::Attribute::U32Type);

    float bx = (xMax - xMin) / 2.0f;
    setBounds(QVector3D(-bx, 0, -half), QVector3D(bx, 2000, half));

    update();
}
