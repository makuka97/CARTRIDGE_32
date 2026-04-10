# Adding Emulators to CARTRIDGE32

Adding a new emulator takes 5 minutes and touches exactly 5 files.

---

## Current Emulators

| System | Folder | Exe | Notes |
|--------|--------|-----|-------|
| NES | `native/nestopia/` | `nestopia.exe` | Primary NES |
| NES (alt) | `native/fceux/` | `fceux.exe` | Bundled, swap in launcher.cpp if preferred |
| SNES | `native/snes9x/` | `snes9x.exe` | |
| Nintendo 64 | `native/mupen64/` | `mupen64plus-ui-console.exe` | Needs --plugindir |
| Nintendo DS | `native/desmume/` | `DeSmuME_0.9.11_x86.exe` | |
| Game Boy / GBC / GBA | `native/mgba/` | `mGBA.exe` | All three in one |
| PlayStation 1 | `native/epsxe/` | `ePSXe.exe` | Needs BIOS in epsxe/bios/ |
| Sega Genesis / SMS | `native/gens/` | `gens.exe` | |
| Sega Saturn | — | — | Download Yabause, uncomment in launcher.cpp |
| Atari 2600 / 7800 | `native/stella/` | `Stella.exe` | |
| Lynx / TurboGrafx-16 / Game Gear / NGP / WonderSwan / Virtual Boy | `native/mednafen/` | `mednafen.exe` | Multi-system |
| Commodore 64 | `native/vice/bin/` | `x64sc.exe` | |
| MS-DOS | `native/dosbox/` | `DOSBox.exe` | Zip import flow |

---

## How to Add a New Emulator

### Step 1 — Drop the emulator folder into `native/`
```
native/
  myemulator/
    myemulator.exe
    (any required DLLs/data files)
```

### Step 2 — Add the System enum value (`src/models.h`)
```cpp
enum class System : uint8_t {
    // ... existing ...
    MySystem,   // My new system
};
```

### Step 3 — Add color + label + ordering (`src/config.h`)
```cpp
// In SYSTEM_ORDER array — increase the array size by 1:
System::MySystem,

// In SYSTEM_COLORS map:
{System::MySystem, {0.80f, 0.30f, 0.10f, 1.0f}},  // your colour

// In SYSTEM_LABELS map:
{System::MySystem, "My System"},
```

### Step 4 — Add file extensions (`src/importer.cpp`)
```cpp
static const std::unordered_map<std::string, System> EXT_MAP = {
    // ... existing ...
    {"ext",  System::MySystem},   // e.g. {"rom", System::MySystem}
};
```

### Step 5 — Add the emulator mapping (`src/launcher.cpp`)
```cpp
static const std::unordered_map<System, EmulatorInfo> EMULATORS = {
    // ... existing ...
    { System::MySystem, {"myemulator", "myemulator.exe"} },
};
```

### Step 6 — Add the Libretro box art repo (`src/scraper.cpp`)
Find the repo name at: https://github.com/libretro-thumbnails
```cpp
static const std::unordered_map<System, std::string> LIBRETRO_REPOS = {
    // ... existing ...
    { System::MySystem, "Publisher_-_System_Name" },
};
```

That's it. Rebuild and the new system appears in the sidebar automatically
as soon as you import a ROM with one of its extensions.

---

## Special Launch Arguments

If your emulator needs special command-line flags, edit the `launch()` function
in `src/launcher.cpp`. Look for the N64 example:

```cpp
} else if (game.system == System::N64) {
    cmd = "\"" + exe + "\""
          " --plugindir \"" + plugin_dir + "\""
          " --datadir \"" + plugin_dir + "\""
          " \"" + game.rom_path + "\"";
```

Add a similar block for your system before the final `else` clause.

---

## Swapping NES Emulator (Nestopia → FCEUX)

FCEUX is bundled but Nestopia is wired by default. To swap:

In `src/launcher.cpp`, change:
```cpp
{ System::NES, {"nestopia", "nestopia.exe"} },
```
to:
```cpp
{ System::NES, {"fceux", "fceux.exe"} },
```

---

## Yabause (Sega Saturn) — When You Get the Binary

1. Install Yabause, copy the installed folder to `native/yabause/`
2. In `src/launcher.cpp`, uncomment:
   ```cpp
   // { System::Saturn, {"yabause", "yabause.exe"} },
   ```
   Remove the `//` and rebuild. Done.
