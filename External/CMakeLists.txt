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
        Python3::Python
        pybind11::embed
)

target_include_directories(ExternalLibs
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/glad/include
        ${CMAKE_CURRENT_SOURCE_DIR}/stb
        ${Python3_INCLUDE_DIRS}
)
