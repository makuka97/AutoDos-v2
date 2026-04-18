#pragma once
// artfetcher.h — SteamGridDB cover art fetcher
// Uses WinINet on Windows for zero extra dependencies

#include <filesystem>
#include <functional>
#include <string>

namespace AutoDOS2 {

// Callback: called on completion with success flag
using ArtCallback = std::function<void(bool success)>;

class ArtFetcher {
public:
    void setApiKey(const std::string& key) { m_apiKey = key; }
    bool hasApiKey() const { return !m_apiKey.empty(); }

    // Fetch cover art for a game title, save to outPath.
    // Runs synchronously — call from a background thread.
    // Returns true if art was downloaded successfully.
    bool fetch(const std::string& gameTitle,
               const std::filesystem::path& outPath);

private:
    std::string m_apiKey;

    // HTTP GET with Bearer auth, returns response body
    std::string httpGet(const std::string& url);

    // Download binary file to path
    bool httpDownload(const std::string& url,
                      const std::filesystem::path& outPath);
};

} // namespace AutoDOS2
