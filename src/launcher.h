#pragma once
#include "models.h"
#include <string>

// ── Launcher ──────────────────────────────────────────────────────────────────
// Spawns the correct emulator subprocess for a given game.
// Emulator binaries live in native/<folder>/ relative to the exe.

namespace Launcher {

// Returns true if the emulator binary for this system exists.
bool emulator_available(System sys);

// Returns the path to the emulator exe, or "" if not found.
std::string emulator_path(System sys);

// Launch the game. Returns true if the process started.
// For DOS games, game.dos_conf holds the exe path inside the zip.
bool launch(const Game& game);

} // namespace Launcher
