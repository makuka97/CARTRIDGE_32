#include "library.h"
#include "../config.h"
#include "../importer.h"
#include "../launcher.h"
#include "../scraper.h"
#include "../texture_cache.h"

#include <imgui.h>
#include <windows.h>
#include <SDL.h>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <cctype>

using namespace Config;

static ImVec4 to_imvec4(const Config::Color& c) { return {c.r, c.g, c.b, c.a}; }
static ImU32  to_imu32 (const Config::Color& c) {
    return IM_COL32((int)(c.r*255),(int)(c.g*255),(int)(c.b*255),(int)(c.a*255));
}
static std::string str_lower(const std::string& s) {
    std::string o = s;
    for (auto& c : o) c = (char)std::tolower((unsigned char)c);
    return o;
}

// ── Constructor ───────────────────────────────────────────────────────────────

LibraryScreen::LibraryScreen(Database& db) : m_db(db) { reload(); }

void LibraryScreen::reload() {
    m_games = m_db.get_all_games();
    apply_filter();

    // Queue scrape for any game without cached boxart
    for (const auto& game : m_games) {
        if (Scraper::cached_path(game.id).empty()) {
            Scraper::queue(game, [this](int64_t id, const std::string& path) {
                // Update DB and evict old texture so next frame reloads
                m_db.update_boxart(id, path);
                TextureCache::evict(path);
                // Update in-memory game list so tile renders the new art
                for (auto& g : m_games) {
                    if (g.id == id) { g.boxart = path; break; }
                }
            });
        }
    }
}

void LibraryScreen::apply_filter() {
    m_filtered.clear();
    std::string q = str_lower(m_search);
    for (const auto& g : m_games) {
        if (m_filter != System::Unknown && g.system != m_filter) continue;
        if (!q.empty() && str_lower(g.name).find(q) == std::string::npos) continue;
        m_filtered.push_back(g);
    }
}

// ── Main draw ─────────────────────────────────────────────────────────────────

void LibraryScreen::draw(float window_w, float window_h) {
    // Dispatch any completed scrape callbacks
    Scraper::poll();

    const float sidebar_w = (float)SIDEBAR_W;
    const float toolbar_h = 44.0f;
    const float search_h  = 44.0f;
    const float header_h  = toolbar_h + search_h;  // toolbar on top, search below

    draw_sidebar(sidebar_w, window_h);
    draw_toolbar(sidebar_w, 0, window_w - sidebar_w);
    draw_search_bar(sidebar_w, toolbar_h, window_w - sidebar_w);
    draw_tile_grid(sidebar_w, header_h, window_w - sidebar_w, window_h - header_h);

    draw_detail_modal();
    draw_dos_picker_modal();

    // Toast
    if (m_toast_timer > 0.0f) {
        m_toast_timer -= ImGui::GetIO().DeltaTime;
        ImVec2 disp = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos({disp.x * 0.5f, disp.y - 60}, ImGuiCond_Always, {0.5f, 0.5f});
        ImGui::SetNextWindowBgAlpha(0.88f);
        ImGui::Begin("##toast", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(COLOR_TEXT));
        ImGui::TextUnformatted(m_toast.c_str());
        ImGui::PopStyleColor();
        ImGui::End();
    }
}

// ── Toolbar ───────────────────────────────────────────────────────────────────

