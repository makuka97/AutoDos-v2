#pragma once

#include "gamedb.h"
#include "ingest.h"
#include "settings.h"
#include "artfetcher.h"

#include <SDL.h>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace AutoDOS2 {

constexpr int   WINDOW_W   = 900;
constexpr int   WINDOW_H   = 700;
constexpr float CARD_W     = 180.0f;
constexpr float CARD_H     = 240.0f;
constexpr float CARD_PAD   = 12.0f;

// ── CoverCache ────────────────────────────────────────────────────────────────
class CoverCache {
public:
    CoverCache() = default;
    ~CoverCache();
    void         setRenderer(SDL_Renderer* r) { m_renderer = r; }
    SDL_Texture* get(const std::string& path);
    void         clear();
private:
    SDL_Renderer* m_renderer    = nullptr;
    std::unordered_map<std::string, SDL_Texture*> m_cache;
    SDL_Texture*  m_placeholder = nullptr;
    SDL_Texture*  makePlaceholder();
};

// ── IngestStatus — shared between worker thread and UI ───────────────────────
struct IngestStatus {
    std::atomic<bool>  busy    {false};
    std::atomic<int>   progress{0};
    std::mutex         mtx;
    std::string        message;
    std::string        lastError;
    bool               resultReady {false};
    AnalyzeResult      result;
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

    // DB + library
    GameDB                  m_db;
    GameDatabase            m_gameJson;
    Ingestor                m_ingestor;
    std::vector<GameRecord> m_allGames;
    std::vector<GameRecord> m_filtered;
    int                     m_selected = -1;

    // Cover art
    CoverCache m_covers;

    // Paths needed at runtime
    std::filesystem::path m_dosboxPath;
    std::filesystem::path m_confsRoot;

    // Launch state
    enum class LaunchState { Idle, Running, Error };
    LaunchState m_launchState = LaunchState::Idle;
    std::string m_launchError;

    // Ingest worker
    IngestStatus  m_ingest;
    std::thread   m_ingestThread;

    // UI state
    bool m_running          = false;
    bool m_showDemoWindow   = false;
    bool m_showSettings     = false;
    bool m_showAbout        = false;
    char m_searchBuf[256]   = {};
    AppSettings m_settings;
    ArtFetcher  m_artFetcher;
    std::filesystem::path m_configPath;

    // Internals
    void processEvents();
    void update();
    void render();
    void renderImGui();
    void renderTopBar(float winW);
    void renderSidebar();
    void renderGrid();
    void renderBottomBar(float winW);
    void renderIngestOverlay();
    void launchGame(const GameRecord& rec);
    void renderLaunchError();
    void renderSettingsPanel();
    void renderAboutPanel();
    void applySearch();
    void refreshLibrary();
    void startIngest(const std::string& path);
    void cleanup();
};

} // namespace AutoDOS2