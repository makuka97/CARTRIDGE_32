#pragma once
#include <string>
#include <cstdint>

// ── System identifiers ────────────────────────────────────────────────────────
enum class System : uint8_t {
    Unknown = 0,
    NES,
    SNES,
    N64,
    GB,
    GBC,
    GBA,
    Genesis,
    SMS,
    Atari2600,
    Atari7800,
    Lynx,
    PCE,
    C64,
    DOS,
    NDS,        // Nintendo DS (DeSmuME)
    PS1,        // PlayStation 1 (ePSXe)
    GG,         // Sega Game Gear (Mednafen)
    NGP,        // Neo Geo Pocket / Color (Mednafen)
    WS,         // WonderSwan / Color (Mednafen)
    VB,         // Virtual Boy (Mednafen)
    Saturn,     // Sega Saturn (Yabause — add when available)
};

// ── Core game record (mirrors the SQLite schema) ──────────────────────────────
struct Game {
    int64_t     id         = 0;
    std::string name;
    std::string rom_path;
    System      system     = System::Unknown;
    std::string boxart;     // local path to downloaded box art, may be empty
    std::string dos_conf;   // DOSBox .conf path, only set for DOS games
    int64_t     play_count = 0;
    std::string added_at;   // ISO8601 timestamp string

    // Convenience: is this record fully loaded from DB
    bool valid() const { return id > 0 && !name.empty(); }
};
