
file(GLOB_RECURSE ENGINE_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(Serialization STATIC ${ENGINE_SRC})

target_include_directories(Serialization
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external
)

target_link_libraries(Serialization
    PRIVATE yaml-cpp
)