void LibraryScreen::draw_toolbar(float x, float y, float w) {
    ImGui::SetNextWindowPos({x, y});
    ImGui::SetNextWindowSize({w, 44});
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {10, 0});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, to_imvec4(BG_SURFACE));
    ImGui::Begin("##toolbar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoResize   |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar
    );
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::SetCursorPosY(7);

    // Select button
    {
        bool active = m_select_mode;
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button,        to_imvec4(BG_ACTIVE));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, to_imvec4(BG_HOVER));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        to_imvec4(BG_HOVER));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, to_imvec4(BG_ACTIVE));
        }
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, to_imvec4(COLOR_BORDER));
        ImGui::PushStyleColor(ImGuiCol_Text,         to_imvec4(COLOR_TEXT));

        if (ImGui::Button(m_select_mode ? "Cancel" : "Select", {72, 30})) {
            m_select_mode = !m_select_mode;
            m_selected_ids.clear();
        }
        ImGui::PopStyleColor(4);
    }

    // Delete button (only visible in select mode)
    if (m_select_mode) {
        ImGui::SameLine(0, 8);

        bool has_selection = !m_selected_ids.empty();
        if (!has_selection) ImGui::BeginDisabled();

        ImGui::PushStyleColor(ImGuiCol_Button,
            has_selection ? ImVec4(0.78f, 0.15f, 0.15f, 1.0f) : to_imvec4(BG_HOVER));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.10f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.55f, 0.08f, 0.08f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,
            has_selection ? ImVec4(1,1,1,1) : to_imvec4(COLOR_MUTED));

        char del_label[32];
        snprintf(del_label, sizeof(del_label),
                 has_selection ? "Delete (%d)" : "Delete",
                 (int)m_selected_ids.size());

        if (ImGui::Button(del_label, {has_selection ? 110.0f : 72.0f, 30}))
            delete_selected();

        ImGui::PopStyleColor(4);
        if (!has_selection) ImGui::EndDisabled();

        // Selection count hint
        ImGui::SameLine(0, 12);
        ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(COLOR_MUTED));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);
        if (m_selected_ids.empty())
            ImGui::TextUnformatted("Click tiles to select");
        else {
            char hint[32];
            snprintf(hint, sizeof(hint), "%d selected", (int)m_selected_ids.size());
            ImGui::TextUnformatted(hint);
        }
        ImGui::PopStyleColor();
    }

    // Bottom border
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    dl->AddLine({wp.x, wp.y + 43}, {wp.x + w, wp.y + 43}, to_imu32(COLOR_BORDER), 1.0f);

    ImGui::End();
}

// ── Sidebar ───────────────────────────────────────────────────────────────────

