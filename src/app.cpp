#include "app.h"
#include "platform.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_syswm.h>

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#ifndef AUTODOS2_VERSION_MAJOR
#  define AUTODOS2_VERSION_MAJOR 0
#  define AUTODOS2_VERSION_MINOR 1
#  define AUTODOS2_VERSION_PATCH 0
#endif

namespace fs = std::filesystem;

namespace AutoDOS2 {

static constexpr ImVec4 ACCENT      = {0.314f, 0.784f, 0.471f, 1.0f};
static constexpr ImU32  ACCENT32    = IM_COL32(80, 200, 120, 255);
static constexpr ImU32  ACCENT32DIM = IM_COL32(60, 140, 90, 160);

static void applyAutoDOSTheme()
{
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding     = 0.0f;  s.ChildRounding  = 6.0f;
    s.FrameRounding      = 5.0f;  s.ScrollbarRounding = 4.0f;
    s.GrabRounding       = 4.0f;  s.PopupRounding  = 6.0f;
    s.FramePadding       = {10,6}; s.ItemSpacing    = {8,6};
    s.ScrollbarSize      = 10.0f; s.WindowBorderSize = 0.0f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]             = {0.071f,0.071f,0.094f,1.00f};
    c[ImGuiCol_ChildBg]              = {0.090f,0.090f,0.118f,1.00f};
    c[ImGuiCol_PopupBg]              = {0.110f,0.110f,0.141f,0.97f};
    c[ImGuiCol_Border]               = {0.149f,0.149f,0.196f,1.00f};
    c[ImGuiCol_FrameBg]              = {0.130f,0.130f,0.165f,1.00f};
    c[ImGuiCol_FrameBgHovered]       = {0.157f,0.220f,0.180f,1.00f};
    c[ImGuiCol_FrameBgActive]        = {0.196f,0.282f,0.220f,1.00f};
    c[ImGuiCol_TitleBg]              = {0.055f,0.055f,0.075f,1.00f};
    c[ImGuiCol_TitleBgActive]        = {0.055f,0.055f,0.075f,1.00f};
    c[ImGuiCol_ScrollbarBg]          = {0.055f,0.055f,0.075f,1.00f};
    c[ImGuiCol_ScrollbarGrab]        = {0.220f,0.220f,0.280f,1.00f};
    c[ImGuiCol_ScrollbarGrabHovered] = {0.314f,0.784f,0.471f,0.6f};
    c[ImGuiCol_CheckMark]            = ACCENT;
    c[ImGuiCol_Button]               = {0.125f,0.204f,0.157f,1.00f};
    c[ImGuiCol_ButtonHovered]        = {0.200f,0.322f,0.247f,1.00f};
    c[ImGuiCol_ButtonActive]         = ACCENT;
    c[ImGuiCol_Text]                 = {0.863f,0.863f,0.863f,1.00f};
    c[ImGuiCol_TextDisabled]         = {0.431f,0.431f,0.471f,1.00f};
    c[ImGuiCol_Separator]            = {0.149f,0.149f,0.196f,1.00f};
}

// ── CoverCache ────────────────────────────────────────────────────────────────

CoverCache::~CoverCache() { clear(); }

SDL_Texture* CoverCache::makePlaceholder()
{
    constexpr int W = 180, H = 187;
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0,W,H,32,SDL_PIXELFORMAT_RGBA32);
    if (!s) return nullptr;
    SDL_FillRect(s, nullptr, SDL_MapRGB(s->format, 22,28,38));
    SDL_Rect r = {W/4,H/4,W/2,H/2};
    SDL_FillRect(s, &r, SDL_MapRGB(s->format, 30,40,54));
    SDL_Texture* t = SDL_CreateTextureFromSurface(m_renderer, s);
    SDL_FreeSurface(s);
    return t;
}

SDL_Texture* CoverCache::get(const std::string& path)
{
    auto it = m_cache.find(path);
    if (it != m_cache.end()) return it->second;
    SDL_Texture* t = nullptr;
    if (!path.empty()) {
        SDL_Surface* s = IMG_Load(path.c_str());
        if (s) { t = SDL_CreateTextureFromSurface(m_renderer,s); SDL_FreeSurface(s); }
    }
    if (!t) { if (!m_placeholder) m_placeholder = makePlaceholder(); t = m_placeholder; }
    m_cache[path] = t;
    return t;
}

