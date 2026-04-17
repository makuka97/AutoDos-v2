// ingest.cpp — Phase 04 ZIP ingest pipeline
#include "ingest.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#endif

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace AutoDOS2 {

// ── String helpers ────────────────────────────────────────────────────────────

std::string toUpper(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::toupper);
    return r;
}

std::string toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return r;
}

// Slugify: lowercase, strip everything that isn't a letter or digit.
// "Duke Nukem 3D" -> "dukenukm3d"  (matches games.json keys exactly)
std::string slugify(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (std::isalnum((unsigned char)c))
            r += std::tolower((unsigned char)c);
    }
    return r;
}

static bool isBlacklisted(const std::string& stem) {
    static const std::vector<std::string> BL = {
        "setup","install","uninst","uninstall","patch","update",
        "config","cfg","register","readme","read","help",
        "directx","dxsetup","vcredist","dotnet",
        "dos4gw","cwsdpmi","himemx","emm386",
        "fixsave","fix","convert","copy","move",
        "dosbox","dosbox_staging","scummvm","boxer"
    };
    std::string lo = toLower(stem);
    for (auto& b : BL) if (lo == b) return true;
    return false;
}

// ── GameDatabase ──────────────────────────────────────────────────────────────

bool GameDatabase::load(const fs::path& jsonPath)
{
    std::ifstream f(jsonPath);
    if (!f.is_open()) return false;

    json root;
    try { f >> root; } catch (...) { return false; }

    auto& games = root["games"];
    if (!games.is_object()) return false;

    m_bySlug.clear();
    m_exeToSlug.clear();

    for (auto it = games.begin(); it != games.end(); ++it) {
        const std::string& slug = it.key();
        const json& g = it.value();

        GameEntry e;
        e.slug        = slug;
        e.title       = g.value("title",        "");
        e.exe         = g.value("exe",           "");
        e.workDir     = g.value("work_dir",      "");
        e.cycles      = g.value("cycles",        "max limit 80000");
        // cycles can be string or number
        if (g.contains("cycles") && g["cycles"].is_number())
            e.cycles  = std::to_string(g["cycles"].get<int>());
        e.memsize     = g.value("memsize",       16);
        e.ems         = g.value("ems",           true);
        e.xms         = g.value("xms",           true);
        e.cdMount     = g.value("cd_mount",      false);
        e.installFirst= g.value("install_first", false);
        e.year        = g.value("year",          0);

        m_bySlug[slug] = e;

        // Index by uppercase exe filename for fast lookup
        if (!e.exe.empty()) {
            std::string exeUp = toUpper(e.exe);
            // Only store first match (some games share an exe name — rare)
            m_exeToSlug.emplace(exeUp, slug);
        }
    }

    m_loaded = true;
    return true;
}

const GameEntry* GameDatabase::bySlug(const std::string& slug) const
{
    auto it = m_bySlug.find(slug);
    return it != m_bySlug.end() ? &it->second : nullptr;
}

const GameEntry* GameDatabase::byExe(const std::string& exeName) const
{
    auto it = m_exeToSlug.find(toUpper(exeName));
    if (it == m_exeToSlug.end()) return nullptr;
    return bySlug(it->second);
}

// ── run7za ────────────────────────────────────────────────────────────────────

std::string Ingestor::run7za(const std::string& args) const
{
#ifdef _WIN32
    if (m_7zaPath.empty()) return "";

    std::string cmd = "\"" + m_7zaPath.string() + "\" " + args;

    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return "";
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si   = { sizeof(si) };
    si.dwFlags        = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput     = hWrite;
    si.hStdError      = hWrite;
    si.hStdInput      = GetStdHandle(STD_INPUT_HANDLE);
    si.wShowWindow    = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::string out;

    if (CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()),
                       nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                       nullptr, nullptr, &si, &pi)) {
        CloseHandle(hWrite); hWrite = nullptr;
        char buf[4096]; DWORD n;
        while (ReadFile(hRead, buf, sizeof(buf)-1, &n, nullptr) && n > 0) {
            buf[n] = '\0'; out += buf;
        }
        WaitForSingleObject(pi.hProcess, 120000);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
    if (hWrite) CloseHandle(hWrite);
    CloseHandle(hRead);
    return out;
#else
    (void)args;
    return "";
#endif
}

