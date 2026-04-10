CARTRIDGE32

A retro game launcher for Windows built in C++. Browse your ROM collection as a tile grid, scrape box art automatically, and launch games directly into their emulators.

Runs on Windows XP and above.

---

 Stack

- C++17
- Dear ImGui (docking branch)
- SDL2 + OpenGL 3.0
- SQLite3
- WinINet (box art scraping)
- WIC / Windows Imaging Component (texture loading)

---
Building

**Requirements**

- CMake 3.16+
- MinGW-w64 (via MSYS2 ucrt64 recommended)
- SDL2 development libraries

**Steps**

```bat
cmake -B build -G "MinGW Makefiles" ^
    -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe ^
    -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe ^
    -DSDL2_DIR="C:/msys64/ucrt64/lib/cmake/SDL2"

cmake --build build --parallel
```

Or just double-click `BUILD.bat` after setting your `SDL2_DIR` at the top of the file.

The emulator binaries are not included in this repo. Place them in `native/` next to the exe following the layout described in `ADDING_EMULATORS.md`.

---

 Features

- Tile grid library with sidebar system filters and live search
- ROM import via file dialog — system detected from file extension
- MS-DOS games imported from zip, executable selected via picker modal, DOSBox conf generated automatically
- Box art scraped from the Libretro CDN in background threads, cached locally
- Select and delete multiple games at once
- Detail modal with keyboard shortcuts: Enter to launch, Delete to remove, Escape to close

---

 Supported Systems

| System | Emulator |
|--------|----------|
| NES | Nestopia |
| Super Nintendo | Snes9x |
| Nintendo 64 | Mupen64Plus |
| Nintendo DS | DeSmuME |
| Game Boy / Color / Advance | mGBA |
| PlayStation 1 | ePSXe |
| Sega Genesis / Master System | Gens GS |
| Atari 2600 / 7800 | Stella |
| Lynx / TurboGrafx-16 / Game Gear / Neo Geo Pocket / WonderSwan / Virtual Boy | Mednafen |
| Commodore 64 | VICE |
| MS-DOS | DOSBox |

See `ADDING_EMULATORS.md` for instructions on adding new systems.

---

 Project Structure

```
src/
  app.cpp / app.h          SDL2 + ImGui init, main loop
  db.cpp / db.h            SQLite layer
  importer.cpp / .h        ROM import, system detection, DOS zip handling
  launcher.cpp / .h        Emulator subprocess spawning
  scraper.cpp / .h         Box art background thread, Libretro CDN
  texture_cache.cpp / .h   WIC PNG loader, OpenGL LRU texture cache
  models.h                 Game struct, System enum
  config.h                 Colors, tile sizes, system labels
  screens/
    library.cpp / .h       Full UI: sidebar, tile grid, modals
  vendor/
    imgui/                 Dear ImGui (docking branch)
    sqlite3/               SQLite amalgamation
```

---

 License

MIT
