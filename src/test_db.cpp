#include "gamedb.h"
#include "platform.h"

#include <SDL.h>
#include <cassert>
#include <cstdio>

// ── Tiny assertion helper ─────────────────────────────────────────────────────
static int g_passed = 0;
static int g_failed = 0;

#define CHECK(expr) \
    do { \
        if (expr) { \
            std::printf("  [PASS] %s\n", #expr); \
            ++g_passed; \
        } else { \
            std::printf("  [FAIL] %s  (line %d)\n", #expr, __LINE__); \
            ++g_failed; \
        } \
    } while(0)

int main(int /*argc*/, char* /*argv*/[])
{
    // SDL must be initialised so SDL_GetPrefPath works
    SDL_Init(SDL_INIT_VIDEO);

    auto dataDir = AutoDOS2::getDataDir();
    auto dbPath  = dataDir / "autodos2.db";

    std::printf("AutoDOS2 — Phase 02 DB Test\n");
    std::printf("DB path: %s\n\n", dbPath.string().c_str());

    AutoDOS2::GameDB db;
    CHECK(db.open(dbPath));
    CHECK(db.isOpen());

    // ── Insert 3 dummy games ──────────────────────────────────────────────────
    std::printf("\n-- Insert 3 dummy games --\n");

    AutoDOS2::GameRecord g1;
    g1.title    = "Duke Nukem 3D";
    g1.slug     = "duke3d";
    g1.platform = "DOS";
    g1.exe_path = "DUKE3D.EXE";
    g1.zip_path = "duke3d.zip";
    CHECK(db.insert(g1));
    CHECK(g1.id > 0);

    AutoDOS2::GameRecord g2;
    g2.title    = "Quake";
    g2.slug     = "quake";
    g2.platform = "DOS";
    g2.exe_path = "QUAKE.EXE";
    g2.zip_path = "quake.zip";
    CHECK(db.insert(g2));

    AutoDOS2::GameRecord g3;
    g3.title    = "Daggerfall";
    g3.slug     = "daggerfall";
    g3.platform = "DOS";
    g3.exe_path = "FALL.EXE";
    g3.zip_path = "daggerfall.zip";
    CHECK(db.insert(g3));

    // ── Query all — should return 3 ───────────────────────────────────────────
    std::printf("\n-- Query all --\n");
    auto all = db.getAll();
    CHECK(all.size() == 3);
    for (auto& r : all)
        std::printf("  id=%-3d  slug=%-12s  title=%s\n",
                    r.id, r.slug.c_str(), r.title.c_str());

    // ── Query by slug ─────────────────────────────────────────────────────────
    std::printf("\n-- Query by slug --\n");
    auto found = db.getBySlug("quake");
    CHECK(found.has_value());
    CHECK(found->title == "Quake");

    auto notFound = db.getBySlug("nonexistent");
    CHECK(!notFound.has_value());

    // ── recordPlay ────────────────────────────────────────────────────────────
    std::printf("\n-- recordPlay --\n");
    CHECK(db.recordPlay(g1.id));
    auto updated = db.getById(g1.id);
    CHECK(updated.has_value());
    CHECK(updated->play_count == 1);
    CHECK(!updated->last_played.empty());

    // ── Delete one — count drops to 2 ─────────────────────────────────────────
    std::printf("\n-- Delete one --\n");
    CHECK(db.remove(g2.id));
    CHECK(db.count() == 2);

    // ── DB persists (reopen) ──────────────────────────────────────────────────
    std::printf("\n-- Reopen DB --\n");
    db.close();
    CHECK(!db.isOpen());
    CHECK(db.open(dbPath));
    CHECK(db.count() == 2);

    // ── Cleanup: remove remaining rows so test is repeatable ──────────────────
    for (auto& r : db.getAll()) db.remove(r.id);
    CHECK(db.count() == 0);

    // ── Summary ───────────────────────────────────────────────────────────────
    std::printf("\n────────────────────────────────\n");
    std::printf("Results: %d passed, %d failed\n", g_passed, g_failed);

    db.close();
    SDL_Quit();

    return (g_failed == 0) ? 0 : 1;
}
