cmake_minimum_required(VERSION 3.25)

if(WIN32)
	project(DarklightDoom3 LANGUAGES C CXX RC)
else()
	project(DarklightDoom3 LANGUAGES C CXX)
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "Darklight Doom 3 is a 32-bit x86 target. Use a Win32 generator or the linux-x86 preset.")
endif()

if(WIN32 AND NOT MSVC)
	message(FATAL_ERROR "The Windows build requires MSVC.")
elseif(NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
	message(FATAL_ERROR "Supported platforms are 32-bit MSVC Windows and 32-bit x86 Linux.")
endif()

set(DARKLIGHT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
get_filename_component(DARKLIGHT_ROOT "${DARKLIGHT_ROOT}" ABSOLUTE)
set(DARKLIGHT_OUT "${DARKLIGHT_ROOT}/out")

# C# TypeInfo header tool. This follows the UnrealHeaderTool model: inert tags
# on declarations are scanned before the DLL compiles, and one generated header
# supplies both event declarations and (at its single implementation include)
# the event objects and class registration tables. A stamp absorbs no-op scans
# so unchanged generated files keep their timestamps and do not force builds.
if(WIN32)
	find_program(DOTNET_EXECUTABLE dotnet REQUIRED)
else()
	find_program(DOTNET_EXECUTABLE dotnet)
endif()
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
	"anim/*.h"
	"game/*.cpp"
	"game/*.h"
)
list(FILTER DARKLIGHT_TYPEINFO_INPUTS EXCLUDE REGEX "/game/generated/")

if(DOTNET_EXECUTABLE)
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
		--extra-source "${CMAKE_CURRENT_SOURCE_DIR}/anim"
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
elseif(EXISTS "${DARKLIGHT_TYPEINFO_HEADER}")
	message(STATUS "dotnet not found; using checked-in generated type metadata")
	add_custom_target(darklight_typeinfo)
else()
	message(FATAL_ERROR "dotnet is required because ${DARKLIGHT_TYPEINFO_HEADER} is missing")
endif()
set_target_properties(darklight_typeinfo PROPERTIES FOLDER "Build Tools")
set_source_files_properties(
	"${DARKLIGHT_TYPEINFO_HEADER}"
	PROPERTIES GENERATED TRUE
)

# Keep Linux outputs isolated from the Windows executable and DLL.
if(WIN32)
	set(DARKLIGHT_RUNTIME_OUT "${DARKLIGHT_ROOT}")
else()
	set(DARKLIGHT_RUNTIME_OUT "${DARKLIGHT_OUT}/linux")
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${DARKLIGHT_RUNTIME_OUT}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${DARKLIGHT_RUNTIME_OUT}")
foreach(configuration Debug Release RelWithDebInfo MinSizeRel)
	string(TOUPPER "${configuration}" configuration_upper)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${configuration_upper} "${DARKLIGHT_RUNTIME_OUT}")
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${configuration_upper} "${DARKLIGHT_RUNTIME_OUT}")
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${configuration_upper} "${DARKLIGHT_OUT}/${configuration}")
	set(CMAKE_PDB_OUTPUT_DIRECTORY_${configuration_upper} "${DARKLIGHT_OUT}/${configuration}")
	set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY_${configuration_upper} "${DARKLIGHT_OUT}/${configuration}")
endforeach()

if(MSVC)
	set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT doom3)

# OpenAL Soft is the engine's only audio backend.  Build it into Doom3.exe so
# there is no OpenAL DLL to deploy or load at runtime, and require the native
# WASAPI backend.  Legacy EAX support is OpenAL Soft's software implementation
# of the EAX 1-5 APIs used by the original Doom 3 sound code.
set(LIBTYPE STATIC CACHE STRING "Build OpenAL Soft as a static library" FORCE)
set(ALSOFT_EAX ON CACHE BOOL "Enable legacy EAX extensions" FORCE)
if(WIN32)
	set(ALSOFT_BACKEND_WASAPI ON CACHE BOOL "Enable WASAPI backend" FORCE)
	set(ALSOFT_REQUIRE_WASAPI ON CACHE BOOL "Require WASAPI backend" FORCE)
	set(ALSOFT_BACKEND_DSOUND OFF CACHE BOOL "Disable DirectSound backend" FORCE)
	set(ALSOFT_BACKEND_WINMM OFF CACHE BOOL "Disable WinMM backend" FORCE)
	set(ALSOFT_BACKEND_WAVE OFF CACHE BOOL "Disable wave writer backend" FORCE)
