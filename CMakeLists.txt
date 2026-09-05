cmake_minimum_required(VERSION 3.20)
project(RockEngine VERSION 1.0 LANGUAGES C CXX)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_definitions(PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

if(MSVC)
    add_compile_options(/Zc:__cplusplus)
    add_compile_options(/EHsc)
     
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL")
    
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /RTC1")
else()
    add_compile_options(-Wall -Wextra)
endif()
 
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- What to build ---------------------------------------------------------------------
# All three default to the full local-dev build, so the normal `--preset local` workflow is
# unchanged. They exist for CI, which wants a fast headless lane: Engine + tests only, with
# no Qt to find and no CPython standard library to copy. See the `ci-*` presets.
option(ROCKENGINE_BUILD_EDITOR         "Build the Qt6 editor (RockEngineEditor + RockEngineLauncher)" ON)
option(ROCKENGINE_BUILD_TESTS          "Build the headless unit tests (RockEngineTests)"              OFF)
option(ROCKENGINE_STAGE_PLAYER_RUNTIME "Stage the shippable Python runtime beside the Player"         ON)

# --- Which MSVC runtime the build links ---------------------------------------------------
# OFF by default, and it has to be: the editor links Qt, whose official binaries use the
# DYNAMIC CRT (/MD). Two CRTs in one process means memory allocated by one is freed by the
# other, which is heap corruption -- see the MSDFGEN_DYNAMIC_RUNTIME note in
# External/CMakeLists.txt for the same trap arriving from the other direction.
#
# ON is for the export-template tree ONLY: the Engine + Player build (no editor, no Qt)
# whose RockEnginePlayer.exe GameBuilder copies into every shipped game. It exists because
# of what the shipped executable imports:
#
#   Debug   /MDd  -> MSVCP140D.dll, VCRUNTIME140D.dll, ucrtbased.dll
#                    Ships ONLY with Visual Studio and may not be redistributed at all.
#                    This is why a zipped game used to run on developer machines and
#                    nowhere else.
#   Release /MD   -> MSVCP140.dll, VCRUNTIME140.dll  (redistributable, but must be copied
#                    next to the exe or installed by the user)
#   Release /MT   -> nothing. The CRT is linked into the executable.
#
# The third is what a game should ship, so the release preset picks it. It must be set
# globally rather than as a property on RockEnginePlayer: every static library the player
# links (RockEngineCore, box2d, yaml-cpp, SDL, freetype, ...) has to use the same CRT or
# the link fails with LNK2038.
#
# The one boundary this does NOT remove is python3XX.dll, which is a stock CPython build
# using the dynamic CRT. That crossing already exists today and is safe for the same reason
# it is safe today: everything passed across it is a PyObject*, allocated and freed by
# CPython's own allocator. Do not start handing FILE* or malloc'd buffers to the C API.
option(ROCKENGINE_STATIC_MSVC_RUNTIME "Link the static MSVC runtime (/MT). Export-template builds only." OFF)

if(ROCKENGINE_STATIC_MSVC_RUNTIME)
    if(NOT MSVC)
        message(FATAL_ERROR "ROCKENGINE_STATIC_MSVC_RUNTIME is an MSVC-only option.")
    endif()
    if(ROCKENGINE_BUILD_EDITOR)
        message(FATAL_ERROR
            "ROCKENGINE_STATIC_MSVC_RUNTIME requires ROCKENGINE_BUILD_EDITOR=OFF. Qt's "
            "binaries use the dynamic CRT and cannot be linked into a /MT process. Use the "
            "windows-msvc-release preset, which builds Engine + Player only.")
    endif()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# AUTOMOC/AUTOUIC/AUTORCC exist for the Editor alone -- it is the only target with Q_OBJECT
# in it. Engine/ and Player/ were being moc-scanned for nothing (Player already opted out
# per-target; see the comment in Player/CMakeLists.txt), so these are scoped to the editor
# build rather than set globally.
if(ROCKENGINE_BUILD_EDITOR)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)

    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGLWidgets Network)
