# BonVGCS Developer Guide

This project is a C++17 Qt 6+ desktop application built with CMake. It uses Qt Quick/QML for the UI, Qt WebEngine for the map view, GStreamer and Qt Multimedia for video, and vendored MAVLink C headers from `MAVLinkStore/c_library_v2`.

## Index

1. [Prerequisites](#prerequisites)
2. [Clone The Repository](#clone-the-repository)
3. [Verify The Toolchain](#verify-the-toolchain)
4. [Bundled GStreamer](#bundled-gstreamer)
5. [Build With Ninja](#build-with-ninja)
6. [Deploy Qt Runtime Files](#deploy-qt-runtime-files)
7. [Run The App](#run-the-app)
8. [Visual Studio Generator Alternative](#visual-studio-generator-alternative)
9. [Build And Run From Existing Build Directory](#build-and-run-from-existing-build-directory)
10. [VS Code Notes](#vs-code-notes)
11. [Runtime Behavior](#runtime-behavior)
12. [Common Issues](#common-issues)
    - [CMake Cannot Find Qt6](#cmake-cannot-find-qt6)
    - [`cl` Is Not Recognized](#cl-is-not-recognized)
    - [The App Opens But The UI Does Not Load](#the-app-opens-but-the-ui-does-not-load)
    - [Missing Qt DLLs Or Platform Plugin Errors](#missing-qt-dlls-or-platform-plugin-errors)
    - [Map View Is Blank](#map-view-is-blank)
    - [CMake Cannot Find GDAL](#cmake-cannot-find-gdal)

<a id="prerequisites"></a>
## 1. Prerequisites

Install the following on a clean Windows machine:

1. **Git**
   - Used to clone the repository.

2. **Visual Studio 2022** or **Visual Studio 2022 Build Tools**
   - Select the **Desktop development with C++** workload.
   - Make sure the MSVC x64 compiler and Windows SDK are installed.

3. **Qt 6 for MSVC 2022 64-bit**
   - Point `QT_MSVC_ROOT` at your Qt MSVC kit before configuring:

     ```powershell
     $env:QT_MSVC_ROOT = "<path-to-your-msvc2022_64-qt-kit>"
     ```

   - Install these Qt components/modules:
     - Qt Core
     - Qt Gui
     - Qt Qml
     - Qt Quick
     - Qt Network
     - Qt Location
     - Qt Positioning
     - Qt Multimedia
     - Qt Svg
     - Qt WebEngine / WebEngineQuick

   The CMake examples below use `QT_MSVC_ROOT` so the project does not depend on a fixed Qt install folder.

4. **CMake**
   - Version 3.16 or newer.

5. **Ninja**
   - Recommended for the command-line build flow.
   - You can also use the Visual Studio CMake generator if Ninja is not installed.

6. **GStreamer**
   - Recommended project layout is to bundle the MSVC x86_64 GStreamer SDK in `GStreamer`.
   - If that folder is present, CMake uses it automatically and copies the runtime files next to the built executable.
   - If it is not present, CMake falls back to a system GStreamer installation found through `pkg-config`.

7. **GDAL** (optional)
   - GDAL is only required for GeoTIFF raster map import.
   - If GDAL is not installed, CMake still configures and the app builds without the raster import worker.
   - To require GeoTIFF support, install GDAL and configure with `-DBONV_REQUIRE_GDAL=ON` plus `GDAL_DIR` or a `CMAKE_PREFIX_PATH` entry that points to GDAL.

<a id="clone-the-repository"></a>
## 2. Clone The Repository

```powershell
git clone <repository-url>
cd bonv-gcs
```

Replace `<repository-url>` with the actual Git remote URL.

<a id="verify-the-toolchain"></a>
## 3. Verify The Toolchain

Open **x64 Native Tools Command Prompt for VS 2022** or **Developer PowerShell for VS 2022**.

From the repository root, check that the required tools are visible:

```powershell
git --version
cmake --version
ninja --version
cl
```

`cl` should print Microsoft C/C++ compiler information. It may end with an error about no input files; that is fine.

<a id="bundled-gstreamer"></a>
## 4. Bundled GStreamer

To make video streaming work for a new developer immediately after cloning, include the GStreamer SDK in the repository under `GStreamer`.

Use one of these layouts:

```text
GStreamer/bin
GStreamer/lib/pkgconfig
GStreamer/lib/gstreamer-1.0
GStreamer/libexec/gstreamer-1.0
```

or the official installer layout:

```text
GStreamer/gstreamer/1.0/msvc_x86_64/bin
GStreamer/gstreamer/1.0/msvc_x86_64/lib/pkgconfig
GStreamer/gstreamer/1.0/msvc_x86_64/lib/gstreamer-1.0
GStreamer/gstreamer/1.0/msvc_x86_64/libexec/gstreamer-1.0
```

Use the MSVC x86_64 package when building this project with Visual Studio/MSVC. After configure, CMake adds the bundled SDK's `lib/pkgconfig` folder to `PKG_CONFIG_PATH`, links against the bundled headers/libs, and copies GStreamer DLLs, plugins, and plugin scanner files into the build output.

If the SDK is stored somewhere else, pass its root explicitly:

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="$env:QT_MSVC_ROOT" `
  -DBONV_BUNDLED_GSTREAMER_ROOT="<path-to-gstreamer-sdk>"
```

<a id="build-with-ninja"></a>
## 5. Build With Ninja

From the repository root:

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="$env:QT_MSVC_ROOT"

cmake --build build
```

Set `QT_MSVC_ROOT` to the Qt kit you want to build against.

<a id="deploy-qt-runtime-files"></a>
## 6. Deploy Qt Runtime Files

The CMake build locates `windeployqt` from the configured Qt kit and deploys Qt DLLs, QML imports, WebEngine files, plugins, and runtime dependencies into `build/deploy`.

```powershell
cmake --build build --config Release
```

The CMake build copies bundled GStreamer runtime files automatically when GStreamer is available.

<a id="run-the-app"></a>
## 7. Run The App

Run the deployed executable:

```powershell
.\build\deploy\BonVGroundStation.exe
```

The main QML files are compiled into Qt resources, so the app no longer depends on the current working directory to find the UI.

<a id="visual-studio-generator-alternative"></a>
## 8. Visual Studio Generator Alternative

If you prefer not to use Ninja:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="$env:QT_MSVC_ROOT" `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_MANIFEST_MODE=ON

cmake --build build --config Release
```

Deployment is handled by the post-build step. Launch the deployed executable:

```powershell
.\build\deploy\BonVGroundStation.exe
```

<a id="build-and-run-from-existing-build-directory"></a>
## 9. Build And Run From Existing Build Directory

If CMake has already been configured and the `build` directory already exists, you can rebuild and run the app using:

```powershell
cd build

cmake --build . --config Release

.\deploy\BonVGroundStation.exe
```

This rebuilds the project using the existing Visual Studio CMake configuration and launches the Release executable. If `CMakeLists.txt` or `vcpkg.json` changed, reconfigure first so vcpkg can install the required OpenSSL 3 runtime:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DVCPKG_MANIFEST_MODE=ON
```

<a id="vs-code-notes"></a>
## 10. VS Code Notes

The repository contains `.vscode/settings.json` with:

```json
{
  "cmake.configureSettings": {
    "CMAKE_PREFIX_PATH": "${env:QT_MSVC_ROOT}",
    "CMAKE_TOOLCHAIN_FILE": "${env:VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
  }
}
```

Set `QT_MSVC_ROOT` and `VCPKG_ROOT` in your shell or user environment before configuring from VS Code. The repository's `vcpkg.json` installs OpenSSL 3, which is deployed with the Qt TLS backend for the release installer.

Recommended VS Code extensions:

- C/C++
- CMake Tools
- Qt QML

<a id="runtime-behavior"></a>
## 11. Runtime Behavior

- The app's MAVLink connection button connects to:

  ```text
  127.0.0.1:5762
  ```

  This is hard-coded in `ui/TopPanel.qml`.

- The 2D map uses Qt WebEngine and MapTiler. Internet is required for uncached map styles and tiles. Loaded map data is stored in `Documents/BonV GCS/Map Tile Cache`; the last 2D map view is restored on the next launch so cached tiles remain usable offline. For reliable offline operation, load the mission while online and use **Settings > Map > Mission Area Cache > Download** before enabling **Offline Maps**. Offline mode can only display the selected style, area, and zoom levels that were cached.

- RTSP video is optional. The stream URL can be entered in the app UI.

<a id="common-issues"></a>
## 12. Common Issues

<a id="cmake-cannot-find-qt6"></a>
### 12.1 CMake Cannot Find Qt6

Make sure `CMAKE_PREFIX_PATH` points to the Qt MSVC 2022 64-bit installation:

```powershell
-DCMAKE_PREFIX_PATH="$env:QT_MSVC_ROOT"
```

Do not point it at the top-level Qt folder; point it at the specific MSVC kit directory.

<a id="cl-is-not-recognized"></a>
### 12.2 `cl` Is Not Recognized

Use **x64 Native Tools Command Prompt for VS 2022** or **Developer PowerShell for VS 2022**. A normal PowerShell window usually does not have the MSVC compiler environment configured.

<a id="the-app-opens-but-the-ui-does-not-load"></a>
### 12.3 The App Opens But The UI Does Not Load

Run the deployed executable:

```powershell
.\build\deploy\BonVGroundStation.exe
```

<a id="missing-qt-dlls-or-platform-plugin-errors"></a>
### 12.4 Missing Qt DLLs Or Platform Plugin Errors

Rebuild the project so the post-build `windeployqt` step can refresh `build/deploy`, then launch the deployed executable.

<a id="map-view-is-blank"></a>
### 12.5 Map View Is Blank

Check that:

- Qt WebEngine was installed.
- `windeployqt` completed successfully.
- The machine has internet access, or the selected style and visible area have already been cached.
- Firewall/proxy settings allow the app to reach MapTiler and the MapLibre CDN.

<a id="cmake-cannot-find-gdal"></a>
### 12.6 CMake Cannot Find GDAL

GDAL is optional. A clean clone can configure without it; GeoTIFF raster map import will be disabled.

If you need GeoTIFF import, install a Windows GDAL package that provides `GDALConfig.cmake`, then configure with one of these options:

```powershell
-DGDAL_DIR="<path-to-gdal>/lib/cmake/gdal"
```

or include the GDAL install prefix with Qt:

```powershell
-DCMAKE_PREFIX_PATH="$env:QT_MSVC_ROOT;<path-to-gdal>"
```

Use `-DBONV_REQUIRE_GDAL=ON` when you want CMake to fail if GDAL is missing.
