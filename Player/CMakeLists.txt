# Player module -- the standalone game executable.
#
# Links RockEngineCore and NOTHING from Editor/. That is the entire point of this target:
# what ships to a player must not contain the editor, and the compiler is what enforces it.
# If you ever find yourself adding Qt6::* or RockEngineEditor here, the code you are trying
# to reach belongs in Engine instead.

cmake_minimum_required(VERSION 3.20)

file(GLOB_RECURSE PLAYER_SOURCES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

add_executable(RockEnginePlayer ${PLAYER_SOURCES})

target_include_directories(RockEnginePlayer
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(RockEnginePlayer
	PRIVATE
		RockEngineCore
		ExternalLibs
)

# AUTOMOC/AUTOUIC/AUTORCC are ON at the root for the editor's sake. This target has no Qt
# in it, so running moc over it is pure build time for nothing -- and leaving it on invites
# a confusing failure the first time someone adds a header here with a Q_OBJECT-looking
# macro in a comment.
set_target_properties(RockEnginePlayer PROPERTIES
	AUTOMOC OFF
	AUTOUIC OFF
	AUTORCC OFF
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
	VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# No console window behind a shipped game. Debug keeps one, because the player logs
# everything Console::Alert/Warn/Comment produces to stdout/stderr in AppMode::Player and
# losing that while developing would be a bad trade.
if(WIN32)
	set_target_properties(RockEnginePlayer PROPERTIES
		WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>
	)
endif()

# --- The shipped Python runtime -------------------------------------------------------------
# The player embeds CPython, so a machine with no Python installed needs one supplied. This
# stages it ONCE into build/bin/player-runtime/python, and the editor's Build window copies
# that folder into each game build -- so the cost is paid at engine build time, not on every
# game build.
#
# Three pieces are needed and the usual mistake is shipping only the first:
#   Lib/          the standard library (.py sources)
#   DLLs/         the extension modules (_socket.pyd, select.pyd, unicodedata.pyd, ...)
#   pythonXY.dll  the interpreter itself
# Miss DLLs/ or the dll and the game launches, then dies on the first `import` that needs
# them -- often not until a script runs, which looks like a scripting bug rather than a
# packaging one.
#
# No pip install step: requirements.txt contains only `watchdog`, which exists for
# FileWatcherSystem's hot-reload, and AppMode::Player does not construct that system.
find_package(Python3 3.10 QUIET COMPONENTS Interpreter Development)

if(Python3_FOUND)
	set(_runtime_dir "${CMAKE_BINARY_DIR}/bin/player-runtime")
	set(_runtime_stamp "${CMAKE_BINARY_DIR}/player-runtime.stamp")

	# Where the interpreter dll and the DLLs/ folder live has to come from sys.base_prefix,
	# NOT from the directory Python3_EXECUTABLE sits in. On this repo that executable is
	# very often .venv-setup/Scripts/python.exe (the setup script's virtualenv), and a venv
	# contains neither DLLs/ nor pythonXY.dll -- it just forwards to the real install. Ask
	# the interpreter where it actually came from instead of guessing from its path.
	execute_process(
		COMMAND "${Python3_EXECUTABLE}" -c "import sys; print(sys.base_prefix)"
		OUTPUT_VARIABLE _py_root
		OUTPUT_STRIP_TRAILING_WHITESPACE
		RESULT_VARIABLE _py_root_result
	)
	if(NOT _py_root_result EQUAL 0 OR NOT EXISTS "${_py_root}")
		get_filename_component(_py_root "${Python3_EXECUTABLE}" DIRECTORY)
	endif()
	file(TO_CMAKE_PATH "${_py_root}" _py_root)

	# Same trap as _py_root: inside a venv, Python3_STDLIB can resolve to the venv's own
	# (nearly empty) Lib. Take the base install's standard library, which is what actually
	# needs shipping.
	execute_process(
		COMMAND "${Python3_EXECUTABLE}" -c
				"import sys,sysconfig;print(sysconfig.get_paths(vars={'installed_base':sys.base_prefix,'base':sys.base_prefix})['stdlib'])"
		OUTPUT_VARIABLE _py_stdlib
		OUTPUT_STRIP_TRAILING_WHITESPACE
		RESULT_VARIABLE _py_stdlib_result
	)
	if(NOT _py_stdlib_result EQUAL 0 OR NOT EXISTS "${_py_stdlib}")
		set(_py_stdlib "${Python3_STDLIB}")
	endif()
	file(TO_CMAKE_PATH "${_py_stdlib}" _py_stdlib)

	set(_stage_cmds
		COMMAND ${CMAKE_COMMAND} -E make_directory "${_runtime_dir}/python"
		COMMAND ${CMAKE_COMMAND} -E copy_directory "${_py_stdlib}" "${_runtime_dir}/python/Lib"
	)
	if(WIN32 AND EXISTS "${_py_root}/DLLs")
		list(APPEND _stage_cmds
			COMMAND ${CMAKE_COMMAND} -E copy_directory "${_py_root}/DLLs" "${_runtime_dir}/python/DLLs"
		)
	endif()

	# Stamped so this runs on the first build and then never again. Without the stamp it
	# would re-copy the whole standard library on every single build of the player.
	add_custom_command(
		OUTPUT "${_runtime_stamp}"
		${_stage_cmds}
		COMMAND ${CMAKE_COMMAND} -E touch "${_runtime_stamp}"
		COMMENT "Staging the shippable Python runtime into bin/player-runtime ..."
		VERBATIM
	)
	add_custom_target(RockEnginePlayerRuntime DEPENDS "${_runtime_stamp}")
	add_dependencies(RockEnginePlayer RockEnginePlayerRuntime)

	# The interpreter shared library, copied next to the player exe. A dev run needs it on
	# PATH anyway, and the Build window picks it up from here for shipped builds. Each is
	# guarded by EXISTS: copy_if_different treats a missing SOURCE as an error, so an
	# unguarded copy would break the build on any layout that names these differently.
	if(WIN32)
		set(_py_dlls "")
		foreach(_name "python3.dll" "python${Python3_VERSION_MAJOR}${Python3_VERSION_MINOR}.dll")
			if(EXISTS "${_py_root}/${_name}")
				list(APPEND _py_dlls "${_py_root}/${_name}")
			endif()
		endforeach()
		if(_py_dlls)
			add_custom_command(TARGET RockEnginePlayer POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy_if_different
						${_py_dlls} "$<TARGET_FILE_DIR:RockEnginePlayer>"
				COMMENT "Copying the Python interpreter DLLs next to RockEnginePlayer..."
				VERBATIM
			)
		else()
			message(WARNING
				"No pythonXY.dll found under '${_py_root}'. RockEnginePlayer will build but "
				"game builds will not run on machines without Python installed.")
		endif()
	endif()
else()
	message(WARNING
		"Python3 not found: RockEnginePlayer will build, but no shippable Python runtime "
		"will be staged and game builds will not run on machines without Python.")
endif()
