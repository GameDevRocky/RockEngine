file(GLOB_RECURSE ENGINE_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(Core STATIC ${ENGINE_SRC})

# Include headers
target_include_directories(Core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(Core
    PUBLIC
        Components
        Serialization
        Debug
        EngineUtils
    PRIVATE 
        yaml-cpp 
        glm::glm 
        box2d
)