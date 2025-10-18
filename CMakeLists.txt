cmake_minimum_required(VERSION 3.16)
project(RockEngine VERSION 1.0 LANGUAGES CXX)

# ----------------------------
# C++ standard
# ----------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------
# Qt setup
# ----------------------------
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# ----------------------------
# Source files
# ----------------------------
set(SOURCES
    src/main.cpp

    src/engine/components/Transform.cpp

    src/engine/core/Application.cpp
    src/engine/core/InputManager.cpp
    src/engine/core/System.cpp
    src/engine/core/SceneManager.cpp
    src/engine/core/Scene.cpp
    src/engine/core/TimeManager.cpp
    src/engine/core/GameObject.cpp
    src/engine/core/Component.cpp
    src/engine/core/Observable.cpp

    src/engine/utils/IDGenerator.cpp

    src/engine/serialization/Registry.cpp
    src/engine/serialization/Serializable.cpp
    src/engine/serialization/SerializableFactory.cpp

    src/engine/inspection/inspector.cpp
    
    src/editor/Editor.cpp
    src/editor/EditorWindow.cpp
)

# ----------------------------
# Executable
# ----------------------------
add_executable(RockEngine ${SOURCES})

target_include_directories(RockEngine
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/editor
    ${CMAKE_SOURCE_DIR}/third_party/glm
    ${CMAKE_SOURCE_DIR}/third_party/yaml/include
    ${CMAKE_SOURCE_DIR}/external/box2d/include
)

# ----------------------------
# Third-party libraries
# ----------------------------
# GLM (header-only)
add_subdirectory(external/glm)

# yaml-cpp
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "")
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "")
add_subdirectory(external/yaml)

# Box2D
add_subdirectory(external/box2d)

# ----------------------------
# Link libraries
# ----------------------------
target_link_libraries(RockEngine
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    yaml-cpp
    glm::glm
    box2d
)
