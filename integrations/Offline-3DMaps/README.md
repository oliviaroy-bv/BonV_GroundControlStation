# Drone Terrain Simulator

A Qt 6, C++, QML, and Qt Quick 3D prototype for flying ArduPilot SITL vehicles over locally cached georeferenced terrain.

## Current slice

- WGS84 geodetic, ECEF, and local ENU conversions in double precision
- Configurable XYZ or Bing quadkey imagery download and disk cache
- First-run Esri World Imagery development source with visible attribution
- Region rendered as a grid of fixed-zoom terrain tiles (Web Mercator XYZ scheme), each with its own imagery mosaic and skirt geometry to hide seams
- SRTM `.hgt`/`.hgt.zip` loading with bilinear elevation sampling, auto-downloaded from the ArduPilot SRTM1 mirror when missing locally
- Center/radius region loading capped at 1000 km radius, with automatic imagery/terrain zoom reduction for large regions
- Custom `QQuick3DGeometry` per-tile terrain mesh, normals, Mercator UVs, and `flipV: true`
- Orbit, top, side, and drone chase cameras
- Terrain picking with latitude, longitude, and DEM altitude readout
- MAVLink 1/2 decoding over simultaneous UDP and TCP transports
- Procedural quadcopter and an automatic demo flight when SITL is disconnected

## Build on Windows

```powershell
.\scripts\configure-windows.bat
ctest --test-dir build-make --output-on-failure
.\scripts\run-windows.bat
```

## SITL

Forward MAVLink telemetry to UDP port `14550`, for example through MAVProxy:

```text
output add 127.0.0.1:14550
```

The app can also connect as a TCP client. Enable **TCP client** in the SITL panel; the default endpoint is `127.0.0.1:5760`, which matches ArduPilot SITL's direct TCP endpoint when SITL is started with `--no-mavproxy`. The TCP connection automatically retries while enabled.

UDP and TCP can remain active together. Each transport has its own MAVLink stream buffer, so fragmented TCP frames and UDP datagrams are decoded independently.

The receiver accepts the first MAVLink system ID it sees. Disable **Demo flight** if the disconnected drone should remain stationary.

## Cache

All offline data lives in one fixed, predictable folder on the Desktop:

```text
Desktop/3DMapsData/
  imagery/<source-hash>/<z>/<x>/<y>.tile      raw downloaded imagery tiles
  stitched/<source-hash>/z{z}_{x}_{y}_d{n}.png  per-terrain-tile imagery mosaics
  dem/N12E077.hgt(.zip)                        SRTM elevation tiles
```

Imagery and elevation both cache under this same root, so it's one coherent offline dataset per machine, independent of where the app binary happens to run from.

SRTM tiles are fetched automatically from the [ArduPilot SRTM1 mirror](https://terrain.ardupilot.org/SRTM1/) the first time a region needs one that isn't already in `Desktop/3DMapsData/dem`. To use your own data instead, just place `.hgt` or `.hgt.zip` files there yourself first — anything already on disk is used as-is and never re-downloaded. The terrain panel shows the exact DEM folder, loaded tile count, missing tile names, and sampled elevation range.

Imagery providers must be configured with a URL whose terms permit downloading and offline caching. Supported placeholders are `{z}`, `{x}`, `{y}`, `{s}`, and `{quadkey}`.

The first run uses the public Esri World Imagery endpoint so the terrain is immediately recognizable. It is a development default, not a grant of imagery redistribution rights. Production deployments should use an account/export workflow and attribution required by the selected provider, or replace the editable URL with a self-hosted/licensed XYZ source.

Each terrain mesh tile's texture is a mosaic of finer-zoom imagery tiles (the **Detail** control in the sidebar, 0-4) so satellite clarity isn't capped by the coarser terrain mesh zoom — this improves the static baseline but does not refine as the camera zooms in; that requires real distance-based level-of-detail, not yet implemented.

The initial renderer covers one region as a grid of fixed-zoom terrain tiles (with skirts to hide seams) picked from the selected center/radius. Camera-distance-driven dynamic LOD, async tile loading, and multiple simultaneous MAVLink system IDs are the next architectural layers.
