# Headless unit tests over RockEngineCore.
#
# The rule for everything in here: no GPU, no window, no Qt. RockEngineCore contains zero
# Qt and confines OpenGL to Engine/src/rendering/, so serialization, id remapping, the
# Observable/Event system, the asset meta conventions and the pure math are all reachable
# from a plain console executable. Anything that needs a GL context, an audio device or
# Engine::PostInit() does NOT belong in this target -- it would turn a 3-second CI lane
# into a flaky one.
#
# Built only when ROCKENGINE_BUILD_TESTS=ON (the ci-* presets); off by default so the
# normal local build is unchanged.

cmake_minimum_required(VERSION 3.20)

# CONFIGURE_DEPENDS, matching Engine/ and Editor/: a newly added test file is picked up on
# the next build instead of silently not running.
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

add_executable(RockEngineTests ${TEST_SOURCES})

target_include_directories(RockEngineTests
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_SOURCE_DIR}/External/doctest
)

target_link_libraries(RockEngineTests
	PRIVATE
		RockEngineCore
		ExternalLibs
)

# No Qt in this target. AUTOMOC is scoped to the editor build at the root now, but pin it
# off here too so enabling tests inside a full editor build never moc-scans them.
set_target_properties(RockEngineTests PROPERTIES
	AUTOMOC OFF
	AUTOUIC OFF
	AUTORCC OFF
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# One CTest entry for the whole binary rather than per-case registration: doctest already
# prints which case failed and why, and this avoids vendoring doctest's CMake module
# alongside the header.
add_test(NAME RockEngineTests COMMAND RockEngineTests)
