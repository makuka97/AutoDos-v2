#include "app.h"
#include "platform.h"

#include <SDL.h>
#include <SDL_image.h>

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include <cstdio>
#include <string>

// Version injected by CMake via compile definitions
#ifndef AUTODOS2_VERSION_MAJOR
#  define AUTODOS2_VERSION_MAJOR 0
#  define AUTODOS2_VERSION_MINOR 1
#  define AUTODOS2_VERSION_PATCH 0
#endif

namespace AutoDOS2 {

// ── Colour palette (matches v1 win32 UI, re-expressed as ImVec4) ─────────────
static void applyAutoDOSTheme()
{
    ImGuiStyle& s = ImGui::GetStyle();

    // Rounded, clean look
    s.WindowRounding   = 6.0f;
    s.FrameRounding    = 4.0f;
    s.ScrollbarRounding = 4.0f;
    s.GrabRounding     = 4.0f;
    s.TabRounding      = 4.0f;
    s.FramePadding     = {8, 5};
    s.ItemSpacing      = {8, 6};

    // v1 colours: bg #12121818, accent #50C878 (emerald)
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]         = {0.071f, 0.071f, 0.094f, 1.00f}; // #121218
    c[ImGuiCol_ChildBg]          = {0.110f, 0.110f, 0.141f, 1.00f}; // #1C1C24
    c[ImGuiCol_PopupBg]          = {0.110f, 0.110f, 0.141f, 0.96f};
    c[ImGuiCol_Border]           = {0.149f, 0.149f, 0.196f, 1.00f}; // #262632
    c[ImGuiCol_FrameBg]          = {0.110f, 0.110f, 0.141f, 1.00f};
    c[ImGuiCol_FrameBgHovered]   = {0.157f, 0.196f, 0.157f, 1.00f};
    c[ImGuiCol_FrameBgActive]    = {0.196f, 0.282f, 0.196f, 1.00f};
    c[ImGuiCol_TitleBg]          = {0.071f, 0.071f, 0.094f, 1.00f};
    c[ImGuiCol_TitleBgActive]    = {0.071f, 0.071f, 0.094f, 1.00f};
    c[ImGuiCol_MenuBarBg]        = {0.090f, 0.090f, 0.118f, 1.00f};
    c[ImGuiCol_ScrollbarBg]      = {0.071f, 0.071f, 0.094f, 1.00f};
    c[ImGuiCol_ScrollbarGrab]    = {0.200f, 0.200f, 0.260f, 1.00f};
    c[ImGuiCol_CheckMark]        = {0.314f, 0.784f, 0.471f, 1.00f}; // #50C878
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
    c[ImGuiCol_Text]             = {0.863f, 0.863f, 0.863f, 1.00f}; // #DCDCDC
    c[ImGuiCol_TextDisabled]     = {0.431f, 0.431f, 0.471f, 1.00f}; // #6E6E78
    c[ImGuiCol_Separator]        = {0.149f, 0.149f, 0.196f, 1.00f};
}

// ── Construction / destruction ────────────────────────────────────────────────

App::App() = default;

App::~App()
{
    cleanup();
}

// ── init ──────────────────────────────────────────────────────────────────────

bool App::init()
{
    // ── SDL2 ──────────────────────────────────────────────────────────────────
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "[AutoDOS2] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    // SDL2_image (PNG + JPG support for cover art later)
    if (!(IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) & (IMG_INIT_PNG | IMG_INIT_JPG))) {
        std::fprintf(stderr, "[AutoDOS2] IMG_Init failed: %s\n", IMG_GetError());
        // Non-fatal in Phase 01 — covers aren't used yet.
    }

    // Enable SDL hints for better rendering on HiDPI displays
    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

    // ── Window ────────────────────────────────────────────────────────────────
    const std::string title = std::string("AutoDOS2 — dev  v")
        + std::to_string(AUTODOS2_VERSION_MAJOR) + "."
        + std::to_string(AUTODOS2_VERSION_MINOR) + "."
        + std::to_string(AUTODOS2_VERSION_PATCH);

    m_window = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!m_window) {
        std::fprintf(stderr, "[AutoDOS2] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    // ── Renderer (hardware-accelerated, vsync) ────────────────────────────────
    m_renderer = SDL_CreateRenderer(
        m_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!m_renderer) {
        // Fall back to software renderer
        std::fprintf(stderr, "[AutoDOS2] HW renderer failed (%s), trying software.\n",
                     SDL_GetError());
        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_SOFTWARE);
        if (!m_renderer) {
            std::fprintf(stderr, "[AutoDOS2] SDL_CreateRenderer failed: %s\n", SDL_GetError());
            return false;
        }
    }

    // Scale renderer to logical size so UI is resolution-independent
    SDL_RenderSetLogicalSize(m_renderer, WINDOW_W, WINDOW_H);

    // ── ImGui context ─────────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    applyAutoDOSTheme();

    ImGui_ImplSDL2_InitForSDLRenderer(m_window, m_renderer);
    ImGui_ImplSDLRenderer2_Init(m_renderer);

    m_running = true;
    return true;
}