endif()

add_subdirectory(External)
add_subdirectory(Engine)
if(ROCKENGINE_BUILD_EDITOR)
    add_subdirectory(Editor)
endif()
# The standalone game executable. Links RockEngineCore only -- no Qt, no Editor. The
# editor's Build window copies the binary this produces into each game build, which is why
# it is built alongside the editor rather than on demand (Unity's "export template" model).
add_subdirectory(Player)

# Headless unit tests over RockEngineCore. No Qt, no GPU, no window -- see Tests/CMakeLists.txt.
if(ROCKENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

if(ROCKENGINE_BUILD_EDITOR)

add_executable(RockEngineLauncher src/main.cpp)

set_target_properties(RockEngineLauncher PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
target_link_libraries(RockEngineLauncher
    PRIVATE
        RockEngineCore
        RockEngineEditor
)     
  
qt_finalize_executable(RockEngineLauncher)

# Locate Qt's bin dir (where the deploy tools live) once, for any platform.
get_target_property(_qt_executable Qt6::Core LOCATION)
get_filename_component(_qt_bin_dir "${_qt_executable}" DIRECTORY)

if(WIN32)
    # Windows has no RPATH, so the Qt DLLs/plugins must sit next to the .exe.
    # windeployqt handles this for both MSVC and MinGW.
    find_program(WINDEPLOY_PATH windeployqt HINTS "${_qt_bin_dir}")
    if(WINDEPLOY_PATH)
        add_custom_command(TARGET RockEngineLauncher POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E env PATH="${_qt_bin_dir}"
                    "${WINDEPLOY_PATH}"
                    "$<$<CONFIG:Debug>:--debug>"
                    --no-compiler-runtime
                    --no-translations
                    "$<TARGET_FILE:RockEngineLauncher>"
            COMMENT "Deploying Qt dependencies from ${_qt_bin_dir}..."
            VERBATIM
        )
    endif()
endif()
# macOS / Linux: for local dev the app finds Qt via RPATH or the library path the
# setup/build scripts export. Self-contained app bundling (Qt + Python) is handled
# by the distribution step below, not by this local-dev build.
 
# --- Optional: bundle a standalone Python runtime + game content next to the launcher ---
# Distribution build. When ON, copies the CPython standard library and the Domain/ content
# beside the executable and pip-installs requirements.txt into the bundle. At runtime the
# engine detects "<exeDir>/python" and points the embedded interpreter there (see
# Engine::Init), so the packaged app runs on machines with no Python installed.
#
# OFF by default: it is a heavier, platform-specific copy step. Validate the produced bundle
# on a clean machine (no system Python) before shipping; you may also need to copy the
# interpreter shared library (e.g. python3xx.dll / libpython3.x.so) and platform "DLLs".
option(ROCKENGINE_BUNDLE_PYTHON "Bundle a standalone Python runtime + Domain content next to the launcher" OFF)
if(ROCKENGINE_BUNDLE_PYTHON)
    # Ensure Python result vars are available in this (root) scope.
    find_package(Python3 3.10 QUIET COMPONENTS Interpreter Development)
    set(_bundle_dir "$<TARGET_FILE_DIR:RockEngineLauncher>")

    add_custom_command(TARGET RockEngineLauncher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${Python3_STDLIB}" "${_bundle_dir}/python/Lib"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Domain" "${_bundle_dir}/Domain"
        COMMENT "Bundling Python standard library + Domain content..."
        VERBATIM
    )
    add_custom_command(TARGET RockEngineLauncher POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" -m pip install --upgrade
                --target "${_bundle_dir}/python/Lib/site-packages"
                -r "${CMAKE_SOURCE_DIR}/requirements.txt"
        COMMENT "Installing Python requirements into the bundle..."
        VERBATIM
    )
endif()

endif() # ROCKENGINE_BUILD_EDITOR