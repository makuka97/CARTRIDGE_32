#include "app.h"
#include "config.h"
#include "scraper.h"
#include "texture_cache.h"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#include <SDL.h>
#include <SDL_opengl.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

// ── Data dir ──────────────────────────────────────────────────────────────────

static std::string get_data_dir() {
#ifdef _WIN32
    char path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        std::string d = std::string(path) + "\\CARTRIDGE32\\";
        CreateDirectoryA(d.c_str(), nullptr);
        return d;
    }
    // Fallback: store next to the exe
    CreateDirectoryA(".\\data", nullptr);
    return ".\\data\\";
#else
    return "./data/";
#endif
}

static void ensure_dir(const std::string& path) {
#ifdef _WIN32
    CreateDirectoryA(path.c_str(), nullptr);
#endif
}

// ── App ───────────────────────────────────────────────────────────────────────

App::App()  = default;

App::~App() {
    m_library.reset();
    Scraper::shutdown();
    TextureCache::clear();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    if (m_gl_ctx)  SDL_GL_DeleteContext(m_gl_ctx);
    if (m_window)  SDL_DestroyWindow(m_window);
    SDL_Quit();
}

bool App::init(const char* title, int w, int h) {
    // SDL2
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

    m_window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w, h,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!m_window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    m_gl_ctx = SDL_GL_CreateContext(m_window);
    if (!m_gl_ctx) {
        SDL_Log("SDL_GL_CreateContext failed: %s", SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(m_window, m_gl_ctx);
    SDL_GL_SetSwapInterval(1);

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = "cartridge32_imgui.ini";
    io.Fonts->AddFontDefault();

    apply_theme();

    if (!ImGui_ImplSDL2_InitForOpenGL(m_window, m_gl_ctx)) return false;
    if (!ImGui_ImplOpenGL3_Init("#version 130"))            return false;

    // Database
    std::string data_dir = get_data_dir();
    ensure_dir(data_dir);
    std::string db_path = data_dir + "cartridge32.db";

    // Phase 3: wipe any stale seed DB from phase 2 testing.
    // This block runs once and is safe to remove after first clean run.
    DeleteFileA(db_path.c_str());

    if (!m_db.init(db_path)) {
        SDL_Log("Database init failed");
        return false;
    }

    // Library screen
    m_library = std::make_unique<LibraryScreen>(m_db);

    m_running = true;
    SDL_Log("CARTRIDGE32 phase 3 initialised OK");
    return true;
}

int App::run() {
    while (m_running) {
        process_events();
        if (!m_running) break;
        new_frame();
        draw_ui();
        render_frame();
    }
    return 0;
}

void App::process_events() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        ImGui_ImplSDL2_ProcessEvent(&e);
        if (e.type == SDL_QUIT) m_running = false;
        if (e.type == SDL_KEYDOWN &&
            e.key.keysym.sym == SDLK_F4 &&
            (e.key.keysym.mod & KMOD_ALT))
            m_running = false;
    }
}

void App::new_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void App::render_frame() {
    ImGui::Render();
    ImGuiIO& io = ImGui::GetIO();
    glViewport(0, 0,
               static_cast<int>(io.DisplaySize.x),
               static_cast<int>(io.DisplaySize.y));
    const auto& bg = Config::BG_BASE;
    glClearColor(bg.r, bg.g, bg.b, bg.a);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(m_window);
}

void App::draw_ui() {
    ImGuiIO& io = ImGui::GetIO();
    if (m_library)
        m_library->draw(io.DisplaySize.x, io.DisplaySize.y);
}

void App::apply_theme() {
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowRounding    = 0.0f;
    s.ChildRounding     = 6.0f;
    s.FrameRounding     = 4.0f;
    s.PopupRounding     = 6.0f;
    s.ScrollbarRounding = 4.0f;
    s.GrabRounding      = 4.0f;
    s.TabRounding       = 4.0f;
    s.WindowBorderSize  = 0.0f;
    s.ChildBorderSize   = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.PopupBorderSize   = 1.0f;
    s.WindowPadding     = {12.0f, 12.0f};
    s.FramePadding      = {8.0f,  4.0f};
    s.ItemSpacing       = {8.0f,  6.0f};
    s.ScrollbarSize     = 10.0f;

    auto set = [&](ImGuiCol idx, const Config::Color& c, float a = -1.0f) {
        s.Colors[idx] = {c.r, c.g, c.b, a >= 0.0f ? a : c.a};
    };

    set(ImGuiCol_WindowBg,          Config::BG_BASE);
    set(ImGuiCol_ChildBg,           Config::BG_SURFACE);
    set(ImGuiCol_PopupBg,           Config::BG_SURFACE);
    set(ImGuiCol_Border,            Config::COLOR_BORDER);
    set(ImGuiCol_BorderShadow,      Config::BG_BASE, 0.0f);
    set(ImGuiCol_Text,              Config::COLOR_TEXT);
    set(ImGuiCol_TextDisabled,      Config::COLOR_MUTED);
    set(ImGuiCol_Header,            Config::BG_ACTIVE);
    set(ImGuiCol_HeaderHovered,     Config::BG_HOVER);
    set(ImGuiCol_HeaderActive,      Config::BG_ACTIVE);
    set(ImGuiCol_FrameBg,           Config::BG_SURFACE);
    set(ImGuiCol_FrameBgHovered,    Config::BG_HOVER);
    set(ImGuiCol_FrameBgActive,     Config::BG_ACTIVE);
    set(ImGuiCol_Button,            Config::BG_HOVER);
    set(ImGuiCol_ButtonHovered,     Config::BG_ACTIVE);
    set(ImGuiCol_ButtonActive,      Config::COLOR_BORDER);
    set(ImGuiCol_TitleBg,           Config::BG_SIDEBAR);
    set(ImGuiCol_TitleBgActive,     Config::BG_SIDEBAR);
    set(ImGuiCol_ScrollbarBg,       Config::BG_BASE);
    set(ImGuiCol_ScrollbarGrab,     Config::COLOR_BORDER);
    set(ImGuiCol_ScrollbarGrabHovered, Config::COLOR_MUTED);
    set(ImGuiCol_ScrollbarGrabActive,  Config::COLOR_TEXT);
    set(ImGuiCol_Separator,         Config::COLOR_BORDER);
    set(ImGuiCol_CheckMark,         Config::COLOR_TEXT);
    set(ImGuiCol_SliderGrab,        Config::COLOR_MUTED);
    set(ImGuiCol_SliderGrabActive,  Config::COLOR_TEXT);
}