// ── Main loop ─────────────────────────────────────────────────────────────────

void App::run()
{
    const Uint32 frameMs = 1000 / TARGET_FPS;

    while (m_running) {
        const Uint32 frameStart = SDL_GetTicks();

        processEvents();
        update();
        render();

        // Cap frame rate if vsync isn't running
        const Uint32 elapsed = SDL_GetTicks() - frameStart;
        if (elapsed < frameMs) {
            SDL_Delay(frameMs - elapsed);
        }
    }
}

// ── processEvents ─────────────────────────────────────────────────────────────

void App::processEvents()
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        ImGui_ImplSDL2_ProcessEvent(&e);

        switch (e.type) {
        case SDL_QUIT:
            m_running = false;
            break;

        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_CLOSE &&
                e.window.windowID == SDL_GetWindowID(m_window)) {
                m_running = false;
            }
            break;

        case SDL_KEYDOWN:
            if (e.key.keysym.sym == SDLK_F1) {
                m_showDemoWindow = !m_showDemoWindow;
            }
            break;

        case SDL_DROPFILE: {
            // Phase 04 will handle this — log for now
            char* droppedFile = e.drop.file;
            std::fprintf(stdout, "[AutoDOS2] Drop event: %s\n", droppedFile);
            SDL_free(droppedFile);
            break;
        }

        default:
            break;
        }
    }
}

// ── update ────────────────────────────────────────────────────────────────────

void App::update()
{
    // Phase 01: nothing to update yet.
}

// ── render ────────────────────────────────────────────────────────────────────

void App::render()
{
    // Background: #121218
    SDL_SetRenderDrawColor(m_renderer, 18, 18, 24, 255);
    SDL_RenderClear(m_renderer);

    // ── ImGui ─────────────────────────────────────────────────────────────────
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    renderImGui();

    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), m_renderer);

    SDL_RenderPresent(m_renderer);
}

// ── renderImGui ───────────────────────────────────────────────────────────────

void App::renderImGui()
{
    // F1: toggle ImGui demo window (Phase 01 test criterion)
    if (m_showDemoWindow) {
        ImGui::ShowDemoWindow(&m_showDemoWindow);
    }

    // ── Status overlay (always visible in Phase 01) ───────────────────────────
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({10, 10}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGui::Begin("##overlay", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoInputs      |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings  |
        ImGuiWindowFlags_NoNav            |
        ImGuiWindowFlags_NoMove);

    ImGui::TextColored({0.314f, 0.784f, 0.471f, 1.0f}, "AutoDOS2");
    ImGui::SameLine();
    ImGui::TextDisabled("v%d.%d.%d — Phase 01",
        AUTODOS2_VERSION_MAJOR, AUTODOS2_VERSION_MINOR, AUTODOS2_VERSION_PATCH);
    ImGui::Separator();
    ImGui::Text("%.0f fps  |  F1: ImGui demo", io.Framerate);
    ImGui::TextDisabled("Phase 02+: game grid, DB, zip ingest");
    ImGui::End();
}

// ── cleanup ───────────────────────────────────────────────────────────────────

void App::cleanup()
{
    if (m_renderer) {
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    IMG_Quit();
    SDL_Quit();
}

} // namespace AutoDOS2
