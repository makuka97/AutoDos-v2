#pragma once

#include "gamedb.h"

#include <SDL.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace AutoDOS2 {

constexpr int WINDOW_W  = 900;
constexpr int WINDOW_H  = 700;
constexpr int TARGET_FPS = 60;

// Card dimensions
constexpr float CARD_W  = 180.0f;
constexpr float CARD_H  = 240.0f;
constexpr float CARD_PAD = 12.0f;

// ─────────────────────────────────────────────────────────────────────────────
// CoverCache — loads PNG/JPG cover art as SDL_Textures, caches by path.
// Returns a placeholder coloured texture when the file is missing.
// ─────────────────────────────────────────────────────────────────────────────
class CoverCache {
public:
    CoverCache() = default;
    ~CoverCache();

    void setRenderer(SDL_Renderer* r) { m_renderer = r; }

    // Returns a valid texture (never null — falls back to placeholder).
    SDL_Texture* get(const std::string& path);

    void clear();

private:
    SDL_Renderer* m_renderer = nullptr;
    std::unordered_map<std::string, SDL_Texture*> m_cache;
    SDL_Texture* m_placeholder = nullptr;

    SDL_Texture* makePlaceholder();
};

// ─────────────────────────────────────────────────────────────────────────────
// App
// ─────────────────────────────────────────────────────────────────────────────
class App {
public:
    App();
    ~App();

    bool init();
    void run();

private:
    // SDL
    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    // DB + library
    GameDB                  m_db;
    std::vector<GameRecord> m_allGames;    // full list from DB
    std::vector<GameRecord> m_filtered;   // after search filter
    int                     m_selected = -1; // selected game id (-1 = none)

    // Cover art
    CoverCache m_covers;

    // UI state
    bool m_running        = false;
    bool m_showDemoWindow = false;
    char m_searchBuf[256] = {};

    // Internals
    void processEvents();
    void update();
    void render();
    void renderImGui();
    void renderTopBar();
    void renderGrid();
    void renderBottomBar();
    void applySearch();
    void refreshLibrary();
    void cleanup();
};

} // namespace AutoDOS2