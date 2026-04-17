#include "app.h"
#include "platform.h"

#include <cstdio>
#include <exception>

// ── Entry point ───────────────────────────────────────────────────────────────
// On Windows we compile as a WIN32 (GUI) application so we need WinMain.
// SDL2's SDL_main.h macro redirects WinMain → main on Windows when
// SDL_MAIN_HANDLED is not defined. We let SDL handle it.

#ifdef AUTODOS2_WINDOWS
#  include <SDL.h>    // SDL_main.h is included transitively; SDL sets up WinMain
#endif

int main(int /*argc*/, char* /*argv*/[])
{
    try {
        AutoDOS2::App app;

        if (!app.init()) {
            std::fprintf(stderr, "[AutoDOS2] Initialisation failed. Exiting.\n");
            return 1;
        }

        app.run();
        return 0;
    }
    catch (const std::exception& ex) {
        std::fprintf(stderr, "[AutoDOS2] Fatal exception: %s\n", ex.what());
        return 1;
    }
    catch (...) {
        std::fprintf(stderr, "[AutoDOS2] Unknown fatal exception.\n");
        return 1;
    }
}
