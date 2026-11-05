#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

class ElevationProvider
{
public:
    void setDemDirectory(const QString &directory);
    [[nodiscard]] QString demDirectory() const;
    [[nodiscard]] double elevationAt(double latitude, double longitude,
                                     bool syntheticFallback = true) const;
    [[nodiscard]] bool hasCoverage(double latitude, double longitude) const;
    [[nodiscard]] QStringList tileNamesForRegion(double minLatitude, double minLongitude,
                                                 double maxLatitude, double maxLongitude) const;
    [[nodiscard]] int coverageCountForRegion(double minLatitude, double minLongitude,
                                             double maxLatitude, double maxLongitude) const;

private:
    struct Tile {
        int samplesPerSide = 0;
        QVector<qint16> samples;
    };

    [[nodiscard]] QString tileName(double latitude, double longitude) const;
    [[nodiscard]] const Tile *loadTile(const QString &name) const;
    [[nodiscard]] static double syntheticElevation(double latitude, double longitude);

    QString m_demDirectory;
    mutable QHash<QString, Tile> m_tiles;
    mutable QHash<QString, bool> m_missingTiles;
};