// ── extract7za ────────────────────────────────────────────────────────────────

bool Ingestor::extract7za(const fs::path& archive, const fs::path& outDir) const
{
    fs::create_directories(outDir);
    std::string args = "x \"" + archive.string() + "\" -o\"" + outDir.string() + "\" -y";
    std::string out  = run7za(args);
    return out.find("Everything is Ok") != std::string::npos
        || out.find("Files:")            != std::string::npos
        || out.find("extracted")         != std::string::npos;
}

// ── scanExtractedDir ──────────────────────────────────────────────────────────
// Walk the extracted folder and collect scored exe candidates.

std::vector<Ingestor::ExeCandidate>
Ingestor::scanExtractedDir(const fs::path& dir, const std::string& archiveStem) const
{
    std::vector<ExeCandidate> candidates;
    if (!fs::exists(dir)) return candidates;

    std::string stemLo = toLower(archiveStem);

    // Recursive walk
    std::function<void(const fs::path&, int)> walk = [&](const fs::path& d, int depth) {
        std::error_code ec;
        for (auto& entry : fs::directory_iterator(d, ec)) {
            if (entry.is_directory(ec)) {
                walk(entry.path(), depth + 1);
                continue;
            }
            if (!entry.is_regular_file(ec)) continue;

            std::string ext = toUpper(entry.path().extension().string());
            if (ext.empty()) continue;
            if (ext[0] == '.') ext = ext.substr(1);
            if (ext != "EXE" && ext != "COM" && ext != "BAT") continue;

            std::string stem = toLower(entry.path().stem().string());
            if (isBlacklisted(stem)) continue;

            // Score
            float score = 0.0f;
            if      (ext == "EXE") score = 1.0f;
            else if (ext == "BAT") score = 0.7f;
            else if (ext == "COM") score = 0.6f;

            score -= depth * 0.15f;
            if (score < 0.01f) score = 0.01f;

            if (stem == stemLo || stem.find(stemLo) == 0 || stemLo.find(stem) == 0)
                score += 0.3f;
            if (stem == "game" || stem == "play" || stem == "start" ||
                stem == "run"  || stem == "main" || stem == "go")
                score += 0.15f;

            // Relative path from extractRoot/slug/
            fs::path rel = fs::relative(entry.path(), dir, ec);
            candidates.push_back({rel.string(), toUpper(entry.path().filename().string()), score});
        }
    };
    walk(dir, 0);

    std::sort(candidates.begin(), candidates.end(),
        [](const ExeCandidate& a, const ExeCandidate& b){ return a.score > b.score; });
    return candidates;
}

// ── analyze ───────────────────────────────────────────────────────────────────
// Fast path: list archive contents with 7za, find exe names, match to DB.
// No extraction yet.

