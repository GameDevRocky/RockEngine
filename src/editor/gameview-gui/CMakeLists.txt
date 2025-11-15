file(GLOB_RECURSE EDITOR_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

add_library(GameViewGui STATIC ${EDITOR_SRC})

target_include_directories(GameViewGui
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/editor
        ${PROJECT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(GameViewGui
    PUBLIC
        EditorUtils
        
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)
