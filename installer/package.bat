@echo off
setlocal EnableDelayedExpansion

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
set "GSTREAMER_ROOT=%PROJECT%\integrations\gstreamer\1.0\msvc_x86_64"

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

@REM set BINARYCREATOR=

@REM if defined QTIFWDIR (
@REM     if exist "%QTIFWDIR%\bin\binarycreator.exe" (
@REM         set "BINARYCREATOR=%QTIFWDIR%\bin\binarycreator.exe"
@REM         goto FoundBinaryCreator
@REM     )
@REM )

@REM for /f "delims=" %%i in ('where binarycreator.exe 2^>nul') do (
@REM     set "BINARYCREATOR=%%i"
@REM     goto FoundBinaryCreator
@REM )

@REM if defined QTIFWDIR (
@REM     for /f "delims=" %%i in ('dir /b /ad /o-n "%QTIFWDIR%" 2^>nul') do (
@REM         if exist "%QTIFWDIR%\%%i\bin\binarycreator.exe" (
@REM             set "BINARYCREATOR=%QTIFWDIR%\%%i\bin\binarycreator.exe"
@REM             goto FoundBinaryCreator
@REM         )
@REM     )
@REM )

@REM if defined QTDIR (
@REM     for /f "delims=" %%i in ('dir /b /ad /o-n "%QTDIR%\Tools\QtInstallerFramework" 2^>nul') do (
@REM         if exist "%QTDIR%\Tools\QtInstallerFramework\%%i\bin\binarycreator.exe" (
@REM             set "BINARYCREATOR=%QTDIR%\Tools\QtInstallerFramework\%%i\bin\binarycreator.exe"
@REM             goto FoundBinaryCreator
@REM         )
@REM     )
@REM )

set "QTIFW_ROOT=C:\Qt\Tools\QtInstallerFramework"
set "BINARYCREATOR="

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
:: Clean package directory
:: -------------------------------------------------

echo Cleaning package directory...

if exist "%PACKAGE%" rd /s /q "%PACKAGE%"

mkdir "%PACKAGE%"

:: -------------------------------------------------
:: Copy deployment
:: -------------------------------------------------

echo Copying deployment files...

if not exist "%DEPLOY%\BonVGroundStation.exe" (
    echo ERROR: Deployment is missing BonVGroundStation.exe: "%DEPLOY%"
    echo Build the Release configuration first so CMake can populate the deployment directory.
    exit /b 1
)

if not exist "%GSTREAMER_ROOT%\bin" (
    echo ERROR: Bundled MSVC x64 GStreamer runtime was not found: "%GSTREAMER_ROOT%"
    exit /b 1
)

xcopy "%DEPLOY%\*" "%PACKAGE%\" /E /I /Y >nul

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
  "$package = $env:PACKAGE; $gstreamer = $env:GSTREAMER_ROOT; " ^
  "$required = @('BonVGroundStation.exe', 'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Qml.dll', 'Qt6Quick.dll', 'Qt6WebEngineCore.dll', 'QtWebEngineProcess.exe', 'vcruntime140.dll', 'msvcp140.dll', 'platforms/qwindows.dll', 'tls/qopensslbackend.dll', 'libexec/gstreamer-1.0/gst-plugin-scanner.exe'); " ^
  "$missing = $required | Where-Object { -not (Test-Path (Join-Path $package $_)) }; " ^
  "$sourcePlugins = Get-ChildItem (Join-Path $gstreamer 'lib/gstreamer-1.0') -Filter '*.dll' -File | Select-Object -ExpandProperty Name; " ^
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
