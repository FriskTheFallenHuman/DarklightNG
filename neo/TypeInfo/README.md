# Darklight TypeInfo header tool

This is the C# replacement for the legacy C++ TypeInfo generator. It follows
the UnrealHeaderTool model: declarations in C++ headers carry inert annotations
and the build scans them before compiling the game DLL.

The declaration annotations are:

- `D3_CLASS()` immediately before a concrete class definition;
- `D3_CLASS( Abstract )` immediately before an abstract class definition;
- `D3_ENUM( BlueprintType )` immediately before an enum declaration whose
  values should be available to Blueprint pins;
- `D3_ENUM_VALUE( Hidden )` after an enumerator that is engine-only and should
  not appear in a Blueprint dropdown;
- `D3_EVENT( Symbol, "command", returnType )` immediately before the callback
  declaration that defines and binds an event;
- `D3_EVENT( Symbol )` for another callback binding to an already-defined event.

The scanner parses the actual class definition following `D3_CLASS`, including
its name and public base class. It parses the function declaration following
each `D3_EVENT` to obtain the callback name, parameter names, and parameter
types. The supported explicit return types are `void`, `integer`, `float`,
`vector`, `string`, and `entity`. `integer` preserves Doom's native integer
event ABI; DoomScript exposes it as `float`, like the original generator.

`int`, reflected enums, `float`, `idVec3`/`idAngles`, C strings, entity pointers, and
`trace_t *` are inferred automatically. Use `D3_NULLABLE` on an entity pointer
parameter when the old event ABI must accept a null entity (`E` rather than
`e`). Multiple event annotations may precede one callback. The rare inherited
callback can be written explicitly as `D3_EVENT( Symbol, Base::Callback )`.
Namespace signatures define events that intentionally have no callback. There
is no end tag.

An optional `D3_NODE(...)` immediately before `D3_EVENT` overrides inferred
editor metadata. It accepts the flags `Pure`, `Latent`, `Hidden`, and
`Deprecated`, plus string-valued `Title`, `Category`, `Description`,
`Keywords`, and `Receiver`. Receiver is one of `auto`, `sys`, `global`, or
`object`. Without an override, the generator derives the node from the event,
its attached function, bindings, class hierarchy, and adjacent `//` comment.

`generate` produces `DoomTypeInfo.generated.h`,
`base/script/doom_events.script`, and
`base/editors/doomscript_nodes.def`. The node catalog gives DoomRadiant a
stable node ID, title, category, command, receiver mode, emission kind, return
type, owning classes, callback, source location, and named typed pins for every
engine-defined script event. In the graph these are ordinary editable Function
Call nodes, exactly like DoomScript-defined functions; there is no separate
read-only native-node type. Enum parameters retain Doom's numeric event ABI but
carry their reflected enum identity and visible symbolic values in the node
catalog. `Event.h` includes the declaration half of
the generated header; `gamesys/Class.cpp` includes its implementation half once
to instantiate every `idEventDef` and class callback table. Files are replaced
only when their contents change. `verify` exits unsuccessfully if any output is
stale. The generated event script also receives Blueprint metadata.

`migrate-blueprints --scripts base/script` imports every script function into
an Unreal-style Event graph and appends versioned layout metadata as an EOF
comment. Function parameters become Event pins, declarations are kept as
Event-local or global/object variables, and executable logic is laid out from
left to right. Re-running it retains matching node positions and is
deterministic; add `--verify` to check the corpus without writing.

In DoomRadiant, open `Editors > DoomScript Blueprint Editor`. This opens a
standalone, double-buffered Dear ImGui/OpenGL window rather than an MFC child
canvas. Every function appears as a red Event entry, branches and loops retain
their control-flow node types, and the executable chain runs left to right.
Branch nodes expose separate `True` and `False` execution outputs; an `else`
clause is the False path rather than a second Branch node. `return`, `break`,
and `continue` are never linked into the next statement in their source block.
Loop nodes expose `Loop Body` and `Completed` execution outputs. Normal body
tails and `continue` return to the loop condition; `break` joins `Completed`,
while `return` terminates the function path.
Double-clicking a Branch, Loop, or its `Evaluate Condition` data node opens the
typed condition builder. Existing top-level `&&`/`||` expressions import as
removable rows. Each row selects a scoped variable, `Is True`/`Is False` or
`==`, `!=`, `<`, `<=`, `>`, `>=`, and either another type-compatible variable
or a constant. Adding or removing rows regenerates the DoomScript condition
and its typed Get-variable pin graph without source-code entry.
Assignments import as green Set nodes. Referenced parameters, Event locals,
file/object variables, and inherited object fields import as Get nodes with
typed data wires into Set, Branch, Loop, and expression pins. Drag an output
pin onto a compatible input pin to change the emitted expression. Function Call
nodes expose every parameter as an editable pin with an inline unconnected
default/constant or a compatible variable connection, plus a typed return pin.
Reflected enum defaults are dropdowns populated from `D3_ENUM`, so animation,
sound-channel, and joint-transform parameters do not require manual code entry.
Double-click Function Call, Branch, Loop, or Set nodes to choose typed variables,
comparisons, and values; the editor emits the DoomScript and never asks for node
source code.
The left scope panel shows file/object, Event-local, and inherited/shared fields,
adds declarations, and creates Set nodes. Right-drag pans, the mouse wheel
zooms, and node movement is rendered without repainting DoomRadiant controls.
The standalone editor is per-monitor DPI-aware and applies an enlarged baseline
scale to fonts, panels, nodes, pins, hit targets, and graph spacing.
Function calls nested directly in Branch, Loop, and Set expressions are expanded
into their own editable Function Call nodes. Unary boolean negation becomes a
separate `Not Boolean` operator node, so a condition such as
`!animDone(channel, blendFrames)` is represented by callable and operator pins
instead of an opaque condition string. Holding the right mouse button captures
the graph camera anywhere inside the canvas until the button is released.

Run from the repository root:

```powershell
dotnet run --project neo/TypeInfo -- generate `
  --source neo/game `
  --header neo/game/generated/DoomTypeInfo.generated.h `
  --script base/script/doom_events.script `
  --nodes base/editors/doomscript_nodes.def `
  --exclude EndLevel.cpp
```

```powershell
dotnet run --project neo/TypeInfo -- migrate-blueprints `
  --scripts base/script
```