void LibraryScreen::draw_sidebar(float sidebar_w, float window_h) {
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({sidebar_w, window_h});
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   {0, 0});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, to_imvec4(BG_SIDEBAR));
    ImGui::Begin("##sidebar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoResize   |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings
    );
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // App title
    ImGui::SetCursorPos({14, 16});
    ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(COLOR_TEXT));
    ImGui::TextUnformatted("CARTRIDGE32");
    ImGui::PopStyleColor();

    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddLine({p.x, p.y + 8}, {p.x + sidebar_w, p.y + 8}, to_imu32(COLOR_BORDER), 1.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 17);

    // All Games
    {
        bool selected = (m_filter == System::Unknown);
        ImVec2 ip = ImGui::GetCursorScreenPos();
        if (selected)
            dl->AddRectFilled(ip, {ip.x + sidebar_w, ip.y + 36}, to_imu32(BG_ACTIVE));
        ImGui::PushStyleColor(ImGuiCol_Header,        to_imu32(BG_ACTIVE));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, to_imu32(BG_HOVER));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  to_imu32(BG_ACTIVE));
        ImGui::PushStyleColor(ImGuiCol_Text,          to_imvec4(COLOR_TEXT));
        ImGui::SetCursorPosX(14);
        if (ImGui::Selectable("All Games###nav_all", selected, 0, {sidebar_w - 14, 36})) {
            m_filter = System::Unknown; apply_filter();
        }
        ImGui::PopStyleColor(4);
        char cnt[16]; snprintf(cnt, sizeof(cnt), "%d", m_db.game_count());
        ImVec2 cs = ImGui::CalcTextSize(cnt);
        dl->AddText({ip.x + sidebar_w - cs.x - 12, ip.y + (36 - cs.y) * 0.5f},
                    to_imu32(COLOR_MUTED), cnt);
    }

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);

    for (auto sys : SYSTEM_ORDER) {
        if (sys == System::Unknown) continue;
        int cnt = m_db.game_count_by_system(sys);
        if (cnt == 0) continue;
        bool selected = (m_filter == sys);
        auto color = system_color(sys);
        const auto& lbl = system_label(sys);
        ImVec2 ip = ImGui::GetCursorScreenPos();
        if (selected)
            dl->AddRectFilled(ip, {ip.x + sidebar_w, ip.y + 32}, to_imu32(BG_ACTIVE));
        dl->AddCircleFilled({ip.x + 18, ip.y + 16}, 4.0f, to_imu32(color));
        ImGui::PushStyleColor(ImGuiCol_Header,        to_imu32(BG_ACTIVE));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, to_imu32(BG_HOVER));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,  to_imu32(BG_ACTIVE));
        ImGui::PushStyleColor(ImGuiCol_Text,          to_imvec4(COLOR_TEXT));
        ImGui::SetCursorPos({32, ImGui::GetCursorPosY()});
        char sel_lbl[128];
        snprintf(sel_lbl, sizeof(sel_lbl), "%s###nav_%d", lbl.c_str(), (int)sys);
        if (ImGui::Selectable(sel_lbl, selected, 0, {sidebar_w - 32, 32})) {
            m_filter = sys; apply_filter();
        }
        ImGui::PopStyleColor(4);
        char cnt_str[16]; snprintf(cnt_str, sizeof(cnt_str), "%d", cnt);
        ImVec2 cs = ImGui::CalcTextSize(cnt_str);
        dl->AddText({ip.x + sidebar_w - cs.x - 12, ip.y + (32 - cs.y) * 0.5f},
                    to_imu32(COLOR_MUTED), cnt_str);
    }

    // Add ROM button
    ImGui::SetCursorPos({8, window_h - 44});
    ImGui::PushStyleColor(ImGuiCol_Button,        to_imvec4(BG_ACTIVE));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, to_imvec4(BG_HOVER));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  to_imvec4(COLOR_BORDER));
    ImGui::PushStyleColor(ImGuiCol_Text,          to_imvec4(COLOR_TEXT));
    if (ImGui::Button("+ Add ROM", {sidebar_w - 16, 32}))
        open_file_dialog();
    ImGui::PopStyleColor(4);

    ImGui::End();
}

// ── Search bar ────────────────────────────────────────────────────────────────

void LibraryScreen::draw_search_bar(float x, float y, float w) {
    ImGui::SetNextWindowPos({x, y});
    ImGui::SetNextWindowSize({w, 44});
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {12, 0});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, to_imvec4(BG_SURFACE));
    ImGui::Begin("##searchbar", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoResize   |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar
    );
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::SetCursorPosY(10);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        to_imvec4(BG_BASE));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, to_imvec4(BG_HOVER));
    ImGui::PushStyleColor(ImGuiCol_Text,           to_imvec4(COLOR_TEXT));
    ImGui::PushItemWidth(w - 24);
    if (ImGui::InputTextWithHint("##search", "Search games...",
                                  m_search_buf, sizeof(m_search_buf))) {
        m_search = m_search_buf;
        apply_filter();
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleColor(3);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    dl->AddLine({wp.x, wp.y + 43}, {wp.x + w, wp.y + 43}, to_imu32(COLOR_BORDER), 1.0f);

    ImGui::End();
}

// ── Tile grid ─────────────────────────────────────────────────────────────────

