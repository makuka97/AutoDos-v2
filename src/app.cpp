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

static constexpr ImVec4 ACCENT      = {0.314f, 0.784f, 0.471f, 1.0f};
static constexpr ImU32  ACCENT32    = IM_COL32(80, 200, 120, 255);
static constexpr ImU32  ACCENT32DIM = IM_COL32(60, 140, 90, 160);

static void applyAutoDOSTheme()
{
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding     = 0.0f;
    s.ChildRounding      = 6.0f;
    s.FrameRounding      = 5.0f;
    s.ScrollbarRounding  = 4.0f;
    s.GrabRounding       = 4.0f;
    s.PopupRounding      = 6.0f;
    s.FramePadding       = {10, 6};
    s.ItemSpacing        = {8, 6};
    s.ScrollbarSize      = 10.0f;
    s.WindowBorderSize   = 0.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]            = {0.071f, 0.071f, 0.094f, 1.00f};
    c[ImGuiCol_ChildBg]             = {0.090f, 0.090f, 0.118f, 1.00f};
    c[ImGuiCol_PopupBg]             = {0.110f, 0.110f, 0.141f, 0.97f};
    c[ImGuiCol_Border]              = {0.149f, 0.149f, 0.196f, 1.00f};
    c[ImGuiCol_FrameBg]             = {0.130f, 0.130f, 0.165f, 1.00f};
    c[ImGuiCol_FrameBgHovered]      = {0.157f, 0.220f, 0.180f, 1.00f};
    c[ImGuiCol_FrameBgActive]       = {0.196f, 0.282f, 0.220f, 1.00f};
    c[ImGuiCol_TitleBg]             = {0.055f, 0.055f, 0.075f, 1.00f};
    c[ImGuiCol_TitleBgActive]       = {0.055f, 0.055f, 0.075f, 1.00f};
    c[ImGuiCol_ScrollbarBg]         = {0.055f, 0.055f, 0.075f, 1.00f};
    c[ImGuiCol_ScrollbarGrab]       = {0.220f, 0.220f, 0.280f, 1.00f};
    c[ImGuiCol_ScrollbarGrabHovered]= {0.314f, 0.784f, 0.471f, 0.6f};
    c[ImGuiCol_CheckMark]           = ACCENT;
    c[ImGuiCol_Button]              = {0.125f, 0.204f, 0.157f, 1.00f};
    c[ImGuiCol_ButtonHovered]       = {0.200f, 0.322f, 0.247f, 1.00f};
    c[ImGuiCol_ButtonActive]        = ACCENT;
    c[ImGuiCol_Text]                = {0.863f, 0.863f, 0.863f, 1.00f};
    c[ImGuiCol_TextDisabled]        = {0.431f, 0.431f, 0.471f, 1.00f};
    c[ImGuiCol_Separator]           = {0.149f, 0.149f, 0.196f, 1.00f};
}

// ── CoverCache ────────────────────────────────────────────────────────────────

CoverCache::~CoverCache() { clear(); }

SDL_Texture* CoverCache::makePlaceholder()
{
    constexpr int W = 180, H = 187;
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surf) return nullptr;
    SDL_FillRect(surf, nullptr, SDL_MapRGB(surf->format, 22, 28, 38));
    SDL_Rect inner = {W/4, H/4, W/2, H/2};
    SDL_FillRect(surf, &inner, SDL_MapRGB(surf->format, 30, 40, 54));
    SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer, surf);
    SDL_FreeSurface(surf);
    return tex;
}

SDL_Texture* CoverCache::get(const std::string& path)
{
    auto it = m_cache.find(path);
    if (it != m_cache.end()) return it->second;
    SDL_Texture* tex = nullptr;
    if (!path.empty()) {
        SDL_Surface* surf = IMG_Load(path.c_str());
        if (surf) { tex = SDL_CreateTextureFromSurface(m_renderer, surf); SDL_FreeSurface(surf); }
    }
    if (!tex) { if (!m_placeholder) m_placeholder = makePlaceholder(); tex = m_placeholder; }
    m_cache[path] = tex;
    return tex;
}

