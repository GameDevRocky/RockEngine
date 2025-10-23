# --- Qt Setup ---
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# --- Subdirectories ---
add_subdirectory(console-gui)

# --- Editor Utils Library ---
file(GLOB_RECURSE EDITOR_UTILS_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.hpp"
)

add_library(EditorUtils STATIC ${EDITOR_UTILS_SRC})

target_include_directories(EditorUtils
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}         # src/editor
)

target_link_libraries(EditorUtils
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

# --- Main Editor Library ---
add_library(Editor STATIC
    Editor.cpp
    Editor.hpp
)

target_include_directories(Editor
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Editor
    PUBLIC 
        Qt6::Core
        Qt6::Gui 
        Qt6::Widgets 
        yaml-cpp
        ConsoleGui
        EditorUtils
)
