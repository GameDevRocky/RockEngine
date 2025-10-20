add_library(Utils STATIC
    IDGenerator.cpp
)

target_include_directories(Utils
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external
)
