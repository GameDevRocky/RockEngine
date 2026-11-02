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

file(GLOB_RECURSE ROCKENGINE_ENGINE_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/*.test.cpp"
)

add_executable(RockEngineTests ${ROCKENGINE_ENGINE_TEST_SOURCES})

target_include_directories(RockEngineTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/engine)
target_link_libraries(RockEngineTests PRIVATE RockEngineCore GTest::gtest)

add_test(NAME RockEngineTests COMMAND RockEngineTests)

if(ROCKENGINE_BUILD_EDITOR)
    file(GLOB_RECURSE ROCKENGINE_EDITOR_TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/editor/*.test.cpp"
    )

    add_executable(RockEngineEditorTests ${ROCKENGINE_EDITOR_TEST_SOURCES})
    target_include_directories(RockEngineEditorTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/editor)
    target_link_libraries(RockEngineEditorTests PRIVATE RockEngineEditor Qt6::Test GTest::gtest)

    add_test(NAME RockEngineEditorTests COMMAND RockEngineEditorTests)
    set_tests_properties(RockEngineEditorTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    if(WIN32)
        get_target_property(_qt_core_location Qt6::Core LOCATION)
        get_filename_component(_qt_bin_dir "${_qt_core_location}" DIRECTORY)
        find_program(WINDEPLOY_PATH windeployqt HINTS "${_qt_bin_dir}")
        if(WINDEPLOY_PATH)
            add_custom_command(TARGET RockEngineEditorTests POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E env PATH="${_qt_bin_dir}"
                        "${WINDEPLOY_PATH}"
                        "$<$<CONFIG:Debug>:--debug>"
                        --no-compiler-runtime
                        --no-translations
                        "$<TARGET_FILE:RockEngineEditorTests>"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_qt_bin_dir}/../plugins/platforms/qoffscreen$<$<CONFIG:Debug>:d>.dll"
                        "$<TARGET_FILE_DIR:RockEngineEditorTests>/platforms/"
                COMMENT "Deploying Qt for RockEngineEditorTests"
                VERBATIM
            )
        endif()
    endif()
endif()
