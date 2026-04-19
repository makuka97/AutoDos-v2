// ingest.cpp -- Phase 04/05 ZIP ingest pipeline
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

// -- String helpers ------------------------------------------------------------

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
        "dosbox","dosbox_staging","dosbox-x","dosboxx","scummvm","boxer",
        "loadpats","intro","movie","logo","start","run"
    };
    std::string lo = toLower(stem);
    for (auto& b : BL) if (lo == b) return true;
    return false;
}

// -- GameDatabase --------------------------------------------------------------

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

// -- byTitle - fuzzy title matching ------------------------------------------

static std::vector<std::string> extractWords(const std::string& s) {
    // Convert to lowercase, replace separators with spaces, split into words
    std::string cleaned;
    for (char c : s) {
        if (std::isalnum((unsigned char)c)) cleaned += std::tolower((unsigned char)c);
        else cleaned += ' ';
    }

    // Strip common non-title suffixes: DOS, EN, WIN, version numbers, years
    static const std::vector<std::string> STRIP = {
        "dos", "en", "win", "pc", "cd", "v1", "v2", "v3", "v4", "v5",
        "lite", "demo", "shareware", "floppy", "hdd", "gog", "rip",
        "patched", "cracked", "nocd", "ost"
    };

    std::istringstream ss(cleaned);
    std::vector<std::string> words;
    std::string w;
    while (ss >> w) {
        // Skip pure numbers (years, version numbers)
        bool allDigits = true;
        for (char c : w) if (!std::isdigit((unsigned char)c)) { allDigits = false; break; }
        if (allDigits) continue;
        // Skip short noise words
        if (w.size() <= 1) continue;
        // Skip common suffixes
        bool skip = false;
        for (auto& sfx : STRIP) if (w == sfx) { skip = true; break; }
        if (skip) continue;
        words.push_back(w);
    }
    return words;
}

const GameEntry* GameDatabase::byTitle(const std::string& archiveName) const
{
    if (m_bySlug.empty()) return nullptr;

    std::vector<std::string> archiveWords = extractWords(archiveName);
    if (archiveWords.empty()) return nullptr;

    const GameEntry* bestEntry = nullptr;
    float bestScore = 0.0f;
    const float MIN_SCORE = 0.60f; // at least 60% word overlap required

    for (auto& [slug, entry] : m_bySlug) {
        std::vector<std::string> titleWords = extractWords(entry.title);
        if (titleWords.empty()) continue;

        // Count words from archive that appear in title
        int matches = 0;
        for (auto& aw : archiveWords)
            for (auto& tw : titleWords)
                if (aw == tw) { matches++; break; }

        // Score = matched words / max(archiveWords, titleWords)
        // This penalises both missing words and extra words
        float score = (float)matches / std::max(archiveWords.size(), titleWords.size());

        if (score > bestScore) {
            bestScore = score;
            bestEntry = &entry;
        }
    }

    return (bestScore >= MIN_SCORE) ? bestEntry : nullptr;
}

// -- run7za --------------------------------------------------------------------

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
        WaitForSingleObject(pi.hProcess, 600000);
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

// -- extract7za ----------------------------------------------------------------

bool Ingestor::extract7za(const fs::path& archive, const fs::path& outDir) const
{
    fs::create_directories(outDir);
    std::string args = "x \"" + archive.string() + "\" -o\"" + outDir.string() + "\" -y";
    std::string out  = run7za(args);
    return out.find("Everything is Ok") != std::string::npos
        || out.find("Files:")            != std::string::npos
        || out.find("extracted")         != std::string::npos;
}

// -- scanExtractedDir ----------------------------------------------------------

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
                score += 0.5f;
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

// -- scanIsoForExe -------------------------------------------------------------

