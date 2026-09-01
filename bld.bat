@echo off
setlocal

REM BoatInfo local Windows package build.
REM Run from a Visual Studio Developer Command Prompt with wxWidgets available
REM to CMake (for example via WXWIN / wxWidgets_ROOT_DIR as configured locally).

set "BUILD_DIR=build-test"

if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

cmake -S . -B "%BUILD_DIR%" -G "Visual Studio 18 2026" -A Win32 -DOCPN_TARGET=MSVC
if errorlevel 1 exit /b %errorlevel%

cmake --build "%BUILD_DIR%" --target package --config RelWithDebInfo
if errorlevel 1 exit /b %errorlevel%

echo.
echo BoatInfo package build completed.
endlocal
