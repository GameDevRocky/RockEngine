set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
add_subdirectory(console-gui)

add_library(Editor STATIC
    Editor.cpp
    Editor.hpp
)

target_include_directories(Editor
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Editor
    PUBLIC 
    Qt6::Core
    Qt6::Gui 
    Qt6::Widgets 
    yaml-cpp
    ConsoleGui
)
