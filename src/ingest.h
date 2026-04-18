#pragma once
// ingest.h — Phase 04 ZIP ingest pipeline

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>

namespace AutoDOS2 {

// ── AnalyzeResult ─────────────────────────────────────────────────────────────
struct AnalyzeResult {
    bool        success    = false;
    std::string error;

    // Identity
    std::string title;
    std::string slug;          // games.json key

    // Launch
    std::string exe;           // just the filename, e.g. "DUKE3D.EXE"
    std::string workDir;       // subdir inside extracted folder (usually "")
    std::string gameType;      // SIMPLE | CD_BASED | ISO_ONLY

    // DOSBox config
    std::string cycles     = "max limit 80000";
    int         memsize    = 16;
    bool        ems        = true;
    bool        xms        = true;
    bool        cdMount    = false;
    bool        installFirst = false;

    // Match quality
    std::string source;        // "exe_match" | "slug_match" | "scored" | "unknown"
    float       confidence = 0.0f;
};

// ── GameEntry — one row from games.json ──────────────────────────────────────
struct GameEntry {
    std::string slug;
    std::string title;
    std::string exe;
    std::string workDir;
    std::string cycles;
    int         memsize      = 16;
    bool        ems          = true;
    bool        xms          = true;
    bool        cdMount      = false;
    bool        installFirst = false;
    int         year         = 0;
};

// ── GameDatabase — loaded once at startup ─────────────────────────────────────
class GameDatabase {
public:
    bool load(const std::filesystem::path& jsonPath);
    bool isLoaded() const { return m_loaded; }
    int  count()    const { return (int)m_bySlug.size(); }

    // Look up by slug key (e.g. "duke3d")
    const GameEntry* bySlug(const std::string& slug) const;

    // Look up by exe filename (e.g. "DUKE3D.EXE") — case-insensitive
    const GameEntry* byExe(const std::string& exeName) const;

    // Access all entries for fuzzy matching
    const std::unordered_map<std::string, GameEntry>& allEntries() const { return m_bySlug; }

private:
    bool m_loaded = false;
    std::unordered_map<std::string, GameEntry>  m_bySlug;
    std::unordered_map<std::string, std::string> m_exeToSlug; // uppercase exe -> slug
};

// ── Ingestor ──────────────────────────────────────────────────────────────────
class Ingestor {
public:
    void setSevenZipPath(const std::filesystem::path& p) { m_7zaPath = p; }
    void setExtractRoot (const std::filesystem::path& p) { m_extractRoot = p; }
    void setConfsRoot   (const std::filesystem::path& p) { m_confsRoot = p; }
    void setDosboxPath  (const std::filesystem::path& p) { m_dosboxPath = p; }
    void setDatabase    (const GameDatabase* db)         { m_db = db; }
    void setDos4gwPath  (const std::filesystem::path& p) { m_dos4gwPath = p; }

    // Ingest a pre-extracted folder (for CD/ISO games user already unzipped)
    AnalyzeResult ingestFolder(const std::filesystem::path& folderPath);

    // Progress callback: called with 0..100 during extract
    using ProgressFn = std::function<void(int)>;

    // Full pipeline: analyze → extract → write conf → return result
    // Runs synchronously; call from a std::thread.
    AnalyzeResult ingest(const std::filesystem::path& archivePath,
                         ProgressFn progress = nullptr);

    // Just analyze (no extraction) — fast
    AnalyzeResult analyze(const std::filesystem::path& archivePath) const;

    // Write .conf for a result (called after extract)
    bool writeDosboxConf(const std::filesystem::path& extractedDir,
                         const AnalyzeResult& result) const;

private:
    std::filesystem::path m_7zaPath;
    std::filesystem::path m_extractRoot;
    std::filesystem::path m_confsRoot;
    std::filesystem::path m_dosboxPath;
    std::filesystem::path m_dos4gwPath;
    const GameDatabase*   m_db = nullptr;

    // Helpers
    std::string  run7za(const std::string& args) const;
    bool         extract7za(const std::filesystem::path& archive,
                            const std::filesystem::path& outDir) const;

    struct ExeCandidate { std::string relPath; std::string name; float score; };
    std::vector<ExeCandidate> scanExtractedDir(
        const std::filesystem::path& dir, const std::string& archiveStem) const;
};

// ── Utilities ─────────────────────────────────────────────────────────────────
std::string slugify(const std::string& s);   // "Duke Nukem 3D" -> "dukenukm3d"
std::string toUpper(const std::string& s);
std::string toLower(const std::string& s);

} // namespace AutoDOS2