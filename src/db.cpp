#include "db.h"
#include <SDL.h>
#include "models.h"
#include <sqlite3.h>
#include <cstdio>
#include <cstring>

// ── Helpers ───────────────────────────────────────────────────────────────────

static const char* CREATE_GAMES_TABLE = R"sql(
CREATE TABLE IF NOT EXISTS games (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT    NOT NULL,
    rom_path    TEXT    NOT NULL UNIQUE,
    system      INTEGER NOT NULL DEFAULT 0,
    boxart      TEXT    NOT NULL DEFAULT '',
    dos_conf    TEXT    NOT NULL DEFAULT '',
    play_count  INTEGER NOT NULL DEFAULT 0,
    added_at    TEXT    NOT NULL DEFAULT (datetime('now'))
);
CREATE INDEX IF NOT EXISTS idx_games_system ON games(system);
)sql";

// ── Database ──────────────────────────────────────────────────────────────────

Database::Database()  = default;
Database::~Database() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool Database::init(const std::string& path) {
    int rc = sqlite3_open(path.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "DB open failed: %s\n", sqlite3_errmsg(m_db));
        return false;
    }
    // WAL mode for better concurrent read performance
    sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA foreign_keys=ON;",  nullptr, nullptr, nullptr);
    return run_migrations();
}

bool Database::run_migrations() {
    char* err = nullptr;
    int rc = sqlite3_exec(m_db, CREATE_GAMES_TABLE, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "DB migration failed: %s\n", err);
        sqlite3_free(err);
        return false;
    }
    return true;
}

// ── Row mapper ────────────────────────────────────────────────────────────────

Game Database::row_to_game(sqlite3_stmt* stmt) {
    Game g;
    g.id         = sqlite3_column_int64(stmt, 0);
    g.name       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    g.rom_path   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    g.system     = static_cast<System>(sqlite3_column_int(stmt, 3));

    auto boxart  = sqlite3_column_text(stmt, 4);
    g.boxart     = boxart ? reinterpret_cast<const char*>(boxart) : "";

    auto conf    = sqlite3_column_text(stmt, 5);
    g.dos_conf   = conf ? reinterpret_cast<const char*>(conf) : "";

    g.play_count = sqlite3_column_int64(stmt, 6);

    auto added   = sqlite3_column_text(stmt, 7);
    g.added_at   = added ? reinterpret_cast<const char*>(added) : "";

    return g;
}

// ── Queries ───────────────────────────────────────────────────────────────────

std::vector<Game> Database::get_all_games() {
    std::vector<Game> games;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id,name,rom_path,system,boxart,dos_conf,play_count,added_at "
                      "FROM games ORDER BY name COLLATE NOCASE ASC;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        SDL_Log("get_all_games prepare failed: %s", sqlite3_errmsg(m_db));
        return games;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW)
        games.push_back(row_to_game(stmt));
    sqlite3_finalize(stmt);
    SDL_Log("get_all_games returned %d games", (int)games.size());
    return games;
}

std::vector<Game> Database::get_games_by_system(System system) {
    std::vector<Game> games;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id,name,rom_path,system,boxart,dos_conf,play_count,added_at "
                      "FROM games WHERE system=? ORDER BY name COLLATE NOCASE ASC;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return games;
    sqlite3_bind_int(stmt, 1, static_cast<int>(system));
    while (sqlite3_step(stmt) == SQLITE_ROW)
        games.push_back(row_to_game(stmt));
    sqlite3_finalize(stmt);
    return games;
}

bool Database::insert_game(Game& game) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT INTO games (name,rom_path,system,boxart,dos_conf) "
        "VALUES (?,?,?,?,?);";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt, 1, game.name.c_str(),     -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, game.rom_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int (stmt, 3, static_cast<int>(game.system));
    sqlite3_bind_text(stmt, 4, game.boxart.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, game.dos_conf.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (ok) game.id = sqlite3_last_insert_rowid(m_db);
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::update_boxart(int64_t id, const std::string& path) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE games SET boxart=? WHERE id=?;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text (stmt, 1, path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, id);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::delete_game(int64_t id) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "DELETE FROM games WHERE id=?;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int64(stmt, 1, id);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::increment_play_count(int64_t id) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE games SET play_count=play_count+1 WHERE id=?;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_int64(stmt, 1, id);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Database::is_duplicate(const std::string& rom_path) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT 1 FROM games WHERE rom_path=? LIMIT 1;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt, 1, rom_path.c_str(), -1, SQLITE_TRANSIENT);
    bool found = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return found;
}

int Database::game_count() {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT COUNT(*) FROM games;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return 0;
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

int Database::game_count_by_system(System system) {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT COUNT(*) FROM games WHERE system=?;";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return 0;
    sqlite3_bind_int(stmt, 1, static_cast<int>(system));
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}
