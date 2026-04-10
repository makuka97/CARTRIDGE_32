#pragma once
#include "models.h"
#include <vector>
#include <string>

// Forward declare sqlite3 so we don't need to include sqlite3.h everywhere
struct sqlite3;

// ── Database ──────────────────────────────────────────────────────────────────
// Owns the SQLite connection. Call init() once at startup.
// All functions are synchronous and safe to call from the main thread only.

class Database {
public:
    Database();
    ~Database();

    // Open (or create) the DB at the given path. Returns false on failure.
    bool init(const std::string& path);

    // ── Game CRUD ─────────────────────────────────────────────────────────────
    std::vector<Game> get_all_games();
    std::vector<Game> get_games_by_system(System system);
    bool              insert_game(Game& game);   // sets game.id on success
    bool              update_boxart(int64_t id, const std::string& path);
    bool              delete_game(int64_t id);
    bool              increment_play_count(int64_t id);

    // ── Utility ───────────────────────────────────────────────────────────────
    bool is_duplicate(const std::string& rom_path);
    int  game_count();
    int  game_count_by_system(System system);

private:
    bool     run_migrations();
    Game     row_to_game(struct sqlite3_stmt* stmt);

    sqlite3* m_db = nullptr;
};
