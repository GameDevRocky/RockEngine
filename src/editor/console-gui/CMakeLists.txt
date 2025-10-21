file(GLOB_RECURSE EDITOR_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(ConsoleGui STATIC ${EDITOR_SRC})

target_include_directories(ConsoleGui
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ConsoleGui
    PRIVATE 
        Qt6::Core 
        Qt6::Gui 
        Qt6::Widgets 
    
)