else()
	set(ALSOFT_BACKEND_PIPEWIRE OFF CACHE BOOL "Disable PipeWire backend" FORCE)
	set(ALSOFT_BACKEND_PULSEAUDIO OFF CACHE BOOL "Disable PulseAudio backend" FORCE)
	set(ALSOFT_BACKEND_ALSA OFF CACHE BOOL "Disable ALSA backend" FORCE)
	set(ALSOFT_BACKEND_OSS OFF CACHE BOOL "Disable OSS backend" FORCE)
	set(ALSOFT_BACKEND_JACK OFF CACHE BOOL "Disable JACK backend" FORCE)
	set(ALSOFT_BACKEND_SDL2 OFF CACHE BOOL "Disable SDL audio backend" FORCE)
	set(ALSOFT_BACKEND_SDL3 OFF CACHE BOOL "Disable SDL audio backend" FORCE)
	set(ALSOFT_BACKEND_WAVE ON CACHE BOOL "Enable headless wave backend" FORCE)
endif()
set(ALSOFT_UTILS OFF CACHE BOOL "Do not build OpenAL Soft utilities" FORCE)
set(ALSOFT_EXAMPLES OFF CACHE BOOL "Do not build OpenAL Soft examples" FORCE)
set(ALSOFT_TESTS OFF CACHE BOOL "Do not build OpenAL Soft tests" FORCE)
set(ALSOFT_INSTALL OFF CACHE BOOL "Do not install OpenAL Soft" FORCE)
set(ALSOFT_INSTALL_CONFIG OFF CACHE BOOL "Do not install OpenAL Soft configuration" FORCE)
set(ALSOFT_INSTALL_HRTF_DATA OFF CACHE BOOL "Do not install external HRTF data" FORCE)
set(ALSOFT_INSTALL_AMBDEC_PRESETS OFF CACHE BOOL "Do not install AmbDec presets" FORCE)
set(ALSOFT_INSTALL_EXAMPLES OFF CACHE BOOL "Do not install OpenAL Soft examples" FORCE)
set(ALSOFT_INSTALL_UTILS OFF CACHE BOOL "Do not install OpenAL Soft utilities" FORCE)
set(ALSOFT_UPDATE_BUILD_VERSION OFF CACHE BOOL "Use the vendored release version" FORCE)
set(ALSOFT_ENABLE_MODULES OFF CACHE BOOL "Avoid generator-specific C++ module artifacts" FORCE)
if(WIN32)
	set(FORCE_STATIC_VCRT ON CACHE BOOL "Match Darklight's static MSVC runtime" FORCE)
endif()
add_subdirectory(openal EXCLUDE_FROM_ALL)
set_target_properties(OpenAL PROPERTIES FOLDER "Libraries/OpenAL Soft")

# SDL2 owns the game window, OpenGL context, input and event pump.  Keep the
# dependency fully vendored and static so Doom3.exe has no SDL2.dll runtime
# dependency and uses the same /MT runtime as the rest of Darklight.
set(SDL_SHARED OFF CACHE BOOL "Do not build the SDL2 shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build the SDL2 static library" FORCE)
set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "Darklight provides its own entry point" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "Do not install vendored SDL2" FORCE)
set(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "Do not create SDL2 uninstall targets" FORCE)
set(SDL_TEST OFF CACHE BOOL "Do not build SDL2 tests" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Do not build the SDL2 test library" FORCE)
if(WIN32)
	set(SDL_FORCE_STATIC_VCRT ON CACHE BOOL "Match Darklight's static MSVC runtime" FORCE)
else()
	set(SDL_ALSA OFF CACHE BOOL "Disable unused SDL audio backend" FORCE)
	set(SDL_PIPEWIRE OFF CACHE BOOL "Disable unused SDL audio backend" FORCE)
	set(SDL_PULSEAUDIO OFF CACHE BOOL "Disable unused SDL audio backend" FORCE)
	set(SDL_WAYLAND OFF CACHE BOOL "Use SDL X11 backend" FORCE)
	set(SDL_KMSDRM OFF CACHE BOOL "Disable direct KMS/DRM" FORCE)
	set(SDL_IBUS OFF CACHE BOOL "Disable optional IBus dependency" FORCE)
	set(SDL_HIDAPI OFF CACHE BOOL "Disable optional HID dependency" FORCE)
	set(SDL_JOYSTICK OFF CACHE BOOL "Disable joystick subsystem" FORCE)
	set(SDL_HAPTIC OFF CACHE BOOL "Disable haptic subsystem" FORCE)
	set(SDL_X11_XCURSOR OFF CACHE BOOL "Disable optional Xcursor dependency" FORCE)
	set(SDL_X11_XINPUT OFF CACHE BOOL "Disable optional XInput dependency" FORCE)
	set(SDL_X11_XFIXES OFF CACHE BOOL "Disable optional Xfixes dependency" FORCE)
	set(SDL_X11_XRANDR OFF CACHE BOOL "Disable optional Xrandr dependency" FORCE)
	set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "Disable optional Xss dependency" FORCE)
	set(SDL_X11_XSHAPE OFF CACHE BOOL "Disable optional XShape dependency" FORCE)
