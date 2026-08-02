cmake_minimum_required(VERSION 3.25)

project(DarklightDoom3 LANGUAGES C CXX RC)

if(NOT WIN32 OR NOT MSVC)
	message(FATAL_ERROR "This source tree currently supports the 32-bit MSVC Windows build only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "Doom 3 uses 32-bit x86 assembly. Configure with a Win32 generator platform (-A Win32).")
endif()

set(DARKLIGHT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
get_filename_component(DARKLIGHT_ROOT "${DARKLIGHT_ROOT}" ABSOLUTE)
set(DARKLIGHT_OUT "${DARKLIGHT_ROOT}/out")

# C# TypeInfo header tool. This follows the UnrealHeaderTool model: inert tags
# on declarations are scanned before the DLL compiles, and one generated header
# supplies both event declarations and (at its single implementation include)
# the event objects and class registration tables. A stamp absorbs no-op scans
# so unchanged generated files keep their timestamps and do not force builds.
find_program(DOTNET_EXECUTABLE dotnet REQUIRED)
set(DARKLIGHT_TYPEINFO_PROJECT "${CMAKE_CURRENT_SOURCE_DIR}/TypeInfo/DoomTypeInfo.csproj")
set(DARKLIGHT_TYPEINFO_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/game/generated/DoomTypeInfo.generated.h")
set(DARKLIGHT_TYPEINFO_SCRIPT "${DARKLIGHT_ROOT}/base/script/doom_events.script")
set(DARKLIGHT_TYPEINFO_NODES "${DARKLIGHT_ROOT}/base/editors/doomscript_nodes.def")
set(DARKLIGHT_TYPEINFO_STAMP "${DARKLIGHT_OUT}/typeinfo/typeinfo.stamp")

file(GLOB_RECURSE DARKLIGHT_TYPEINFO_TOOL_SOURCES CONFIGURE_DEPENDS
	"TypeInfo/*.cs"
	"TypeInfo/*.csproj"
)
list(FILTER DARKLIGHT_TYPEINFO_TOOL_SOURCES EXCLUDE REGEX "/(bin|obj)/")
file(GLOB_RECURSE DARKLIGHT_TYPEINFO_INPUTS CONFIGURE_DEPENDS
	"game/*.cpp"
	"game/*.h"
)
list(FILTER DARKLIGHT_TYPEINFO_INPUTS EXCLUDE REGEX "/game/generated/")

add_custom_command(
	OUTPUT "${DARKLIGHT_TYPEINFO_STAMP}"
	BYPRODUCTS
		"${DARKLIGHT_TYPEINFO_HEADER}"
		"${DARKLIGHT_TYPEINFO_SCRIPT}"
		"${DARKLIGHT_TYPEINFO_NODES}"
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${DARKLIGHT_OUT}/typeinfo"
	# MSBuild's native VC environment carries a legacy LIB entry that Roslyn
	# reports as CS1668. The C# tool has no native library dependencies.
	COMMAND "${CMAKE_COMMAND}" -E env --unset=LIB "${DOTNET_EXECUTABLE}" run
		--project "${DARKLIGHT_TYPEINFO_PROJECT}"
		--configuration Release
		--no-launch-profile
		--
		generate
		--source "${CMAKE_CURRENT_SOURCE_DIR}/game"
		--header "${DARKLIGHT_TYPEINFO_HEADER}"
		--script "${DARKLIGHT_TYPEINFO_SCRIPT}"
		--nodes "${DARKLIGHT_TYPEINFO_NODES}"
		--stamp "${DARKLIGHT_TYPEINFO_STAMP}"
		--exclude EndLevel.cpp
	DEPENDS ${DARKLIGHT_TYPEINFO_TOOL_SOURCES} ${DARKLIGHT_TYPEINFO_INPUTS}
	WORKING_DIRECTORY "${DARKLIGHT_ROOT}"
	COMMENT "Generating Doom events and runtime class metadata"
	VERBATIM
)

add_custom_target(darklight_typeinfo DEPENDS "${DARKLIGHT_TYPEINFO_STAMP}")
set_target_properties(darklight_typeinfo PROPERTIES FOLDER "Build Tools")
set_source_files_properties(
	"${DARKLIGHT_TYPEINFO_HEADER}"
	PROPERTIES GENERATED TRUE
)

# Keep every playable configuration beside base/ so Visual Studio always emits
# Doom3.exe and gamex86.dll into the project root.
foreach(configuration Debug Release RelWithDebInfo MinSizeRel)
	string(TOUPPER "${configuration}" configuration_upper)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${configuration_upper} "${DARKLIGHT_ROOT}")
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${configuration_upper} "${DARKLIGHT_ROOT}")
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${configuration_upper} "${DARKLIGHT_OUT}/${configuration}")
	set(CMAKE_PDB_OUTPUT_DIRECTORY_${configuration_upper} "${DARKLIGHT_OUT}/${configuration}")
	set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY_${configuration_upper} "${DARKLIGHT_OUT}/${configuration}")