void CoverCache::clear()
{
    for (auto& [k, v] : m_cache) if (v && v != m_placeholder) SDL_DestroyTexture(v);
    m_cache.clear();
    if (m_placeholder) { SDL_DestroyTexture(m_placeholder); m_placeholder = nullptr; }
}

// ── App ───────────────────────────────────────────────────────────────────────

App::App()  = default;
App::~App() { cleanup(); }

bool App::init()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "[AutoDOS2] SDL_Init: %s\n", SDL_GetError()); return false;
    }
    if (!(IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & (IMG_INIT_PNG | IMG_INIT_JPG)))
        std::fprintf(stderr, "[AutoDOS2] IMG_Init: %s\n", IMG_GetError());

    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

    m_window = SDL_CreateWindow("AutoDOS2",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!m_window) return false;

    m_renderer = SDL_CreateRenderer(m_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!m_renderer)
        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_SOFTWARE);
    if (!m_renderer) return false;

    // Do NOT set logical size — let it track actual window size so resize works
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    applyAutoDOSTheme();
    ImGui_ImplSDL2_InitForSDLRenderer(m_window, m_renderer);
    ImGui_ImplSDLRenderer2_Init(m_renderer);

    m_covers.setRenderer(m_renderer);
    auto dbPath = getDataDir() / "autodos2.db";
    if (!m_db.open(dbPath)) return false;

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
            GameRecord r; r.title = titles[i]; r.slug = slugs[i]; m_db.insert(r);
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
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);
    if (q.empty()) { m_filtered = m_allGames; return; }
    m_filtered.clear();
    for (auto& g : m_allGames) {
        std::string t = g.title;
        std::transform(t.begin(), t.end(), t.begin(), ::tolower);
        if (t.find(q) != std::string::npos) m_filtered.push_back(g);
    }
    bool ok = false;
    for (auto& g : m_filtered) if (g.id == m_selected) { ok = true; break; }
    if (!ok) m_selected = -1;
}

void App::run()
{
    while (m_running) { processEvents(); update(); render(); }
}

void App::processEvents()
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        ImGui_ImplSDL2_ProcessEvent(&e);
        if (e.type == SDL_QUIT) m_running = false;
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) m_running = false;
        if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F1) m_showDemoWindow = !m_showDemoWindow;
        if (e.type == SDL_DROPFILE) { std::fprintf(stdout,"[Drop] %s\n",e.drop.file); SDL_free(e.drop.file); }
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
    if (m_showDemoWindow) ImGui::ShowDemoWindow(&m_showDemoWindow);
    renderImGui();
    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), m_renderer);
    SDL_RenderPresent(m_renderer);
}

// ── Layout ────────────────────────────────────────────────────────────────────

void App::renderImGui()
{
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0,0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0,0});
    ImGui::Begin("##root", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();

    const float topH    = 56.0f;
    const float bottomH = 50.0f;
    const float midH    = io.DisplaySize.y - topH - bottomH;

    renderTopBar(io.DisplaySize.x);

    // Middle row: sidebar | grid
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0,0});
    ImGui::BeginChild("##mid", {0, midH}, false, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();
    renderSidebar();
    ImGui::SameLine(0, 0);
    renderGrid();
    ImGui::EndChild();

    renderBottomBar(io.DisplaySize.x);
    ImGui::End();
}

// ── Top bar ───────────────────────────────────────────────────────────────────