void LibraryScreen::draw_tile_grid(float x, float y, float w, float h) {
    ImGui::SetNextWindowPos({x, y});
    ImGui::SetNextWindowSize({w, h});
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {(float)TILE_GAP, (float)TILE_GAP});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, to_imvec4(BG_BASE));
    ImGui::Begin("##tilegrid", nullptr,
        ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove      | ImGuiWindowFlags_NoResize   |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysVerticalScrollbar
    );
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    if (m_filtered.empty()) {
        ImVec2 ws = ImGui::GetWindowSize();
        const char* msg = m_games.empty()
            ? "No games yet\nClick \"+ Add ROM\" to get started"
            : "No games match your search";
        ImVec2 ts = ImGui::CalcTextSize(msg);
        ImGui::SetCursorPos({(ws.x - ts.x) * 0.5f, (ws.y - ts.y) * 0.5f});
        ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(COLOR_MUTED));
        ImGui::TextUnformatted(msg);
        ImGui::PopStyleColor();
        ImGui::End();
        return;
    }

    const float tile_w = (float)TILE_W;
    const float tile_h = (float)TILE_H;
    const float gap    = (float)TILE_GAP;
    const float grid_w = w - gap * 2 - 14;
    int cols = (int)(grid_w / (tile_w + gap));
    if (cols < 1) cols = 1;

    int idx = 0;
    for (const auto& game : m_filtered) {
        int col = idx % cols;
        int row = idx / cols;
        ImGui::SetCursorPos({(float)col * (tile_w + gap), (float)row * (tile_h + gap)});
        draw_tile(game, (float)col * (tile_w + gap), (float)row * (tile_h + gap));
        idx++;
    }

    int rows = ((int)m_filtered.size() + cols - 1) / cols;
    ImGui::SetCursorPos({0, (float)rows * (tile_h + gap) + gap});
    ImGui::Dummy({1, 1});

    ImGui::End();
}

// ── Single tile ───────────────────────────────────────────────────────────────

void LibraryScreen::draw_tile(const Game& game, float tx, float ty) {
    const float tw = (float)TILE_W;
    const float th = (float)TILE_H;

    bool is_selected = m_selected_ids.count(game.id) > 0;
    auto color   = system_color(game.system);
    ImU32 col32  = to_imu32(color);
    ImU32 bg32   = to_imu32(BG_SURFACE);
    ImU32 bord32 = is_selected ? col32 : to_imu32(COLOR_BORDER);
    float border_thickness = is_selected ? 2.0f : 0.5f;

    ImDrawList* dl  = ImGui::GetWindowDrawList();
    ImVec2 win_pos  = ImGui::GetWindowPos();
    ImVec2 scroll   = {ImGui::GetScrollX(), ImGui::GetScrollY()};
    ImVec2 p0 = {win_pos.x + tx - scroll.x, win_pos.y + ty - scroll.y};
    ImVec2 p1 = {p0.x + tw, p0.y + th};

    dl->AddRectFilled(p0, p1, bg32, TILE_ROUNDING);
    dl->AddRect(p0, p1, bord32, TILE_ROUNDING, 0, border_thickness);

    // Color header strip
    dl->AddRectFilled(p0, {p1.x, p0.y + 6}, col32, TILE_ROUNDING, ImDrawFlags_RoundCornersTop);

    // Selected checkmark overlay
    if (is_selected) {
        dl->AddRectFilled({p1.x - 24, p0.y + 6}, {p1.x, p0.y + 26}, col32);
        dl->AddText({p1.x - 18, p0.y + 7}, IM_COL32(255,255,255,255), "✓");
    }

    // Art area — box art texture if available, else colored placeholder
    const float art_h = 80.0f;
    const float art_y = p0.y + 6;

    // Always use ID-keyed cache file ({game_id}.png) as the source of truth.
    // game.boxart from DB may be a stale path from a previous run with different IDs.
    std::string art_path = Scraper::cached_path(game.id);
    ImTextureID tex = art_path.empty() ? (ImTextureID)0 : TextureCache::load(art_path);

    if (tex != (ImTextureID)0) {
        // Draw box art — fit the art area
        dl->AddImage(ImTextureRef(tex),
            {p0.x, art_y}, {p1.x, art_y + art_h},
            {0, 0}, {1, 1});
    } else {
        // Placeholder tinted background + system label
        ImU32 art_bg = IM_COL32((int)(color.r*30),(int)(color.g*30),(int)(color.b*30), 30);
        dl->AddRectFilled({p0.x, art_y}, {p1.x, art_y + art_h}, art_bg);
        const auto& sys_str = system_label(game.system);
        ImVec2 sys_sz = ImGui::CalcTextSize(sys_str.c_str());
        dl->AddText({p0.x + (tw - sys_sz.x) * 0.5f, art_y + (art_h - sys_sz.y) * 0.5f},
                    col32, sys_str.c_str());
    }

    // Game name
    const float name_y = art_y + art_h + 8.0f;
    std::string dname  = game.name;
    while (ImGui::CalcTextSize(dname.c_str()).x > tw - 12.0f && dname.size() > 3)
        dname = dname.substr(0, dname.size() - 4) + "...";
    dl->AddText({p0.x + 6.0f, name_y}, to_imu32(COLOR_TEXT), dname.c_str());

    // Invisible button
    ImGui::SetCursorPos({tx, ty});
    char btn_id[32]; snprintf(btn_id, sizeof(btn_id), "##tile_%lld", (long long)game.id);
    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(0,0,0,0));

    if (ImGui::Button(btn_id, {tw, th})) {
        if (m_select_mode) {
            // Toggle selection
            if (is_selected) m_selected_ids.erase(game.id);
            else             m_selected_ids.insert(game.id);
        } else {
            // Open detail modal
            m_selected_game = game;
            m_show_detail   = true;
        }
    }
    if (ImGui::IsItemHovered() && !m_select_mode)
        dl->AddRect(p0, p1, col32, TILE_ROUNDING, 0, 1.5f);

    ImGui::PopStyleColor(3);
}

