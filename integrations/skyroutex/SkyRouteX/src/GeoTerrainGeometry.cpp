#include "GeoTerrainGeometry.h"
#include "srtm_reader.h"
#include <QVector3D>
#include <QtMath>
#include <QDebug>
#include <cmath>

GeoTerrainGeometry::GeoTerrainGeometry(QQuick3DObject* parent)
    : QQuick3DGeometry(parent)
{
}

void GeoTerrainGeometry::setSrtmReaderObject(QObject* obj)
{
    m_srtm = qobject_cast<SrtmReader*>(obj);
}

void GeoTerrainGeometry::rebuild()
{
    if (!m_srtm) {
        qWarning() << "GeoTerrainGeometry: no SrtmReader set";
        return;
    }

    if (m_maxLat <= m_minLat || m_maxLon <= m_minLon) {
        qWarning() << "GeoTerrainGeometry: invalid bounds";
        return;
    }

    // ── Grid setup ────────────────────────────────────────────────────────────
    m_midLat = (m_minLat + m_maxLat) / 2.0;
    m_midLon = (m_minLon + m_maxLon) / 2.0;

    // Metres per degree (approximate, good for ±10° from equator)
    const double mPerDegLat = 111320.0;
    const double mPerDegLon = 111320.0 * std::cos(m_midLat * M_PI / 180.0);

    // Build lat/lon arrays
    QVector<double> lats, lons;
    for (double lat = m_minLat; lat <= m_maxLat + m_stepDeg * 0.5; lat += m_stepDeg)
        lats.append(qBound(m_minLat, lat, m_maxLat));
    for (double lon = m_minLon; lon <= m_maxLon + m_stepDeg * 0.5; lon += m_stepDeg)
        lons.append(qBound(m_minLon, lon, m_maxLon));

    if (lats.size() < 2 || lons.size() < 2) {
        qWarning() << "GeoTerrainGeometry: grid too small" << lats.size() << lons.size();
        return;
    }

    int rows = lats.size();
    int cols = lons.size();
    qDebug() << "GeoTerrainGeometry: building" << rows << "x" << cols << "grid";

    // ── Sample SRTM elevation ─────────────────────────────────────────────────
    QVector<float> elev(rows * cols);
    float eMin = 1e9f, eMax = -1e9f;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            double h = m_srtm->elevation(lats[r], lons[c]);
            if (std::isnan(h) || h < -1000) h = 0.0;
            float fh = static_cast<float>(h);
            elev[r * cols + c] = fh;
            if (fh < eMin) eMin = fh;
            if (fh > eMax) eMax = fh;
        }
    }

    m_yMin = static_cast<double>(eMin);
    emit midChanged();

    // Texture bounds (may differ slightly from mesh bounds)
    double tMinLat = m_texMinLat > 0 ? m_texMinLat : m_minLat;
    double tMaxLat = m_texMaxLat > 0 ? m_texMaxLat : m_maxLat;
    double tMinLon = m_texMinLon > 0 ? m_texMinLon : m_minLon;
    double tMaxLon = m_texMaxLon > 0 ? m_texMaxLon : m_maxLon;
    double tLatRange = tMaxLat - tMinLat;
    double tLonRange = tMaxLon - tMinLon;
    if (tLatRange < 1e-9) tLatRange = 1e-9;
    if (tLonRange < 1e-9) tLonRange = 1e-9;

    // ── Build vertex buffer ───────────────────────────────────────────────────
    // Layout: position(3f) normal(3f) uv(2f) = 8 floats per vertex
    const int STRIDE = 8;
    QByteArray vbuf;
    vbuf.resize(rows * cols * STRIDE * sizeof(float));
    float* vd = reinterpret_cast<float*>(vbuf.data());
    int vi = 0;

    auto posX = [&](int c) -> float {
        return static_cast<float>((lons[c] - m_midLon) * mPerDegLon);
    };
    auto posZ = [&](int r) -> float {
        // Z increases northward → negate lat direction
        return static_cast<float>((lats[r] - m_midLat) * mPerDegLat);
    };
    auto posY = [&](int r, int c) -> float {
        return elev[r * cols + c] - eMin;
    };

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            // Position
            vd[vi++] = posX(c);
            vd[vi++] = posY(r, c);
            vd[vi++] = posZ(r);

            // Normal — finite differences from neighbours
            float nx = 0, ny = 1, nz = 0;
            if (c > 0 && c < cols - 1 && r > 0 && r < rows - 1) {
                float dx = posX(c+1) - posX(c-1);
                float dz = posZ(r+1) - posZ(r-1);
                float hy1 = posY(r,   c+1) - posY(r,   c-1);
                float hy2 = posY(r+1, c)   - posY(r-1, c);
                QVector3D t1(dx, hy1, 0);
                QVector3D t2(0, hy2, dz);
                QVector3D n = QVector3D::crossProduct(t1, t2).normalized();
                nx = n.x(); ny = n.y(); nz = n.z();
            }
            vd[vi++] = nx;
            vd[vi++] = ny;
            vd[vi++] = nz;

            // UV — map to texture geographic extent
            float u = static_cast<float>((lons[c] - tMinLon) / tLonRange);
            // V: tile images have y=0 at top (north), so flip
            float v = static_cast<float>(1.0 - (lats[r] - tMinLat) / tLatRange);
            vd[vi++] = u;
            vd[vi++] = v;
        }
    }

    // ── Build index buffer ────────────────────────────────────────────────────
    int numTris = (rows - 1) * (cols - 1) * 2;
    QByteArray ibuf;
    ibuf.resize(numTris * 3 * sizeof(uint32_t));
    uint32_t* id = reinterpret_cast<uint32_t*>(ibuf.data());
    int ii = 0;

    for (int r = 0; r < rows - 1; r++) {
        for (int c = 0; c < cols - 1; c++) {
            uint32_t tl = r * cols + c;
            uint32_t tr = tl + 1;
            uint32_t bl = tl + cols;
            uint32_t br = bl + 1;
            // Two triangles per quad
            id[ii++] = tl; id[ii++] = bl; id[ii++] = tr;
            id[ii++] = tr; id[ii++] = bl; id[ii++] = br;
        }
    }

    // ── Upload ────────────────────────────────────────────────────────────────
    clear();
    setVertexData(vbuf);
    setStride(STRIDE * sizeof(float));
    setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);

    addAttribute(QQuick3DGeometry::Attribute::PositionSemantic, 0,
                 QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::NormalSemantic, 3 * sizeof(float),
                 QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::TexCoordSemantic, 6 * sizeof(float),
                 QQuick3DGeometry::Attribute::F32Type);

    setIndexData(ibuf);
    addAttribute(QQuick3DGeometry::Attribute::IndexSemantic, 0,
                 QQuick3DGeometry::Attribute::U32Type);

    float bx = static_cast<float>((m_maxLon - m_minLon) / 2.0 * mPerDegLon);
    float bz = static_cast<float>((m_maxLat - m_minLat) / 2.0 * mPerDegLat);
    float by = eMax - eMin;
    setBounds(QVector3D(-bx, 0, -bz), QVector3D(bx, by, bz));

    update();
    emit rebuildDone();

    qDebug() << "GeoTerrainGeometry: built" << rows * cols << "vertices,"
             << numTris << "triangles, elevation range"
             << eMin << "to" << eMax << "m";
}
