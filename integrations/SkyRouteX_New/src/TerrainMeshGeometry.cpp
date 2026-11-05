#include "TerrainMeshGeometry.h"
#include <QVector3D>
#include <cmath>

TerrainMeshGeometry::TerrainMeshGeometry(QQuick3DObject* parent)
    : QQuick3DGeometry(parent)
{
}

void TerrainMeshGeometry::setProfilePoints(const QVariantList& pts)
{
    m_profilePoints = pts;
    emit profilePointsChanged();
    rebuild();
}

void TerrainMeshGeometry::setCorridorWidth(float w)
{
    m_corridorWidth = w;
    emit corridorWidthChanged();
    rebuild();
}

void TerrainMeshGeometry::rebuild()
{
    if (m_profilePoints.size() < 2) {
        clear();
        update();
        return;
    }

    // ── Collect points ────────────────────────────────────────────────────────
    struct Pt { float x, terrainY, droneY; };
    QVector<Pt> pts;
    pts.reserve(m_profilePoints.size());

    float xMin = 0.0f, xMax = 0.0f;
    float yMin = 1e9f, yMax = -1e9f;

    for (int i = 0; i < m_profilePoints.size(); i++) {
        QVariantMap m = m_profilePoints[i].toMap();
        float x  = static_cast<float>(m["distKm"].toDouble() * 1000.0); // metres
        float ty = static_cast<float>(m["terrainAlt"].toDouble());
        float dy = static_cast<float>(m["droneAlt"].toDouble());
        pts.append({x, ty, dy});
        if (x  < xMin) xMin = x;
        if (x  > xMax) xMax = x;
        if (ty < yMin) yMin = ty;
        if (dy > yMax) yMax = dy;
    }

    // Emit scale info so QML can label axes
    emit scaleInfoChanged(xMax - xMin, yMax - yMin, yMin);

    // Normalise: centre X, translate Y to start near 0
    float xMid = (xMin + xMax) / 2.0f;
    float yBase = yMin;

    // ── Build terrain mesh ────────────────────────────────────────────────────
    // We create a flat terrain ribbon: each profile point generates 2 vertices
    // (left and right edges of the corridor) at terrain height.
    // Plus we need a "floor" quad at yBase to give the terrain volume.
    //
    // Vertex layout: position (3f) + normal (3f) + uv (2f) = 8 floats
    const int STRIDE = 8;
    int n = pts.size();
    int numVerts = n * 4;  // 2 surface + 2 floor per point
    QByteArray vertData;
    vertData.resize(numVerts * STRIDE * sizeof(float));
    float* vd = reinterpret_cast<float*>(vertData.data());
    int vi = 0;

    auto addV = [&](float x, float y, float z,
                     float nx, float ny, float nz,
                     float u,  float v) {
        vd[vi++] = x;  vd[vi++] = y;  vd[vi++] = z;
        vd[vi++] = nx; vd[vi++] = ny; vd[vi++] = nz;
        vd[vi++] = u;  vd[vi++] = v;
    };

    float half = m_corridorWidth / 2.0f;
    float xRange = xMax - xMin;
    float yRange = yMax - yBase;
    if (yRange < 1.0f) yRange = 1.0f;

    for (int i = 0; i < n; i++) {
        float x  = pts[i].x - xMid;
        float ty = pts[i].terrainY - yBase;
        float u  = (pts[i].x - xMin) / xRange;

        // Compute normal from neighbours
        QVector3D normal(0, 1, 0);
        if (i > 0 && i < n - 1) {
            float dx = pts[i+1].x - pts[i-1].x;
            float dy = pts[i+1].terrainY - pts[i-1].terrainY;
            QVector3D tangent(dx, dy, 0);
            normal = QVector3D::crossProduct(tangent, QVector3D(0, 0, 1)).normalized();
        }

        // Surface left edge
        addV(x, ty, -half,  normal.x(), normal.y(), normal.z(),  u, 0.0f);
        // Surface right edge
        addV(x, ty,  half,  normal.x(), normal.y(), normal.z(),  u, 1.0f);
        // Floor left edge (for terrain volume)
        addV(x, 0.0f, -half,  0, -1, 0,  u, 0.0f);
        // Floor right edge
        addV(x, 0.0f,  half,  0, -1, 0,  u, 1.0f);
    }

    // ── Index buffer ──────────────────────────────────────────────────────────
    // Surface quads: verts 0,1 of i and 0,1 of i+1
    // Floor quads:   verts 2,3 of i and 2,3 of i+1
    // Side quads:    left wall and right wall connecting surface to floor
    int numTris = (n - 1) * 4 * 2;  // 4 quads × 2 triangles each per segment
    QByteArray idxData;
    idxData.resize(numTris * 3 * sizeof(uint32_t));
    uint32_t* id = reinterpret_cast<uint32_t*>(idxData.data());
    int ii = 0;

    auto addTri = [&](uint32_t a, uint32_t b, uint32_t c) {
        id[ii++] = a; id[ii++] = b; id[ii++] = c;
    };

    for (int i = 0; i < n - 1; i++) {
        uint32_t b = i * 4;       // current row base
        uint32_t n = (i+1) * 4;  // next row base

        // Top surface  (b+0, b+1, n+0, n+1)
        addTri(b+0, n+0, b+1);
        addTri(b+1, n+0, n+1);

        // Floor (b+2, b+3, n+2, n+3)
        addTri(b+2, b+3, n+2);
        addTri(b+3, n+3, n+2);

        // Left wall  (b+0, b+2, n+0, n+2)
        addTri(b+0, b+2, n+0);
        addTri(b+2, n+2, n+0);

        // Right wall (b+1, b+3, n+1, n+3)
        addTri(b+1, n+1, b+3);
        addTri(b+3, n+1, n+3);
    }

    // ── Upload to QQuick3DGeometry ─────────────────────────────────────────
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

    // Bounding box
    float bx = (xMax - xMin) / 2.0f;
    float by = yRange;
    setBounds(QVector3D(-bx, 0, -half), QVector3D(bx, by, half));

    update();
}
