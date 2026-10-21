@echo off
setlocal EnableExtensions DisableDelayedExpansion

echo ======================================
echo Building BonV Ground Station Installer
echo ======================================

:: -------------------------------------------------
:: Paths
:: -------------------------------------------------

set "ROOT=%~dp0"
for %%i in ("%ROOT%\..") do set "PROJECT=%%~fi"
if not defined DEPLOY set "DEPLOY=%PROJECT%\build\deploy"
set "PACKAGE=%ROOT%\packages\com.bonv.gcs\data"
set "INSTALLER_CONFIG=%ROOT%\config\config.xml"
set "PACKAGE_METADATA=%ROOT%\packages\com.bonv.gcs\meta\package.xml"
:: Prefer an explicit override, otherwise use the SDK configured for the build.
:: CMAKE_CACHE may be overridden when packaging a different build directory.
if not defined CMAKE_CACHE set "CMAKE_CACHE=%PROJECT%\build\CMakeCache.txt"
if not defined GSTREAMER_ROOT (
    for /f "usebackq delims=" %%v in (`powershell -NoProfile -Command "$ErrorActionPreference = 'Stop'; $root = Join-Path $env:PROJECT 'integrations/gstreamer'; if (Test-Path -LiteralPath $env:CMAKE_CACHE) { $entry = Get-Content -LiteralPath $env:CMAKE_CACHE | Select-String '^BONV_BUNDLED_GSTREAMER_ROOT:PATH=(.*)$' | Select-Object -First 1; if ($entry) { $root = $entry.Matches[0].Groups[1].Value } }; $candidates = @($root, (Join-Path $root '1.0/msvc_x86_64'), (Join-Path $root 'gstreamer/msvc_x86_64'), (Join-Path $root 'msvc_x86_64')); foreach ($candidate in $candidates) { if ((Test-Path (Join-Path $candidate 'bin')) -and (Test-Path (Join-Path $candidate 'lib/pkgconfig'))) { $candidate; break } }"`) do set "GSTREAMER_ROOT=%%v"
)
set "INSTALLER_VERSION="
set "PACKAGE_VERSION="
set "INSTALLER_SUFFIX="

for /f "usebackq delims=" %%v in (`powershell -NoProfile -Command "$config = [xml](Get-Content -LiteralPath $env:INSTALLER_CONFIG); $config.Installer.Version"`) do (
    set "INSTALLER_VERSION=%%v"
)

for /f "usebackq delims=" %%v in (`powershell -NoProfile -Command "$package = [xml](Get-Content -LiteralPath $env:PACKAGE_METADATA); $package.Package.Version"`) do (
    set "PACKAGE_VERSION=%%v"
)

if not defined INSTALLER_VERSION (
    echo ERROR: Could not read the installer version from "%INSTALLER_CONFIG%".
    exit /b 1
)

if not defined PACKAGE_VERSION (
    echo ERROR: Could not read the package version from "%PACKAGE_METADATA%".
    exit /b 1
)

if /I not "%INSTALLER_VERSION%"=="%PACKAGE_VERSION%" (
    echo ERROR: Installer version mismatch.
    echo        config.xml:  %INSTALLER_VERSION%
    echo        package.xml: %PACKAGE_VERSION%
    exit /b 1
)

:: The metadata version is the single release version. Format the installer
:: artifact suffix from it so STAGED_1.1 becomes _Staged_v1.1.
for /f "usebackq delims=" %%v in (`powershell -NoProfile -Command "$version = $env:INSTALLER_VERSION; if ($version -match '^(?i:staged)[_.-]?v?(.+)$') { 'Staged_v' + $Matches[1] } else { 'v' + $version }"`) do (
    set "INSTALLER_SUFFIX=%%v"
)

if not defined INSTALLER_SUFFIX (
    echo ERROR: Could not format the installer filename version.
    exit /b 1
)

set "INSTALLER_NAME=BonVGroundStation_%INSTALLER_SUFFIX%.exe"

:: -------------------------------------------------
:: Locate binarycreator.exe dynamically
:: -------------------------------------------------

set "BINARYCREATOR="
if defined QTIFWDIR if exist "%QTIFWDIR%\bin\binarycreator.exe" (
    set "BINARYCREATOR=%QTIFWDIR%\bin\binarycreator.exe"
    goto FoundBinaryCreator
)

if not defined QTIFW_ROOT if defined QTDIR if exist "%QTDIR%\Tools\QtInstallerFramework" set "QTIFW_ROOT=%QTDIR%\Tools\QtInstallerFramework"
if not defined QTIFW_ROOT set "QTIFW_ROOT=C:\Qt\Tools\QtInstallerFramework"

:: Look for the newest installed Qt Installer Framework version
for /f "delims=" %%i in ('dir /b /ad /o-n "%QTIFW_ROOT%" 2^>nul') do (
    if exist "%QTIFW_ROOT%\%%i\bin\binarycreator.exe" (
        set "BINARYCREATOR=%QTIFW_ROOT%\%%i\bin\binarycreator.exe"
        goto FoundBinaryCreator
    )
)

:: Fallback: search PATH
for /f "delims=" %%i in ('where binarycreator.exe 2^>nul') do (
    set "BINARYCREATOR=%%i"
    goto FoundBinaryCreator
)

:FoundBinaryCreator

if not defined BINARYCREATOR (
    echo.
    echo ERROR: Could not find binarycreator.exe
    echo.
    echo Please install Qt Installer Framework.
    pause
    exit /b 1
)

