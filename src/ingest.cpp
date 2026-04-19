// ingest.cpp — Phase 04/05 ZIP ingest pipeline
#include "ingest.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
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

std::string slugify(const std::string& s) {
    std::string r;
    for (char c : s)
        if (std::isalnum((unsigned char)c))
            r += std::tolower((unsigned char)c);
    return r;
}

static bool isBlacklisted(const std::string& stem) {
    static const std::vector<std::string> BL = {
        "setup","install","uninst","uninstall","patch","update",
        "config","cfg","register","readme","read","help",
        "directx","dxsetup","vcredist","dotnet",
        "dos4gw","cwsdpmi","himemx","emm386",
        "fixsave","fix","convert","copy","move",
        "dosbox","dosbox_staging","dosbox-x","dosboxx","scummvm","boxer","loadpats","intro","movie","logo","start","run"
    };
    std::string lo = toLower(stem);
    for (auto& b : BL) if (lo == b) return true;
    return false;
}

// ── GameDatabase ──────────────────────────────────────────────────────────────

bool GameDatabase::load(const fs::path& jsonPath)
{
    std::ifstream f(jsonPath, std::ios::binary);
    if (!f.is_open()) return false;

    f.seekg(0, std::ios::end);
    auto sz = f.tellg();
    f.seekg(0, std::ios::beg);
    if (sz <= 0) return false;

    json root;
    try { f >> root; } catch (...) { return false; }

    auto& games = root["games"];
    if (!games.is_object()) return false;

    m_bySlug.clear();
    m_exeToSlug.clear();

    for (auto it = games.begin(); it != games.end(); ++it) {
        try {
            const std::string& slug = it.key();
            const json& g = it.value();
            if (!g.is_object()) continue;

            GameEntry e;
            e.slug = slug;

            if (g.contains("title") && g["title"].is_string())
                e.title = g["title"].get<std::string>();
            if (g.contains("exe") && g["exe"].is_string())
                e.exe = g["exe"].get<std::string>();
            if (g.contains("work_dir") && g["work_dir"].is_string())
                e.workDir = g["work_dir"].get<std::string>();

            if (g.contains("cycles")) {
                const auto& cyc = g["cycles"];
                if      (cyc.is_string()) e.cycles = cyc.get<std::string>();
                else if (cyc.is_number()) e.cycles = std::to_string(cyc.get<int>());
                else                      e.cycles = "max limit 80000";
            } else {
                e.cycles = "max limit 80000";
            }

            if (g.contains("memsize") && g["memsize"].is_number())
                e.memsize = g["memsize"].get<int>();
            if (g.contains("ems") && g["ems"].is_boolean())
                e.ems = g["ems"].get<bool>();
            if (g.contains("xms") && g["xms"].is_boolean())
                e.xms = g["xms"].get<bool>();
            if (g.contains("cd_mount") && g["cd_mount"].is_boolean())
                e.cdMount = g["cd_mount"].get<bool>();
            if (g.contains("install_first") && g["install_first"].is_boolean())
                e.installFirst = g["install_first"].get<bool>();
            if (g.contains("year") && g["year"].is_number())
                e.year = g["year"].get<int>();

            m_bySlug[slug] = e;
            if (!e.exe.empty())
                m_exeToSlug.emplace(toUpper(e.exe), slug);

        } catch (...) {}
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
        WaitForSingleObject(pi.hProcess, 600000); // 10 min timeout for large archives
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

std::vector<Ingestor::ExeCandidate>
Ingestor::scanExtractedDir(const fs::path& dir, const std::string& archiveStem) const
{
    std::vector<ExeCandidate> candidates;
    if (!fs::exists(dir)) return candidates;

    std::string stemLo = toLower(archiveStem);
    std::error_code ec;

    std::function<void(const fs::path&, int)> walk = [&](const fs::path& d, int depth) {
        for (auto& entry : fs::directory_iterator(d, ec)) {
            if (entry.is_directory(ec)) {
                // Skip DOSBOX and DOSBOX-X bundled emulator folders
                std::string dirName = toLower(entry.path().filename().string());
                if (dirName == "dosbox" || dirName == "dosbox-x" || dirName == "dosbx")
                    continue;
                walk(entry.path(), depth + 1);
                continue;
            }
            if (!entry.is_regular_file(ec)) continue;

            std::string ext = toUpper(entry.path().extension().string());
            if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
            if (ext != "EXE" && ext != "COM" && ext != "BAT") continue;

            std::string stem = toLower(entry.path().stem().string());
            if (isBlacklisted(stem)) continue;

            float score = (ext == "EXE") ? 1.0f : (ext == "BAT") ? 0.8f : 0.6f;
            score -= depth * 0.15f;
            if (score < 0.01f) score = 0.01f;

            if (stem == stemLo || stem.find(stemLo) == 0 || stemLo.find(stem) == 0)
                score += 0.5f;  // strong boost for slug match
            if (stem == "game" || stem == "play" || stem == "main" || stem == "go")
                score += 0.15f;

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

AnalyzeResult Ingestor::analyze(const fs::path& archivePath) const
{
    AnalyzeResult result;

    // List archive contents via 7za
    std::string listArgs = "l -slt \"" + archivePath.string() + "\"";
    std::string listing  = run7za(listArgs);

    if (listing.empty()) {
        result.error = "Could not read archive (7za failed or file not found)";
        return result;
    }

    // Parse file names from 7za listing
    std::vector<std::string> exeNames;
    std::istringstream ss(listing);
    std::string line, currentPath;
    bool isDir = false;

    auto commit = [&]() {
        if (currentPath.empty() || isDir) return;
        std::replace(currentPath.begin(), currentPath.end(), '\\', '/');
        std::string fname = currentPath;
        size_t sl = fname.rfind('/');
        if (sl != std::string::npos) fname = fname.substr(sl + 1);
        std::string ext = fname.size() > 4 ? toUpper(fname.substr(fname.size() - 3)) : "";
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
        while (!key.empty() && key.back() == ' ') key.pop_back();
        while (!val.empty() && val.back() == ' ') val.pop_back();
        if      (key == "Path")       { commit(); currentPath = val; }
        else if (key == "Attributes") isDir = val.find('D') != std::string::npos;
    }
    commit();

    if (exeNames.empty()) {
        result.error = "No executable files found in archive";
        return result;
    }

    // ── Match 1: exe filename → games.json ───────────────────────────────────
    if (m_db) {
        for (const auto& exeName : exeNames) {
            std::string stemLo = toLower(exeName.substr(0, exeName.rfind('.')));
            if (isBlacklisted(stemLo)) continue;

            const GameEntry* entry = m_db->byExe(exeName);
            if (entry) {
                result.success     = true;
                result.slug        = entry->slug;
                result.title       = entry->title;
                result.exe         = entry->exe;
                result.workDir     = entry->workDir;
                result.cycles      = entry->cycles;
                result.memsize     = entry->memsize;
                result.ems         = entry->ems;
                result.xms         = entry->xms;
                result.cdMount     = entry->cdMount;
                result.installFirst= entry->installFirst;
                result.source      = "exe_match";
                result.confidence  = 1.0f;
                result.gameType    = entry->cdMount ? "CD_BASED" : "SIMPLE";
                return result;
            }
        }

        // ── Match 2: archive stem → slugify → JSON key ────────────────────────
        std::string archiveStem = archivePath.stem().string();
        std::string slug        = slugify(archiveStem);
        const GameEntry* entry  = m_db->bySlug(slug);

        // Try stripping trailing year (4 digits) from slug if no match
        if (!entry && slug.size() > 4) {
            std::string noYear = slug;
            // Strip trailing 4-digit year
            bool hasYear = true;
            for (int i = 0; i < 4; i++)
                if (!std::isdigit((unsigned char)noYear[noYear.size()-1-i]))
                    { hasYear = false; break; }
            if (hasYear) {
                noYear = noYear.substr(0, noYear.size() - 4);
                entry = m_db->bySlug(noYear);
                if (entry) slug = noYear;
            }
        }
        if (entry) {
            result.success     = true;
            result.slug        = entry->slug;
            result.title       = entry->title;
            result.exe         = entry->exe;
            result.workDir     = entry->workDir;
            result.cycles      = entry->cycles;
            result.memsize     = entry->memsize;
            result.ems         = entry->ems;
            result.xms         = entry->xms;
            result.cdMount     = entry->cdMount;
            result.installFirst= entry->installFirst;
            result.source      = "slug_match";
            result.confidence  = 0.9f;
            result.gameType    = entry->cdMount ? "CD_BASED" : "SIMPLE";
            return result;
        }
    }

    // ── Match 3: best scored exe ──────────────────────────────────────────────
    std::string archiveStem = archivePath.stem().string();
    std::string stemLo = toLower(archiveStem);
    std::vector<std::pair<float, std::string>> scored;

    for (const auto& exeName : exeNames) {
        std::string s = toLower(exeName.substr(0, exeName.rfind('.')));
        if (isBlacklisted(s)) continue;
        float score = 1.0f;
        if (s == stemLo || s.find(stemLo) == 0 || stemLo.find(s) == 0) score += 0.3f;
        scored.push_back({score, exeName});
    }
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b){ return a.first > b.first; });

    if (!scored.empty()) {
        result.success    = true;
        result.slug       = slugify(archiveStem);
        result.title      = archiveStem;
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

// ── scanIsoForExe — find best exe inside an ISO using 7za ────────────────────

std::string Ingestor::scanIsoForExe(const std::string& isoPath) const
{
    std::string listing = run7za("l \"" + isoPath + "\"");
    if (listing.empty()) return "";

    std::vector<std::pair<float,std::string>> candidates;
    std::istringstream ss(listing);
    std::string line;

    while (std::getline(ss, line)) {
        if (line.size() < 4) continue;
        // 7za output lines end with the filename after spaces
        // Look for .EXE or .COM at end of line
        std::string up = toUpper(line);
        if (up.size() < 4) continue;
        std::string ext = up.substr(up.size()-3);
        if (ext != "EXE" && ext != "COM" && ext != "BAT") continue;

        // Extract filename from end of line
        size_t sp = line.rfind(' ');
        std::string fname = (sp != std::string::npos) ? line.substr(sp+1) : line;
        // Remove path, keep just filename
        size_t sl = fname.find_last_of("/\\");
        std::string name = (sl != std::string::npos) ? fname.substr(sl+1) : fname;
        name = toUpper(name);
        if (name.empty()) continue;

        std::string stem = toLower(name.substr(0, name.rfind('.')));
        if (isBlacklisted(stem)) continue;

        candidates.push_back({1.0f, name});
    }

    if (candidates.empty()) return "";
    return candidates[0].second;
}

// ── writeDosboxConf ───────────────────────────────────────────────────────────

bool Ingestor::writeDosboxConf(const fs::path& extractedDir,
                               const AnalyzeResult& result) const
{
    if (m_confsRoot.empty()) return false;
    fs::create_directories(m_confsRoot);

    fs::path confPath = m_confsRoot / (result.slug + ".conf");
    if (fs::exists(confPath)) return true;

    // Exe name only (strip path prefix)
    std::string exeName = result.exe;
    {
        size_t sl = exeName.find_last_of("/\\");
        if (sl != std::string::npos) exeName = exeName.substr(sl + 1);
    }

    // ── Find all ISO/CUE/MDF images in extracted dir ─────────────────────────
    // Skip bundled emulator subfolders (dosbox, dosbox-x) and pre-mounted
    // C: drive subfolders (DOSBox-X style zips with a C\ subfolder)
    std::vector<std::string> isoFiles;
    {
        std::error_code ec;
        for (auto& f : fs::recursive_directory_iterator(extractedDir, ec)) {
            if (!f.is_regular_file(ec)) continue;
            // Skip files inside bundled emulator folders
            std::string pathStr = toLower(f.path().string());
            bool skip = false;
            for (auto& seg : f.path()) {
                std::string s = toLower(seg.string());
                if (s == "dosbox" || s == "dosbox-x" || s == "dosboxx")
                    { skip = true; break; }
            }
            if (skip) continue;
            std::string ext = toUpper(f.path().extension().string());
            if (ext == ".ISO" || ext == ".CUE" || ext == ".MDF")
                isoFiles.push_back(f.path().string());
        }
        std::sort(isoFiles.begin(), isoFiles.end());
    }

    // ── Find exe location on disk ─────────────────────────────────────────────
    std::string mountDir = extractedDir.string();
    {
        std::error_code ec;
        std::string exeUp = toUpper(exeName);
        bool found = false;

        for (auto& f : fs::recursive_directory_iterator(extractedDir, ec)) {
            if (!f.is_regular_file(ec)) continue;
            if (toUpper(f.path().filename().string()) == exeUp) {
                mountDir = f.path().parent_path().string();
                found = true;
                break;
            }
        }

        if (!found) {
            // Try scanning inside ISOs first (exe lives on disc)
            bool foundInIso = false;
            if (!isoFiles.empty()) {
                std::string isoExe = scanIsoForExe(isoFiles[0]);
                if (!isoExe.empty()) {
                    // Also try to match against DB
                    if (m_db) {
                        const GameEntry* e = m_db->byExe(isoExe);
                        if (e) {
                            // Update result with correct DB entry
                            exeName = isoExe;
                            foundInIso = true;
                        }
                    }
                    if (!foundInIso) {
                        exeName = isoExe;
                        foundInIso = true;
                    }
                }
            }
            // Fall back to scanning filesystem
            if (!foundInIso) {
                auto candidates = scanExtractedDir(extractedDir, result.slug);
                if (!candidates.empty()) {
                    fs::path bestExe = extractedDir / candidates[0].relPath;
                    exeName  = candidates[0].name;
                    mountDir = bestExe.parent_path().string();
                }
            }
        }
    }

    std::string cycles  = result.cycles.empty() ? "max limit 80000" : result.cycles;
    int         memsize = result.memsize > 0 ? result.memsize : 16;

    std::ostringstream autoexec;
    autoexec << "@echo off\r\n";

    // Check if exe actually exists on disk (not inside an ISO)
    bool exeOnDisk = false;
    if (!exeName.empty()) {
        std::error_code ec2;
        std::string exeUp = toUpper(exeName);
        for (auto& f : fs::recursive_directory_iterator(extractedDir, ec2)) {
            if (!f.is_regular_file(ec2)) continue;
            if (toUpper(f.path().filename().string()) == exeUp) { exeOnDisk = true; break; }
        }
    }

    if (result.cdMount && !isoFiles.empty()) {
        // ── CD-based game ─────────────────────────────────────────────────────
        autoexec << "mount C \"" << mountDir << "\"\r\n";
        autoexec << "imgmount D";
        for (auto& iso : isoFiles) autoexec << " \"" << iso << "\"";
        autoexec << " -t iso\r\n";

        if (exeOnDisk) {
            // Exe is on the filesystem (C:)
            autoexec << "C:\r\n";
            autoexec << exeName << "\r\n";
        } else {
            // Exe is on the CD (D:) — common for ISO-only games
            autoexec << "D:\r\n";
            autoexec << exeName << "\r\n";
        }
    } else if (result.cdMount && isoFiles.empty()) {
        // cd_mount=true but no ISO — exe is on C:, CD just for music
        autoexec << "mount C \"" << mountDir << "\"\r\n";
        autoexec << "C:\r\n";
        autoexec << exeName << "\r\n";
    } else {
        // Standard non-CD game
        autoexec << "mount C \"" << mountDir << "\"\r\n";
        autoexec << "C:\r\n";
        autoexec << exeName << "\r\n";
    }

    autoexec << "exit\r\n";

    std::ostringstream conf;
    conf << "[sdl]\r\nfullscreen=true\r\nfullresolution=desktop\r\noutput=opengl\r\n\r\n";
    conf << "[dosbox]\r\nmachine=svga_s3\r\nmemsize=" << memsize << "\r\n\r\n";
    conf << "[cpu]\r\ncore=auto\r\ncputype=auto\r\ncpu_cycles=" << cycles << "\r\n";
    conf << "cycleup=500\r\ncycledown=20\r\n\r\n";
    conf << "[dos]\r\nems=" << (result.ems ? "true" : "false") << "\r\n";
    conf << "xms=" << (result.xms ? "true" : "false") << "\r\n\r\n";
    conf << "[mixer]\r\nrate=44100\r\nblocksize=1024\r\nprebuffer=20\r\n\r\n";
    conf << "[render]\r\naspect=true\r\n\r\n";
    conf << "[autoexec]\r\n" << autoexec.str() << "\r\n";

    std::ofstream out(confPath);
    if (!out.is_open()) return false;
    out << conf.str();
    return true;
}
// ── ingest — full pipeline ────────────────────────────────────────────────────

AnalyzeResult Ingestor::ingest(const fs::path& archivePath, ProgressFn progress)
{
    AnalyzeResult result = analyze(archivePath);
    if (!result.success) return result;

    if (progress) progress(5);

    // Extract
    fs::path extractDir = m_extractRoot / result.slug;
    if (!fs::exists(extractDir) || fs::is_empty(extractDir)) {
        if (!extract7za(archivePath, extractDir)) {
            result.success = false;
            result.error   = "Extraction failed";
            return result;
        }
    }

    if (progress) progress(80);

    // Auto-copy DOS4GW.EXE into every game folder
    if (!m_dos4gwPath.empty()) {
        std::error_code ec;
        fs::path dst = extractDir / "DOS4GW.EXE";
        if (!fs::exists(dst)) fs::copy_file(m_dos4gwPath, dst, ec);
    }

    if (progress) progress(90);

    // Always generate our own DOSBox Staging conf.
    // Bundled confs from zips are ignored — they target DOSBox-X or old
    // DOSBox SVN and use relative paths that break outside their original tree.
    writeDosboxConf(extractDir, result);

    if (progress) progress(100);
    return result;
}

// ── ingestFolder — for pre-extracted CD/ISO games ─────────────────────────────
// User unzipped the game themselves and points us at the folder.
// We scan for ISOs/EXEs, match games.json, write conf, no extraction needed.

AnalyzeResult Ingestor::ingestFolder(const fs::path& folderPath)
{
    AnalyzeResult result;

    if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
        result.error = "Folder not found: " + folderPath.string();
        return result;
    }

    // Use the folder name as the slug hint
    std::string folderName = folderPath.filename().string();
    std::string slug       = slugify(folderName);

    // ── Step 1: collect all EXEs and ISOs recursively ──────────────────────
    std::vector<std::string> exeNames;
    std::vector<std::string> isoFiles;
    std::error_code ec;

    for (auto& f : fs::recursive_directory_iterator(folderPath, ec)) {
        if (!f.is_regular_file(ec)) continue;
        std::string ext = toUpper(f.path().extension().string());
        std::string fnUp = toUpper(f.path().filename().string());

        // Skip bundled emulator folders
        bool skip = false;
        for (auto& seg : f.path())
            if (toLower(seg.string()) == "dosbox" || toLower(seg.string()) == "dosbox-x")
                { skip = true; break; }
        if (skip) continue;

        if (ext == ".EXE" || ext == ".COM" || ext == ".BAT")
            exeNames.push_back(fnUp);
        else if (ext == ".ISO" || ext == ".CUE" || ext == ".MDF")
            isoFiles.push_back(f.path().string());
    }
    std::sort(isoFiles.begin(), isoFiles.end());

    // ── Step 2: match against games.json ───────────────────────────────────
    const GameEntry* entry = nullptr;

    // Try exe match first
    if (m_db) {
        for (auto& exeName : exeNames) {
            std::string s = toLower(exeName.substr(0, exeName.rfind('.')));
            if (isBlacklisted(s)) continue;
            entry = m_db->byExe(exeName);
            if (entry) break;
        }
        // Try slug match
        if (!entry) entry = m_db->bySlug(slug);
        // Try stripping year
        if (!entry && slug.size() > 4 && slug.back() >= '0' && slug.back() <= '9') {
            std::string noYear = slug.substr(0, slug.size() - 4);
            entry = m_db->bySlug(noYear);
            if (entry) slug = noYear;
        }
        // If still no match and we have ISOs, scan inside first ISO for exe names
        if (!entry && !isoFiles.empty()) {
            std::string listing = run7za("l \"" + isoFiles[0] + "\"");
            std::istringstream ss(listing);
            std::string line;
            while (std::getline(ss, line)) {
                if (line.size() < 4) continue;
                std::string ext = toUpper(line.substr(line.size()-3));
                if (ext == "EXE" || ext == "COM") {
                    size_t sp = line.rfind(' ');
                    std::string fname = (sp != std::string::npos) ? line.substr(sp+1) : line;
                    fname = toUpper(fname);
                    size_t sl = fname.find_last_of("/\\");
                    if (sl != std::string::npos) fname = fname.substr(sl+1);
                    if (!fname.empty()) {
                        entry = m_db->byExe(fname);
                        if (entry) break;
                    }
                }
            }
        }
    }

    if (entry) {
        result.slug        = entry->slug;
        result.title       = entry->title;
        result.exe         = entry->exe;
        result.workDir     = entry->workDir;
        result.cycles      = entry->cycles;
        result.memsize     = entry->memsize;
        result.ems         = entry->ems;
        result.xms         = entry->xms;
        result.cdMount     = entry->cdMount;
        result.installFirst= entry->installFirst;
        result.source      = "exe_match";
        result.confidence  = 1.0f;
    } else {
        // Unknown game — use folder name as title
        result.slug       = slug;
        result.title      = folderName;
        result.source     = "scored";
        result.confidence = 0.5f;
        result.cdMount    = !isoFiles.empty();

        // Pick best exe
        auto candidates = scanExtractedDir(folderPath, folderName);
        if (!candidates.empty()) result.exe = candidates[0].name;
    }

    result.gameType = isoFiles.empty() ? "SIMPLE" : "CD_BASED";

    // ── Step 3: symlink/copy the folder into extractRoot ───────────────────
    // Instead of extracting, we just point the conf at the original folder.
    // We still create a stub entry in extractRoot so the rest of the pipeline
    // works (conf lives next to it, art lives in art/).
    fs::path extractDir = m_extractRoot / result.slug;
    if (!fs::exists(extractDir)) {
        // Create a small marker file so we know this is a folder-linked game
        fs::create_directories(extractDir, ec);
        std::ofstream marker(extractDir / ".folder_link");
        marker << folderPath.string();
    }

    // ── Step 4: write conf pointing at the ORIGINAL folder ─────────────────
    // Override extractedDir with the user's actual folder path
    fs::path confPath = m_confsRoot / (result.slug + ".conf");
    if (!fs::exists(confPath)) {
        // Build the conf manually — use the original folder path as mount point
        std::string mountDir = folderPath.string();

        // Find exe on disk if db entry has one
        if (!result.exe.empty()) {
            std::error_code ec2;
            std::string exeUp = toUpper(result.exe);
            for (auto& f : fs::recursive_directory_iterator(folderPath, ec2)) {
                if (!f.is_regular_file(ec2)) continue;
                if (toUpper(f.path().filename().string()) == exeUp) {
                    mountDir = f.path().parent_path().string();
                    break;
                }
            }
        }

        std::string cycles  = result.cycles.empty() ? "max limit 80000" : result.cycles;
        int         memsize = result.memsize > 0 ? result.memsize : 16;
        std::string exeName = result.exe;
        { size_t sl=exeName.find_last_of("/\\"); if(sl!=std::string::npos) exeName=exeName.substr(sl+1); }

        std::ostringstream autoexec;
        autoexec << "@echo off\r\n";
        autoexec << "mount C \"" << mountDir << "\"\r\n";

        if (!isoFiles.empty()) {
            autoexec << "imgmount D";
            for (auto& iso : isoFiles) autoexec << " \"" << iso << "\"";
            autoexec << " -t iso\r\n";
        }


        // If no exe on disk but ISOs present, the exe lives on the disc (D:)
        if (exeName.empty() && !isoFiles.empty()) {
            autoexec << "D:\r\n";
            if (!result.exe.empty()) {
                std::string dbExe = result.exe;
                size_t sl = dbExe.find_last_of("/\\");
                if (sl != std::string::npos) dbExe = dbExe.substr(sl+1);
                autoexec << dbExe << "\r\n";
            }
        } else {
            autoexec << "C:\r\n";
            if (!exeName.empty()) autoexec << exeName << "\r\n";
        }

        std::ostringstream conf;
        conf << "[sdl]\r\nfullscreen=true\r\nfullresolution=desktop\r\noutput=opengl\r\n\r\n";
        conf << "[dosbox]\r\nmachine=svga_s3\r\nmemsize=" << memsize << "\r\n\r\n";
        conf << "[cpu]\r\ncore=auto\r\ncputype=auto\r\ncpu_cycles=" << cycles << "\r\n";
        conf << "cycleup=500\r\ncycledown=20\r\n\r\n";
        conf << "[dos]\r\nems=" << (result.ems?"true":"false") << "\r\n";
        conf << "xms=" << (result.xms?"true":"false") << "\r\n\r\n";
        conf << "[mixer]\r\nrate=44100\r\nblocksize=1024\r\nprebuffer=20\r\n\r\n";
        conf << "[render]\r\naspect=true\r\n\r\n";
        conf << "[autoexec]\r\n" << autoexec.str() << "\r\n";

        fs::create_directories(m_confsRoot, ec);
        std::ofstream out(confPath);
        if (out.is_open()) out << conf.str();
    }

    result.success = true;
    return result;
}

} // namespace AutoDOS2