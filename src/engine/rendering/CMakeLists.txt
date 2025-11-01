# Collect all cpp files in src/engine/rendering
file(GLOB_RECURSE ENGINE_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# Add GLAD source
set(GLAD_SRC "${PROJECT_SOURCE_DIR}/external/glad/src/glad.c")  # adjust path

# Create a static library
add_library(Rendering STATIC ${ENGINE_SRC} ${GLAD_SRC})

# Include directories
target_include_directories(Rendering
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external/glad/include  # where glad/glad.h lives
)

# Link libraries
target_link_libraries(Rendering
    PRIVATE 
        yaml-cpp 
        glm::glm 
        box2d
)