void App::renderTopBar(float winW)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{0.055f, 0.055f, 0.075f, 1.0f});
    ImGui::BeginChild("##top", {0, 56}, false, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();

    // Title left
    ImGui::SetCursorPos({14, 16});
    ImGui::TextColored(ACCENT, "AutoDOS2");
    ImGui::SameLine();
    ImGui::SetCursorPosY(16);
    ImGui::TextDisabled("v%d.%d", AUTODOS2_VERSION_MAJOR, AUTODOS2_VERSION_MINOR);

    // Search right — use TableNextColumn trick: just SetCursorPosX
    const float searchW = 260.0f;
    const float searchX = winW - searchW - 14.0f;
    ImGui::SetCursorPos({searchX, 14});
    ImGui::SetNextItemWidth(searchW);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
    if (ImGui::InputTextWithHint("##search", "Search Games...",
            m_searchBuf, sizeof(m_searchBuf)))
        applySearch();
    ImGui::PopStyleVar();

    ImGui::EndChild();

    // Bottom divider
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddLine(p, {p.x + winW, p.y}, IM_COL32(38,38,60,255), 1.0f);
}

// ── Sidebar ───────────────────────────────────────────────────────────────────

void App::renderSidebar()
{
    const float sideW = 200.0f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{0.055f,0.060f,0.080f,1.0f});
    ImGui::BeginChild("##sidebar", {sideW, 0}, false, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();

    const GameRecord* sel = nullptr;
    for (auto& g : m_filtered) if (g.id == m_selected) { sel = &g; break; }

    if (sel) {
        // Large cover art
        SDL_Texture* tex = m_covers.get(sel->cover_path);
        if (tex) {
            const float imgW = sideW - 16.0f;
            const float imgH = imgW * 1.2f;
            ImGui::SetCursorPos({8, 10});
            ImGui::Image(reinterpret_cast<ImTextureID>(tex), {imgW, imgH});
        }
        ImGui::SetCursorPosX(10);
        ImGui::PushTextWrapPos(sideW - 10);
        ImGui::TextColored(ACCENT, "%s", sel->title.c_str());
        ImGui::Spacing();
        if (!sel->platform.empty())
            ImGui::TextDisabled("Platform: %s", sel->platform.c_str());
        ImGui::TextDisabled("Played: %d times", sel->play_count);
        if (!sel->last_played.empty())
            ImGui::TextDisabled("Last: %.10s", sel->last_played.c_str());
        ImGui::PopTextWrapPos();
    } else {
        ImGui::SetCursorPos({10, 16});
        ImGui::TextColored(ACCENT, "Library");
        ImGui::SetCursorPosX(10);
        ImGui::TextDisabled("%d games", (int)m_allGames.size());
        ImGui::Spacing();
        ImGui::SetCursorPosX(10);
        ImGui::TextDisabled("Drag a DOS zip\nonto the window\nto add a game.");
    }

    // Right border
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 tl = ImGui::GetWindowPos();
    float  h  = ImGui::GetWindowHeight();
    dl->AddLine({tl.x+sideW-1, tl.y}, {tl.x+sideW-1, tl.y+h},
        IM_COL32(38,38,60,255), 1.0f);

    ImGui::EndChild();
}

// ── Game grid ─────────────────────────────────────────────────────────────────
// Key fix: draw everything via DrawList ONLY — never call ImGui::Image inside
// a column after InvisibleButton, which shifts the layout cursor.