endforeach()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT doom3)

function(darklight_target_defaults target)
	set_target_properties(${target} PROPERTIES
		VS_GLOBAL_VcpkgEnabled false
		VS_GLOBAL_VcpkgEnableManifest false
		VS_GLOBAL_VcpkgApplocalDeps false
	)
	target_compile_definitions(${target} PRIVATE
		WIN32
		_WINDOWS
		_CRT_SECURE_NO_DEPRECATE
		_CRT_NONSTDC_NO_DEPRECATE
		_USE_32BIT_TIME_T
	)
	target_compile_options(${target} PRIVATE
		$<$<COMPILE_LANGUAGE:CXX>:/GR>
		/wd4018
		/wd4100
		/wd4244
		/wd4267
		/wd4714
		/wd4996
	)
	target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
	target_precompile_headers(${target} PRIVATE
		"$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/idlib/precompiled.h>"
	)
	source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES $<TARGET_PROPERTY:${target},SOURCES>)
endfunction()

file(GLOB_RECURSE IDLIB_SOURCES CONFIGURE_DEPENDS "idlib/*.cpp")
list(REMOVE_ITEM IDLIB_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/idlib/bv/Frustum_gcc.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/idlib/math/Simd_AltiVec.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/idlib/precompiled.cpp"
)

add_library(idlib STATIC ${IDLIB_SOURCES})
target_compile_definitions(idlib PRIVATE __IDLIB__ __DOOM__ ID_DISABLE_TOOLS ID_ENABLE_CURL=0)
darklight_target_defaults(idlib)
set_target_properties(idlib PROPERTIES FOLDER "Libraries")

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "game/*.cpp")
list(REMOVE_ITEM GAME_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/game/EndLevel.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/game/gamesys/Callbacks.cpp"
)

