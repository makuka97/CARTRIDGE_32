#pragma once
#include "models.h"
#include <unordered_map>
#include <string>
#include <array>

// ── Window / UI layout ────────────────────────────────────────────────────────
namespace Config {

constexpr int   WINDOW_W       = 1100;
constexpr int   WINDOW_H       = 720;
constexpr int   TILE_W         = 120;
constexpr int   TILE_H         = 160;
constexpr int   TILE_GAP       = 10;
constexpr int   SIDEBAR_W      = 180;
constexpr float TILE_ROUNDING  = 6.0f;
constexpr float FONT_SIZE_UI   = 14.0f;
constexpr float FONT_SIZE_SM   = 12.0f;
constexpr float FONT_SIZE_LG   = 18.0f;

// ── Colors (RGBA packed as 0xRRGGBBAA for ImGui) ─────────────────────────────
// ImGui uses 0xAABBGGRR in hex literals via IM_COL32, but we store as floats.
// These are defined as ImVec4-compatible {r,g,b,a} in 0..1 range.

struct Color { float r, g, b, a = 1.0f; };

constexpr Color BG_BASE       = {0.961f, 0.961f, 0.953f, 1.0f}; // #f5f5f3
constexpr Color BG_SURFACE    = {1.000f, 1.000f, 1.000f, 1.0f}; // #ffffff
constexpr Color BG_SIDEBAR    = {0.941f, 0.941f, 0.933f, 1.0f}; // #f0f0ee
constexpr Color BG_HOVER      = {0.910f, 0.910f, 0.898f, 1.0f}; // #e8e8e5
constexpr Color BG_ACTIVE     = {0.878f, 0.878f, 0.863f, 1.0f}; // #e0e0dc
constexpr Color COLOR_BORDER  = {0.847f, 0.847f, 0.831f, 1.0f}; // #d8d8d4
constexpr Color COLOR_TEXT    = {0.102f, 0.102f, 0.094f, 1.0f}; // #1a1a18
constexpr Color COLOR_MUTED   = {0.533f, 0.533f, 0.518f, 1.0f}; // #888884

// ── Per-system accent colors ──────────────────────────────────────────────────
inline const std::unordered_map<System, Color> SYSTEM_COLORS = {
    { System::NES,       {0.898f, 0.224f, 0.208f, 1.0f} }, // #e53935
    { System::SNES,      {0.427f, 0.310f, 0.761f, 1.0f} }, // #6d4fc2
    { System::N64,       {0.753f, 0.224f, 0.169f, 1.0f} }, // #c0392b
    { System::GB,        {0.333f, 0.545f, 0.184f, 1.0f} }, // #558b2f
    { System::GBC,       {0.180f, 0.490f, 0.196f, 1.0f} }, // #2e7d32
    { System::GBA,       {0.118f, 0.533f, 0.898f, 1.0f} }, // #1e88e5
    { System::Genesis,   {0.086f, 0.396f, 0.753f, 1.0f} }, // #1565c0
    { System::SMS,       {0.009f, 0.467f, 0.741f, 1.0f} }, // #0277bd
    { System::Atari2600, {0.902f, 0.318f, 0.000f, 1.0f} }, // #e65100
    { System::Atari7800, {0.749f, 0.212f, 0.047f, 1.0f} }, // #bf360c
    { System::Lynx,      {0.416f, 0.106f, 0.604f, 1.0f} }, // #6a1b9a
    { System::PCE,       {0.000f, 0.514f, 0.561f, 1.0f} }, // #00838f
    { System::C64,       {0.475f, 0.333f, 0.282f, 1.0f} }, // #795548
    { System::DOS,       {0.290f, 0.620f, 0.290f, 1.0f} }, // #4a9e4a
    { System::Unknown,   {0.533f, 0.533f, 0.518f, 1.0f} }, // gray
};

// ── Per-system display labels ─────────────────────────────────────────────────
inline const std::unordered_map<System, std::string> SYSTEM_LABELS = {
    { System::NES,       "NES" },
    { System::SNES,      "Super Nintendo" },
    { System::N64,       "Nintendo 64" },
    { System::GB,        "Game Boy" },
    { System::GBC,       "Game Boy Color" },
    { System::GBA,       "Game Boy Advance" },
    { System::Genesis,   "Sega Genesis" },
    { System::SMS,       "Sega Master System" },
    { System::Atari2600, "Atari 2600" },
    { System::Atari7800, "Atari 7800" },
    { System::Lynx,      "Atari Lynx" },
    { System::PCE,       "TurboGrafx-16" },
    { System::C64,       "Commodore 64" },
    { System::DOS,       "MS-DOS" },
    { System::Unknown,   "Unknown" },
};

// ── Sidebar ordering ──────────────────────────────────────────────────────────
inline const std::array<System, 22> SYSTEM_ORDER = {
    System::Unknown,  // "All" sentinel
    System::NES,
    System::SNES,
    System::N64,
    System::NDS,
    System::GB,
    System::GBC,
    System::GBA,
    System::Genesis,
    System::SMS,
    System::Atari2600,
    System::Atari7800,
    System::Lynx,
    System::PCE,
    System::C64,
    System::DOS,
    System::PS1,
    System::GG,
    System::NGP,
    System::WS,
    System::VB,
    System::Saturn,
};

// ── Helper: safe color lookup with fallback ───────────────────────────────────
inline Color system_color(System s) {
    auto it = SYSTEM_COLORS.find(s);
    return it != SYSTEM_COLORS.end() ? it->second : SYSTEM_COLORS.at(System::Unknown);
}

inline const std::string& system_label(System s) {
    auto it = SYSTEM_LABELS.find(s);
    static const std::string fallback = "Unknown";
    return it != SYSTEM_LABELS.end() ? it->second : fallback;
}

} // namespace Config
