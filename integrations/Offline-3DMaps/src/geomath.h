#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

namespace GeoMath {

struct Vec3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Geodetic {
    double latitude = 0.0;
    double longitude = 0.0;
    double height = 0.0;
};

inline constexpr double Wgs84A = 6378137.0;
inline constexpr double Wgs84F = 1.0 / 298.257223563;
inline constexpr double Wgs84B = Wgs84A * (1.0 - Wgs84F);
inline constexpr double Wgs84E2 =
    (Wgs84A * Wgs84A - Wgs84B * Wgs84B) / (Wgs84A * Wgs84A);
inline constexpr double MaxMercatorLatitude = 85.0511287798066;

inline double degreesToRadians(double degrees)
{
    return degrees * std::numbers::pi / 180.0;
}

inline double radiansToDegrees(double radians)
{
    return radians * 180.0 / std::numbers::pi;
}

inline Vec3d latLonHeightToEcef(double latDeg, double lonDeg, double height)
{
    const double lat = degreesToRadians(latDeg);
    const double lon = degreesToRadians(lonDeg);
    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double sinLon = std::sin(lon);
    const double cosLon = std::cos(lon);
    const double n = Wgs84A / std::sqrt(1.0 - Wgs84E2 * sinLat * sinLat);

    return {
        (n + height) * cosLat * cosLon,
        (n + height) * cosLat * sinLon,
        (n * (1.0 - Wgs84E2) + height) * sinLat
    };
}

inline Geodetic ecefToLatLonHeight(const Vec3d &ecef)
{
    const double longitude = std::atan2(ecef.y, ecef.x);
    const double p = std::hypot(ecef.x, ecef.y);
    double latitude = std::atan2(ecef.z, p * (1.0 - Wgs84E2));
    double height = 0.0;

    for (int i = 0; i < 10; ++i) {
        const double sinLat = std::sin(latitude);
        const double n = Wgs84A / std::sqrt(1.0 - Wgs84E2 * sinLat * sinLat);
        height = p / std::max(std::cos(latitude), 1.0e-15) - n;
        const double next = std::atan2(ecef.z, p * (1.0 - Wgs84E2 * n / (n + height)));
        if (std::abs(next - latitude) < 1.0e-13) {
            latitude = next;
            break;
        }
        latitude = next;
    }

    const double sinLat = std::sin(latitude);
    const double n = Wgs84A / std::sqrt(1.0 - Wgs84E2 * sinLat * sinLat);
    height = p / std::max(std::cos(latitude), 1.0e-15) - n;
    return {radiansToDegrees(latitude), radiansToDegrees(longitude), height};
}

inline Vec3d ecefToEnu(const Vec3d &target, double originLatDeg,
                       double originLonDeg, double originHeight)
{
    const Vec3d origin = latLonHeightToEcef(originLatDeg, originLonDeg, originHeight);
    const double lat0 = degreesToRadians(originLatDeg);
    const double lon0 = degreesToRadians(originLonDeg);
    const double sinLat = std::sin(lat0);
    const double cosLat = std::cos(lat0);
    const double sinLon = std::sin(lon0);
    const double cosLon = std::cos(lon0);
    const double dx = target.x - origin.x;
    const double dy = target.y - origin.y;
    const double dz = target.z - origin.z;

    return {
        -sinLon * dx + cosLon * dy,
        -sinLat * cosLon * dx - sinLat * sinLon * dy + cosLat * dz,
        cosLat * cosLon * dx + cosLat * sinLon * dy + sinLat * dz
    };
}

inline Vec3d latLonHeightToEnu(double lat, double lon, double height,
                               double originLat, double originLon,
                               double originHeight)
{
    return ecefToEnu(latLonHeightToEcef(lat, lon, height),
                     originLat, originLon, originHeight);
}

inline Vec3d enuToEcef(const Vec3d &enu, double originLatDeg,
                       double originLonDeg, double originHeight)
{
    const Vec3d origin = latLonHeightToEcef(originLatDeg, originLonDeg, originHeight);
    const double lat0 = degreesToRadians(originLatDeg);
    const double lon0 = degreesToRadians(originLonDeg);
    const double sinLat = std::sin(lat0);
    const double cosLat = std::cos(lat0);
    const double sinLon = std::sin(lon0);
    const double cosLon = std::cos(lon0);

    return {
        origin.x - sinLon * enu.x - sinLat * cosLon * enu.y + cosLat * cosLon * enu.z,
        origin.y + cosLon * enu.x - sinLat * sinLon * enu.y + cosLat * sinLon * enu.z,
        origin.z + cosLat * enu.y + sinLat * enu.z
    };
}

inline Geodetic enuToLatLonHeight(const Vec3d &enu, double originLat,
                                   double originLon, double originHeight)
{
    return ecefToLatLonHeight(enuToEcef(enu, originLat, originLon, originHeight));
}

inline double mercatorY(double latitudeDeg)
{
    const double lat = degreesToRadians(std::clamp(latitudeDeg,
                                                    -MaxMercatorLatitude,
                                                    MaxMercatorLatitude));
    return std::log(std::tan(std::numbers::pi / 4.0 + lat / 2.0));
}

inline double inverseMercatorY(double value)
{
    return radiansToDegrees(2.0 * std::atan(std::exp(value)) - std::numbers::pi / 2.0);
}

inline double longitudeToTileX(double longitude, int zoom)
{
    return (longitude + 180.0) / 360.0 * std::ldexp(1.0, zoom);
}

inline double latitudeToTileY(double latitude, int zoom)
{
    const double n = std::ldexp(1.0, zoom);
    return (1.0 - mercatorY(latitude) / std::numbers::pi) * 0.5 * n;
}

inline double tileXToLongitude(double x, int zoom)
{
    return x / std::ldexp(1.0, zoom) * 360.0 - 180.0;
}

inline double tileYToLatitude(double y, int zoom)
{
    const double normalized = std::numbers::pi * (1.0 - 2.0 * y / std::ldexp(1.0, zoom));
    return radiansToDegrees(std::atan(std::sinh(normalized)));
}

} // namespace GeoMath