// ── Delete selected ───────────────────────────────────────────────────────────

void LibraryScreen::delete_selected() {
    if (m_selected_ids.empty()) return;

    char msg[128];
    snprintf(msg, sizeof(msg),
             "Delete %d game%s from your library?\n\n"
             "This removes the entry but does not delete the ROM file.",
             (int)m_selected_ids.size(),
             m_selected_ids.size() == 1 ? "" : "s");

    int result = MessageBoxA(nullptr, msg, "Delete Games",
                              MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (result != IDYES) return;

    for (int64_t id : m_selected_ids)
        m_db.delete_game(id);

    int count = (int)m_selected_ids.size();
    m_selected_ids.clear();
    m_select_mode = false;

    reload();

    char toast[64];
    snprintf(toast, sizeof(toast), "Deleted %d game%s",
             count, count == 1 ? "" : "s");
    m_toast       = toast;
    m_toast_timer = 3.0f;
}

// ── Detail modal ──────────────────────────────────────────────────────────────

void LibraryScreen::draw_detail_modal() {
    if (!m_show_detail) return;

    ImGui::OpenPopup("##detail");
    ImVec2 disp = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({disp.x * 0.5f, disp.y * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({360, 0});
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, to_imvec4(BG_SURFACE));

    if (ImGui::BeginPopupModal("##detail", &m_show_detail,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {

        auto color = system_color(m_selected_game.system);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        dl->AddRectFilled(wp, {wp.x + ws.x, wp.y + 4}, to_imu32(color));

        ImGui::SetCursorPosY(12);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(color.r, color.g, color.b, 1.0f));
        ImGui::TextUnformatted(system_label(m_selected_game.system).c_str());
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(COLOR_TEXT));
        ImGui::SetWindowFontScale(1.2f);
        ImGui::TextWrapped("%s", m_selected_game.name.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(COLOR_MUTED));
        ImGui::TextWrapped("%s", m_selected_game.rom_path.c_str());
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool available = Launcher::emulator_available(m_selected_game.system);
        if (!available) {
            ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(COLOR_MUTED));
            ImGui::TextWrapped("No emulator found in native/%s/",
                system_label(m_selected_game.system).c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        // Keyboard shortcut hints
        ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(COLOR_MUTED));
        if (available)
            ImGui::TextUnformatted("Enter  launch    Del  remove    Esc  close");
        else
            ImGui::TextUnformatted("Del  remove    Esc  close");
        ImGui::PopStyleColor();

        // DOS-specific in-game hotkey reference (mirrors Python _show_launch_modal)
        if (m_selected_game.system == System::DOS && available) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(COLOR_MUTED));
            ImGui::TextUnformatted("DOSBox hotkeys");
            ImGui::Spacing();

            struct HotkeyRow { const char* key; const char* action; };
            static constexpr HotkeyRow DOS_KEYS[] = {
                { "Ctrl+F9",   "Exit game"        },
                { "Alt+Enter", "Toggle fullscreen" },
                { "Ctrl+F10",  "Lock / free mouse" },
                { "Ctrl+F12",  "Speed up CPU"      },
            };
            for (auto& k : DOS_KEYS) {
                ImGui::TextUnformatted(k.key);
                ImGui::SameLine(130);
                ImGui::TextUnformatted(k.action);
            }
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();

        // Keyboard handling
        bool do_launch = available && ImGui::IsKeyPressed(ImGuiKey_Enter);
        bool do_delete = ImGui::IsKeyPressed(ImGuiKey_Delete);
        bool do_close  = ImGui::IsKeyPressed(ImGuiKey_Escape);

        if (!available) ImGui::BeginDisabled();
        ImGui::PushStyleColor(ImGuiCol_Button,
            ImVec4(color.r, color.g, color.b, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4(color.r * 0.85f, color.g * 0.85f, color.b * 0.85f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,1));
        if (ImGui::Button("Launch", {160, 34}) || do_launch) {
            m_db.increment_play_count(m_selected_game.id);
            Launcher::launch(m_selected_game);
            m_show_detail = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);
        if (!available) ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.78f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.10f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1,1,1,1));
        if (ImGui::Button("Delete", {90, 34}) || do_delete) {
            m_db.delete_game(m_selected_game.id);
            m_show_detail = false;
            ImGui::CloseCurrentPopup();
            reload();
            m_toast = "Deleted: " + m_selected_game.name;
            m_toast_timer = 3.0f;
        }
        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button,        to_imvec4(BG_HOVER));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, to_imvec4(BG_ACTIVE));
        ImGui::PushStyleColor(ImGuiCol_Text,          to_imvec4(COLOR_TEXT));
        if (ImGui::Button("Close", {90, 28}) || do_close) {
            m_show_detail = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();

    if (!m_show_detail) ImGui::CloseCurrentPopup();
}

