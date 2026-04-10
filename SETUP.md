# CARTRIDGE32 — Phase 1 Setup & Build

## What you need before building

### 1. Extract SDL2 (you already downloaded this)
Extract `SDL2-devel-2.32.10-mingw.tar.gz` somewhere, e.g. `C:\SDL2-2.32.10\`

### 2. Edit BUILD.bat
Open `BUILD.bat` in Notepad and change this line to match where you extracted SDL2:
```
set SDL2_DIR=C:\SDL2-2.32.10\x86_64-w64-mingw32\lib\cmake\SDL2
```
Use the `x86_64` path for 64-bit builds (recommended).
Use the `i686` path only if you have a 32-bit MinGW toolchain.

### 3. Make sure you have CMake + MinGW on your PATH
- CMake: https://cmake.org/download/ — tick "Add to PATH" during install
- MinGW (w64): https://www.mingw-w64.org/ — or use MSYS2

### 4. Double-click BUILD.bat
That's it. The script will configure, build, and launch the exe automatically.

---

## What Phase 1 gives you
- A 1100×720 window with the CARTRIDGE32 title
- All 14 system color swatches rendered across the center
- F1 toggles the ImGui demo window
- Alt+F4 / close button exits cleanly

---

## File structure
```
cartridge32/
├── BUILD.bat              ← edit SDL2_DIR here, then double-click
├── BUILD_RELEASE.bat      ← same but strips console window
├── CMakeLists.txt
├── README.md
├── src/
│   ├── main.cpp
│   ├── app.h / app.cpp    ← SDL2 + ImGui window + theme
│   ├── models.h           ← Game struct, System enum
│   ├── config.h           ← colors, tile sizes, system labels
│   └── vendor/
│       ├── imgui/         ← Dear ImGui (docking branch) — included
│       └── sqlite3/       ← SQLite 3.53.0 amalgamation — included
└── assets/
    └── fonts/             ← custom fonts go here in phase 2
```

---

## Troubleshooting

**"SDL2 not found"** — SDL2_DIR in BUILD.bat is wrong. The path must point to
the folder containing `sdl2-config.cmake`, not the SDL2 root.

**"mingw32-make not found"** — MinGW bin folder isn't on your PATH.
Add `C:\mingw64\bin` (or wherever you installed it) to System Environment Variables → PATH.

**Black screen / crash on launch** — your GPU driver may not support OpenGL 3.0.
This is very rare on any machine made after 2010. Update your GPU drivers.

**Window opens but immediately closes** — run from a terminal instead of double-clicking
so you can see the error output:
```
cd path\to\cartridge32
build\bin\cartridge32.exe
```
