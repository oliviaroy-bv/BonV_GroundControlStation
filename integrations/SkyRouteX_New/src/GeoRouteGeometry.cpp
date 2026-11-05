#include "GeoRouteGeometry.h"
#include <QVector3D>
#include <QtMath>
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

GeoRouteGeometry::GeoRouteGeometry(QQuick3DObject* parent)
    : QQuick3DGeometry(parent)
{
}

void GeoRouteGeometry::rebuild()
{
    if (m_waypoints.size() < 2) {
        clear(); update(); return;
    }

    constexpr double originHeightM = 0.0;

    // Convert waypoints to world-space positions
    struct WP { float x, y, z; };
    QVector<WP> wps;
    wps.reserve(m_waypoints.size());

    for (const QVariant& v : m_waypoints) {
        QVariantMap m = v.toMap();
        double lat = m["lat"].toDouble();
        double lon = m["lon"].toDouble();
        double alt = m["droneAlt"].toDouble();
        Vec3d enu = latLonHeightToENU(lat, lon, alt, m_midLat, m_midLon, originHeightM);
        float x = static_cast<float>(enu.x);
        float y = static_cast<float>((enu.z - m_yMin) * m_verticalScale);
        float z = static_cast<float>(-enu.y);
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
