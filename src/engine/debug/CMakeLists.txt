file(GLOB_RECURSE ENGINE_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(Debug STATIC ${ENGINE_SRC})

target_include_directories(Debug
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(Debug
    PRIVATE yaml-cpp glm::glm box2d 
)