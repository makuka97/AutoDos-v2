# AutoDOS2
> DOS game frontend for Windows — C++20 · SDL2 · Dear ImGui · SQLite · DOSBox Staging

---

## What it does

AutoDOS2 is a lightweight DOS game launcher. Drop a ZIP or point it at a folder, and it figures out the rest — identifies the game from a 7,600+ title database, extracts it, generates a DOSBox Staging config with the correct settings, fetches cover art, and launches it.

No manual config editing. No DOSBox knowledge required.

---

## Features

- Drag-and-drop ZIP ingest with automatic game identification
- Add Folder support for pre-extracted ISO/CD games (multi-disc included)
- Per-game DOSBox config generation using eXoDOS database settings
- Cover art via SteamGridDB API
- 7,619-entry games.json database with cycles, memsize, and cd_mount per game
- Settings panel with DOSBox path, cycles, and API key configuration
- In-app hotkey reference sidebar for new DOS users
- SQLite library database

---

## Prerequisites

| Tool | Minimum |
|------|---------|
| CMake | 3.28 |
| vcpkg | any recent |
| MSVC 2022 | C++20 |
| DOSBox Staging | 0.82+ |

---

## Building

### 1. Clone

```powershell
git clone https://github.com/makuka97/AutoDos-v2.git
cd AutoDos-v2
```

### 2. Configure & build

```powershell
cmake -B build -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release --parallel
```

vcpkg will automatically install all dependencies declared in `vcpkg.json`:
SDL2, SDL2_image, SDL2_ttf, nlohmann-json, sqlite3.

### 3. Setup

Place the following next to `AutoDOS2.exe`:

```
AutoDOS2.exe
games.json          # eXoDOS game database
7za.exe             # 7-Zip command line (for ZIP extraction)
7za.dll
DOS4GW.EXE          # DOS extender (bundled with many DOS games)
dosbox/
  dosbox.exe        # DOSBox Staging 0.82+
```

### 4. Run

```powershell
.\build\bin\Release\AutoDOS2.exe
```

---

## Usage

**Adding games (ZIP):**
Drag and drop a DOS game ZIP onto the window. AutoDOS2 extracts it, identifies it, writes a DOSBox config, and adds it to your library.

**Adding games (ISO/CD):**
Unzip the game yourself, then click Add Folder and select the game folder. AutoDOS2 finds the ISOs, writes the config, and adds it to your library. Multi-disc games use DOSBox Staging's built-in disc swapping — press Ctrl+F4 when the game asks to swap discs.

**Cover art:**
Enter a SteamGridDB API key in Settings (gear icon). Art is fetched automatically when games are added.

---

## Project structure

```
AutoDOS2/
├── CMakeLists.txt
├── vcpkg.json
├── games.json                  # eXoDOS game database
├── src/
│   ├── main.cpp                # Entry point
│   ├── app.h / app.cpp         # SDL2 window + ImGui loop + UI
│   ├── gamedb.h / gamedb.cpp   # SQLite library database
│   ├── ingest.h / ingest.cpp   # ZIP/folder ingest pipeline
│   ├── artfetcher.h / .cpp     # SteamGridDB cover art
│   ├── settings.h / .cpp       # App config persistence
│   └── platform.h / .cpp       # File dialogs, paths
└── src/imgui/                  # Dear ImGui (vendored)
```

---

## Hotkeys (in-game)

| Key | Action |
|-----|--------|
| Alt+Enter | Toggle fullscreen |
| Ctrl+F4 | Swap disc |
| Ctrl+F7 | Screenshot |
| Ctrl+F9 | Quit game |
| Ctrl+F10 | Release mouse |
| Ctrl+F11 | Speed down |
| Ctrl+F12 | Speed up |

---

## Roadmap

| # | Phase | Status |
|---|-------|--------|
| 01 | Project Scaffold & Window | Done |
| 02 | SQLite Game Database | Done |
| 03 | Game Grid UI | Done |
| 04 | ZIP Ingest Pipeline | Done |
| 05 | DOSBox Staging Launch | Done |
| 06 | ISO & Multi-CD Support | Done |
| 07 | Hotkey Reference & Save Hints | Done |
| 08 | Controller Support | Deferred |
| 09 | Settings, Cover Art & Polish | Done |
| 10 | ExoDOS Batch Import | Planned |