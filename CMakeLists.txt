cmake_minimum_required(VERSION 3.20)
project(RockEngine VERSION 1.0 LANGUAGES C CXX)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_definitions(PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
if(MSVC)
    add_compile_options(/Zc:__cplusplus)
    add_compile_options(/EHsc) # <--- ADD THIS LINE TO FIX THE C4530 WARNING
    
    # Enable Hot Reload (Edit and Continue)
    string(REPLACE "/Zi" "/ZI" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/Zi" "/ZI" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} /INCREMENTAL")
    
    # Ensure optimizations are off for Debug
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /RTC1")
else()
    add_compile_options(-Wall -Wextra)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# Tell Visual Studio where the debugger should start

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON) 

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGLWidgets)

add_subdirectory(External)
add_subdirectory(Engine)
add_subdirectory(Editor)

add_executable(RockEngineLauncher src/main.cpp)

set_target_properties(RockEngineLauncher PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)
target_link_libraries(RockEngineLauncher
    PRIVATE
        RockEngineCore
        RockEngineEditor
)  

qt_finalize_executable(RockEngineLauncher)
if(MSVC)
    # 1. Dynamically find where Qt's binaries are located
    get_target_property(_qt_executable Qt6::Core LOCATION)
    get_filename_component(_qt_bin_dir "${_qt_executable}" DIRECTORY)
    find_program(WINDEPLOY_PATH windeployqt HINTS "${_qt_bin_dir}")

    # 2. Run the deployment tool after the build finishes
    add_custom_command(TARGET RockEngineLauncher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E env PATH="${_qt_bin_dir}" 
                "${WINDEPLOY_PATH}" 
                --debug 
                --no-compiler-runtime 
                --no-translations
                "$<TARGET_FILE:RockEngineLauncher>"
        COMMENT "Deploying Qt dependencies from ${_qt_bin_dir}..."
    )
endif()