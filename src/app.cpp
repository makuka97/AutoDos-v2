#pragma once

#include "gamedb.h"

#include <SDL.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace AutoDOS2 {

constexpr int   WINDOW_W  = 900;
constexpr int   WINDOW_H  = 700;
constexpr int   TARGET_FPS = 60;
constexpr float CARD_W    = 180.0f;
constexpr float CARD_H    = 240.0f;
constexpr float CARD_PAD  = 12.0f;

// ── CoverCache ────────────────────────────────────────────────────────────────
class CoverCache {
public:
    CoverCache() = default;
    ~CoverCache();
    void         setRenderer(SDL_Renderer* r) { m_renderer = r; }
    SDL_Texture* get(const std::string& path);
    void         clear();
private:
    SDL_Renderer* m_renderer   = nullptr;
    std::unordered_map<std::string, SDL_Texture*> m_cache;
    SDL_Texture*  m_placeholder = nullptr;
    SDL_Texture*  makePlaceholder();
};

// ── App ───────────────────────────────────────────────────────────────────────
class App {
public:
    App();
    ~App();
    bool init();
    void run();

private:
    SDL_Window*   m_window   = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    GameDB                  m_db;
    std::vector<GameRecord> m_allGames;
    std::vector<GameRecord> m_filtered;
    int                     m_selected    = -1;

    CoverCache m_covers;

    bool m_running        = false;
    bool m_showDemoWindow = false;
    char m_searchBuf[256] = {};

    void processEvents();
    void update();
    void render();
    void renderImGui();
    void renderTopBar();
    void renderSidebar();
    void renderGrid();
    void renderBottomBar();
    void applySearch();
    void refreshLibrary();
    void cleanup();
};

} // namespace AutoDOS2