std::string Ingestor::scanIsoForExe(const std::string& isoPath) const
{
    std::string listing = run7za("l \"" + isoPath + "\"");
    if (listing.empty()) return "";

    std::vector<std::pair<float,std::string>> candidates;
    std::istringstream ss(listing);
    std::string line;

    while (std::getline(ss, line)) {
        if (line.size() < 4) continue;
        std::string up = toUpper(line);
        std::string ext = up.substr(up.size()-3);
        if (ext != "EXE" && ext != "COM" && ext != "BAT") continue;

        size_t sp = line.rfind(' ');
        std::string fname = (sp != std::string::npos) ? line.substr(sp+1) : line;
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

// -- analyze -------------------------------------------------------------------

AnalyzeResult Ingestor::analyze(const fs::path& archivePath) const
{
    AnalyzeResult result;

    std::string listArgs = "l -slt \"" + archivePath.string() + "\"";
    std::string listing  = run7za(listArgs);

    if (listing.empty()) {
        result.error = "Could not read archive (7za failed or file not found)";
        return result;
    }

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

    // Detect ScummVM packages -- these need ScummVM, not DOSBox
    for (auto& exe : exeNames) {
        if (toLower(exe) == "scummvm.exe") {
            result.error = "This game uses ScummVM and cannot run in DOSBox. AutoDOS2 supports DOSBox games only.";
            return result;
        }
    }

    if (m_db) {
        // Match 1: exe filename
        for (const auto& exeName : exeNames) {
            std::string stemLo = toLower(exeName.substr(0, exeName.rfind('.')));
            if (isBlacklisted(stemLo)) continue;
            const GameEntry* entry = m_db->byExe(exeName);
            if (entry) {
                result.success      = true;
                result.slug         = entry->slug;
                result.title        = entry->title;
                result.exe          = entry->exe;
                result.workDir      = entry->workDir;
                result.cycles       = entry->cycles;
                result.memsize      = entry->memsize;
                result.ems          = entry->ems;
                result.xms          = entry->xms;
                result.cdMount      = entry->cdMount;
                result.installFirst = entry->installFirst;
                result.source       = "exe_match";
                result.confidence   = 1.0f;
                result.gameType     = entry->cdMount ? "CD_BASED" : "SIMPLE";
                return result;
            }
        }

        // Match 2: slug from archive name
        std::string archiveStem = archivePath.stem().string();
        std::string slug        = slugify(archiveStem);
        const GameEntry* entry  = m_db->bySlug(slug);

        // Try stripping trailing year
        if (!entry && slug.size() > 4) {
            bool hasYear = true;
            for (int i = 0; i < 4; i++)
                if (!std::isdigit((unsigned char)slug[slug.size()-1-i]))
                    { hasYear = false; break; }
            if (hasYear) {
                std::string noYear = slug.substr(0, slug.size() - 4);
                entry = m_db->bySlug(noYear);
                if (entry) slug = noYear;
            }
        }

        if (entry) {
            result.success      = true;
            result.slug         = entry->slug;
            result.title        = entry->title;
            result.exe          = entry->exe;
            result.workDir      = entry->workDir;
            result.cycles       = entry->cycles;
            result.memsize      = entry->memsize;
            result.ems          = entry->ems;
            result.xms          = entry->xms;
            result.cdMount      = entry->cdMount;
            result.installFirst = entry->installFirst;
            result.source       = "slug_match";
            result.confidence   = 0.9f;
            result.gameType     = entry->cdMount ? "CD_BASED" : "SIMPLE";
            return result;
        }
    }

    // Match 3: fuzzy title match against games.json
    // Handles zips from non-ExoDOS sources with different naming conventions
    {
        std::string archiveStem = archivePath.stem().string();
        if (m_db) {
            const GameEntry* entry = m_db->byTitle(archiveStem);
            if (entry) {
                result.success      = true;
                result.slug         = entry->slug;
                result.title        = entry->title;
                result.exe          = entry->exe;
                result.workDir      = entry->workDir;
                result.cycles       = entry->cycles;
                result.memsize      = entry->memsize;
                result.ems          = entry->ems;
                result.xms          = entry->xms;
                result.cdMount      = entry->cdMount;
                result.installFirst = entry->installFirst;
                result.source       = "title_match";
                result.confidence   = 0.75f;
                result.gameType     = entry->cdMount ? "CD_BASED" : "SIMPLE";
                return result;
            }
        }
    }

    // Match 4: best scored exe (last resort)
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

// -- writeDosboxConf -----------------------------------------------------------
//
// Bulletproof logic:
//   cd_mount=true  -> trust games.json exe, run from D: (ISO)
//   cd_mount=false -> find exe on disk, mount its parent as C:

bool Ingestor::writeDosboxConf(const fs::path& extractedDir,
                               const AnalyzeResult& result) const
{
    if (m_confsRoot.empty()) return false;
    fs::create_directories(m_confsRoot);

    fs::path confPath = m_confsRoot / (result.slug + ".conf");
    if (fs::exists(confPath)) return true;

    std::string cycles  = result.cycles.empty() ? "max limit 80000" : result.cycles;
    int         memsize = result.memsize > 0 ? result.memsize : 16;

    std::string exeName = result.exe;
    {
        size_t sl = exeName.find_last_of("/\\");
        if (sl != std::string::npos) exeName = exeName.substr(sl + 1);
    }

    // Find all ISOs
    std::vector<std::string> isoFiles;
    {
        std::error_code ec;
        for (auto& f : fs::recursive_directory_iterator(extractedDir, ec)) {
            if (!f.is_regular_file(ec)) continue;
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

    // Mount dir for C:
    std::string mountDir = extractedDir.string();

    // Always search for exe on disk to find the correct mount dir.
    // For cd_mount games with no ISOs (floppy/disk versions), the exe
    // is on the filesystem just like a non-CD game.
    // Only skip this for cd_mount games that actually have ISOs.
    if (isoFiles.empty()) {
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
            auto candidates = scanExtractedDir(extractedDir, result.slug);
            if (!candidates.empty()) {
                fs::path bestExe = extractedDir / candidates[0].relPath;
                exeName  = candidates[0].name;
                mountDir = bestExe.parent_path().string();
            }
        }
    }

    // Build autoexec
    std::ostringstream autoexec;
    autoexec << "@echo off\r\n";
    autoexec << "mount C \"" << mountDir << "\"\r\n";

    if (!isoFiles.empty()) {
        autoexec << "imgmount D";
        for (auto& iso : isoFiles) autoexec << " \"" << iso << "\"";
        autoexec << " -t iso\r\n";
    }

    if (result.cdMount && !isoFiles.empty()) {
        // CD game with ISOs -- exe is on the disc
        autoexec << "D:\r\n";
        if (!exeName.empty()) autoexec << exeName << "\r\n";
    } else {
        // Disk game OR cd_mount game with no ISOs (floppy version) -- exe on C:
        autoexec << "C:\r\n";
        if (!exeName.empty()) autoexec << exeName << "\r\n";
    }
    autoexec << "exit\r\n";

    // Write conf
    std::ostringstream conf;
    conf << "[sdl]\r\nfullscreen=true\r\nfullresolution=desktop\r\noutput=opengl\r\n\r\n";
    conf << "[dosbox]\r\nmachine=svga_s3\r\nmemsize=" << memsize << "\r\n\r\n";
    conf << "[cpu]\r\ncore=auto\r\ncputype=auto\r\ncycles=" << cycles << "\r\n";
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

// -- ingest -- full pipeline ----------------------------------------------------

AnalyzeResult Ingestor::ingest(const fs::path& archivePath, ProgressFn progress)
{
    AnalyzeResult result = analyze(archivePath);
    if (!result.success) return result;

    if (progress) progress(5);

    fs::path extractDir = m_extractRoot / result.slug;
    if (!fs::exists(extractDir) || fs::is_empty(extractDir)) {
        if (!extract7za(archivePath, extractDir)) {
            result.success = false;
            result.error   = "Extraction failed";
            return result;
        }
    }

    if (progress) progress(80);

    if (!m_dos4gwPath.empty()) {
        std::error_code ec;
        fs::path dst = extractDir / "DOS4GW.EXE";
        if (!fs::exists(dst)) fs::copy_file(m_dos4gwPath, dst, ec);
    }

    if (progress) progress(90);

    writeDosboxConf(extractDir, result);

    if (progress) progress(100);
    return result;
}

// -- ingestFolder -- for pre-extracted CD/ISO games -----------------------------

AnalyzeResult Ingestor::ingestFolder(const fs::path& folderPath)
{
    AnalyzeResult result;

    if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
        result.error = "Folder not found: " + folderPath.string();
        return result;
    }

    std::string folderName = folderPath.filename().string();
    std::string slug       = slugify(folderName);

    // Collect EXEs and ISOs
    std::vector<std::string> exeNames;
    std::vector<std::string> isoFiles;
    std::error_code ec;

    for (auto& f : fs::recursive_directory_iterator(folderPath, ec)) {
        if (!f.is_regular_file(ec)) continue;
        bool skip = false;
        for (auto& seg : f.path()) {
            std::string s = toLower(seg.string());
            if (s == "dosbox" || s == "dosbox-x" || s == "dosboxx")
                { skip = true; break; }
        }
        if (skip) continue;
        std::string ext = toUpper(f.path().extension().string());
        if (ext == ".EXE" || ext == ".COM" || ext == ".BAT")
            exeNames.push_back(toUpper(f.path().filename().string()));
        else if (ext == ".ISO" || ext == ".CUE" || ext == ".MDF")
            isoFiles.push_back(f.path().string());
    }
    std::sort(isoFiles.begin(), isoFiles.end());

    // Match against games.json
    const GameEntry* entry = nullptr;

    if (m_db) {
        // 1. EXE match from files on disk
        for (auto& exeName : exeNames) {
            std::string s = toLower(exeName.substr(0, exeName.rfind('.')));
            if (isBlacklisted(s)) continue;
            entry = m_db->byExe(exeName);
            if (entry) break;
        }

        // 2. Slug match
        if (!entry) entry = m_db->bySlug(slug);

        // 3. Strip trailing year
        if (!entry && slug.size() > 4) {
            bool allDigits = true;
            for (int i = 1; i <= 4; i++)
                if (!std::isdigit((unsigned char)slug[slug.size()-i]))
                    { allDigits = false; break; }
            if (allDigits) {
                std::string noYear = slug.substr(0, slug.size() - 4);
                entry = m_db->bySlug(noYear);
                if (entry) slug = noYear;
            }
        }
        // Match 4: fuzzy title match using folder name
        if (!entry) {
            entry = m_db->byTitle(folderName);
        }
    }

    if (entry) {
        result.slug         = entry->slug;
        result.title        = entry->title;
        result.exe          = entry->exe;
        result.workDir      = entry->workDir;
        result.cycles       = entry->cycles;
        result.memsize      = entry->memsize;
        result.ems          = entry->ems;
        result.xms          = entry->xms;
        result.cdMount      = entry->cdMount;
        result.installFirst = entry->installFirst;
        result.source       = "matched";
        result.confidence   = 1.0f;
    } else {
        result.slug       = slug;
        result.title      = folderName;
        result.source     = "unmatched";
        result.confidence = 0.0f;
        result.cdMount    = !isoFiles.empty();
        auto candidates   = scanExtractedDir(folderPath, folderName);
        if (!candidates.empty()) result.exe = candidates[0].name;
    }

    result.gameType = isoFiles.empty() ? "SIMPLE" : "CD_BASED";
    result.success  = true;

    // Create stub folder
    fs::path extractDir = m_extractRoot / result.slug;
    if (!fs::exists(extractDir)) {
        fs::create_directories(extractDir, ec);
        std::ofstream marker(extractDir / ".folder_link");
        marker << folderPath.string();
    }

    // Write conf
    fs::path confPath = m_confsRoot / (result.slug + ".conf");
    if (!fs::exists(confPath)) {
        std::string exeName = result.exe;
        {
            size_t sl = exeName.find_last_of("/\\");
            if (sl != std::string::npos) exeName = exeName.substr(sl+1);
        }

        std::string cycles  = result.cycles.empty() ? "max limit 80000" : result.cycles;
        int         memsize = result.memsize > 0 ? result.memsize : 16;
        std::string mountDir = folderPath.string();
        bool exeOnDisk = false;

        if (!result.cdMount && !exeName.empty()) {
            std::string exeUp = toUpper(exeName);
            for (auto& f : fs::recursive_directory_iterator(folderPath, ec)) {
                if (!f.is_regular_file(ec)) continue;
                if (toUpper(f.path().filename().string()) == exeUp) {
                    mountDir  = f.path().parent_path().string();
                    exeOnDisk = true;
                    break;
                }
            }
            if (!exeOnDisk) {
                auto candidates = scanExtractedDir(folderPath, result.slug);
                if (!candidates.empty()) {
                    fs::path bestExe = folderPath / candidates[0].relPath;
                    exeName  = candidates[0].name;
                    mountDir = bestExe.parent_path().string();
                }
            }
        }

        std::ostringstream autoexec;
        autoexec << "@echo off\r\n";
        autoexec << "mount C \"" << mountDir << "\"\r\n";

        if (!isoFiles.empty()) {
            autoexec << "imgmount D";
            for (auto& iso : isoFiles) autoexec << " \"" << iso << "\"";
            autoexec << " -t iso\r\n";
        }

        if (result.cdMount && !isoFiles.empty()) {
            autoexec << "D:\r\n";
            if (!exeName.empty()) autoexec << exeName << "\r\n";
        } else {
            autoexec << "C:\r\n";
            if (!exeName.empty()) autoexec << exeName << "\r\n";
        }
        autoexec << "exit\r\n";

        std::ostringstream conf;
        conf << "[sdl]\r\nfullscreen=true\r\nfullresolution=desktop\r\noutput=opengl\r\n\r\n";
        conf << "[dosbox]\r\nmachine=svga_s3\r\nmemsize=" << memsize << "\r\n\r\n";
        conf << "[cpu]\r\ncore=auto\r\ncputype=auto\r\ncycles=" << cycles << "\r\n";
        conf << "cycleup=500\r\ncycledown=20\r\n\r\n";
        conf << "[dos]\r\nems=" << (result.ems ? "true" : "false") << "\r\n";
        conf << "xms=" << (result.xms ? "true" : "false") << "\r\n\r\n";
        conf << "[mixer]\r\nrate=44100\r\nblocksize=1024\r\nprebuffer=20\r\n\r\n";
        conf << "[render]\r\naspect=true\r\n\r\n";
        conf << "[autoexec]\r\n" << autoexec.str() << "\r\n";

        fs::create_directories(m_confsRoot, ec);
        std::ofstream out(confPath);
        if (out.is_open()) out << conf.str();
    }

    return result;
}

} // namespace AutoDOS2