#!/bin/bash
set -e

echo "[CARTRIDGE32] Checking dependencies..."

check() {
    command -v "$1" > /dev/null 2>&1 || {
        echo "ERROR: $1 not found. Install with: sudo apt install $2"
        exit 1
    }
}

check cmake cmake
check gcc gcc
pkg-config --exists sdl2 || { echo "ERROR: SDL2 not found. Run: sudo apt install libsdl2-dev"; exit 1; }
pkg-config --exists libcurl || { echo "ERROR: libcurl not found. Run: sudo apt install libcurl4-openssl-dev"; exit 1; }
pkg-config --exists libpng || { echo "ERROR: libpng not found. Run: sudo apt install libpng-dev"; exit 1; }

echo "[CARTRIDGE32] Configuring..."
cmake -B build -DCMAKE_BUILD_TYPE=Debug

echo ""
echo "[CARTRIDGE32] Building..."
cmake --build build --parallel

echo ""
echo "============================================================"
echo " BUILD SUCCESSFUL"
echo " Executable: build/bin/cartridge32"
echo "============================================================"
echo ""
echo "Launching..."
./build/bin/cartridge32
