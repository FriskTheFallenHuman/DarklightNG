# OpenAL Soft vendor information

- Upstream: https://github.com/kcat/openal-soft
- Release: 1.25.2
- Tag: `1.25.2`
- Commit: `b2c48f7718ef3fcf67921a8b6534c4914e328970`
- License: LGPL-2.0-or-later; see `COPYING` and the component license files

Darklight builds this source as a static library from the parent CMake project.
The build enables the legacy EAX implementation, requires WASAPI, and disables
the DirectSound and WinMM backends as well as upstream tools and examples.

`include/eax4.h` and `include/efxlib.h` are Doom 3 compatibility headers kept
alongside the upstream public OpenAL headers. They are not part of OpenAL Soft's
public API.