endif()
add_subdirectory(sys/sdl2 EXCLUDE_FROM_ALL)
set_target_properties(SDL2-static PROPERTIES FOLDER "Libraries/SDL2")

function(darklight_target_defaults target)
	if(WIN32)
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
	else()
		target_compile_definitions(${target} PRIVATE
			ID_DISABLE_TOOLS
			ID_GL_HARDLINK
			_FILE_OFFSET_BITS=64
		)
		target_compile_options(${target} PRIVATE
			$<$<COMPILE_LANGUAGE:CXX>:-fno-strict-aliasing>
			$<$<COMPILE_LANGUAGE:CXX>:-fno-omit-frame-pointer>
			$<$<COMPILE_LANGUAGE:CXX>:-Wno-write-strings>
			$<$<COMPILE_LANGUAGE:CXX>:-Wno-invalid-offsetof>
		)
	endif()
	target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
	target_precompile_headers(${target} PRIVATE
		"$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/idlib/precompiled.h>"
	)
	source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES $<TARGET_PROPERTY:${target},SOURCES>)
endfunction()

file(GLOB_RECURSE IDLIB_SOURCES CONFIGURE_DEPENDS "idlib/*.cpp")
list(REMOVE_ITEM IDLIB_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/idlib/math/Simd_AltiVec.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/idlib/precompiled.cpp"
)
if(WIN32)
	list(REMOVE_ITEM IDLIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/idlib/bv/Frustum_gcc.cpp")
endif()

add_library(idlib STATIC ${IDLIB_SOURCES})
target_compile_definitions(idlib PRIVATE __IDLIB__ __DOOM__ ID_DISABLE_TOOLS ID_ENABLE_CURL=0)
darklight_target_defaults(idlib)
set_target_properties(idlib PROPERTIES FOLDER "Libraries")

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "game/*.cpp")
list(REMOVE_ITEM GAME_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/game/EndLevel.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/game/gamesys/Callbacks.cpp"
)

if(WIN32)
	list(APPEND GAME_SOURCES "game/Game.def")
