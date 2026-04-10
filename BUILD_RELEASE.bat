@echo off
setlocal

REM Path to SDL2 cmake folder — change this to your actual path
set SDL2_DIR=C:\SDL2-2.32.10\x86_64-w64-mingw32\lib\cmake\SDL2

echo [CARTRIDGE32] Configuring Release build...
cmake -B build_release -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DSDL2_DIR="%SDL2_DIR%"

if errorlevel 1 ( echo ERROR: CMake failed. & pause & exit /b 1 )

echo [CARTRIDGE32] Building Release...
cmake --build build_release --parallel

if errorlevel 1 ( echo ERROR: Build failed. & pause & exit /b 1 )

echo.
echo BUILD SUCCESSFUL: build_release\bin\cartridge32.exe
pause
endlocal