void CoverCache::clear()
{
    for (auto& [k,v] : m_cache) if (v && v!=m_placeholder) SDL_DestroyTexture(v);
    m_cache.clear();
    if (m_placeholder) { SDL_DestroyTexture(m_placeholder); m_placeholder=nullptr; }
}

// ── App::init ─────────────────────────────────────────────────────────────────

App::App()  = default;
App::~App() { cleanup(); }

bool App::init()
{
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER|SDL_INIT_GAMECONTROLLER)!=0) return false;
    IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED,"0");
    SDL_SetHint(SDL_HINT_RENDER_VSYNC,"1");

    m_window = SDL_CreateWindow("AutoDOS2",
        SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,WINDOW_W,WINDOW_H,
        SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    if (!m_window) return false;

    m_renderer = SDL_CreateRenderer(m_window,-1,
        SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    if (!m_renderer)
        m_renderer = SDL_CreateRenderer(m_window,-1,SDL_RENDERER_SOFTWARE);
    if (!m_renderer) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::GetIO().IniFilename  = nullptr;
    applyAutoDOSTheme();
    ImGui_ImplSDL2_InitForSDLRenderer(m_window, m_renderer);
    ImGui_ImplSDLRenderer2_Init(m_renderer);

    m_covers.setRenderer(m_renderer);

    // ── Paths ─────────────────────────────────────────────────────────────────
    fs::path dataDir   = getDataDir();          // AppData/AutoDOS2/autodos2/
    fs::path exeDir    = getExeDir();           // directory containing AutoDOS2.exe
    fs::path extractRoot = dataDir / "games";
    fs::path confsRoot   = dataDir / "games";
    fs::path artRoot     = dataDir / "art";
    fs::path gamesJson   = dataDir / "games.json";
    fs::path sevenZip    = exeDir  / "7za.exe";
    fs::path dosbox      = exeDir  / "dosbox" / "dosbox.exe";

    fs::create_directories(extractRoot);
    fs::create_directories(artRoot);

    // Copy games.json from exe dir if not yet in AppData
    if (!fs::exists(gamesJson)) {
        fs::path srcJson = exeDir / "games.json";
        if (fs::exists(srcJson)) {
            std::error_code ec;
            fs::copy_file(srcJson, gamesJson, ec);
        }
    }

    // Load games.json
    if (!m_gameJson.load(gamesJson)) {
        std::fprintf(stderr, "[AutoDOS2] Warning: games.json not found at %s\n",
            gamesJson.string().c_str());
        std::fprintf(stderr, "[AutoDOS2] Place games.json next to AutoDOS2.exe\n");
        // Non-fatal: unknown games still work via scorer
    } else {
        std::fprintf(stdout, "[AutoDOS2] Loaded %d games from games.json\n",
            m_gameJson.count());
    }

    // Configure ingestor
    m_ingestor.setSevenZipPath(sevenZip);
    m_ingestor.setExtractRoot(extractRoot);
    m_ingestor.setConfsRoot(confsRoot);
    m_ingestor.setDosboxPath(dosbox);
    m_dosboxPath = dosbox;
    m_confsRoot  = confsRoot;
    m_ingestor.setDatabase(&m_gameJson);
    m_ingestor.setDos4gwPath(exeDir / "DOS4GW.EXE");

    // Open SQLite DB
    fs::path dbPath = dataDir / "autodos2.db";
    if (!m_db.open(dbPath)) return false;

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
    std::transform(q.begin(),q.end(),q.begin(),::tolower);
    if (q.empty()) { m_filtered=m_allGames; return; }
    m_filtered.clear();
    for (auto& g : m_allGames) {
        std::string t=g.title;
        std::transform(t.begin(),t.end(),t.begin(),::tolower);
        if (t.find(q)!=std::string::npos) m_filtered.push_back(g);
    }
    bool ok=false;
    for (auto& g:m_filtered) if (g.id==m_selected){ok=true;break;}
    if (!ok) m_selected=-1;
}

// ── startIngest ───────────────────────────────────────────────────────────────

void App::startIngest(const std::string& path)
{
    if (m_ingest.busy) return; // already working

    m_ingest.busy     = true;
    m_ingest.progress = 0;
    m_ingest.resultReady = false;
    {
        std::lock_guard<std::mutex> lk(m_ingest.mtx);
        m_ingest.message   = "Analysing...";
        m_ingest.lastError = "";
    }

    if (m_ingestThread.joinable()) m_ingestThread.join();

    m_ingestThread = std::thread([this, path]() {
        auto setMsg = [this](const std::string& msg) {
            std::lock_guard<std::mutex> lk(m_ingest.mtx);
            m_ingest.message = msg;
        };

        setMsg("Analysing archive...");
        AnalyzeResult res = m_ingestor.ingest(path,
            [this, &setMsg](int pct) {
                m_ingest.progress = pct;
                if (pct < 80) setMsg("Extracting...");
                else          setMsg("Finalising...");
            });

        if (res.success) {
            setMsg("Adding to library...");
            // Write to SQLite
            GameRecord rec;
            rec.title      = res.title;
            rec.slug       = res.slug;
            rec.platform   = "DOS";
            rec.exe_path   = res.exe;

            // conf path = confsRoot / slug.conf
            fs::path dataDir  = getDataDir();
            rec.zip_path   = path;
            rec.cover_path = (dataDir / "art" / (res.slug + ".jpg")).string();

            // Check for duplicate slug
            if (!m_db.getBySlug(res.slug).has_value()) {
                m_db.insert(rec);
            }
        } else {
            std::lock_guard<std::mutex> lk(m_ingest.mtx);
            m_ingest.lastError = res.error;
        }

        {
            std::lock_guard<std::mutex> lk(m_ingest.mtx);
            m_ingest.result      = res;
            m_ingest.resultReady = true;
        }
        m_ingest.progress = 100;
        m_ingest.busy     = false;
    });
}

// ── Loop ──────────────────────────────────────────────────────────────────────

void App::run()
{
    while (m_running) { processEvents(); update(); render(); }
}

void App::processEvents()
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        ImGui_ImplSDL2_ProcessEvent(&e);
        if (e.type==SDL_QUIT) m_running=false;
        if (e.type==SDL_WINDOWEVENT && e.window.event==SDL_WINDOWEVENT_CLOSE)
            m_running=false;
        if (e.type==SDL_KEYDOWN && e.key.keysym.sym==SDLK_F1)
            m_showDemoWindow=!m_showDemoWindow;
        if (e.type==SDL_DROPFILE) {
            std::string dropped = e.drop.file;
            SDL_free(e.drop.file);
            if (!m_ingest.busy) startIngest(dropped);
        }
    }
}

