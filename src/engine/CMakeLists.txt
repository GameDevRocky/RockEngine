add_subdirectory(components)
add_subdirectory(core)
add_subdirectory(rendering)
add_subdirectory(serialization)
add_subdirectory(utils)

add_library(Engine STATIC
    Engine.cpp
)

target_include_directories(Engine
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Engine
    PUBLIC
        Components
        Core
        Serialization
        Utils
        glm::glm
        box2d
        yaml-cpp
)
