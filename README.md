# DarklightNG

DarklightNG is a modernized fork of the GPL Doom 3 engine. It updates the
runtime, renderer, audio, build system, and original content-creation tools
while preserving the engine's data-driven workflow and game DLL architecture.

> [!IMPORTANT]
> This repository does not include the retail Doom 3 or Resurrection of Evil
> PK4 archives. A legally owned copy of the game data is required to play.
> DEMO FILES ARE INCLUDED

## Highlights

- SDL2-based windowing, OpenGL context creation, input, and event handling
- GLSL rendering, GPU skinning, lower draw-call overhead, and an in-engine GPU
  profiler
- Unlocked rendering(240hz) with a fixed 60 Hz gameplay simulation
- OpenAL Soft audio with WASAPI and software support for the legacy EAX APIs
- Faster collision detection using BVHs and optimized clip-model sweeps
- Implemented offline lightmapping and higher-quality baked lighting
- A dark, Dear ImGui-based DoomRadiant interface and ImGui ports of the
  original GUI, material, particle, sound, PDA, AF, and declaration tools
- A visual DoomScript Blueprint editor backed by generated event and type
  metadata
- Vendored SDL2, OpenAL Soft, and Dear ImGui dependencies; no Git submodules
  are required
- Animation and AAS refactors. 

DarklightNG is under active development. The supplied presets currently target
32-bit x86 Windows; 64-bit configurations are intentionally rejected.

## Requirements

- Windows 10 or newer
- [CMake](https://cmake.org/) 3.25 or newer
- Visual Studio 2022 with:
  - Desktop development with C++
  - MSVC x86 build tools
  - C++ MFC for the installed MSVC toolset
  - A Windows SDK
- [.NET 8 SDK](https://dotnet.microsoft.com/download/dotnet/8.0) or newer for
  the TypeInfo generator
- A legally owned, fully patched Doom 3 installation

The engine, game DLL, SDL2, and OpenAL Soft are built from source. No separate
SDL or OpenAL runtime DLL is needed.

## Game data

Copy the retail game data into this repository's `base/` directory so the PK4
files sit beside `base/DoomConfig.cfg`. To run Resurrection of Evil, also copy
its `d3xp/` directory to the repository root.

Do not redistribute the retail game data. It remains subject to the original
game EULA and is not covered by the engine's GPL license.

## Build

Run these commands from the repository root:

```powershell
cmake --preset vs2022-x86 -S neo
cmake --build build --config Release --target doom3
```

For a debug build, replace `Release` with `Debug`. CMake writes the matching
`Doom3.exe` and `gamex86.dll` directly to the repository root; libraries,
symbols, and other intermediate artifacts go under `out/`.

### Visual Studio 2026 preset

An additional preset is provided for Visual Studio 2026. From the repository
root:

```powershell
Set-Location neo
cmake --preset vs2026-x86
cmake --build --preset release
Set-Location ..
```

Use the `debug` build preset for a debug build. This configuration places its
generated solution under `out/tools-build/`.

## Run

Launch the base game from the repository root:

```powershell
.\Doom3.exe
```

Launch Resurrection of Evil with:

```powershell
.\Doom3.exe +set fs_game d3xp
```

The working directory matters: the engine expects to find `base/` relative to
the executable.

## Tools

DoomRadiant and the content tools are built into `Doom3.exe`:

```powershell
# Level editor
.\Doom3.exe +editor

# Standalone GUI editor
.\Doom3.exe +guieditor

# Material editor
.\Doom3.exe +materialEditor
```

The remaining editors are available from DoomRadiant. Open the visual scripting
tool through **Editors > DoomScript Blueprint Editor**.

### Generated TypeInfo

The build runs the C# TypeInfo tool before compiling `gamex86.dll`. It scans the
annotated game and animation declarations and updates:

- `neo/game/generated/DoomTypeInfo.generated.h`
- `base/script/doom_events.script`
- `base/editors/doomscript_nodes.def`

Generated files are only replaced when their content changes. See
[`neo/TypeInfo/README.md`](neo/TypeInfo/README.md) for annotation syntax,
manual generation, Blueprint migration, and editor behavior.

## Project layout

| Path | Contents |
| --- | --- |
| `neo/` | Engine, game DLL, tools, third-party source, and CMake configuration |
| `neo/game/` | Gameplay code compiled as `gamex86.dll` |
| `neo/renderer/` | OpenGL/GLSL renderer and offline rendering utilities |
| `neo/tools/` | DoomRadiant, content editors, and map compilers |
| `neo/TypeInfo/` | C# metadata and DoomScript node generator |
| `base/` | Loose game definitions, scripts, editor data, configuration, and local retail data |
| `build/` | Visual Studio 2022 CMake build tree |
| `out/` | Build intermediates, generated files, libraries, and symbols |

## Troubleshooting

**CMake reports that the target must be 32-bit.** Use one of the supplied x86
presets. DarklightNG is not currently a 64-bit engine.

**Visual Studio cannot find MFC headers or libraries.** Open the Visual Studio
Installer, modify the installation, and add the C++ MFC component matching the
installed MSVC toolset.

**The engine starts but cannot find game data.** Check that the retail PK4 files
are directly inside `base/` and launch `Doom3.exe` with the repository root as
its working directory.

**The build cannot run the TypeInfo tool.** Install the .NET 8 SDK and confirm
that `dotnet --version` works in a new terminal.

## License

The Doom 3 source is licensed under the GNU General Public License v3 with the
additional Doom 3 terms in [`COPYING.txt`](COPYING.txt). Third-party components
retain their own licenses and notices; see [`README.txt`](README.txt) and the
license files in their respective source directories.

Doom and Doom 3 are trademarks of id Software LLC. This project is an altered
source version and is not the original id Software release.