void App::update()
{
    // Check if ingest just finished and refresh library
    if (m_ingest.resultReady) {
        bool ready = false;
        {
            std::lock_guard<std::mutex> lk(m_ingest.mtx);
            ready = m_ingest.resultReady;
            m_ingest.resultReady = false;
        }
        if (ready) refreshLibrary();
    }
}

void App::render()
{
    SDL_SetRenderDrawColor(m_renderer,18,18,24,255);
    SDL_RenderClear(m_renderer);
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    if (m_showDemoWindow) ImGui::ShowDemoWindow(&m_showDemoWindow);
    renderImGui();
    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(),m_renderer);
    SDL_RenderPresent(m_renderer);
}

// ── Layout ────────────────────────────────────────────────────────────────────

void App::renderImGui()
{
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({0,0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::Begin("##root",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();

    const float topH    = 56.0f;
    const float bottomH = 50.0f;
    const float midH    = io.DisplaySize.y - topH - bottomH;

    renderTopBar(io.DisplaySize.x);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::BeginChild("##mid",{0,midH},false,ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();
    renderSidebar();
    ImGui::SameLine(0,0);
    renderGrid();
    ImGui::EndChild();

    renderBottomBar(io.DisplaySize.x);
    ImGui::End();

    // Ingest progress overlay
    if (m_ingest.busy || m_ingest.progress > 0) renderIngestOverlay();

    // Launch error overlay
    if (m_launchState == LaunchState::Error) renderLaunchError();


}

// ── Top bar ───────────────────────────────────────────────────────────────────

void App::renderTopBar(float winW)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4{0.055f,0.055f,0.075f,1.0f});
    ImGui::BeginChild("##top",{0,56},false,ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();

    ImGui::SetCursorPos({14,16});
    ImGui::TextColored(ACCENT,"AutoDOS2");
    ImGui::SameLine(); ImGui::SetCursorPosY(16);
    ImGui::TextDisabled("v%d.%d  |  %d games  |  DB: %d entries",
        AUTODOS2_VERSION_MAJOR,AUTODOS2_VERSION_MINOR,
        (int)m_allGames.size(), m_gameJson.count());

    const float searchW=260.0f;
    ImGui::SetCursorPos({winW-searchW-14,14});
    ImGui::SetNextItemWidth(searchW);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,20.0f);
    if (ImGui::InputTextWithHint("##search","Search Games...",
            m_searchBuf,sizeof(m_searchBuf)))
        applySearch();
    ImGui::PopStyleVar();
    ImGui::EndChild();

    ImDrawList* dl=ImGui::GetWindowDrawList();
    ImVec2 p=ImGui::GetCursorScreenPos();
    dl->AddLine(p,{p.x+winW,p.y},IM_COL32(38,38,60,255),1.0f);
}

// ── Sidebar ───────────────────────────────────────────────────────────────────

void App::renderSidebar()
{
    const float sideW=200.0f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4{0.055f,0.060f,0.080f,1.0f});
    ImGui::BeginChild("##sidebar",{sideW,0},false,ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();

    const GameRecord* sel=nullptr;
    for (auto& g:m_filtered) if (g.id==m_selected){sel=&g;break;}

    if (sel) {
        ImGui::SetCursorPos({10, 12});
        ImGui::PushTextWrapPos(sideW - 10);
        ImGui::TextColored(ACCENT, "%s", sel->title.c_str());
        ImGui::Spacing();
        ImGui::TextDisabled("Played: %d times", sel->play_count);
        if (!sel->last_played.empty())
            ImGui::TextDisabled("Last: %.10s", sel->last_played.c_str());
        ImGui::PopTextWrapPos();

        // Hotkey reference
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ACCENT, "Hotkeys");
        ImGui::Spacing();

        auto hk = [&](const char* key, const char* desc) {
            ImGui::TextColored({0.314f,0.784f,0.471f,0.9f}, "%s", key);
            ImGui::SameLine(90);
            ImGui::TextDisabled("%s", desc);
        };
        hk("Alt+Enter","Fullscreen");
        hk("Ctrl+F10", "Mouse lock");
        hk("Ctrl+F11", "Speed down");
        hk("Ctrl+F12", "Speed up");
        hk("Ctrl+F4",  "Swap disc");
        hk("Ctrl+F7",  "Screenshot");
        hk("Ctrl+F9",  "Quit game");
    } else {
        ImGui::SetCursorPos({10,16});
        ImGui::TextColored(ACCENT,"Library");
        ImGui::SetCursorPosX(10);
        ImGui::TextDisabled("%d games",(int)m_allGames.size());
        ImGui::Spacing();
        ImGui::SetCursorPosX(10);
        ImGui::TextDisabled("Drag a DOS zip\nonto the window\nto add a game.");
    }

    ImDrawList* dl=ImGui::GetWindowDrawList();
    ImVec2 tl=ImGui::GetWindowPos();
    float h=ImGui::GetWindowHeight();
    dl->AddLine({tl.x+sideW-1,tl.y},{tl.x+sideW-1,tl.y+h},IM_COL32(38,38,60,255),1.0f);
    ImGui::EndChild();
}

// ── Grid ──────────────────────────────────────────────────────────────────────

void App::renderGrid()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{CARD_PAD,CARD_PAD});
    ImGui::BeginChild("##grid",{0,0},false);
    ImGui::PopStyleVar();

    const float availW=ImGui::GetContentRegionAvail().x;
    const int   cols=std::max(1,(int)((availW+CARD_PAD)/(CARD_W+CARD_PAD)));
    const float imgH=CARD_H*0.76f;
    const float lblH=CARD_H-imgH;
    ImDrawList* dl=ImGui::GetWindowDrawList();

    for (int i=0;i<(int)m_filtered.size();++i) {
        const GameRecord& g=m_filtered[i];
        const bool sel=(g.id==m_selected);
        if (i%cols!=0) ImGui::SameLine(0,CARD_PAD);
        ImGui::PushID(g.id);

        ImVec2 pos=ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##card",{CARD_W,CARD_H});
        bool hov=ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) m_selected=sel?-1:g.id;
        if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(0) && !sel)
            launchGame(g);
        else if (ImGui::IsMouseDoubleClicked(0) && g.id==m_selected)
            launchGame(g);

        ImU32 bg=sel?IM_COL32(32,58,42,255):hov?IM_COL32(28,38,38,255):IM_COL32(18,22,30,255);
        dl->AddRectFilled(pos,{pos.x+CARD_W,pos.y+CARD_H},bg,8.0f);

        SDL_Texture* tex=m_covers.get(g.cover_path);
        if (tex) dl->AddImage(reinterpret_cast<ImTextureID>(tex),pos,{pos.x+CARD_W,pos.y+imgH});

        ImVec2 gTL={pos.x,pos.y+imgH-24.0f};
        ImVec2 gBR={pos.x+CARD_W,pos.y+CARD_H};
        dl->AddRectFilledMultiColor(gTL,gBR,
            IM_COL32(0,0,0,0),IM_COL32(0,0,0,0),
            IM_COL32(0,0,0,220),IM_COL32(0,0,0,220));

        ImVec2 tp={pos.x+8.0f,pos.y+imgH+(lblH-14.0f)*0.35f};
        dl->AddText(nullptr,0,tp,IM_COL32(225,225,225,255),
            g.title.c_str(),nullptr,CARD_W-10.0f);

        if (sel) dl->AddRect(pos,{pos.x+CARD_W,pos.y+CARD_H},ACCENT32,8.0f,0,2.5f);
        else if (hov) dl->AddRect(pos,{pos.x+CARD_W,pos.y+CARD_H},ACCENT32DIM,8.0f,0,1.5f);

        ImGui::PopID();
    }

    if (m_filtered.empty()) {
        ImGui::SetCursorPosY(80);
        const char* msg=m_allGames.empty()
            ?"No games yet — drag a DOS zip onto the window"
            :"No games match your search";
        float tw=ImGui::CalcTextSize(msg).x;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x-tw)*0.5f);
        ImGui::TextDisabled("%s",msg);
    }

    ImGui::EndChild();
}

