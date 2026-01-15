cmake_minimum_required(VERSION 3.20)
project(RockEngine VERSION 1.0 LANGUAGES C CXX)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/Zc:__cplusplus)
else()
    add_compile_options(-Wall -Wextra)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON) 

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGLWidgets)

add_subdirectory(External)
add_subdirectory(Engine)
add_subdirectory(Editor)

add_executable(RockEngineLauncher 
    src/main.cpp
)
target_link_libraries(RockEngineLauncher
    PRIVATE
        RockEngineCore
        RockEngineEditor
)