// ── DOS picker modal ──────────────────────────────────────────────────────────

void LibraryScreen::draw_dos_picker_modal() {
    if (!m_show_dos_picker) return;

    ImGui::OpenPopup("##dospicker");
    ImVec2 disp = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({disp.x * 0.5f, disp.y * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({400, 320});
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, to_imvec4(BG_SURFACE));

    if (ImGui::BeginPopupModal("##dospicker", &m_show_dos_picker,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {

        auto color = system_color(System::DOS);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        ImVec2 ws = ImGui::GetWindowSize();
        dl->AddRectFilled(wp, {wp.x + ws.x, wp.y + 4}, to_imu32(color));

        ImGui::SetCursorPosY(12);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(color.r, color.g, color.b, 1.0f));
        ImGui::TextUnformatted("MS-DOS Game");
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(COLOR_MUTED));
        ImGui::TextUnformatted("Select the executable to launch:");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::BeginChild("##exe_list", {0, 200}, false);
        for (const auto& exe : m_dos_exes) {
            std::string sp = exe;
            for (auto& c : sp) if (c == '\\') c = '/';
            auto last  = sp.rfind('/');
            std::string fname = (last == std::string::npos) ? sp : sp.substr(last + 1);
            std::string dir   = (last == std::string::npos) ? "" : sp.substr(0, last);

            ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(COLOR_TEXT));
            bool clicked = ImGui::Selectable(("  " + fname + "###exe_" + exe).c_str(),
                                              false, 0, {0, 28});
            if (!dir.empty()) {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, to_imvec4(COLOR_MUTED));
                ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::CalcTextSize(dir.c_str()).x - 12);
                ImGui::TextUnformatted(dir.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::PopStyleColor();

            if (clicked) {
                auto result = Importer::import_dos(m_dos_zip_path, exe);
                if (result.ok) {
                    Game g = result.game;
                    if (m_db.insert_game(g)) {
                        m_games.push_back(g);
                        apply_filter();
                        m_toast       = "Added: " + g.name;
                        m_toast_timer = 3.0f;
                        Scraper::queue(g, [this](int64_t id, const std::string& path) {
                            m_db.update_boxart(id, path);
                            TextureCache::evict(path);
                            for (auto& game : m_games) { if (game.id == id) { game.boxart = path; break; } }
                        });
                    }
                }
                m_show_dos_picker = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button,        to_imvec4(BG_HOVER));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, to_imvec4(BG_ACTIVE));
        ImGui::PushStyleColor(ImGuiCol_Text,          to_imvec4(COLOR_TEXT));
        if (ImGui::Button("Cancel", {80, 28})) {
            m_show_dos_picker = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

// ── File dialog & import ──────────────────────────────────────────────────────

void LibraryScreen::open_file_dialog() {
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter =
        "ROM Files\0"
        "*.nes;*.smc;*.sfc;*.snes;*.n64;*.z64;*.v64;"
        "*.gb;*.gbc;*.gba;*.md;*.gen;*.bin;*.sms;*.gg;"
        "*.a26;*.a78;*.lnx;*.pce;*.d64;*.t64;*.prg;*.crt;*.zip"
        "\0All Files\0*.*\0";
    ofn.lpstrFile  = filename;
    ofn.nMaxFile   = MAX_PATH;
    ofn.lpstrTitle = "Open ROM";
    ofn.Flags      = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameA(&ofn)) return;
    try_import(std::string(filename));
}

void LibraryScreen::try_import(const std::string& path) {
    if (m_db.is_duplicate(path)) {
        m_toast = "Already in library"; m_toast_timer = 3.0f; return;
    }

    std::string lower = path;
    for (auto& c : lower) c = (char)std::tolower((unsigned char)c);

    if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".zip") {
        auto exes = Importer::list_dos_executables(path);
        if (exes.empty()) {
            m_toast = "No executables found in zip"; m_toast_timer = 3.0f; return;
        }
        if (exes.size() == 1) {
            auto result = Importer::import_dos(path, exes[0]);
            if (result.ok) {
                Game g = result.game;
                if (m_db.insert_game(g)) {
                    m_games.push_back(g); apply_filter();
                    m_toast = "Added: " + g.name; m_toast_timer = 3.0f;
                    Scraper::queue(g, [this](int64_t id, const std::string& path) {
                        m_db.update_boxart(id, path);
                        TextureCache::evict(path);
                        for (auto& game : m_games) { if (game.id == id) { game.boxart = path; break; } }
                    });
                }
            }
            return;
        }
        m_dos_zip_path = path; m_dos_exes = exes; m_show_dos_picker = true;
        return;
    }

    auto result = Importer::import_rom(path);
    if (!result.ok) {
        m_toast = "Import failed: " + result.error; m_toast_timer = 3.0f; return;
    }

    Game g = result.game;
    if (!m_db.insert_game(g)) {
        m_toast = "Database error"; m_toast_timer = 3.0f; return;
    }

    m_games.push_back(g);
    apply_filter();
    m_toast = "Added: " + g.name;
    m_toast_timer = 3.0f;

    // Kick off box art scrape for the new game
    Scraper::queue(g, [this](int64_t id, const std::string& path) {
        m_db.update_boxart(id, path);
        TextureCache::evict(path);
        for (auto& game : m_games) {
            if (game.id == id) { game.boxart = path; break; }
        }
    });
}
