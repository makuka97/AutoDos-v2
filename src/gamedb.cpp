#include "gamedb.h"

#include <sqlite3.h>
#include <cstdio>
#include <cstring>

namespace AutoDOS2 {

// ── Helpers ───────────────────────────────────────────────────────────────────

static const char* colText(sqlite3_stmt* s, int col)
{
    const unsigned char* t = sqlite3_column_text(s, col);
    return t ? reinterpret_cast<const char*>(t) : "";
}

// ── Destructor / close ────────────────────────────────────────────────────────

GameDB::~GameDB() { close(); }

void GameDB::close()
{
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

// ── open ──────────────────────────────────────────────────────────────────────

bool GameDB::open(const std::filesystem::path& path)
{
    // Ensure parent directory exists
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    int rc = sqlite3_open(path.string().c_str(), &m_db);
    if (rc != SQLITE_OK) {
        std::fprintf(stderr, "[GameDB] Cannot open database: %s\n",
                     sqlite3_errmsg(m_db));
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    // WAL mode for better concurrent read performance
    execSQL("PRAGMA journal_mode=WAL;");
    execSQL("PRAGMA foreign_keys=ON;");

    return createSchema();
}

// ── createSchema ──────────────────────────────────────────────────────────────

bool GameDB::createSchema()
{
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS games (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            title       TEXT    NOT NULL DEFAULT '',
            slug        TEXT    NOT NULL DEFAULT '' UNIQUE,
            platform    TEXT    NOT NULL DEFAULT 'DOS',
            exe_path    TEXT    NOT NULL DEFAULT '',
            zip_path    TEXT    NOT NULL DEFAULT '',
            cover_path  TEXT    NOT NULL DEFAULT '',
            added_at    TEXT    NOT NULL DEFAULT (datetime('now')),
            last_played TEXT    NOT NULL DEFAULT '',
            play_count  INTEGER NOT NULL DEFAULT 0
        );
        CREATE INDEX IF NOT EXISTS idx_games_slug  ON games(slug);
        CREATE INDEX IF NOT EXISTS idx_games_title ON games(title COLLATE NOCASE);
    )";
    return execSQL(sql);
}

// ── execSQL ───────────────────────────────────────────────────────────────────

bool GameDB::execSQL(const char* sql)
{
    char* errMsg = nullptr;
    int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::fprintf(stderr, "[GameDB] SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

// ── rowToRecord ───────────────────────────────────────────────────────────────

GameRecord GameDB::rowToRecord(sqlite3_stmt* stmt)
{
    GameRecord r;
    r.id          = sqlite3_column_int (stmt, 0);
    r.title       = colText(stmt, 1);
    r.slug        = colText(stmt, 2);
    r.platform    = colText(stmt, 3);
    r.exe_path    = colText(stmt, 4);
    r.zip_path    = colText(stmt, 5);
    r.cover_path  = colText(stmt, 6);
    r.added_at    = colText(stmt, 7);
    r.last_played = colText(stmt, 8);
    r.play_count  = sqlite3_column_int (stmt, 9);
    return r;
}

// ── insert ────────────────────────────────────────────────────────────────────

bool GameDB::insert(GameRecord& rec)
{
    const char* sql = R"(
        INSERT INTO games (title, slug, platform, exe_path, zip_path, cover_path)
        VALUES (?, ?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "[GameDB] insert prepare: %s\n", sqlite3_errmsg(m_db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, rec.title.c_str(),      -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, rec.slug.c_str(),       -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, rec.platform.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, rec.exe_path.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, rec.zip_path.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, rec.cover_path.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (ok) {
        rec.id = static_cast<int>(sqlite3_last_insert_rowid(m_db));
    } else {
        std::fprintf(stderr, "[GameDB] insert step: %s\n", sqlite3_errmsg(m_db));
    }

    sqlite3_finalize(stmt);
    return ok;
}

// ── getAll ────────────────────────────────────────────────────────────────────

std::vector<GameRecord> GameDB::getAll()
{
    std::vector<GameRecord> results;
    const char* sql = R"(
        SELECT id, title, slug, platform, exe_path, zip_path,
               cover_path, added_at, last_played, play_count
        FROM games
        ORDER BY title COLLATE NOCASE ASC;
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "[GameDB] getAll prepare: %s\n", sqlite3_errmsg(m_db));
        return results;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        results.push_back(rowToRecord(stmt));
    }

    sqlite3_finalize(stmt);
    return results;
}

// ── getBySlug ─────────────────────────────────────────────────────────────────

std::optional<GameRecord> GameDB::getBySlug(const std::string& slug)
{
    const char* sql = R"(
        SELECT id, title, slug, platform, exe_path, zip_path,
               cover_path, added_at, last_played, play_count
        FROM games WHERE slug = ? LIMIT 1;
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return std::nullopt;

    sqlite3_bind_text(stmt, 1, slug.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<GameRecord> result;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = rowToRecord(stmt);

    sqlite3_finalize(stmt);
    return result;
}

// ── getById ───────────────────────────────────────────────────────────────────

std::optional<GameRecord> GameDB::getById(int id)
{
    const char* sql = R"(
        SELECT id, title, slug, platform, exe_path, zip_path,
               cover_path, added_at, last_played, play_count
        FROM games WHERE id = ? LIMIT 1;
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return std::nullopt;

    sqlite3_bind_int(stmt, 1, id);

    std::optional<GameRecord> result;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        result = rowToRecord(stmt);

    sqlite3_finalize(stmt);
    return result;
}

// ── remove ────────────────────────────────────────────────────────────────────

bool GameDB::remove(int id)
{
    const char* sql = "DELETE FROM games WHERE id = ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int(stmt, 1, id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE) &&
              (sqlite3_changes(m_db) > 0);
    sqlite3_finalize(stmt);
    return ok;
}

// ── recordPlay ────────────────────────────────────────────────────────────────

bool GameDB::recordPlay(int id)
{
    const char* sql = R"(
        UPDATE games
        SET play_count  = play_count + 1,
            last_played = datetime('now')
        WHERE id = ?;
    )";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int(stmt, 1, id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// ── count ─────────────────────────────────────────────────────────────────────

int GameDB::count()
{
    const char* sql = "SELECT COUNT(*) FROM games;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return -1;

    int n = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        n = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);
    return n;
}

} // namespace AutoDOS2