add_library(game SHARED ${GAME_SOURCES} "game/Game.def")
add_dependencies(game darklight_typeinfo)
set_source_files_properties("game/gamesys/TypeInfo.cpp" PROPERTIES
	SKIP_PRECOMPILE_HEADERS ON
	COMPILE_DEFINITIONS _ALLOW_KEYWORD_MACROS
)
target_compile_definitions(game PRIVATE
	__DOOM__
	GAME_DLL
	_D3XP
	CTF
	ID_ENABLE_CURL=0
	# VS 2026 enables STL bounds hardening in Release by default. The original
	# game DLL was authored for unchecked release iterators and otherwise stops
	# tool startup in the CRT before an editor window can be created.
	_MSVC_STL_HARDENING=0
)
target_link_libraries(game PRIVATE idlib)
darklight_target_defaults(game)
set_target_properties(game PROPERTIES
	OUTPUT_NAME gamex86
	PREFIX ""
	FOLDER "Game"
)

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
	"cm/*.cpp"
	"framework/*.cpp"
	"renderer/*.cpp"
	"renderer/jpeg-6/*.c"
	"sound/*.cpp"
	"sound/OggVorbis/oggsrc/*.c"
	"sound/OggVorbis/vorbissrc/*.c"
	"tools/compilers/*.cpp"
	"ui/*.cpp"
)
file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS
	"tools/af/*.cpp"
	"tools/comafx/*.cpp"
	"tools/common/*.cpp"
	"tools/debugger/*.cpp"
	"tools/decl/*.cpp"
	"tools/guied/*.cpp"
	"tools/materialeditor/*.cpp"
	"tools/particle/*.cpp"
	"tools/pda/*.cpp"
	"tools/radiant/*.cpp"
	"tools/script/*.cpp"
	"tools/sound/*.cpp"
)
list(REMOVE_ITEM EDITOR_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/tools/common/RenderBumpFlatDialog.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/tools/decl/DialogEntityDefEditor.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/tools/guied/GEWindowWrapper_stub.cpp"
)
list(FILTER EDITOR_SOURCES EXCLUDE REGEX "/tools/debugger/")
file(GLOB WIN32_SOURCES CONFIGURE_DEPENDS "sys/win32/*.cpp")
list(APPEND ENGINE_SOURCES
	${EDITOR_SOURCES}
	${WIN32_SOURCES}
	"${CMAKE_CURRENT_SOURCE_DIR}/libbink/bink.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/sys/sys_local.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/sys/win32/rc/CreateResourceIDs.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/sys/win32/rc/doom.rc"
)
set(DARKLIGHT_IMGUI_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_tables.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_win32.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_opengl2.cpp"
)
list(APPEND ENGINE_SOURCES ${DARKLIGHT_IMGUI_SOURCES})
set_source_files_properties(${DARKLIGHT_IMGUI_SOURCES} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
list(REMOVE_ITEM ENGINE_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6/jcapistd.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6/jctrans.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6/jload.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6/jmemansi.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6/jmemdos.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6/jmemname.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6/jpegtran.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/sound/OggVorbis/vorbissrc/psytune.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/sound/OggVorbis/vorbissrc/tone.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/sys/win32/gl_logfuncs.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/sys/win32/win_gamma.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/ui/GameWindow.cpp"
)

add_executable(doom3 WIN32 ${ENGINE_SOURCES})
# Dmap's offline light baker is a compute kernel.  Keep the rest of the Debug
# engine fully debuggable, but compile this translation unit with optimization
# and frame pointers so SIMD packet tracing does not spill every lane to the
# stack and profiler call stacks remain usable.
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tools/compilers/dmap/lightmap.cpp" APPEND PROPERTY COMPILE_OPTIONS
	"$<$<CONFIG:Debug>:/O2>"
	"$<$<CONFIG:Debug>:/Ob2>"
	"$<$<CONFIG:Debug>:/Oy->"
)
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tools/compilers/dmap/lightmap.cpp" APPEND PROPERTY VS_SETTINGS
	"BasicRuntimeChecks=Default"
)
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tools/compilers/dmap/lightmap.cpp" PROPERTY SKIP_PRECOMPILE_HEADERS ON)
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/libbink/bink.cpp" PROPERTY SKIP_PRECOMPILE_HEADERS ON)
target_compile_definitions(doom3 PRIVATE
	__DOOM__
	__DOOM_DLL__
	_D3XP
	ID_ENABLE_CURL=0
)
target_include_directories(doom3 PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/sound/OggVorbis"
	"${CMAKE_CURRENT_SOURCE_DIR}/imgui"
	"${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends"
)
target_link_libraries(doom3 PRIVATE
	idlib
	"${CMAKE_CURRENT_SOURCE_DIR}/openal/lib/eaxguid.lib"
	dbghelp
	dinput8
	dsound
	dxguid
	glu32
	comctl32
	iphlpapi
	odbc32
	odbccp32
	opengl32
	shell32
	shlwapi
	winmm
	wsock32
)
target_link_options(doom3 PRIVATE /STACK:16777216,16777216 /LARGEADDRESSAWARE)
darklight_target_defaults(doom3)
set_target_properties(doom3 PROPERTIES
	OUTPUT_NAME Doom3
	FOLDER "Engine"
	VS_GLOBAL_UseOfMfc Static
	VS_DEBUGGER_WORKING_DIRECTORY "${DARKLIGHT_ROOT}"
	VS_DEBUGGER_COMMAND_ARGUMENTS "+set fs_game d3xp"
)

# Doom3 loads gamex86.dll dynamically, so it must not link against the game
# import library.  Keep an explicit target dependency instead: building the
# Doom3 executable (from CMake or by building Doom3.vcxproj in Visual Studio)
# always builds the matching-config game DLL first.
add_dependencies(doom3 game)
