# SagaEngine — Tools Roadmap (Index + Isolation Policy)

> Last updated: 2026-04-28
> Companion documents:
> - `ROADMAP.md` — engine core
> - `EDITOR_ROADMAP.md` — editor (Qt-backed authoring tools)
> - `SHARED_ROADMAP.md` — subsystems linked into both editor and runtime
> - `Tools/EditorLab/EDITORLAB_ROADMAP.md` — interactive integration lab
> - `Tools/SystemDefinitionEngine/SDE_ROADMAP.md` — model definition + validation engine

This roadmap is the index for everything that lives under `Tools/`. The
two flagship tools today are **EditorLab** and the **System Definition
Engine (SDE)**; future tools live next to them.

The single most important rule of this layer is that **tools are not
the engine and not the editor.** They sit on top of the editor / engine
public surface (or, in the SDE's case, depend on neither). Each tool
must remain extractable into its own git repository on a single day's
work — meaning a tool's folder contains everything the tool needs
except the engine / editor public headers it consumes.

---

## Anchor tools

| Tool | Purpose | Edges |
|------|---------|-------|
| **EditorLab** | Interactive integration environment that drives an editor instance through scripted scenarios, simulates multi-user collaboration, inspects state, and exercises data flows under controllable conditions. | Consumes editor public surface (`EditorApp`, `IUIFactory`, `EditorHost`). May NOT modify editor source. |
| **SDE — System Definition Engine** | Defines, validates, and compiles game-system models (items, skills, recipes, …). The "compile-time" layer for game data: catches errors before runtime. The editor is the UI for it; the runtime consumes its output. | Depends on **nothing** in editor or engine. Editor + runtime depend on SDE, not the reverse. |

---

## Strict Isolation Policy

**Tools may NOT live inside `Editor/`, `Engine/`, `Server/`, `Backends/`, or `Apps/`.** Every tool folder is self-contained:

```
Tools/<ToolName>/
├── include/<ToolHeader>/      ← public headers
├── src/<ToolHeader>/          ← implementation
├── README.md                  ← stand-alone repo readme (lives WITH the tool)
├── <TOOL>_ROADMAP.md          ← travels with the tool when split off
└── tests/                     ← optional self-contained test suite
```

**Edge rules** (a single violation is a code-review block):

1. **No tool may modify editor / engine / server / backend source.**
   Tools consume them through public headers only.
2. **SDE may not include any `SagaEditor/` or `SagaEngine/` header.**
   SDE is fully stand-alone. Editor and runtime depend on SDE; the
   reverse arrow does not exist.
3. **EditorLab may include `SagaEditor/` public headers**, but only
   from inside `Tools/EditorLab/src/Adapters/`. Anything else inside
   EditorLab uses the abstraction those adapters expose.
4. **No engine / editor source file is allowed to include any
   `SagaEditorLab/` or `SagaSDE/` header.** Tools are downstream
   consumers, never upstream dependencies. The lint script
   `Tools/Scripts/check_tools_isolation.py` enforces this in CI.
5. **Tools live in their own CMake targets.** Linking the editor or
   engine library does not pull a tool into the build.
6. **Each tool's roadmap and README live inside the tool's folder.**
   The day a tool is extracted into its own git repo, the folder
   becomes the new repo root with zero rewrites.

---

## Why two tools, why now

The editor's design pillars — high customisability, multi-user
collaboration, third-party extensions — cannot be developed without
both an interactive integration environment and a model-validation
engine in place. Without EditorLab, we ship the editor and discover
collaboration races in production. Without SDE, every "save broke my
project" report becomes a runtime archeological dig instead of an
edit-time validation failure.

These tools also do *not* belong inside the editor or the engine. They
have separate concerns, separate consumers (CI, content authors, future
third-party studios), and a real chance of being independently useful
to projects that do not use the rest of the engine. Building them
inside `Tools/` from day one is what keeps that possibility open.

---

## Where to look next

- **EditorLab** — `Tools/EditorLab/EDITORLAB_ROADMAP.md`
- **SDE**       — `Tools/SystemDefinitionEngine/SDE_ROADMAP.md`
- **Isolation lint** — `Tools/Scripts/check_tools_isolation.py`
