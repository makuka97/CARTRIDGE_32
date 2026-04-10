#pragma once
#include "db.h"
#include "screens/library.h"
#include <SDL.h>
#include <memory>

class App {
public:
    App();
    ~App();

    bool init(const char* title, int w, int h);
    int  run();
    void quit() { m_running = false; }

private:
    void process_events();
    void new_frame();
    void render_frame();
    void draw_ui();
    void apply_theme();

    SDL_Window*   m_window  = nullptr;
    SDL_GLContext m_gl_ctx  = nullptr;
    bool          m_running = false;

    Database                       m_db;
    std::unique_ptr<LibraryScreen> m_library;
};
