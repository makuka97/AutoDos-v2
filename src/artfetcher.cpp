// artfetcher.cpp — SteamGridDB cover art fetcher (WinINet, no extra deps)
#include "artfetcher.h"

#include <nlohmann/json.hpp>
#include <cstdio>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#  include <wininet.h>
#  pragma comment(lib, "wininet.lib")
#endif

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace AutoDOS2 {

// ── httpGet ───────────────────────────────────────────────────────────────────
// Simple HTTPS GET with Bearer token auth via WinINet.

std::string ArtFetcher::httpGet(const std::string& url)
{
#ifdef _WIN32
    // Parse URL into host + path
    std::string host, path;
    bool https = false;
    {
        std::string u = url;
        if (u.substr(0,8) == "https://") { https = true; u = u.substr(8); }
        else if (u.substr(0,7) == "http://") { u = u.substr(7); }
        auto slash = u.find('/');
        if (slash == std::string::npos) { host = u; path = "/"; }
        else { host = u.substr(0, slash); path = u.substr(slash); }
    }

    HINTERNET hInet = InternetOpenA("AutoDOS2/0.9",
        INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInet) return "";

    HINTERNET hConn = InternetConnectA(hInet, host.c_str(),
        https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT,
        nullptr, nullptr,
        INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConn) { InternetCloseHandle(hInet); return ""; }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (https) flags |= INTERNET_FLAG_SECURE;

    HINTERNET hReq = HttpOpenRequestA(hConn, "GET", path.c_str(),
        nullptr, nullptr, nullptr, flags, 0);
    if (!hReq) {
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return "";
    }

    // Set Authorization header
    std::string authHeader = "Authorization: Bearer " + m_apiKey;
    HttpAddRequestHeadersA(hReq, authHeader.c_str(), (DWORD)authHeader.size(),
        HTTP_ADDREQ_FLAG_ADD | HTTP_ADDREQ_FLAG_REPLACE);

    if (!HttpSendRequestA(hReq, nullptr, 0, nullptr, 0)) {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return "";
    }

    std::string result;
    char buf[4096];
    DWORD read = 0;
    while (InternetReadFile(hReq, buf, sizeof(buf)-1, &read) && read > 0) {
        buf[read] = '\0';
        result += buf;
    }

    InternetCloseHandle(hReq);
    InternetCloseHandle(hConn);
    InternetCloseHandle(hInet);
    return result;
#else
    (void)url;
    return "";
#endif
}

// ── httpDownload ──────────────────────────────────────────────────────────────

bool ArtFetcher::httpDownload(const std::string& url, const fs::path& outPath)
{
#ifdef _WIN32
    std::string host, path;
    bool https = false;
    {
        std::string u = url;
        if (u.substr(0,8) == "https://") { https = true; u = u.substr(8); }
        else if (u.substr(0,7) == "http://") { u = u.substr(7); }
        auto slash = u.find('/');
        if (slash == std::string::npos) { host = u; path = "/"; }
        else { host = u.substr(0, slash); path = u.substr(slash); }
    }

    HINTERNET hInet = InternetOpenA("AutoDOS2/0.9",
        INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInet) return false;

    HINTERNET hConn = InternetConnectA(hInet, host.c_str(),
        https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT,
        nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConn) { InternetCloseHandle(hInet); return false; }

    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
    if (https) flags |= INTERNET_FLAG_SECURE;

    HINTERNET hReq = HttpOpenRequestA(hConn, "GET", path.c_str(),
        nullptr, nullptr, nullptr, flags, 0);
    if (!hReq) {
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return false;
    }

    if (!HttpSendRequestA(hReq, nullptr, 0, nullptr, 0)) {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return false;
    }

    // Create parent directories
    std::error_code ec;
    fs::create_directories(outPath.parent_path(), ec);

    std::ofstream f(outPath, std::ios::binary);
    if (!f.is_open()) {
        InternetCloseHandle(hReq);
        InternetCloseHandle(hConn);
        InternetCloseHandle(hInet);
        return false;
    }

    char buf[8192];
    DWORD read = 0;
    bool ok = false;
    while (InternetReadFile(hReq, buf, sizeof(buf), &read) && read > 0) {
        f.write(buf, read);
        ok = true;
    }

    f.close();
    InternetCloseHandle(hReq);
    InternetCloseHandle(hConn);
    InternetCloseHandle(hInet);

    if (!ok) fs::remove(outPath, ec);
    return ok;
#else
    (void)url; (void)outPath;
    return false;
#endif
}

// ── fetch ─────────────────────────────────────────────────────────────────────

bool ArtFetcher::fetch(const std::string& gameTitle, const fs::path& outPath)
{
    if (m_apiKey.empty()) return false;

    // Skip if art already exists
    if (fs::exists(outPath)) return true;

    // Step 1: Search for game by title
    // URL encode the title (simple: replace spaces with %20)
    std::string encoded;
    for (char c : gameTitle) {
        if (c == ' ') encoded += "%20";
        else if (std::isalnum((unsigned char)c) || c=='-' || c=='_' || c=='.')
            encoded += c;
        else { char buf[8]; snprintf(buf,sizeof(buf),"%%%02X",(unsigned char)c); encoded+=buf; }
    }

    std::string searchUrl = "https://www.steamgriddb.com/api/v2/search/autocomplete/" + encoded;
    std::string searchResp = httpGet(searchUrl);
    if (searchResp.empty()) return false;

    int gameId = 0;
    try {
        json j = json::parse(searchResp);
        if (!j.value("success", false)) return false;
        auto& data = j["data"];
        if (!data.is_array() || data.empty()) return false;
        gameId = data[0].value("id", 0);
    } catch (...) { return false; }

    if (gameId == 0) return false;

    // Step 2: Get grids for game — request portrait style (600x900) for cards
    std::string gridsUrl = "https://www.steamgriddb.com/api/v2/grids/game/"
        + std::to_string(gameId)
        + "?dimensions=600x900,342x482,660x930";
    std::string gridsResp = httpGet(gridsUrl);
    if (gridsResp.empty()) return false;

    std::string imageUrl;
    try {
        json j = json::parse(gridsResp);
        if (!j.value("success", false)) return false;
        auto& data = j["data"];
        if (!data.is_array() || data.empty()) return false;
        // Pick first result
        imageUrl = data[0].value("url", "");
    } catch (...) { return false; }

    if (imageUrl.empty()) return false;

    // Step 3: Download image
    return httpDownload(imageUrl, outPath);
}

} // namespace AutoDOS2
