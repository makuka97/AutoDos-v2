# AutoDOS2

> Cross-platform DOS game frontend — C++20 · SDL2 · Dear ImGui · SQLite · DOSBox Staging

[![CI](https://github.com/YOUR_USERNAME/AutoDOS2/actions/workflows/ci.yml/badge.svg)](https://github.com/YOUR_USERNAME/AutoDOS2/actions)

---

## Phase 01 — Project Scaffold & Window ✅

Blank 900×700 dark window, 60 fps, ImGui wired up, CI on Windows/Linux/macOS.

---

## Prerequisites

| Tool | Minimum |
|------|---------|
| CMake | 3.28 |
| vcpkg | any recent |
| MSVC 2022 / Clang 16 / GCC 13 | C++20 |

---

## Building

### 1. Clone + bootstrap vcpkg

```bash
git clone https://github.com/YOUR_USERNAME/AutoDOS2.git
cd AutoDOS2

# If you don't have vcpkg globally:
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh   # or bootstrap-vcpkg.bat on Windows
export VCPKG_ROOT=$PWD/vcpkg  # Windows: set VCPKG_ROOT=%CD%\vcpkg
```

### 2. Configure & build

```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

cmake --build build --config Release --parallel
```

vcpkg will automatically install all dependencies declared in `vcpkg.json`
(SDL2, SDL2_image, SDL2_ttf, Dear ImGui, nlohmann-json).

### 3. Run

```bash
./build/bin/AutoDOS2        # Linux / macOS
build\bin\Release\AutoDOS2.exe  # Windows
```

**F1** toggles the ImGui demo window. Close button exits cleanly.

---

## Project structure

```
AutoDOS2/
├── CMakeLists.txt          # Root build script
├── vcpkg.json              # Dependency manifest
├── app_config.json         # Config skeleton (written to data_dir at runtime)
├── src/
│   ├── main.cpp            # Entry point (WinMain / main)
│   ├── app.h / app.cpp     # SDL2 window + ImGui loop
│   └── platform.h / .cpp   # Cross-platform paths & file dialog
└── .github/
    └── workflows/
        └── ci.yml          # Windows / Linux / macOS build matrix
```

---

## Roadmap

| # | Phase | Status |
|---|-------|--------|
| 01 | Project Scaffold & Window | ✅ Done |
| 02 | SQLite Game Database | ⬜ Next |
| 03 | Game Grid UI | ⬜ |
| 04 | ZIP Ingest Pipeline | ⬜ |
| 05 | DOSBox Staging Launch | ⬜ |
| 06 | ISO & Multi-CD Support | ⬜ |
| 07 | Save States & Snapshots | ⬜ |
| 08 | Bluetooth Controller Support | ⬜ |
| 09 | Settings, Polish & First Release | ⬜ |
| 10 | ExoDOS Batch Import | ⬜ |
