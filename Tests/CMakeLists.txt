include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

if(MSVC AND NOT ROCKENGINE_STATIC_MSVC_RUNTIME)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()

FetchContent_MakeAvailable(googletest)

file(GLOB_RECURSE ROCKENGINE_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.test.cpp"
)

add_executable(RockEngineTests ${ROCKENGINE_TEST_SOURCES})

target_include_directories(RockEngineTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(RockEngineTests PRIVATE RockEngineCore GTest::gtest)

add_test(NAME RockEngineTests COMMAND RockEngineTests)