AnalyzeResult Ingestor::analyze(const fs::path& archivePath) const
{
    AnalyzeResult result;

    // List archive contents
    std::string listArgs = "l -slt \"" + archivePath.string() + "\"";
    std::string listing  = run7za(listArgs);

    if (listing.empty()) {
        result.error = "Could not read archive (7za failed or file not found)";
        return result;
    }

    // Parse 7za listing to get file names
    std::vector<std::string> exeNames;
    std::istringstream ss(listing);
    std::string line;
    std::string currentPath;
    bool        isDir = false;

    auto commit = [&]() {
        if (currentPath.empty() || isDir) return;
        // Normalise separators
        std::replace(currentPath.begin(), currentPath.end(), '\\', '/');
        std::string fname = currentPath;
        size_t sl = fname.rfind('/');
        if (sl != std::string::npos) fname = fname.substr(sl + 1);
        std::string ext = toUpper(fname.size() > 4 ? fname.substr(fname.size()-3) : "");
        if (ext == "EXE" || ext == "COM" || ext == "BAT")
            exeNames.push_back(toUpper(fname));
        currentPath.clear(); isDir = false;
    };

    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.find("----------") == 0) { commit(); continue; }
        size_t eq = line.find(" = ");
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 3);
        // trim
        while (!key.empty() && key.back() == ' ') key.pop_back();
        while (!val.empty() && val.back() == ' ') val.pop_back();

        if (key == "Path")       { commit(); currentPath = val; }
        else if (key == "Attributes") isDir = val.find('D') != std::string::npos;
    }
    commit();

    if (exeNames.empty()) {
        result.error = "No executable files found in archive";
        return result;
    }

    // ── Match 1: exe filename against games.json ──────────────────────────────
    if (m_db) {
        for (const auto& exeName : exeNames) {
            std::string stemOnly = exeName;
            // strip blacklisted
            std::string stemLo = toLower(stemOnly.substr(0, stemOnly.rfind('.')));
            if (isBlacklisted(stemLo)) continue;

            const GameEntry* entry = m_db->byExe(exeName);
            if (entry) {
                result.success    = true;
                result.slug       = entry->slug;
                result.title      = entry->title;
                result.exe        = entry->exe;
                result.workDir    = entry->workDir;
                result.cycles     = entry->cycles;
                result.memsize    = entry->memsize;
                result.ems        = entry->ems;
                result.xms        = entry->xms;
                result.cdMount    = entry->cdMount;
                result.installFirst = entry->installFirst;
                result.source     = "exe_match";
                result.confidence = 1.0f;
                result.gameType   = entry->cdMount ? "CD_BASED" : "SIMPLE";
                return result;
            }
        }

        // ── Match 2: archive stem → slugify → JSON key ────────────────────────
        std::string archiveStem = archivePath.stem().string();
        std::string slug        = slugify(archiveStem);
        const GameEntry* entry  = m_db->bySlug(slug);
        if (entry) {
            result.success    = true;
            result.slug       = entry->slug;
            result.title      = entry->title;
            result.exe        = entry->exe;
            result.workDir    = entry->workDir;
            result.cycles     = entry->cycles;
            result.memsize    = entry->memsize;
            result.ems        = entry->ems;
            result.xms        = entry->xms;
            result.cdMount    = entry->cdMount;
            result.installFirst = entry->installFirst;
            result.source     = "slug_match";
            result.confidence = 0.9f;
            result.gameType   = entry->cdMount ? "CD_BASED" : "SIMPLE";
            return result;
        }
    }

    // ── Match 3: best scored exe (unknown game) ───────────────────────────────
    // Pick the highest-scoring non-blacklisted exe from the listing
    std::vector<std::pair<float, std::string>> scored;
    std::string archiveStem = archivePath.stem().string();
    std::string stemLo = toLower(archiveStem);

    for (const auto& exeName : exeNames) {
        std::string s = toLower(exeName.substr(0, exeName.rfind('.')));
        if (isBlacklisted(s)) continue;
        float score = 1.0f;
        if (s == stemLo || s.find(stemLo) == 0 || stemLo.find(s) == 0)
            score += 0.3f;
        scored.push_back({score, exeName});
    }
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b){ return a.first > b.first; });

    if (!scored.empty()) {
        result.success    = true;
        result.slug       = slugify(archiveStem);
        result.title      = archiveStem; // user can rename later
        result.exe        = scored[0].second;
        result.cycles     = "max limit 80000";
        result.memsize    = 16;
        result.ems        = true;
        result.xms        = true;
        result.source     = "scored";
        result.confidence = 0.5f;
        result.gameType   = "SIMPLE";
        return result;
    }

    result.error = "Could not identify game";
    return result;
}

// ── writeDosboxConf ───────────────────────────────────────────────────────────