// ── Bottom bar ────────────────────────────────────────────────────────────────

void App::renderBottomBar(float winW)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg,ImVec4{0.055f,0.055f,0.075f,1.0f});
    ImGui::BeginChild("##bot",{0,0},false,ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleColor();

    ImDrawList* dl=ImGui::GetWindowDrawList();
    ImVec2 p=ImGui::GetWindowPos();
    dl->AddLine(p,{p.x+winW,p.y},IM_COL32(38,38,60,255),1.0f);
    ImGui::SetCursorPos({10,10});

    const GameRecord* sel=nullptr;
    for (auto& g:m_filtered) if (g.id==m_selected){sel=&g;break;}
    const bool hasSel=(sel!=nullptr);
    const bool busy=m_ingest.busy.load();

    // Launch
    ImGui::PushStyleColor(ImGuiCol_Button,       IM_COL32(30,70,45,255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(50,110,70,255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(80,200,120,255));
    if (!hasSel) ImGui::BeginDisabled();
    if (ImGui::Button("  Launch  ")) {
        if (sel) launchGame(*sel);
    }
    if (!hasSel) ImGui::EndDisabled();
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0,8);

    // Add Zip
    if (busy) ImGui::BeginDisabled();
    if (ImGui::Button("  Add Zip  ")) {
        auto path = openFileDialog("Archives","*.zip;*.7z;*.rar");
        if (!path.empty()) startIngest(path.string());
    }
    if (busy) ImGui::EndDisabled();

    ImGui::SameLine(0,8);

    // Delete
    ImGui::PushStyleColor(ImGuiCol_Button,       IM_COL32(70,30,30,255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(110,50,50,255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(200,80,80,255));
    if (!hasSel) ImGui::BeginDisabled();
    if (ImGui::Button("  Delete  ")) {
        if (sel) { m_db.remove(sel->id); m_selected=-1; refreshLibrary(); }
    }
    if (!hasSel) ImGui::EndDisabled();
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if (sel) ImGui::TextDisabled("  %s",sel->title.c_str());
    else ImGui::TextDisabled("  Drag a DOS zip to add  |  Double-click to launch");

    ImGui::SameLine(winW-70);
    ImGui::TextDisabled("%.0f fps",ImGui::GetIO().Framerate);

    ImGui::EndChild();
}

// ── Ingest overlay ────────────────────────────────────────────────────────────

void App::renderIngestOverlay()
{
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({io.DisplaySize.x*0.5f, io.DisplaySize.y*0.5f},
        ImGuiCond_Always, {0.5f,0.5f});
    ImGui::SetNextWindowSize({360,120});
    ImGui::SetNextWindowBgAlpha(0.92f);
    ImGui::Begin("##ingest",nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
        ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBringToFrontOnFocus);

    std::string msg, err;
    {
        std::lock_guard<std::mutex> lk(m_ingest.mtx);
        msg = m_ingest.message;
        err = m_ingest.lastError;
    }
    int pct = m_ingest.progress.load();

    if (!err.empty()) {
        ImGui::TextColored({1.0f,0.4f,0.4f,1.0f},"Error:");
        ImGui::PushTextWrapPos(340);
        ImGui::TextWrapped("%s",err.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        if (ImGui::Button("OK",{80,0})) { m_ingest.progress=0; }
    } else {
        ImGui::TextColored(ACCENT,"Adding game...");
        ImGui::Spacing();
        ImGui::TextDisabled("%s",msg.c_str());
        ImGui::Spacing();
        ImGui::ProgressBar(pct/100.0f,{-1,0});
    }

    ImGui::End();
}



// ── launchGame ────────────────────────────────────────────────────────────────

void App::launchGame(const GameRecord& rec)
{
    if (m_dosboxPath.empty() || !std::filesystem::exists(m_dosboxPath)) {
        m_launchState = LaunchState::Error;
        m_launchError = "DOSBox not found.\n\nPlace dosbox.exe inside the dosbox\\ folder next to AutoDOS2.exe.";
        return;
    }

    std::filesystem::path confPath = m_confsRoot / (rec.slug + ".conf");
    if (!std::filesystem::exists(confPath)) {
        m_launchState = LaunchState::Error;
        m_launchError = "No .conf found for: " + rec.title + "\n\nTry removing and re-adding the game.";
        return;
    }

#ifdef _WIN32
    std::string cmd = "\"" + m_dosboxPath.string() + "\" -conf \"" + confPath.string() + "\"";

    STARTUPINFOA si        = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    si.dwFlags             = STARTF_USESHOWWINDOW;
    si.wShowWindow         = SW_SHOWNORMAL;

    if (!CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()),
                        nullptr, nullptr, FALSE, 0,
                        nullptr, nullptr, &si, &pi)) {
        m_launchState = LaunchState::Error;
        m_launchError = "Failed to launch DOSBox.\nCommand: " + cmd;
        return;
    }

    m_launchState = LaunchState::Running;

    // Record play
    m_db.recordPlay(rec.id);
    refreshLibrary();

    // Wait for DOSBox in detached thread, then bring AutoDOS2 back to front
    HANDLE hProc = pi.hProcess;
    SDL_SysWMinfo wmInfo = {};
    SDL_VERSION(&wmInfo.version);
    HWND hWnd = nullptr;
    if (SDL_GetWindowWMInfo(m_window, &wmInfo))
        hWnd = wmInfo.info.win.window;

    CloseHandle(pi.hThread);

    std::thread([this, hProc, hWnd]() {
        WaitForSingleObject(hProc, INFINITE);
        CloseHandle(hProc);
        m_launchState = LaunchState::Idle;
        if (hWnd) {
            SetForegroundWindow(hWnd);
            ShowWindow(hWnd, SW_RESTORE);
        }
    }).detach();
#else
    (void)rec;
    m_launchState = LaunchState::Error;
    m_launchError = "Launch not yet implemented on this platform.";
#endif
}

// ── renderLaunchError ─────────────────────────────────────────────────────────

void App::renderLaunchError()
{
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f},
        ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({400, 160});
    ImGui::SetNextWindowBgAlpha(0.95f);
    ImGui::Begin("##launcherr", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings);

    ImGui::TextColored({1.0f, 0.4f, 0.4f, 1.0f}, "Launch Error");
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::PushTextWrapPos(380);
    ImGui::TextWrapped("%s", m_launchError.c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    if (ImGui::Button("OK", {80, 0}))
        m_launchState = LaunchState::Idle;

    ImGui::End();
}

// ── Cleanup ───────────────────────────────────────────────────────────────────

void App::cleanup()
{
    if (m_ingestThread.joinable()) m_ingestThread.join();
    m_covers.clear();
    m_db.close();
    if (m_renderer) {
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_DestroyRenderer(m_renderer);
        m_renderer=nullptr;
    }
    if (m_window) { SDL_DestroyWindow(m_window); m_window=nullptr; }
    IMG_Quit();
    SDL_Quit();
}

} // namespace AutoDOS2