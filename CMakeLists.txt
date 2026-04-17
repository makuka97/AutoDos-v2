cmake_minimum_required(VERSION 3.28)
project(AutoDOS2 VERSION 0.1.0 LANGUAGES CXX)

# ── C++20 ─────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Output dirs ───────────────────────────────────────────────────────────────
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ── vcpkg toolchain (auto-detected if VCPKG_ROOT is set) ─────────────────────
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain")
endif()

# ── Dependencies (all via vcpkg) ──────────────────────────────────────────────
find_package(SDL2         CONFIG REQUIRED)
find_package(SDL2_image   CONFIG REQUIRED)
find_package(SDL2_ttf     CONFIG REQUIRED)
find_package(imgui        CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# ── Platform detection ────────────────────────────────────────────────────────
if(WIN32)
    set(AUTODOS2_PLATFORM "Windows")
    set(AUTODOS2_WIN32 ON)
elseif(APPLE)
    set(AUTODOS2_PLATFORM "macOS")
    set(AUTODOS2_MACOS ON)
else()
    set(AUTODOS2_PLATFORM "Linux")
    set(AUTODOS2_LINUX ON)
endif()
message(STATUS "AutoDOS2 target platform: ${AUTODOS2_PLATFORM}")

# ── Sources ───────────────────────────────────────────────────────────────────
set(AUTODOS2_SOURCES
    src/main.cpp
    src/app.cpp
    src/app.h
    src/platform.cpp
    src/platform.h
)

# ── Executable ────────────────────────────────────────────────────────────────
if(WIN32)
    # Windows GUI app — no console window
    add_executable(AutoDOS2 WIN32 ${AUTODOS2_SOURCES})
else()
    add_executable(AutoDOS2 ${AUTODOS2_SOURCES})
endif()

# ── Compile definitions ───────────────────────────────────────────────────────
target_compile_definitions(AutoDOS2 PRIVATE
    AUTODOS2_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    AUTODOS2_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    AUTODOS2_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    $<$<BOOL:${AUTODOS2_WIN32}>:AUTODOS2_WINDOWS>
    $<$<BOOL:${AUTODOS2_MACOS}>:AUTODOS2_MACOS>
    $<$<BOOL:${AUTODOS2_LINUX}>:AUTODOS2_LINUX>
    # Suppress MSVC min/max macro pollution
    $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX WIN32_LEAN_AND_MEAN>
)

# ── Compiler warnings ─────────────────────────────────────────────────────────
target_compile_options(AutoDOS2 PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX /utf-8>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Werror>
)

# ── Link ──────────────────────────────────────────────────────────────────────
target_link_libraries(AutoDOS2 PRIVATE
    # SDL2
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    # SDL2_image
    $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
    # SDL2_ttf
    $<IF:$<TARGET_EXISTS:SDL2_ttf::SDL2_ttf>,SDL2_ttf::SDL2_ttf,SDL2_ttf::SDL2_ttf-static>
    # ImGui (vcpkg imgui port exposes SDL2+SDL_Renderer backend as a separate component)
    imgui::imgui
    # JSON
    nlohmann_json::nlohmann_json
)

# macOS: link Cocoa/IOKit for SDL2
if(APPLE)
    target_link_libraries(AutoDOS2 PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework CoreFoundation"
    )
endif()

# ── Install (used by CI artifact packaging) ───────────────────────────────────
install(TARGETS AutoDOS2 DESTINATION bin)
