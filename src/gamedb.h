#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Forward-declare sqlite3 types OUTSIDE any namespace
struct sqlite3;
struct sqlite3_stmt;

namespace AutoDOS2 {

// ─────────────────────────────────────────────────────────────────────────────
// GameRecord — mirrors the `games` table schema
// ─────────────────────────────────────────────────────────────────────────────
struct GameRecord {
    int         id          = 0;
    std::string title;
    std::string slug;
    std::string platform;
    std::string exe_path;
    std::string zip_path;
    std::string cover_path;
    std::string added_at;       // ISO-8601 text, set by DB default
    std::string last_played;    // ISO-8601 text, empty until first play
    int         play_count  = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// GameDB — thin RAII wrapper around SQLite
// ─────────────────────────────────────────────────────────────────────────────
class GameDB {
public:
    GameDB() = default;
    ~GameDB();

    // Non-copyable
    GameDB(const GameDB&)            = delete;
    GameDB& operator=(const GameDB&) = delete;

    // Opens (and creates if needed) the database at `path`.
    // Returns false and logs to stderr on failure.
    bool open(const std::filesystem::path& path);

    void close();
    bool isOpen() const { return m_db != nullptr; }

    // ── CRUD ──────────────────────────────────────────────────────────────────

    // Insert a new game. Populates rec.id on success. Returns false on error.
    bool insert(GameRecord& rec);

    // Returns all games ordered by title.
    std::vector<GameRecord> getAll();

    // Returns the game with the given slug, or nullopt if not found.
    std::optional<GameRecord> getBySlug(const std::string& slug);

    // Returns the game with the given id, or nullopt if not found.
    std::optional<GameRecord> getById(int id);

    // Delete game by id. Returns false if not found or error.
    bool remove(int id);

    // Increment play_count and update last_played for the given id.
    bool recordPlay(int id);

    // ── Save state tracking ──────────────────────────────────────────────────
    // Record that a save state exists for a game (called when user presses Ctrl+F5)
    bool recordSave(int gameId);

    // Returns true if any save state has been recorded for this game
    bool hasSave(int gameId);

    // Total number of games in the DB.
    int count();

private:
    sqlite3* m_db = nullptr;

    bool execSQL(const char* sql);
    bool createSchema();

    // Reads a full GameRecord from the current row of a prepared statement.
    static GameRecord rowToRecord(struct sqlite3_stmt* stmt);
};

} // namespace AutoDOS2