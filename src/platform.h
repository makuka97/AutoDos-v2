#pragma once
#include <string>
#include <filesystem>

namespace AutoDOS2 {

// Returns the platform-appropriate user data directory:
//   Windows : %APPDATA%\AutoDOS2\
//   Linux   : $HOME/.local/share/autodos2/
//   macOS   : $HOME/Library/Application Support/AutoDOS2/
//
// Uses SDL_GetPrefPath under the hood so SDL must be initialised first.
std::filesystem::path getDataDir();

// Returns the directory containing the running executable.
std::filesystem::path getExeDir();

// Opens a native file-open dialog. Returns an empty path on cancel.
// filter_desc   e.g. "ZIP Archives"
// filter_pat    e.g. "*.zip;*.ZIP"
std::filesystem::path openFileDialog(const std::string& filter_desc,
                                     const std::string& filter_pat);

// Opens a native folder picker. Returns empty path on cancel.
std::filesystem::path openFolderDialog();

} // namespace AutoDOS2