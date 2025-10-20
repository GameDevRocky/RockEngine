cmake_minimum_required(VERSION 3.16)
project(RockEngine VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
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


target_link_libraries(RockEngine
    PRIVATE
        Engine
        Editor
)
