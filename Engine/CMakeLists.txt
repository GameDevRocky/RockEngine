cmake_minimum_required(VERSION 3.20)

file(GLOB_RECURSE ENGINE_SOURCES "src/*.cpp")
file(GLOB_RECURSE ENGINE_HEADERS "include/*.hpp")

add_library(RockEngineCore STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})
target_include_directories(RockEngineCore 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# 4. Link Libraries
target_link_libraries(RockEngineCore 
    PUBLIC 
        EngineAPI 
        ExternalLibs
)

# 5. Compile Definitions
# Note: Keep the export define for now; it is harmless for STATIC builds.
target_compile_definitions(RockEngineCore PRIVATE ROCK_ENGINE_EXPORT)

# 6. Precompiled Headers (Kept the extra headers from your AI)
target_precompile_headers(RockEngineCore PUBLIC 
    <vector>
    <string>
    <map>
    <memory>
    <iostream>
    <unordered_map>
    <algorithm>
    <functional>
)

# 7. Output properties
set_target_properties(RockEngineCore PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    CXX_STANDARD 20  # Updated to 20 as we discussed earlier
    CXX_STANDARD_REQUIRED ON
)