#pragma once

#include <SDL.h>
#include <string>

// Forward-declare ImGuiIO to avoid including imgui.h in every TU.
struct ImGuiIO;

namespace AutoDOS2 {

// Target window dimensions (Phase 01).
constexpr int WINDOW_W = 900;
constexpr int WINDOW_H = 700;
constexpr int TARGET_FPS = 60;

// ─────────────────────────────────────────────────────────────────────────────
// App — owns the SDL window/renderer and the ImGui context.
// ─────────────────────────────────────────────────────────────────────────────
class App {
public:
    App();
    ~App();

    // Returns false if initialisation failed (error logged to stderr).
    bool init();

    // Runs the event/render loop until the user closes the window.
    void run();

private:
    // ── SDL handles ───────────────────────────────────────────────────────────
    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    // ── State ─────────────────────────────────────────────────────────────────
    bool m_running        = false;
    bool m_showDemoWindow = false;   // toggled by F1

    // ── Internals ─────────────────────────────────────────────────────────────
    void processEvents();
    void update();
    void render();
    void renderImGui();
    void cleanup();
};

} // namespace AutoDOS2
