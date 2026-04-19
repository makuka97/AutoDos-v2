#include "platform.h"
#include <shobjidl.h>
#include <shlobj.h>

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
    // Use IFileOpenDialog — modern Windows Explorer style folder picker
    // Must initialize COM first
    HRESULT comInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    std::filesystem::path result;
    IFileOpenDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));

    if (SUCCEEDED(hr)) {
        DWORD opts = 0;
        pfd->GetOptions(&opts);
        pfd->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        pfd->SetTitle(L"Select game folder");
        hr = pfd->Show(nullptr);
        if (SUCCEEDED(hr)) {
            IShellItem* psi = nullptr;
            if (SUCCEEDED(pfd->GetResult(&psi))) {
                PWSTR pszPath = nullptr;
                if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                    result = std::filesystem::path(pszPath);
                    CoTaskMemFree(pszPath);
                }
                psi->Release();
            }
        }
        pfd->Release();
    }

    if (SUCCEEDED(comInit) || comInit == S_FALSE)
        CoUninitialize();

    return result;
#else
    return {};
#endif
}