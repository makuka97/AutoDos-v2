#include "app.h"

#include <cstdio>
#include <exception>
#include <string>

#ifdef _WIN32
#  include <windows.h>
#endif

static void fatalBox(const std::string& msg)
{
#ifdef _WIN32
    MessageBoxA(nullptr, msg.c_str(), "AutoDOS2 — Fatal Error", MB_OK | MB_ICONERROR);
#else
    std::fprintf(stderr, "FATAL: %s\n", msg.c_str());
#endif
}

int main(int /*argc*/, char* /*argv*/[])
{
    try {
        AutoDOS2::App app;

        if (!app.init()) {
            fatalBox("app.init() failed.\n\nCheck that SDL2.dll, games.json, and 7za.exe are next to AutoDOS2.exe.");
            return 1;
        }

        app.run();
        return 0;
    }
    catch (const std::exception& ex) {
        fatalBox(std::string("Unhandled exception:\n\n") + ex.what());
        return 1;
    }
    catch (...) {
        fatalBox("Unknown fatal exception. Check that all DLLs are present.");
        return 1;
    }
}