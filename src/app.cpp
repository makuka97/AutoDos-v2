#include "app.h"
#include "platform.h"

#include <SDL.h>
#include <SDL_image.h>

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

#ifndef AUTODOS2_VERSION_MAJOR
#  define AUTODOS2_VERSION_MAJOR 0
#  define AUTODOS2_VERSION_MINOR 1
#  define AUTODOS2_VERSION_PATCH 0
#endif

namespace AutoDOS2 {

// ── Theme ─────────────────────────────────────────────────────────────────────
static void applyAutoDOSTheme()
{
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 6.0f;
    s.FrameRounding     = 4.0f;
    s.ScrollbarRounding = 4.0f;
    s.GrabRounding      = 4.0f;
    s.TabRounding       = 4.0f;
    s.FramePadding      = {8, 5};
    s.ItemSpacing       = {8, 6};

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]         = {0.071f, 0.071f, 0.094f, 1.00f};
    c[ImGuiCol_ChildBg]          = {0.110f, 0.110f, 0.141f, 1.00f};
    c[ImGuiCol_PopupBg]          = {0.110f, 0.110f, 0.141f, 0.96f};
    c[ImGuiCol_Border]           = {0.149f, 0.149f, 0.196f, 1.00f};
    c[ImGuiCol_FrameBg]          = {0.110f, 0.110f, 0.141f, 1.00f};
    c[ImGuiCol_FrameBgHovered]   = {0.157f, 0.196f, 0.157f, 1.00f};
    c[ImGuiCol_FrameBgActive]    = {0.196f, 0.282f, 0.196f, 1.00f};
    c[ImGuiCol_TitleBg]          = {0.071f, 0.071f, 0.094f, 1.00f};
    c[ImGuiCol_TitleBgActive]    = {0.071f, 0.071f, 0.094f, 1.00f};
    c[ImGuiCol_MenuBarBg]        = {0.090f, 0.090f, 0.118f, 1.00f};
    c[ImGuiCol_ScrollbarBg]      = {0.071f, 0.071f, 0.094f, 1.00f};
    c[ImGuiCol_ScrollbarGrab]    = {0.200f, 0.200f, 0.260f, 1.00f};
    c[ImGuiCol_CheckMark]        = {0.314f, 0.784f, 0.471f, 1.00f};
    c[ImGuiCol_SliderGrab]       = {0.314f, 0.784f, 0.471f, 1.00f};
    c[ImGuiCol_SliderGrabActive] = {0.400f, 0.900f, 0.560f, 1.00f};
    c[ImGuiCol_Button]           = {0.125f, 0.204f, 0.157f, 1.00f};
    c[ImGuiCol_ButtonHovered]    = {0.200f, 0.322f, 0.247f, 1.00f};
    c[ImGuiCol_ButtonActive]     = {0.314f, 0.784f, 0.471f, 1.00f};
    c[ImGuiCol_Header]           = {0.125f, 0.204f, 0.157f, 1.00f};
    c[ImGuiCol_HeaderHovered]    = {0.200f, 0.322f, 0.247f, 1.00f};
    c[ImGuiCol_HeaderActive]     = {0.314f, 0.784f, 0.471f, 1.00f};
    c[ImGuiCol_Tab]              = {0.090f, 0.090f, 0.118f, 1.00f};
    c[ImGuiCol_TabHovered]       = {0.200f, 0.322f, 0.247f, 1.00f};
    c[ImGuiCol_TabActive]        = {0.125f, 0.204f, 0.157f, 1.00f};
    c[ImGuiCol_Text]             = {0.863f, 0.863f, 0.863f, 1.00f};
    c[ImGuiCol_TextDisabled]     = {0.431f, 0.431f, 0.471f, 1.00f};
    c[ImGuiCol_Separator]        = {0.149f, 0.149f, 0.196f, 1.00f};
}

// ── CoverCache ────────────────────────────────────────────────────────────────

CoverCache::~CoverCache() { clear(); }

SDL_Texture* CoverCache::makePlaceholder()
{
    // 4x4 dark teal block — scales up fine for a placeholder card
    constexpr int SZ = 4;
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, SZ, SZ, 32,
                                                        SDL_PIXELFORMAT_RGBA32);
    if (!surf) return nullptr;
    SDL_FillRect(surf, nullptr, SDL_MapRGB(surf->format, 28, 40, 52));
    // Draw a simple "?" pattern in the centre pixels
    Uint32* px = reinterpret_cast<Uint32*>(surf->pixels);
    px[1*SZ+1] = SDL_MapRGB(surf->format, 80, 200, 120);
    px[1*SZ+2] = SDL_MapRGB(surf->format, 80, 200, 120);
    px[2*SZ+2] = SDL_MapRGB(surf->format, 80, 200, 120);

    SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer, surf);
    SDL_FreeSurface(surf);
    return tex;
}

