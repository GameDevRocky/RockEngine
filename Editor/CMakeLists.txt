# Editor module

cmake_minimum_required(VERSION 3.20)

file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

add_library(RockEngineEditor STATIC ${EDITOR_SOURCES})

# InspectorVisitor.cpp instantiates BindProperty<T, TargetT> once per property row
# across every inspected type, which overruns MSVC's default 65535-section limit
# per object file (fatal error C1128). /bigobj raises it; it has no effect on the
# generated code.
if(MSVC)
	target_compile_options(RockEngineEditor PRIVATE /bigobj)
endif()

target_include_directories(RockEngineEditor
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
		${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(RockEngineEditor
	PUBLIC
		RockEngineCore
		ExternalLibs
		
		Qt6::Core
		Qt6::Gui
		Qt6::Widgets
		Qt6::OpenGLWidgets
		# QLocalServer/QLocalSocket for the MCP bridge (src/mcp/).
		Qt6::Network
)

set_target_properties(RockEngineEditor PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
