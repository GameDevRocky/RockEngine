add_library(Editor STATIC
    Editor.cpp
    Editor.hpp
    EditorWindow.cpp
    EditorWindow.hpp
)

target_include_directories(Editor
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Editor
    PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets yaml-cpp
)