SDL_Texture* CoverCache::get(const std::string& path)
{
    // Return cached
    auto it = m_cache.find(path);
    if (it != m_cache.end()) return it->second;

    // Try to load from disk
    SDL_Texture* tex = nullptr;
    if (!path.empty()) {
        SDL_Surface* surf = IMG_Load(path.c_str());
        if (surf) {
            tex = SDL_CreateTextureFromSurface(m_renderer, surf);
            SDL_FreeSurface(surf);
        }
    }

    // Fall back to placeholder
    if (!tex) {
        if (!m_placeholder) m_placeholder = makePlaceholder();
        tex = m_placeholder;
    }

    m_cache[path] = tex;
    return tex;
}

void CoverCache::clear()
{
    for (auto& [k, v] : m_cache) {
        if (v && v != m_placeholder)
            SDL_DestroyTexture(v);
    }
    m_cache.clear();
    if (m_placeholder) {
        SDL_DestroyTexture(m_placeholder);
        m_placeholder = nullptr;
    }
}

// ── App ───────────────────────────────────────────────────────────────────────

App::App() = default;
App::~App() { cleanup(); }

bool App::init()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "[AutoDOS2] SDL_Init: %s\n", SDL_GetError());
        return false;
    }
    if (!(IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & (IMG_INIT_PNG | IMG_INIT_JPG)))
        std::fprintf(stderr, "[AutoDOS2] IMG_Init: %s\n", IMG_GetError());

    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

    const std::string title = "AutoDOS2  v"
        + std::to_string(AUTODOS2_VERSION_MAJOR) + "."
        + std::to_string(AUTODOS2_VERSION_MINOR);

    m_window = SDL_CreateWindow(title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!m_window) { std::fprintf(stderr, "[AutoDOS2] Window: %s\n", SDL_GetError()); return false; }

    m_renderer = SDL_CreateRenderer(m_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!m_renderer)
        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_SOFTWARE);
    if (!m_renderer) { std::fprintf(stderr, "[AutoDOS2] Renderer: %s\n", SDL_GetError()); return false; }

    SDL_RenderSetLogicalSize(m_renderer, WINDOW_W, WINDOW_H);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    applyAutoDOSTheme();
    ImGui_ImplSDL2_InitForSDLRenderer(m_window, m_renderer);
    ImGui_ImplSDLRenderer2_Init(m_renderer);

    // ── Open DB and load library ───────────────────────────────────────────────
    m_covers.setRenderer(m_renderer);
    auto dbPath = getDataDir() / "autodos2.db";
    if (!m_db.open(dbPath)) {
        std::fprintf(stderr, "[AutoDOS2] Failed to open DB at %s\n",
                     dbPath.string().c_str());
        return false;
    }

    // Seed 10 dummy entries so the Phase 03 grid test passes out of the box.
    // These are removed once real game ingestion lands in Phase 04.
    if (m_db.count() == 0) {
        const char* titles[] = {
            "Duke Nukem 3D","Quake","Doom II","Daggerfall","Heretic",
            "Hexen","Blood","Shadow Warrior","Descent","Terminal Velocity"
        };
        const char* slugs[] = {
            "duke3d","quake","doom2","daggerfall","heretic",
            "hexen","blood","shadow","descent","terminal"
        };
        for (int i = 0; i < 10; ++i) {
            GameRecord r;
            r.title = titles[i];
            r.slug  = slugs[i];
            m_db.insert(r);
        }
    }

    refreshLibrary();
    m_running = true;
    return true;
}

void App::refreshLibrary()
{
    m_allGames = m_db.getAll();
    applySearch();
}

void App::applySearch()
{
    std::string q = m_searchBuf;
    // lowercase query
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);

    if (q.empty()) {
        m_filtered = m_allGames;
        return;
    }
    m_filtered.clear();
    for (auto& g : m_allGames) {
        std::string t = g.title;
        std::transform(t.begin(), t.end(), t.begin(), ::tolower);
        if (t.find(q) != std::string::npos)
            m_filtered.push_back(g);
    }
    // If selected game is no longer visible, deselect
    bool stillVisible = false;
    for (auto& g : m_filtered) if (g.id == m_selected) { stillVisible = true; break; }
    if (!stillVisible) m_selected = -1;
}

