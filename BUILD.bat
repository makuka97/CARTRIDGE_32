@echo off
setlocal

set SDL2_DIR=C:\msys64\ucrt64\lib\cmake\SDL2

echo.
echo [CARTRIDGE32] Checking for cmake...
where cmake >nul 2>&1
if errorlevel 1 ( echo ERROR: cmake not found. & pause & exit /b 1 )

echo [CARTRIDGE32] Checking for MinGW...
where mingw32-make >nul 2>&1
if errorlevel 1 ( echo ERROR: mingw32-make not found. & pause & exit /b 1 )

echo [CARTRIDGE32] Configuring...
cmake -B build -G "MinGW Makefiles" ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe ^
    -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe ^
    -DSDL2_DIR="%SDL2_DIR%"

if errorlevel 1 ( echo ERROR: CMake failed. & pause & exit /b 1 )

echo.
echo [CARTRIDGE32] Building...
cmake --build build --parallel

if errorlevel 1 ( echo ERROR: Build failed. & pause & exit /b 1 )

REM Copy SDL2.dll if needed
if not exist "build\bin\SDL2.dll" (
    copy C:\msys64\ucrt64\bin\SDL2.dll build\bin\ >nul 2>&1
)

REM Copy native emulators next to exe
echo [CARTRIDGE32] Copying emulators...
if exist "native" (
    xcopy /E /I /Y /Q "native" "build\bin\native\" >nul
    echo Emulators copied to build\bin\native\
) else (
    echo WARNING: native\ folder not found - emulators will not be available
)

echo.
echo ============================================================
echo  BUILD SUCCESSFUL
echo  Executable: build\bin\cartridge32.exe
echo ============================================================
echo.
echo Launching...
build\bin\cartridge32.exe

endlocal
