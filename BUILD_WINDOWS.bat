@echo off
setlocal
cd /d "%~dp0"

echo ========================================
echo  ZCRUSH - Windows VST3 build
echo ========================================

where cmake >nul 2>nul
if errorlevel 1 (
    echo ERROR: CMake is not installed or is not in PATH.
    echo Install CMake 3.22+ and Visual Studio 2022 with Desktop development with C++.
    pause
    exit /b 1
)

cmake -S . -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :failed

cmake --build build --config Release --parallel 2
if errorlevel 1 goto :failed

echo.
echo Build complete:
echo %CD%\build\ZCRUSH_artefacts\Release\VST3\ZCRUSH.vst3
pause
exit /b 0

:failed
echo.
echo BUILD FAILED. Read the CMake/compiler error above.
pause
exit /b 1
