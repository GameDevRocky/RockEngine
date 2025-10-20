file(GLOB_RECURSE ENGINE_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(Rendering STATIC ${ENGINE_SRC})

# Include headers
target_include_directories(Rendering
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(Rendering
    PRIVATE yaml-cpp glm::glm box2d
    
)