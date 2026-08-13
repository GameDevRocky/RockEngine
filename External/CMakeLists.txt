cmake_minimum_required(VERSION 3.16)

# --- GLM (header-only) ---
add_subdirectory(glm)

# --- YAML-CPP ---
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_UTILS OFF CACHE BOOL "" FORCE)
add_subdirectory(yaml)

# --- Box2D ---
set(BOX2D_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)

# Add Box2D
add_subdirectory(box2d)

# Embedded interpreter for pybind11. Require a minimum so developers don't diverge
# across wildly different Python versions (the shipped app bundles its own runtime).
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter Development)

set(PYBIND11_FINDPYTHON ON)
add_subdirectory(pybind11)

# --- FreeType ---
# Only here because msdfgen-ext needs it to read glyph outlines and metrics out of
# a font file (msdfgen-core can generate a distance field but cannot parse a font).
# Every optional codec/shaper is off: we hand FreeType a .ttf/.otf and take outlines
# back out, nothing more.
set(FT_DISABLE_ZLIB     ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2    ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG      ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI   ON CACHE BOOL "" FORCE)
add_subdirectory(freetype)

# FreeType's own CMakeLists only attaches the name `Freetype::Freetype` as an
# EXPORT_NAME on an interface target, which exists for installed packages and NOT
# as a target in this build. msdfgen guards its dependency with
# `if(NOT TARGET Freetype::Freetype)` and would otherwise fall through to a
# find_package(Freetype REQUIRED) that fails on a machine with no system FreeType.
# Creating the alias here is what makes that guard see the submodule.
add_library(Freetype::Freetype ALIAS freetype)

# --- MSDF font atlas generation ---
# Extensions ON: font loading lives in msdfgen-ext. Everything else off -- Skia is
# only for geometry preprocessing we don't need, and the SVG/PNG codecs would drag
# in tinyxml2 and libpng for features we never call.
set(MSDFGEN_CORE_ONLY        OFF CACHE BOOL "" FORCE)
set(MSDFGEN_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
set(MSDFGEN_USE_VCPKG        OFF CACHE BOOL "" FORCE)
set(MSDFGEN_USE_SKIA         OFF CACHE BOOL "" FORCE)
set(MSDFGEN_DISABLE_SVG      ON  CACHE BOOL "" FORCE)
set(MSDFGEN_DISABLE_PNG      ON  CACHE BOOL "" FORCE)
set(MSDFGEN_INSTALL          OFF CACHE BOOL "" FORCE)
# MUST stay ON. msdfgen defaults this OFF, which forces its targets onto the STATIC
# MSVC runtime (/MT) while the rest of RockEngine and all of Qt use the dynamic one
# (/MD). Mixing the two links with LNK4098 warnings and then corrupts the heap at
# runtime, because memory allocated by one CRT is freed by the other.
set(MSDFGEN_DYNAMIC_RUNTIME  ON  CACHE BOOL "" FORCE)
add_subdirectory(msdfgen)

# --- SDL3 (gamepad subsystem only) ---
# Here for ONE reason: reading game controllers. Qt6 dropped QtGamepad and Engine must not
# depend on Qt anyway, so there was no cross-platform way to talk to a pad.
#
# SDL_VIDEO is OFF, which is the whole point -- the video code is never compiled, so this
# build of SDL is incapable of creating a window even by accident. Qt keeps owning every
# window in the editor; Engine only ever calls SDL_INIT_GAMEPAD. Turning video off also
# forces GPU/Render/Camera off automatically (they are cmake_dependent_options on SDL_VIDEO).
# Audio is off too -- miniaudio owns audio and there is no reason to have two device layers
# fighting over one sound card.
#
# What stays ON is deliberate: HIDAPI is what gives DualSense/DualShock 4 their gyro,
# accelerometer, touchpad fingers and lightbar (SDL's own controller drivers parse the raw
# reports); SENSOR carries the motion data; POWER carries battery level; HAPTIC backs rumble.
# Drop any of those and the PlayStation feature set silently degrades to a generic pad.
set(SDL_SHARED        OFF CACHE BOOL "" FORCE)  # static-link; nothing extra to ship
set(SDL_STATIC        ON  CACHE BOOL "" FORCE)
set(SDL_VIDEO         OFF CACHE BOOL "" FORCE)
set(SDL_AUDIO         OFF CACHE BOOL "" FORCE)
set(SDL_DIALOG        OFF CACHE BOOL "" FORCE)
set(SDL_TRAY          OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK      ON  CACHE BOOL "" FORCE)
set(SDL_HAPTIC        ON  CACHE BOOL "" FORCE)
set(SDL_HIDAPI        ON  CACHE BOOL "" FORCE)
set(SDL_SENSOR        ON  CACHE BOOL "" FORCE)
set(SDL_POWER         ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY  OFF CACHE BOOL "" FORCE)
set(SDL_TESTS         OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES      OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL       OFF CACHE BOOL "" FORCE)
# Same /MD-vs-/MT trap documented on MSDFGEN_DYNAMIC_RUNTIME above: SDL defaults this OFF
# (dynamic CRT), which is what we want. Pinned so a future default flip can't corrupt the
# heap by linking a /MT SDL against a /MD engine + Qt.
set(SDL_FORCE_STATIC_VCRT OFF CACHE BOOL "" FORCE)
add_subdirectory(SDL)

# --- GLAD ---
add_library(glad STATIC glad/src/glad.c)
target_include_directories(glad 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/glad/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/glad/src
)

# --- ImGui ---
add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
)
target_link_libraries(imgui PUBLIC glad)

# --- ImGuizmo ---
add_library(imguizmo STATIC
    imguizmo/ImGuizmo.cpp
)
target_include_directories(imguizmo 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/imguizmo
)
target_link_libraries(imguizmo PUBLIC imgui)

add_library(ExternalLibs INTERFACE)

target_link_libraries(ExternalLibs
    INTERFACE
        glm
        yaml-cpp
        box2d
        glad
        imgui
        imguizmo
        SDL3::SDL3-static
        Python3::Python
        pybind11::embed
)

target_include_directories(ExternalLibs
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/glad/include
        ${CMAKE_CURRENT_SOURCE_DIR}/stb
        ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio
        ${Python3_INCLUDE_DIRS}
)

# miniaudio (header-only, vendored like stb/glad -- see Engine/src/audio/AudioEngine.cpp for
# the single translation unit that defines MINIAUDIO_IMPLEMENTATION). Per its own docs, Windows
# and macOS need no extra linking (device backends are loaded at runtime), but Linux/BSD require
# these three system libs.
if(UNIX AND NOT APPLE)
    target_link_libraries(ExternalLibs INTERFACE dl pthread m)
endif()
