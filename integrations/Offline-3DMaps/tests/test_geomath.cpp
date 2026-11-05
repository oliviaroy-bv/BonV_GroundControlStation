#include "geomath.h"

#include <QTest>

class GeoMathTests : public QObject
{
    Q_OBJECT

private slots:
    void geodeticEcefRoundTrip();
    void enuRoundTrip();
    void mercatorRoundTrip();
};

void GeoMathTests::geodeticEcefRoundTrip()
{
    const GeoMath::Geodetic input{12.9715987, 77.594566, 920.25};
    const auto output = GeoMath::ecefToLatLonHeight(
        GeoMath::latLonHeightToEcef(input.latitude, input.longitude, input.height));
    QVERIFY(std::abs(output.latitude - input.latitude) < 1.0e-9);
    QVERIFY(std::abs(output.longitude - input.longitude) < 1.0e-9);
    QVERIFY(std::abs(output.height - input.height) < 1.0e-5);
}

void GeoMathTests::enuRoundTrip()
{
    const GeoMath::Geodetic origin{12.9716, 77.5946, 900.0};
    const GeoMath::Geodetic target{12.978, 77.603, 1040.0};
    const auto enu = GeoMath::latLonHeightToEnu(target.latitude, target.longitude, target.height,
                                                origin.latitude, origin.longitude, origin.height);
    const auto output = GeoMath::enuToLatLonHeight(enu, origin.latitude,
                                                   origin.longitude, origin.height);
    QVERIFY(std::abs(output.latitude - target.latitude) < 1.0e-9);
    QVERIFY(std::abs(output.longitude - target.longitude) < 1.0e-9);
    QVERIFY(std::abs(output.height - target.height) < 1.0e-5);
}

void GeoMathTests::mercatorRoundTrip()
{
    const double latitude = 51.5074;
    QVERIFY(std::abs(GeoMath::inverseMercatorY(GeoMath::mercatorY(latitude)) - latitude) < 1.0e-10);
}

QTEST_APPLESS_MAIN(GeoMathTests)
#include "test_geomath.moc"
