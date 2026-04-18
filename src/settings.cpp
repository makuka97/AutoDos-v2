#include "settings.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace AutoDOS2 {

bool AppSettings::load(const fs::path& configPath)
{
    std::ifstream f(configPath);
    if (!f.is_open()) return false;
    json j;
    try { f >> j; } catch (...) { return false; }

    if (j.contains("dosbox_path") && j["dosbox_path"].is_string())
        dosboxPath = j["dosbox_path"];
    if (j.contains("data_dir") && j["data_dir"].is_string())
        dataDir = j["data_dir"];
    if (j.contains("dosbox_defaults")) {
        auto& d = j["dosbox_defaults"];
        if (d.contains("cycles") && d["cycles"].is_string())
            defaultCycles = d["cycles"];
        if (d.contains("memsize") && d["memsize"].is_number())
            defaultMemsize = d["memsize"];
        if (d.contains("fullscreen") && d["fullscreen"].is_boolean())
            fullscreen = d["fullscreen"];
    }
    if (j.contains("ui") && j["ui"].contains("grid_columns"))
        gridColumns = j["ui"]["grid_columns"];
    return true;
}

bool AppSettings::save(const fs::path& configPath) const
{
    json j;
    j["dosbox_path"] = dosboxPath;
    j["data_dir"]    = dataDir;
    j["dosbox_defaults"] = {
        {"cycles",     defaultCycles},
        {"memsize",    defaultMemsize},
        {"fullscreen", fullscreen}
    };
    j["ui"] = {{"grid_columns", gridColumns}};

    std::ofstream f(configPath);
    if (!f.is_open()) return false;
    f << j.dump(2);
    return true;
}

void AppSettings::applyDefaults(const fs::path& exeDir, const fs::path& dataDirPath)
{
    if (dosboxPath.empty())
        dosboxPath = (exeDir / "dosbox" / "dosbox.exe").string();
    if (dataDir.empty())
        dataDir = dataDirPath.string();
}

} // namespace AutoDOS2
