file(GLOB_RECURSE ENGINE_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(Components STATIC ${ENGINE_SRC})

target_include_directories(Components
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(Components
    PUBLIC 
        Serialization
    PRIVATE 
        yaml-cpp 
        glm::glm 
        box2d
)