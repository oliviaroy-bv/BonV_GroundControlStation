@echo off
setlocal

set "QT_ROOT=C:\Qt\Qt6.10.1\6.10.1\mingw_64"
set "MINGW_ROOT=C:\Qt\Qt6.10.1\Tools\mingw1310_64"
set "MAKE=C:\Qt\Qt6.10.1\Tools\mingw1310_64\bin\mingw32-make.exe"
set "PATH=%QT_ROOT%\bin;%MINGW_ROOT%\bin;C:\Qt\Qt6.10.1\Tools\Ninja;%PATH%"

call "%QT_ROOT%\bin\qt-cmake.bat" -S "%~dp0.." -B "%~dp0..\build-make" -G "MinGW Makefiles" ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_MAKE_PROGRAM="%MAKE%" ^
  -DCMAKE_CXX_COMPILER="%MINGW_ROOT%\bin\g++.exe"

if errorlevel 1 exit /b %errorlevel%
"%MAKE%" -C "%~dp0..\build-make" -j4