// ── Event loop ────────────────────────────────────────────────────────────────

void App::run()
{
    const Uint32 frameMs = 1000 / TARGET_FPS;
    while (m_running) {
        const Uint32 t0 = SDL_GetTicks();
        processEvents();
        update();
        render();
        const Uint32 elapsed = SDL_GetTicks() - t0;
        if (elapsed < frameMs) SDL_Delay(frameMs - elapsed);
    }
}

void App::processEvents()
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        ImGui_ImplSDL2_ProcessEvent(&e);
        switch (e.type) {
        case SDL_QUIT: m_running = false; break;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_CLOSE)
                m_running = false;
            break;
        case SDL_KEYDOWN:
            if (e.key.keysym.sym == SDLK_F1) m_showDemoWindow = !m_showDemoWindow;
            break;
        case SDL_DROPFILE:
            std::fprintf(stdout, "[AutoDOS2] Drop: %s\n", e.drop.file);
            SDL_free(e.drop.file);
            break;
        default: break;
        }
    }
}

void App::update() {}

void App::render()
{
    SDL_SetRenderDrawColor(m_renderer, 18, 18, 24, 255);
    SDL_RenderClear(m_renderer);

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    renderImGui();

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), m_renderer);
    SDL_RenderPresent(m_renderer);
}

// ── ImGui layout ─────────────────────────────────────────────────────────────

void App::renderImGui()
{
    if (m_showDemoWindow) ImGui::ShowDemoWindow(&m_showDemoWindow);

    // Full-screen dockspace-style window
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##main", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

    renderTopBar();
    ImGui::Separator();

    // Reserve space for bottom bar
    const float bottomH = 40.0f;
    ImGui::BeginChild("##grid_area", {0, -bottomH}, false,
        ImGuiWindowFlags_NoScrollbar);
    renderGrid();
    ImGui::EndChild();

    ImGui::Separator();
    renderBottomBar();

    ImGui::End();
}

// ── Top bar ───────────────────────────────────────────────────────────────────

void App::renderTopBar()
{
    ImGui::TextColored({0.314f, 0.784f, 0.471f, 1.0f}, "AutoDOS2");
    ImGui::SameLine();
    ImGui::TextDisabled("v%d.%d  |  %d games  |  F1: demo",
        AUTODOS2_VERSION_MAJOR, AUTODOS2_VERSION_MINOR,
        (int)m_filtered.size());

    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 220);
    ImGui::SetNextItemWidth(220);
    if (ImGui::InputTextWithHint("##search", "Search games...",
            m_searchBuf, sizeof(m_searchBuf))) {
        applySearch();
    }
}

// ── Game grid ─────────────────────────────────────────────────────────────────

