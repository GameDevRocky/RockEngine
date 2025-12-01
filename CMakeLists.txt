cmake_minimum_required(VERSION 3.16)
project(RockEngine VERSION 1.0 LANGUAGES C CXX)

# C++ settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# C settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# MSVC-specific settings
if(MSVC)
    add_compile_options(/Zc:__cplusplus)
endif()

# Qt configuration
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON) 

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_subdirectory(external)
add_subdirectory(src/engine)
add_subdirectory(src/editor)

add_executable(RockEngine
    src/main.cpp
)
target_include_directories(RockEngine
    PRIVATE
        ${CMAKE_SOURCE_DIR}/external
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

option(EMBED_APP_ICON "Embed Windows .ico into executable resource" OFF)


set(APP_RC "${CMAKE_SOURCE_DIR}/domain/assets/icons/RockEngine_Icon.rc")
set(APP_ICO "${CMAKE_SOURCE_DIR}/domain/assets/icons/RockEngine_Icon.ico")

if(EMBED_APP_ICON AND EXISTS ${APP_RC} AND EXISTS ${APP_ICO})
    target_sources(RockEngine PRIVATE "${APP_RC}")
    message(STATUS "Embedding Windows icon: ${APP_ICO}")
else()
    message(STATUS "EMBED_APP_ICON is ${EMBED_APP_ICON}. If you want the EXE to contain an embedded icon, create ${APP_ICO} and ${APP_RC} and set -DEMBED_APP_ICON=ON when configuring CMake.")
endif()

target_link_libraries(RockEngine
    PRIVATE
        Engine
        Editor
)
