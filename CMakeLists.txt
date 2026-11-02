cmake_minimum_required(VERSION 3.23)
project(RockEngine VERSION 1.0 LANGUAGES C CXX)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_definitions(PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/EHsc>)
     
    string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL")
    
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /RTC1")
else()
    add_compile_options(-Wall -Wextra)
endif()
 
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(ROCKENGINE_BUILD_EDITOR         "Build the Qt6 editor (RockEngineEditor + RockEngineLauncher)" ON)
option(ROCKENGINE_BUILD_TESTS "Build the RockEngine tests" OFF)
option(ROCKENGINE_STAGE_PLAYER_RUNTIME "Stage the shippable Python runtime beside the Player"         ON)
set(ROCKENGINE_ENABLE_CUDA "AUTO" CACHE STRING
    "Build the optional CUDA particle backend (AUTO, ON, or OFF)")
set_property(CACHE ROCKENGINE_ENABLE_CUDA PROPERTY STRINGS AUTO ON OFF)
string(TOUPPER "${ROCKENGINE_ENABLE_CUDA}" ROCKENGINE_ENABLE_CUDA)
if(NOT ROCKENGINE_ENABLE_CUDA MATCHES "^(AUTO|ON|OFF)$")
    message(FATAL_ERROR "ROCKENGINE_ENABLE_CUDA must be AUTO, ON, or OFF")
endif()

set(ROCKENGINE_HAS_CUDA OFF)
if(NOT ROCKENGINE_ENABLE_CUDA STREQUAL "OFF")
    include(CheckLanguage)
    check_language(CUDA)
    if(CMAKE_CUDA_COMPILER)
        enable_language(CUDA)
        find_package(CUDAToolkit REQUIRED)
        set(ROCKENGINE_HAS_CUDA ON)
        message(STATUS "RockEngine CUDA particles enabled with ${CMAKE_CUDA_COMPILER}")
    elseif(ROCKENGINE_ENABLE_CUDA STREQUAL "ON")
        message(FATAL_ERROR
            "ROCKENGINE_ENABLE_CUDA=ON but no compatible CUDA compiler was found. "
            "Set CMAKE_CUDA_COMPILER or CUDACXX to the desired nvcc executable.")
    else()
        message(STATUS "CUDA compiler not found; particle simulation will use OpenGL compute")
    endif()
endif()

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

if(ROCKENGINE_BUILD_EDITOR)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)

    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGLWidgets Network)
    if(ROCKENGINE_BUILD_TESTS)
        find_package(Qt6 REQUIRED COMPONENTS Test)
    endif()
endif()

add_subdirectory(External)
add_subdirectory(Engine)
if(ROCKENGINE_BUILD_EDITOR)
    add_subdirectory(Editor)
endif()
if(ROCKENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
add_subdirectory(Player)

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

get_target_property(_qt_executable Qt6::Core LOCATION)
get_filename_component(_qt_bin_dir "${_qt_executable}" DIRECTORY)

if(WIN32)
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
 
option(ROCKENGINE_BUNDLE_PYTHON "Bundle a standalone Python runtime + Domain content next to the launcher" OFF)
if(ROCKENGINE_BUNDLE_PYTHON)
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

endif()
