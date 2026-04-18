cmake_minimum_required(VERSION 3.28)
project(AutoDOS2 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain")
endif()

find_package(SDL2               CONFIG REQUIRED)
find_package(SDL2_image         CONFIG REQUIRED)
find_package(SDL2_ttf           CONFIG REQUIRED)
find_package(nlohmann_json      CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

if(WIN32)
    set(AUTODOS2_WIN32 ON)
elseif(APPLE)
    set(AUTODOS2_MACOS ON)
else()
    set(AUTODOS2_LINUX ON)
endif()

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/src/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/imgui_impl_sdlrenderer2.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR})
target_link_libraries(imgui PUBLIC
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)

set(AUTODOS2_DEFS
    AUTODOS2_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    AUTODOS2_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    AUTODOS2_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    $<$<BOOL:${AUTODOS2_WIN32}>:AUTODOS2_WINDOWS>
    $<$<BOOL:${AUTODOS2_MACOS}>:AUTODOS2_MACOS>
    $<$<BOOL:${AUTODOS2_LINUX}>:AUTODOS2_LINUX>
    $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX WIN32_LEAN_AND_MEAN>
)
set(AUTODOS2_WARN
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /utf-8>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)
set(AUTODOS2_COMMON_LIBS
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    unofficial::sqlite3::sqlite3
    nlohmann_json::nlohmann_json
)

if(WIN32)
    add_executable(AutoDOS2 WIN32
        src/main.cpp src/app.cpp src/app.h
        src/gamedb.cpp src/gamedb.h
    src/ingest.cpp src/ingest.h
    src/settings.cpp src/settings.h
    src/artfetcher.cpp src/artfetcher.h
        src/platform.cpp src/platform.h
    )
else()
    add_executable(AutoDOS2
        src/main.cpp src/app.cpp src/app.h
        src/gamedb.cpp src/gamedb.h
    src/ingest.cpp src/ingest.h
    src/settings.cpp src/settings.h
    src/artfetcher.cpp src/artfetcher.h
        src/platform.cpp src/platform.h
    )
endif()
target_include_directories(AutoDOS2 PRIVATE ${CMAKE_SOURCE_DIR}/src ${IMGUI_DIR})
target_compile_definitions(AutoDOS2 PRIVATE ${AUTODOS2_DEFS})
target_compile_options(AutoDOS2 PRIVATE ${AUTODOS2_WARN})
target_link_libraries(AutoDOS2 PRIVATE
    imgui
    ${AUTODOS2_COMMON_LIBS}
    $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
    $<IF:$<TARGET_EXISTS:SDL2_ttf::SDL2_ttf>,SDL2_ttf::SDL2_ttf,SDL2_ttf::SDL2_ttf-static>
)

add_executable(test_db
    src/test_db.cpp
    src/gamedb.cpp src/gamedb.h
    src/ingest.cpp src/ingest.h
    src/settings.cpp src/settings.h
    src/artfetcher.cpp src/artfetcher.h
    src/platform.cpp src/platform.h
)
target_include_directories(test_db PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(test_db PRIVATE ${AUTODOS2_DEFS})
target_compile_options(test_db PRIVATE ${AUTODOS2_WARN})
target_link_libraries(test_db PRIVATE ${AUTODOS2_COMMON_LIBS})

if(APPLE)
    foreach(TGT AutoDOS2 test_db)
        target_link_libraries(${TGT} PRIVATE
            "-framework Cocoa" "-framework IOKit"
            "-framework CoreVideo" "-framework CoreFoundation"
        )
    endforeach()
endif()

install(TARGETS AutoDOS2 DESTINATION bin)