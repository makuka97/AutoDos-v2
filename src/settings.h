#pragma once
#include <filesystem>
#include <string>

namespace AutoDOS2 {

// ── AppSettings — persisted to app_config.json ────────────────────────────────
struct AppSettings {
    // Paths
    std::string dosboxPath;
    std::string dataDir;

    // DOSBox defaults
    std::string defaultCycles = "max limit 80000";
    int         defaultMemsize = 16;
    bool        fullscreen     = true;
    std::string sgdbApiKey;

    // UI
    int  gridColumns = 0;  // 0 = auto

    // Load/save from app_config.json next to the exe
    bool load(const std::filesystem::path& configPath);
    bool save(const std::filesystem::path& configPath) const;

    // Fill in any missing paths with sensible defaults
    void applyDefaults(const std::filesystem::path& exeDir,
                       const std::filesystem::path& dataDir);
};

} // namespace AutoDOS2