echo Using:
echo %BINARYCREATOR%
echo.

:: -------------------------------------------------
:: Check inputs before cleaning the previous package
:: -------------------------------------------------

if not exist "%DEPLOY%\BonVGroundStation.exe" (
    echo ERROR: Deployment is missing BonVGroundStation.exe: "%DEPLOY%"
    echo Build the Release configuration first so CMake can populate the deployment directory.
    exit /b 1
)

:: Check before modifying the previous package. windeployqt does not remove
:: Debug DLLs left in a shared deployment folder by an earlier Debug build.
if exist "%DEPLOY%\Qt6Cored.dll" (
    echo ERROR: Debug Qt runtime found: "%DEPLOY%\Qt6Cored.dll"
    if exist "%DEPLOY%\Qt6Core.dll" echo Both Release and Debug Qt runtimes are present; the deployment is mixed.
    echo Rename the deployment folder as a backup, then rebuild Release into a fresh folder.
    echo Recreate it before rebuilding: cmake -E make_directory "%DEPLOY%"
    echo From the project root, run: cmake --build build --config Release --clean-first
    echo A normal incremental build may skip the post-build deployment step.
    echo For a custom DEPLOY path, ensure BONV_DEPLOY_DIR in CMake points to the same folder.
    exit /b 1
)

if not defined GSTREAMER_ROOT (
    echo ERROR: Could not resolve the GStreamer SDK from "%CMAKE_CACHE%".
    echo Set GSTREAMER_ROOT to the MSVC x64 SDK used to build the deployment.
    exit /b 1
)
if not exist "%GSTREAMER_ROOT%\lib\gstreamer-1.0\*.dll" (
    echo ERROR: No source GStreamer plugin DLLs found in "%GSTREAMER_ROOT%\lib\gstreamer-1.0".
    echo Set GSTREAMER_ROOT to the MSVC x64 SDK used to build the deployment.
    exit /b 1
)
echo Validating against GStreamer SDK: "%GSTREAMER_ROOT%"

echo Cleaning package directory...
if exist "%PACKAGE%" rd /s /q "%PACKAGE%"
if exist "%PACKAGE%" (
    echo ERROR: Could not clean package directory: "%PACKAGE%"
    exit /b 1
)
mkdir "%PACKAGE%"
if errorlevel 1 exit /b 1

echo Copying deployment files...
xcopy "%DEPLOY%\*" "%PACKAGE%\" /E /I /Y >nul
if errorlevel 1 (
    echo ERROR: Could not copy the complete deployment payload.
    exit /b 1
)

echo Removing debug files...

:: Remove debug symbol files
del /S /Q "%PACKAGE%\*.pdb" >nul 2>&1

:: Do not use a wildcard such as *d.dll here.  Valid release files such as
:: Qt6Quick3D.dll, gstd3d.dll, and gstdav1d.dll match it and are required at
:: runtime.  A Release deployment must be created with windeployqt --release.
if exist "%PACKAGE%\Qt6Cored.dll" (
    echo ERROR: Debug Qt runtime detected. Package only a Release deployment.
    exit /b 1
)

echo Validating standalone runtime payload...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference = 'Stop'; $package = $env:PACKAGE; $gstreamer = $env:GSTREAMER_ROOT; " ^
  "$required = @('BonVGroundStation.exe', 'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Qml.dll', 'Qt6Quick.dll', 'Qt6WebEngineCore.dll', 'QtWebEngineProcess.exe', 'vcruntime140.dll', 'msvcp140.dll', 'platforms/qwindows.dll', 'tls/qopensslbackend.dll', 'libexec/gstreamer-1.0/gst-plugin-scanner.exe'); " ^
  "$missing = $required | Where-Object { -not (Test-Path (Join-Path $package $_)) }; " ^
  "$sourcePlugins = @(Get-ChildItem (Join-Path $gstreamer 'lib/gstreamer-1.0') -Filter '*.dll' -File | Select-Object -ExpandProperty Name); if ($sourcePlugins.Count -eq 0) { throw 'No source GStreamer plugins were found.' }; " ^
  "$packagedPlugins = Get-ChildItem (Join-Path $package 'lib/gstreamer-1.0') -Filter '*.dll' -File -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name; " ^
  "$missingPlugins = $sourcePlugins | Where-Object { $_ -notin $packagedPlugins }; " ^
  "if ($missing -or $missingPlugins) { Write-Host 'ERROR: Deployment payload is incomplete.'; if ($missing) { Write-Host ('Missing required runtime files: ' + ($missing -join ', ')) }; if ($missingPlugins) { Write-Host ('Missing GStreamer plugins: ' + ($missingPlugins -join ', ')) }; exit 1 }; " ^
  "Write-Host ('Runtime validation passed: ' + $packagedPlugins.Count + ' GStreamer plugins.');"
if errorlevel 1 exit /b 1
:: -------------------------------------------------
:: Build installer
:: -------------------------------------------------

echo Creating installer...

"%BINARYCREATOR%" ^
-c "%ROOT%\config\config.xml" ^
-p "%ROOT%\packages" ^
"%PROJECT%\%INSTALLER_NAME%"

if errorlevel 1 (
    echo.
    echo Installer creation failed.
    pause
    exit /b 1
)

echo.
echo ======================================
echo Installer created successfully!
echo ======================================
echo.
echo Output:
echo %PROJECT%\%INSTALLER_NAME%
echo.

pause
