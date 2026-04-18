#include "platform.h"

#include <SDL.h>
#include <stdexcept>

#ifdef AUTODOS2_WINDOWS
#  include <windows.h>
#  include <commdlg.h>
#  include <shlobj.h>
#endif

namespace AutoDOS2 {

std::filesystem::path getDataDir()
{
    // SDL_GetPrefPath creates the directory if it doesn't exist and
    // returns a UTF-8 string on all platforms.
    char* raw = SDL_GetPrefPath("AutoDOS2", "autodos2");
    if (!raw) {
        throw std::runtime_error(std::string("SDL_GetPrefPath failed: ") + SDL_GetError());
    }
    std::filesystem::path result(raw);
    SDL_free(raw);
    return result;
}

std::filesystem::path getExeDir()
{
    char* raw = SDL_GetBasePath();
    if (!raw) {
        throw std::runtime_error(std::string("SDL_GetBasePath failed: ") + SDL_GetError());
    }
    std::filesystem::path result(raw);
    SDL_free(raw);
    return result;
}

std::filesystem::path openFileDialog(const std::string& filter_desc,
                                     const std::string& filter_pat)
{
#ifdef AUTODOS2_WINDOWS
    // Win32 OPENFILENAMEA — synchronous, returns a path or empty.
    char buf[MAX_PATH] = {};

    // Build the null-delimited filter string: "Desc\0pattern\0\0"
    std::string filter = filter_desc + '\0' + filter_pat + '\0' + '\0';

    OPENFILENAMEA ofn   = {};
    ofn.lStructSize     = sizeof(ofn);
    ofn.hwndOwner       = nullptr; // will be updated in Phase 03 when we have a HWND
    ofn.lpstrFilter     = filter.data();
    ofn.nFilterIndex    = 1;
    ofn.lpstrFile       = buf;
    ofn.nMaxFile        = MAX_PATH;
    ofn.Flags           = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn)) {
        return std::filesystem::path(buf);
    }
    return {}; // user cancelled

#else
    // Phase 01 stub — native dialogs (tinyfd/SDL3) land in Phase 04.
    // Return empty to signal "no file selected".
    (void)filter_desc;
    (void)filter_pat;
    return {};
#endif
}

} // namespace AutoDOS2

std::filesystem::path AutoDOS2::openFolderDialog()
{
#ifdef _WIN32
    BROWSEINFOA bi = {};
    char buf[MAX_PATH] = {};
    bi.lpszTitle  = "Select game folder";
    bi.ulFlags    = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.pszDisplayName = buf;

    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (!pidl) return {};

    char path[MAX_PATH] = {};
    SHGetPathFromIDListA(pidl, path);
    CoTaskMemFree(pidl);
    return std::filesystem::path(path);
#else
    return {};
#endif
}