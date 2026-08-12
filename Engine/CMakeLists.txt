cmake_minimum_required(VERSION 3.20)
 
# CONFIGURE_DEPENDS so a newly added source is picked up on the next build.
# Without it the glob is only evaluated at configure time, and a new .cpp silently
# fails to link (with an unresolved-symbol error that points at the caller, not at
# the missing file) until someone reconfigures by hand. Editor/CMakeLists.txt has
# always done this; Engine's not doing it was the odd one out.
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE ENGINE_HEADERS CONFIGURE_DEPENDS "include/*.hpp")

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
        ExternalLibs
        pybind11::embed
    PRIVATE
        # Deliberately PRIVATE, and deliberately not part of ExternalLibs: msdfgen's
        # headers stay off the Editor's include path entirely. Only FontAtlasBaker.cpp
        # includes them -- everything downstream sees a plain glyph table and a GL
        # texture id, so swapping the generator later touches one file.
        msdfgen::msdfgen
)

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