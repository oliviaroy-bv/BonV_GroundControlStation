#pragma once

#include <QString>

// Fetches a single SRTM1 tile from the ArduPilot terrain mirror into a local DEM
// directory, if it isn't already there. Used synchronously (blocks the caller via a
// nested event loop) so mesh generation can rely on the tile being on disk immediately
// afterward - the rest of the terrain pipeline has no async elevation path.
class SrtmDownloader
{
public:
    // Returns true if tileBaseName (e.g. "N28E085") is available under demDirectory as
    // either <tileBaseName>.hgt or <tileBaseName>.hgt.zip afterward, whether it was
    // already cached or freshly downloaded.
    [[nodiscard]] static bool ensureTile(const QString &demDirectory, const QString &tileBaseName);
};
