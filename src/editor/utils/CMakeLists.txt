file(GLOB_RECURSE EDITOR_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(EditorUtils STATIC ${EDITOR_SRC})

target_include_directories(EditorUtils
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/editor/utils
)

target_link_libraries(EditorUtils
    PRIVATE 
        Qt6::Core
        Qt6::Gui 
        Qt6::Widgets 
)