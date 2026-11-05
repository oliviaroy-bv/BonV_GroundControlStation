#include "GeoTerrainGeometry.h"
#include "srtm_reader.h"
#include <QVector3D>
#include <QtMath>
#include <QDebug>
#include <cmath>

namespace {
struct Vec3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

constexpr double kWgs84A = 6378137.0;
constexpr double kWgs84F = 1.0 / 298.257223563;
constexpr double kWgs84B = kWgs84A * (1.0 - kWgs84F);
constexpr double kWgs84E2 = (kWgs84A * kWgs84A - kWgs84B * kWgs84B) / (kWgs84A * kWgs84A);

double mercatorTileY(double lat)
{
    const double clampedLat = qBound(-85.05112878, lat, 85.05112878);
    const double latR = clampedLat * M_PI / 180.0;
    return (1.0 - std::log(std::tan(latR) + 1.0 / std::cos(latR)) / M_PI) / 2.0;
}

Vec3d latLonHeightToECEF(double latitudeDegrees, double longitudeDegrees, double heightMeters)
{
    const double lat = latitudeDegrees * M_PI / 180.0;
    const double lon = longitudeDegrees * M_PI / 180.0;
    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double sinLon = std::sin(lon);
    const double cosLon = std::cos(lon);
    const double n = kWgs84A / std::sqrt(1.0 - kWgs84E2 * sinLat * sinLat);

    return {
        (n + heightMeters) * cosLat * cosLon,
        (n + heightMeters) * cosLat * sinLon,
        (n * (1.0 - kWgs84E2) + heightMeters) * sinLat
    };
}

Vec3d ecefToENU(const Vec3d& targetECEF,
                double originLatitudeDegrees,
                double originLongitudeDegrees,
                double originHeightMeters)
{
    const Vec3d originECEF = latLonHeightToECEF(originLatitudeDegrees,
                                                originLongitudeDegrees,
                                                originHeightMeters);
    const double lat0 = originLatitudeDegrees * M_PI / 180.0;
    const double lon0 = originLongitudeDegrees * M_PI / 180.0;
    const double sinLat = std::sin(lat0);
    const double cosLat = std::cos(lat0);
    const double sinLon = std::sin(lon0);
    const double cosLon = std::cos(lon0);

    const double dx = targetECEF.x - originECEF.x;
    const double dy = targetECEF.y - originECEF.y;
    const double dz = targetECEF.z - originECEF.z;

    return {
        -sinLon * dx + cosLon * dy,
        -sinLat * cosLon * dx - sinLat * sinLon * dy + cosLat * dz,
        cosLat * cosLon * dx + cosLat * sinLon * dy + sinLat * dz
    };
}

Vec3d latLonHeightToENU(double lat, double lon, double height,
                        double originLat, double originLon, double originHeight)
{
    return ecefToENU(latLonHeightToECEF(lat, lon, height),
                     originLat, originLon, originHeight);
}
}

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
    constexpr double originHeightM = 0.0;

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
    QVector<double> up(rows * cols);
    float eMin = 1e9f, eMax = -1e9f;
    double upMin = 1e100, upMax = -1e100;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            double h = m_srtm->elevation(lats[r], lons[c]);
            if (std::isnan(h) || h < -1000) h = 0.0;
            float fh = static_cast<float>(h);
            elev[r * cols + c] = fh;
            if (fh < eMin) eMin = fh;
            if (fh > eMax) eMax = fh;
            Vec3d enu = latLonHeightToENU(lats[r], lons[c], h,
                                          m_midLat, m_midLon, originHeightM);
            up[r * cols + c] = enu.z;
            if (enu.z < upMin) upMin = enu.z;
            if (enu.z > upMax) upMax = enu.z;
        }
    }

    m_yMin = upMin;
    emit midChanged();

    // Texture bounds (may differ slightly from mesh bounds)
    double tMinLat = m_texMinLat > 0 ? m_texMinLat : m_minLat;
    double tMaxLat = m_texMaxLat > 0 ? m_texMaxLat : m_maxLat;
    double tMinLon = m_texMinLon > 0 ? m_texMinLon : m_minLon;
    double tMaxLon = m_texMaxLon > 0 ? m_texMaxLon : m_maxLon;
    double tLonRange = tMaxLon - tMinLon;
    if (tLonRange < 1e-9) tLonRange = 1e-9;
    double tTopY = mercatorTileY(tMaxLat);
    double tBottomY = mercatorTileY(tMinLat);
    double tYRange = tBottomY - tTopY;
    if (std::abs(tYRange) < 1e-12) tYRange = 1e-12;

    // ── Build vertex buffer ───────────────────────────────────────────────────
    // Layout: position(3f) normal(3f) uv(2f) = 8 floats per vertex
    const int STRIDE = 8;
    QByteArray vbuf;
    vbuf.resize(rows * cols * STRIDE * sizeof(float));
    float* vd = reinterpret_cast<float*>(vbuf.data());
    int vi = 0;

    auto posX = [&](int c) -> float {
        Vec3d enu = latLonHeightToENU(m_midLat, lons[c], originHeightM,
                                      m_midLat, m_midLon, originHeightM);
        return static_cast<float>(enu.x);
    };
    auto posZ = [&](int r) -> float {
        Vec3d enu = latLonHeightToENU(lats[r], m_midLon, originHeightM,
                                      m_midLat, m_midLon, originHeightM);
        return static_cast<float>(-enu.y);
    };
    auto posY = [&](int r, int c) -> float {
        return static_cast<float>((up[r * cols + c] - upMin) * m_verticalScale);
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
                if (n.y() < 0.0f)
                    n = -n;
                nx = n.x(); ny = n.y(); nz = n.z();
            }
            vd[vi++] = nx;
            vd[vi++] = ny;
            vd[vi++] = nz;

            // UV — map to texture geographic extent
            float u = static_cast<float>((lons[c] - tMinLon) / tLonRange);
            // Web map tiles use Mercator Y, not linear latitude.
            float v = static_cast<float>((mercatorTileY(lats[r]) - tTopY) / tYRange);
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
            // Two triangles per quad. Winding matches the flipped Z axis.
            id[ii++] = tl; id[ii++] = tr; id[ii++] = bl;
            id[ii++] = tr; id[ii++] = br; id[ii++] = bl;
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

    float bx = qMax(std::abs(posX(0)), std::abs(posX(cols - 1)));
    float bz = qMax(std::abs(posZ(0)), std::abs(posZ(rows - 1)));
    float by = static_cast<float>((upMax - upMin) * m_verticalScale);
    setBounds(QVector3D(-bx, 0, -bz), QVector3D(bx, by, bz));

    update();
    emit rebuildDone();

    qDebug() << "GeoTerrainGeometry: built" << rows * cols << "vertices,"
             << numTris << "triangles, elevation range"
             << eMin << "to" << eMax << "m";
}
