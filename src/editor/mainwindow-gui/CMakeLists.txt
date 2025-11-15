
file(GLOB_RECURSE EDITOR_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

add_library(MainWindowGui STATIC ${EDITOR_SRC})

target_include_directories(MainWindowGui
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/editor
        ${PROJECT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(MainWindowGui
    PUBLIC
        EditorUtils
        
    PRIVATE
        ConsoleGui
        InspectorGui
        HierarchyGui
        GameViewGui
        SceneViewGui

        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)