bool Ingestor::writeDosboxConf(const fs::path& extractedDir,
                               const AnalyzeResult& result) const
{
    if (m_confsRoot.empty()) return false;
    fs::create_directories(m_confsRoot);

    fs::path confPath = m_confsRoot / (result.slug + ".conf");

    // If conf already exists, preserve it (only written once — v1 behaviour)
    if (fs::exists(confPath)) return true;

    // Determine mount dir: if workDir specified, mount that subdir as C:
    std::string mountDir = extractedDir.string();
    if (!result.workDir.empty()) {
        fs::path sub = extractedDir / result.workDir;
        if (fs::exists(sub)) mountDir = sub.string();
    }

    // Exe name only (no path) — C: is already mounted at the right dir
    std::string exeName = result.exe;
    {
        size_t sl = exeName.find_last_of("/\\");
        if (sl != std::string::npos) exeName = exeName.substr(sl + 1);
    }

    std::string cycles  = result.cycles.empty() ? "max limit 80000" : result.cycles;
    int         memsize = result.memsize > 0 ? result.memsize : 16;

    std::ostringstream autoexec;
    autoexec << "@echo off\r\n";
    autoexec << "mount C \"" << mountDir << "\"\r\n";

    if (result.cdMount) {
        // Look for ISO/CUE in extracted dir
        std::string isoPath;
        std::error_code ec;
        for (auto& f : fs::recursive_directory_iterator(extractedDir, ec)) {
            if (!f.is_regular_file(ec)) continue;
            std::string ext = toUpper(f.path().extension().string());
            if (ext == ".ISO" || ext == ".CUE" || ext == ".MDF") {
                isoPath = f.path().string();
                break;
            }
        }
        if (!isoPath.empty())
            autoexec << "imgmount D \"" << isoPath << "\" -t iso\r\n";
    }

    autoexec << "C:\r\n";
    autoexec << exeName << "\r\n";
    autoexec << "exit\r\n";

    std::ostringstream conf;
    conf << "[sdl]\r\n"
         << "fullscreen=true\r\nfullresolution=desktop\r\noutput=openglnb\r\n\r\n";
    conf << "[dosbox]\r\n"
         << "machine=svga_s3\r\nmemsize=" << memsize << "\r\n\r\n";
    conf << "[cpu]\r\n"
         << "core=dynamic\r\ncputype=pentium_slow\r\ncycles=" << cycles << "\r\n"
         << "cycleup=500\r\ncycledown=20\r\n\r\n";
    conf << "[dos]\r\n"
         << "ems=" << (result.ems ? "true" : "false") << "\r\n"
         << "xms=" << (result.xms ? "true" : "false") << "\r\n\r\n";
    conf << "[mixer]\r\nrate=44100\r\nblocksize=1024\r\nprebuffer=20\r\n\r\n";
    conf << "[render]\r\nframeskip=0\r\naspect=true\r\n\r\n";
    conf << "[autoexec]\r\n" << autoexec.str() << "\r\n";

    std::ofstream out(confPath);
    if (!out.is_open()) return false;
    out << conf.str();
    return true;
}

// ── ingest — full pipeline ────────────────────────────────────────────────────

AnalyzeResult Ingestor::ingest(const fs::path& archivePath, ProgressFn progress)
{
    // Step 1: analyze (fast — no extraction)
    AnalyzeResult result = analyze(archivePath);
    if (!result.success) return result;

    if (progress) progress(5);

    // Step 2: extract
    fs::path extractDir = m_extractRoot / result.slug;

    // Skip extraction if already extracted (re-add scenario)
    if (!fs::exists(extractDir) || fs::is_empty(extractDir)) {
        if (!extract7za(archivePath, extractDir)) {
            result.success = false;
            result.error   = "Extraction failed";
            return result;
        }
    }

    if (progress) progress(80);

    // Step 2b: auto-copy DOS4GW.EXE — required by many 32-bit DOS games
    // (Doom, Quake, Duke3D, etc.). Games that don't need it will ignore it.
    if (!m_dos4gwPath.empty()) {
        std::error_code ec;
        fs::path dst = extractDir / "DOS4GW.EXE";
        if (!fs::exists(dst))
            fs::copy_file(m_dos4gwPath, dst, ec);
    }

    // Step 3: for scored/unknown matches, rescan extracted dir to confirm exe
    if (result.source == "scored" || result.source == "unknown") {
        auto candidates = scanExtractedDir(extractDir, archivePath.stem().string());
        if (!candidates.empty()) {
            result.exe = candidates[0].name;
        }
    }

    if (progress) progress(90);

    // Step 4: write .conf (skipped if already exists)
    writeDosboxConf(extractDir, result);

    if (progress) progress(100);

    return result;
}

} // namespace AutoDOS2