@echo off
setlocal

set "QT_ROOT=C:\Qt\Qt6.10.1\6.10.1\mingw_64"
set "MINGW_ROOT=C:\Qt\Qt6.10.1\Tools\mingw1310_64"
set "PATH=%QT_ROOT%\bin;%MINGW_ROOT%\bin;%PATH%"

"%~dp0..\build-make\DroneTerrainSim.exe" %*
