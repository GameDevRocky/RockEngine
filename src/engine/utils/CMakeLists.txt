file(GLOB_RECURSE ENGINE_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(EngineUtils STATIC ${ENGINE_SRC})

target_include_directories(EngineUtils
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external
    
        
)
