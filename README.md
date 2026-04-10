# CARTRIDGE32 — C++ Rewrite

Retro game launcher for Windows XP and above.
Built with: C++17 · Dear ImGui · SDL2 · OpenGL 3.0 · SQLite3

## Quick Start

1. Clone this repo
2. Follow `src/vendor/SETUP.md` to populate dependencies
3. Build:

```bat
cmake -B build -G "MinGW Makefiles" -DSDL2_DIR="C:/SDL2/cmake"
cmake --build build --parallel
build\bin\cartridge32.exe
```

## Phase Roadmap

| Phase | Status      | Description                                    |
|-------|-------------|------------------------------------------------|
| 1     | ✅ Done     | CMake scaffold · SDL2 + ImGui window · theme   |
| 2     | 🔜 Next     | SQLite layer · models · Library screen tiles   |
| 3     | ⏳ Planned  | ROM import · drag-drop · system detection      |
| 4     | ⏳ Planned  | Emulator launcher · per-system subprocess      |
| 5     | ⏳ Planned  | Box art scraper · background thread            |
| 6     | ⏳ Planned  | DOS picker modal · DOSBox config generation    |
| 7     | ⏳ Planned  | Search · select/delete · polish                |

## Supported Systems

NES · Super Nintendo · Nintendo 64 · Game Boy / Color / Advance ·
Sega Genesis · Sega Master System · Atari 2600 / 7800 · Atari Lynx ·
TurboGrafx-16 · Commodore 64 · MS-DOS

## Controls

| Key     | Action                      |
|---------|-----------------------------|
| F1      | Toggle ImGui demo window    |
| Ctrl+O  | Open ROM (Phase 3+)         |
| Alt+F4  | Quit                        |