void App::renderGrid()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {CARD_PAD, CARD_PAD});
    ImGui::BeginChild("##grid", {0,0}, false);
    ImGui::PopStyleVar();

    const float availW = ImGui::GetContentRegionAvail().x;
    const int   cols   = std::max(1, (int)((availW + CARD_PAD) / (CARD_W + CARD_PAD)));
    const float imgH   = CARD_H * 0.76f;
    const float lblH   = CARD_H - imgH;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int i = 0; i < (int)m_filtered.size(); ++i) {
        const GameRecord& g   = m_filtered[i];
        const bool        sel = (g.id == m_selected);

        if (i % cols != 0) ImGui::SameLine(0, CARD_PAD);

        ImGui::PushID(g.id);

        // InvisibleButton defines the card's rect and handles hover/click.
        // We record its screen position BEFORE calling it, then draw everything
        // via DrawList so the cursor is never moved by Image calls.
        ImVec2 cardPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##card", {CARD_W, CARD_H});
        const bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) m_selected = sel ? -1 : g.id;

        // Card background
        ImU32 bgCol = sel
            ? IM_COL32(32, 58, 42, 255)
            : hovered ? IM_COL32(28, 38, 38, 255)
                      : IM_COL32(18, 22, 30, 255);
        dl->AddRectFilled(cardPos, {cardPos.x+CARD_W, cardPos.y+CARD_H}, bgCol, 8.0f);

        // Cover image — drawn via DrawList directly, NOT via ImGui::Image
        SDL_Texture* tex = m_covers.get(g.cover_path);
        if (tex) {
            dl->AddImage(
                reinterpret_cast<ImTextureID>(tex),
                cardPos,
                {cardPos.x + CARD_W, cardPos.y + imgH}
            );
        }

        // Gradient overlay at bottom of image
        ImVec2 gradTL = {cardPos.x,       cardPos.y + imgH - 24.0f};
        ImVec2 gradBR = {cardPos.x+CARD_W, cardPos.y + CARD_H};
        dl->AddRectFilledMultiColor(gradTL, gradBR,
            IM_COL32(0,0,0,0), IM_COL32(0,0,0,0),
            IM_COL32(0,0,0,220), IM_COL32(0,0,0,220));

        // Title text
        ImVec2 textPos = {cardPos.x + 8.0f, cardPos.y + imgH + (lblH - 14.0f)*0.35f};
        dl->AddText(nullptr, 0, textPos, IM_COL32(225,225,225,255),
            g.title.c_str(), nullptr, CARD_W - 10.0f);

        // Border
        if (sel)
            dl->AddRect(cardPos, {cardPos.x+CARD_W, cardPos.y+CARD_H}, ACCENT32, 8.0f, 0, 2.5f);
        else if (hovered)
            dl->AddRect(cardPos, {cardPos.x+CARD_W, cardPos.y+CARD_H}, ACCENT32DIM, 8.0f, 0, 1.5f);

        ImGui::PopID();
    }

    if (m_filtered.empty()) {
        ImGui::SetCursorPosY(80);
        const char* msg = m_allGames.empty()
            ? "No games yet — drag a DOS zip onto the window"
            : "No games match your search";
        float tw = ImGui::CalcTextSize(msg).x;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - tw) * 0.5f);
        ImGui::TextDisabled("%s", msg);
    }

    ImGui::EndChild();
}

// ── Bottom bar ────────────────────────────────────────────────────────────────

void App::renderBottomBar(float winW)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{0.055f,0.055f,0.075f,1.0f});
    ImGui::BeginChild("##bot", {0,0}, false, ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetWindowPos();
    dl->AddLine(p, {p.x+winW, p.y}, IM_COL32(38,38,60,255), 1.0f);

    ImGui::SetCursorPos({10, 10});

    const GameRecord* sel = nullptr;
    for (auto& g : m_filtered) if (g.id == m_selected) { sel = &g; break; }
    const bool hasSel = (sel != nullptr);

    // Launch
    ImGui::PushStyleColor(ImGuiCol_Button,       IM_COL32(30,70,45,255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(50,110,70,255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(80,200,120,255));
    if (!hasSel) ImGui::BeginDisabled();
    if (ImGui::Button("  Launch  ")) { /* Phase 05 */ }
    if (!hasSel) ImGui::EndDisabled();
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0, 8);
    if (ImGui::Button("  Add Zip  ")) { /* Phase 04 */ }

    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Button,       IM_COL32(70,30,30,255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(110,50,50,255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(200,80,80,255));
    if (!hasSel) ImGui::BeginDisabled();
    if (ImGui::Button("  Select/Delete  ")) {
        if (sel) { m_db.remove(sel->id); m_selected = -1; refreshLibrary(); }
    }
    if (!hasSel) ImGui::EndDisabled();
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if (sel)
        ImGui::TextDisabled("  %s", sel->title.c_str());
    else
        ImGui::TextDisabled("  Drag a DOS zip to add  |  Double-click to launch");

    ImGui::SameLine(winW - 70);
    ImGui::TextDisabled("%.0f fps", ImGui::GetIO().Framerate);

    ImGui::EndChild();
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