endif()
add_library(game SHARED ${GAME_SOURCES})
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
	"aas/*.cpp"
	"anim/*.cpp"
	"cm/*.cpp"
	"decllib/*.cpp"
	"framework/*.cpp"
	"renderer/*.cpp"
	"sound/*.cpp"
	"tools/compilers/*.cpp"
	"ui/*.cpp"
)
if(WIN32)
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
# The standalone editor dialogs have been replaced in-place by one ImGui
# implementation in each original tool folder. Keep their runtime backends,
# but do not compile the retired MFC windows or private control library.
list(FILTER EDITOR_SOURCES EXCLUDE REGEX "/tools/(af|decl|materialeditor|particle|pda|sound)/")
list(FILTER EDITOR_SOURCES EXCLUDE REGEX "/tools/script/DialogScriptEditor\\.cpp$")
list(FILTER EDITOR_SOURCES EXCLUDE REGEX "/tools/common/PropTree/")
list(REMOVE_ITEM EDITOR_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/tools/common/RollupPanel.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/tools/comafx/CPathTreeCtrl.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/tools/comafx/CSyntaxRichEditCtrl.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/tools/comafx/DialogGoToLine.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/tools/comafx/VectorCtl.cpp"
)
list(APPEND EDITOR_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/tools/af/AFEditorImGui.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/tools/decl/DeclBrowserImGui.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/tools/materialeditor/MaterialEditorImGui.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/tools/particle/ParticleEditorImGui.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/tools/pda/PDAEditorImGui.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/tools/sound/SoundEditorImGui.cpp"
)
file(GLOB SYS_PLATFORM_SOURCES CONFIGURE_DEPENDS "sys/sys_*.cpp")
list(REMOVE_ITEM SYS_PLATFORM_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/sys/sys_gl_logfuncs.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/sys/sys_sdl_linux.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/sys/sys_sdl_net.cpp"
)
list(APPEND ENGINE_SOURCES
	${EDITOR_SOURCES}
	${SYS_PLATFORM_SOURCES}
	"${CMAKE_CURRENT_SOURCE_DIR}/libbink/bink.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/sys/rc/CreateResourceIDs.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/sys/rc/doom.rc"
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
else()
	set(SYS_PLATFORM_SOURCES
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/sys_local.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/sys_sdl_linux.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/sys_sdl_net.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/sys_glimp.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/sys_gamma.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/sys_input.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/sys_wndproc.cpp"
	)
	list(APPEND ENGINE_SOURCES
		${SYS_PLATFORM_SOURCES}
		"${CMAKE_CURRENT_SOURCE_DIR}/libbink/bink.cpp"
	)
endif()
list(REMOVE_ITEM ENGINE_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/ui/GameWindow.cpp"
)

if(WIN32)
	add_executable(doom3 WIN32 ${ENGINE_SOURCES})
else()
	add_executable(doom3 ${ENGINE_SOURCES})
endif()
# Dmap's offline light baker is a compute kernel.  Keep the rest of the Debug
# engine fully debuggable, but compile this translation unit with optimization
# and frame pointers so SIMD packet tracing does not spill every lane to the
# stack and profiler call stacks remain usable.
if(MSVC)
	set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tools/compilers/dmap/lightmap.cpp" APPEND PROPERTY COMPILE_OPTIONS
		"$<$<CONFIG:Debug>:/O2>"
		"$<$<CONFIG:Debug>:/Ob2>"
		"$<$<CONFIG:Debug>:/Oy->"
	)
	set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tools/compilers/dmap/lightmap.cpp" APPEND PROPERTY VS_SETTINGS
		"BasicRuntimeChecks=Default"
	)
endif()
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tools/compilers/dmap/lightmap.cpp" PROPERTY SKIP_PRECOMPILE_HEADERS ON)
if(NOT MSVC)
	set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tools/compilers/dmap/lightmap.cpp" APPEND PROPERTY COMPILE_OPTIONS -msse2)
endif()
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/libbink/bink.cpp" PROPERTY SKIP_PRECOMPILE_HEADERS ON)
target_compile_definitions(doom3 PRIVATE
	__DOOM__
	__DOOM_DLL__
	_D3XP
	ID_ENABLE_CURL=0
	SDL_MAIN_HANDLED
)

if(WIN32)
	target_include_directories(doom3 PRIVATE
		"${CMAKE_CURRENT_SOURCE_DIR}/imgui"
		"${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends"
	)
endif()
target_link_libraries(doom3 PRIVATE
	idlib
	OpenAL::OpenAL
	SDL2::SDL2-static
)
if(WIN32)
	target_link_libraries(doom3 PRIVATE
		dbghelp glu32 comctl32 iphlpapi odbc32 odbccp32 opengl32
		shell32 shlwapi winmm wsock32
	)
	target_link_options(doom3 PRIVATE /STACK:16777216,16777216 /LARGEADDRESSAWARE)
else()
	find_package(Threads REQUIRED)
	find_package(OpenGL REQUIRED)
	target_link_libraries(doom3 PRIVATE Threads::Threads OpenGL::GL dl m)
	target_link_options(doom3 PRIVATE -Wl,-z,stack-size=16777216 -Wl,--export-dynamic)
endif()
darklight_target_defaults(doom3)
set_target_properties(doom3 PROPERTIES
	OUTPUT_NAME Doom3
	FOLDER "Engine"
)
if(WIN32)
	set_target_properties(doom3 PROPERTIES
		VS_GLOBAL_UseOfMfc Static
		VS_DEBUGGER_WORKING_DIRECTORY "${DARKLIGHT_ROOT}"
		VS_DEBUGGER_COMMAND_ARGUMENTS "+set fs_game d3xp"
	)
else()
	set_target_properties(doom3 PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
endif()

# Doom3 loads gamex86.dll dynamically, so it must not link against the game
# import library.  Keep an explicit target dependency instead: building the
# Doom3 executable (from CMake or by building Doom3.vcxproj in Visual Studio)
# always builds the matching-config game DLL first.
add_dependencies(doom3 game)
