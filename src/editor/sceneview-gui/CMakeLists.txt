cmake_minimum_required(VERSION 3.16)

add_library(SceneViewGui STATIC
    SceneViewGui.cpp
    SceneViewGui.hpp
)

target_include_directories(SceneViewGui
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/src/editor
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(SceneViewGui
   
    PUBLIC
        EditorUtils
        Rendering      
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::OpenGLWidgets
)