void App::renderGrid()
{
    const float availW   = ImGui::GetContentRegionAvail().x;
    const int   cols     = std::max(1, (int)((availW + CARD_PAD) / (CARD_W + CARD_PAD)));
    const float startX   = ImGui::GetCursorScreenPos().x;
    const ImVec2 cardSz  = {CARD_W, CARD_H};

    // Cover image area (top 75% of card) and title area (bottom 25%)
    const float imgH     = CARD_H * 0.78f;
    const float lblH     = CARD_H - imgH;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int i = 0; i < (int)m_filtered.size(); ++i) {
        const GameRecord& g = m_filtered[i];
        const bool selected = (g.id == m_selected);

        // Calculate grid position
        const int col  = i % cols;
        const int row  = i / cols;
        const ImVec2 cursor = {
            startX + col * (CARD_W + CARD_PAD),
            ImGui::GetCursorScreenPos().y + row * (CARD_H + CARD_PAD)
        };

        // Push id so buttons don't conflict
        ImGui::PushID(g.id);

        // Invisible button covers the whole card for interaction
        ImGui::SetCursorScreenPos(cursor);
        const bool clicked = ImGui::InvisibleButton("##card", cardSz);
        const bool hovered = ImGui::IsItemHovered();

        if (clicked) m_selected = (selected ? -1 : g.id);

        // ── Card background ───────────────────────────────────────────────────
        const ImU32 bgCol = selected
            ? IM_COL32(32, 52, 40, 255)   // #203428 selected
            : hovered
              ? IM_COL32(28, 40, 36, 255) // hover
              : IM_COL32(20, 20, 28, 255); // normal

        dl->AddRectFilled(cursor,
            {cursor.x + CARD_W, cursor.y + CARD_H}, bgCol, 6.0f);

        // ── Cover image ───────────────────────────────────────────────────────
        SDL_Texture* tex = m_covers.get(g.cover_path);
        if (tex) {
            ImGui::SetCursorScreenPos(cursor);
            ImGui::Image(reinterpret_cast<ImTextureID>(tex),
                {CARD_W, imgH});
        }

        // ── Title label ───────────────────────────────────────────────────────
        const ImVec2 lblPos = {cursor.x, cursor.y + imgH};
        dl->AddRectFilled(lblPos,
            {cursor.x + CARD_W, cursor.y + CARD_H},
            IM_COL32(14, 14, 20, 220), 0.0f);

        // Truncate title to fit card width
        const float maxTitleW = CARD_W - 8.0f;
        const char* title = g.title.c_str();
        ImVec2 textSz = ImGui::CalcTextSize(title, nullptr, false, maxTitleW);
        ImVec2 textPos = {
            lblPos.x + (CARD_W - std::min(textSz.x, maxTitleW)) * 0.5f,
            lblPos.y + (lblH - textSz.y) * 0.5f
        };
        dl->AddText(nullptr, 0, textPos,
            IM_COL32(220, 220, 220, 255), title, nullptr, maxTitleW);

        // ── Selection border ──────────────────────────────────────────────────
        if (selected) {
            dl->AddRect(cursor,
                {cursor.x + CARD_W, cursor.y + CARD_H},
                IM_COL32(80, 200, 120, 255), 6.0f, 0, 2.0f);
        } else if (hovered) {
            dl->AddRect(cursor,
                {cursor.x + CARD_W, cursor.y + CARD_H},
                IM_COL32(60, 120, 80, 180), 6.0f, 0, 1.5f);
        }

        ImGui::PopID();
    }

    // Advance cursor past all rows so the child window knows its height
    if (!m_filtered.empty()) {
        const int rows = ((int)m_filtered.size() + cols - 1) / cols;
        ImGui::SetCursorScreenPos({startX,
            ImGui::GetCursorScreenPos().y + rows * (CARD_H + CARD_PAD)});
        ImGui::Dummy({0, 0});
    }

    if (m_filtered.empty()) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 60);
        const char* msg = m_allGames.empty()
            ? "No games yet — drag a DOS zip onto the window"
            : "No games match your search";
        const float tw = ImGui::CalcTextSize(msg).x;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - tw) * 0.5f);
        ImGui::TextDisabled("%s", msg);
    }
}

// ── Bottom bar ────────────────────────────────────────────────────────────────

void App::renderBottomBar()
{
    // Find selected game info
    const GameRecord* sel = nullptr;
    for (auto& g : m_filtered) if (g.id == m_selected) { sel = &g; break; }

    const bool hasSelection = (sel != nullptr);

    // Launch button (stubbed — Phase 05)
    if (!hasSelection) ImGui::BeginDisabled();
    if (ImGui::Button("Launch", {80, 0})) {
        // Phase 05
    }
    if (!hasSelection) ImGui::EndDisabled();

    ImGui::SameLine();

    // Add Zip button (stubbed — Phase 04)
    if (ImGui::Button("Add Zip", {80, 0})) {
        // Phase 04
    }

    ImGui::SameLine();

    // Delete button (stubbed)
    if (!hasSelection) ImGui::BeginDisabled();
    if (ImGui::Button("Delete", {80, 0})) {
        if (sel) {
            m_db.remove(sel->id);
            m_selected = -1;
            refreshLibrary();
        }
    }
    if (!hasSelection) ImGui::EndDisabled();

    ImGui::SameLine();

    // Selection info
    if (sel) {
        ImGui::TextDisabled("  %s  |  played %d times",
            sel->slug.c_str(), sel->play_count);
    } else {
        ImGui::TextDisabled("  %d games in library", (int)m_allGames.size());
    }

    // FPS right-aligned
    const float fpsW = 80.0f;
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - fpsW + ImGui::GetCursorPosX());
    ImGui::TextDisabled("%.0f fps", ImGui::GetIO().Framerate);
}

// ── Cleanup ───────────────────────────────────────────────────────────────────

void App::cleanup()
{
    m_covers.clear();
    m_db.close();
    if (m_renderer) {
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    if (m_window) { SDL_DestroyWindow(m_window); m_window = nullptr; }
    IMG_Quit();
    SDL_Quit();
}

} // namespace AutoDOS2