#include "app.h"
#include "config.h"
#include <cstdlib>

// On Windows, SDL2main redefines main → WinMain for us when we link SDL2main.
// This file stays portable — no platform ifdefs needed here.

int main(int /*argc*/, char* /*argv*/[]) {
    App app;
    if (!app.init("CARTRIDGE32", Config::WINDOW_W, Config::WINDOW_H))
        return EXIT_FAILURE;
    return app.run();
}
