cmake_minimum_required(VERSION 3.24)
project(hexdectc C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static CRT -> no vcredist dependency for the shipped exe.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.16
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(SDL3)

# FreeType (static, no optional codecs) -- RmlUi's font engine. The
# OVERRIDE_FIND_PACKAGE makes RmlUi's find_package(Freetype) resolve to it.
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
FetchContent_Declare(Freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG VER-2-13-3
    GIT_SHALLOW TRUE
    OVERRIDE_FIND_PACKAGE)
FetchContent_MakeAvailable(Freetype)
if(NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype)
endif()

# RmlUi 6.3 (HTML/CSS-like UI), static. The SDL3 platform + GL3 renderer
# backends are compiled into the exe directly.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
set(RMLUI_FONT_ENGINE "freetype" CACHE STRING "" FORCE)
FetchContent_Declare(rmlui
    GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
    GIT_TAG 6.3
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(rmlui)
add_library(rmlui_backend STATIC
    ${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_SDL.cpp
    ${rmlui_SOURCE_DIR}/Backends/RmlUi_Renderer_GL3.cpp)
target_include_directories(rmlui_backend PUBLIC ${rmlui_SOURCE_DIR}/Backends)
target_compile_definitions(rmlui_backend PUBLIC RMLUI_SDL_VERSION_MAJOR=3)
target_link_libraries(rmlui_backend PUBLIC RmlUi::RmlUi SDL3::SDL3-static)

FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(json)

# Prebuilt Skia (aseprite fork, m124, clang + static CRT -MT).
FetchContent_Declare(skia_bin
    URL https://github.com/aseprite/skia/releases/download/m124-08a5439a6b/Skia-Windows-Release-x64.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(skia_bin)
file(GLOB SKIA_LIBS "${skia_bin_SOURCE_DIR}/out/Release-x64/*.lib")
add_library(skia_prebuilt INTERFACE)
# skia headers self-include via "include/..." — the zip root is the include root
target_include_directories(skia_prebuilt SYSTEM INTERFACE ${skia_bin_SOURCE_DIR})
target_link_libraries(skia_prebuilt INTERFACE ${SKIA_LIBS})

add_executable(hexdectc WIN32
    src/main.cpp
    src/app.cpp
    src/core.cpp
    src/fx.cpp
    src/stl_shims.cpp
    src/hexdectc.rc)

target_link_libraries(hexdectc PRIVATE
    rmlui_backend
    SDL3::SDL3-static
    nlohmann_json::nlohmann_json
    skia_prebuilt
    shell32 dwmapi ole32 opengl32)

target_compile_definitions(hexdectc PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
# ui/*.rml|rcss + opcodes.json are embedded as RCDATA -> rebuild the .rc
# when they change
set_property(SOURCE src/hexdectc.rc APPEND PROPERTY OBJECT_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ui.rml
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/ui.rcss
    ${CMAKE_CURRENT_SOURCE_DIR}/../opcodes.json)

if(MSVC)
    target_compile_options(hexdectc PRIVATE /W3 /utf-8 /MP)
    target_link_options(hexdectc PRIVATE /SUBSYSTEM:WINDOWS)
endif()
