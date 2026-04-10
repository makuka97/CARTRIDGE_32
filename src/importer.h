#pragma once
#include "models.h"
#include <string>
#include <vector>

// ── RomImporter ───────────────────────────────────────────────────────────────
// Handles everything needed to take a file path and turn it into a Game record.

namespace Importer {

// Detect system from file extension. Returns System::Unknown if unrecognised.
System detect_system(const std::string& path);

// Build a display name from the filename (strips extension, cleans up).
std::string make_name(const std::string& path);

// Simple MD5-style hash of file contents for duplicate detection.
// Returns hex string.
std::string hash_file(const std::string& path);

// For DOS zip files: list all .exe / .com / .bat paths inside the zip.
std::vector<std::string> list_dos_executables(const std::string& zip_path);

// Result of an import attempt
struct ImportResult {
    Game        game;
    bool        ok        = false;
    bool        duplicate = false;
    std::string error;
};

// Import a ROM file. Fills game.name, game.system, game.rom_path.
// Does NOT insert into DB — caller does that.
ImportResult import_rom(const std::string& path);

// Import a DOS zip with a chosen executable inside.
ImportResult import_dos(const std::string& zip_path, const std::string& exe_inside);

} // namespace Importer
