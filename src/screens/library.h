#pragma once
#include "../models.h"
#include "../db.h"
#include <vector>
#include <string>
#include <set>

class LibraryScreen {
public:
    explicit LibraryScreen(Database& db);

    void draw(float window_w, float window_h);
    void reload();

private:
    void draw_sidebar(float sidebar_w, float window_h);
    void draw_search_bar(float x, float y, float w);
    void draw_tile_grid(float x, float y, float w, float h);
    void draw_tile(const Game& game, float x, float y);
    void draw_detail_modal();
    void draw_dos_picker_modal();
    void draw_toolbar(float x, float y, float w);

    void open_file_dialog();
    void try_import(const std::string& path);
    void apply_filter();
    void delete_selected();

    // Filter & search
    System      m_filter = System::Unknown;
    std::string m_search;
    char        m_search_buf[256] = {};

    // Data
    Database&         m_db;
    std::vector<Game> m_games;
    std::vector<Game> m_filtered;

    // Detail modal
    bool m_show_detail  = false;
    Game m_selected_game;

    // Select mode
    bool              m_select_mode = false;
    std::set<int64_t> m_selected_ids;

    // DOS picker
    bool                     m_show_dos_picker = false;
    std::string              m_dos_zip_path;
    std::vector<std::string> m_dos_exes;

    // Toast
    std::string m_toast;
    float       m_toast_timer = 0.0f;
};
