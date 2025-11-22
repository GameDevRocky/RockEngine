add_subdirectory(components)
add_subdirectory(core)
add_subdirectory(rendering)
add_subdirectory(serialization)
add_subdirectory(utils)
add_subdirectory(debug)

add_library(Engine STATIC
    Engine.cpp
)

target_include_directories(Engine
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

target_link_libraries(Engine
    PUBLIC
        Components
        Core
        Rendering
        Debug
        Serialization
        EngineUtils
        glm::glm
        box2d
        yaml-cpp
        Python3::Python
        pybind11::embed
)
