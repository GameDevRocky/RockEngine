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

find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

set(PYBIND11_FINDPYTHON ON)
add_subdirectory(pybind11)

add_library(glad SHARED glad/src/glad.c)

target_include_directories(glad 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/glad/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/glad/src
)

# Create ExternalLibs interface library to aggregate all external dependencies
add_library(ExternalLibs INTERFACE)

target_link_libraries(ExternalLibs
    INTERFACE
        glm
        yaml-cpp
        box2d
        glad
)

target_include_directories(ExternalLibs
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/glad/include
        ${CMAKE_CURRENT_SOURCE_DIR}/stb
)
