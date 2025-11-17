

file(GLOB_RECURSE ENGINE_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(Rendering STATIC ${ENGINE_SRC})

target_include_directories(Rendering
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external/yaml/include  # adjust if needed
        ${PROJECT_SOURCE_DIR}/external/stb
)


target_link_libraries(Rendering
    PUBLIC  
        glm::glm
        glad
        yaml-cpp
        
)
