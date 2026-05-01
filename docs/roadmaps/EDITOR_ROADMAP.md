# SagaEditor — Authoring Tools Roadmap

> Last updated: 2026-04-25
> Target: A production-grade authoring environment for SagaEngine.
> Companion documents: `ROADMAP.md` (engine core) and `SHARED_ROADMAP.md`
> (subsystems shared between editor and runtime).

This document is the single source of truth for the SagaEditor — the
authoring shell, the panels, the docking system, the visual scripting
graph editor, the package SDK, and every other tool that ships only
inside the editor binary. Items that live in the runtime side of the
engine are tracked in `ROADMAP.md`. Items whose ownership is split
between editor and runtime (the C# scripting host, the canonical IR
compiler, the security boundary policy, sample content, runtime
profiling and validation) live in `SHARED_ROADMAP.md`.

Conventions used throughout:

- `[x]` — Shipped. The note after the item names the files that
  represent the work and highlights any decisions worth preserving.
- `[ ]` — Open. Either unstarted or partially explored; the item
  describes the finished state rather than interim scaffolding.
- All editor headers live under `Editor/include/SagaEditor/...` and the
  matching translation units live under `Editor/src/SagaEditor/...`.
- The editor is a Qt application, but the SagaEditor headers must stay
  Qt-free. The Qt surface is hidden behind the `IUIApplication`,
  `IUIFactory`, `IUIMainWindow`, and `IUIWidget` interfaces under
  `Editor/include/SagaEditor/UI/`.

- **STRICT RULE — Qt isolation.** Qt headers (`<Q*>`) may appear in
  exactly two places:
    1. `Editor/include/SagaEditor/UI/Qt/**` — the Qt backend's public
       interface. Today this directory holds `QtUIApplication.h`,
       `QtUIFactory.h`, `QtUIMainWindow.h`, `QtPanel.h`.
    2. `Editor/src/SagaEditor/UI/Qt/**` — every Qt-using `.cpp` file
       in the editor. Concrete panel implementations, the command
       palette dialog, the theme applicator, anything that touches a
       `QWidget` lives here. The matching framework-free header
       stays in its original folder (`Panels/InspectorPanel.h`,
       `Commands/CommandPalette.h`, etc.) so call sites never learn
       about Qt.

  No other file in `Editor/` is allowed to `#include <Q*>`. A single
  `#include <QWidget>` outside this boundary is a code-review block
  and a reason to reject the patch. The `tools/check_qt_boundary.py`
  script enforces this in CI; running it locally takes a fraction of
  a second.

  Why: a future ImGui or web backend should be a swap, not a rewrite.
  When Qt goes, only `Editor/.../UI/Qt/` deletes; nothing else moves.
- **The editor never includes SDL.** SDL is the engine runtime's
  client-side platform layer — it has no place in authoring tools.
  The editor calls into the `IUIMainWindow` / `IUIFactory` /
  `IUIApplication` abstraction and the Qt backend (or any future
  backend) lives only inside `Editor/include/SagaEditor/UI/Qt/` and
  the matching `.cpp`s. Any `#include <SDL2/SDL.h>` that appears
  inside `Editor/` is a code-review block. This rule is what keeps
  Qt swappable: a future ImGui or web backend is one `IUIFactory`
  swap, not a global `#include` rewrite.

---

## 1. Shell, Docking, and Window Chrome

The editor shell hosts every panel, owns the menu bar and toolbar, and
is the only place where the Qt event loop is started and stopped.
Panels never talk to Qt directly — they implement `IPanel` and the
shell binds the panel's native widget pointer into the dock workspace.

| Status | Item |
|--------|------|
| [x] | `EditorShell` — owns the main window, registers default panels, builds the chrome. `Shell/EditorShell.h` / `.cpp`. |
| [x] | `EditorApp` — application entry point that bootstraps `IUIApplication`, `EditorHost`, and `EditorShell`. `Host/EditorApp.h` / `.cpp`. |
| [x] | `EditorHost` — service host that owns and exposes editor subsystems through typed accessors with ordered init / shutdown. `Host/EditorHost.h` / `.cpp`. |
| [x] | `IPanel` — Qt-free base interface every dockable panel implements. `Panels/IPanel.h`. |
| [x] | `DockWorkspace` — wraps dock widget creation, panel ↔ dock mapping, and `SaveState` / `RestoreState`. `Docking/DockWorkspace.h` / `.cpp`. |
| [x] | `DockLayoutManager` — bridges `DockWorkspace` and `LayoutSerializer` for save / load / reset. `Docking/DockLayoutManager.h` / `.cpp`. |
| [x] | `DockTab` — reusable tab abstraction so multiple panels can share a dock area. `Docking/DockTab.h` / `.cpp`. |
| [x] | `LayoutSerializer` — reads and writes `LayoutPreset` and `WorkspacePreset` JSON files from `<workspace>/Layouts/`. `Layouts/LayoutSerializer.h` / `.cpp`. |
| [x] | `LayoutPreset` and `WorkspacePreset` data types — dock arrangement, theme, and toolbar presets. `Layouts/LayoutPreset.h`, `WorkspacePreset.h`. |
| [x] | `ShellLayout` — declarative menu / toolbar / status-bar visibility descriptor. `Shell/ShellLayout.h`. |
| [x] | `RegisterShellCommands` — populates File, Edit, View, World command ids into the registry. `Shell/ShellCommands.h` / `.cpp`. |
| [x] | `ShellWindow` — concrete Qt main-window bridge for `IUIMainWindow`. `Shell/ShellWindow.h` / `.cpp`. |
| [ ] | UI factory completion — finalise `IUIFactory` so every panel can be instantiated without including a Qt header in any SagaEditor public surface. |
| [ ] | Status bar service — a structured surface for transient progress, build status, simulation tick rate, and connection indicators. |
| [ ] | Toolbar customisation — runtime add / remove / reorder of toolbar entries from packages and from user preferences. |
| [ ] | Workspace presets shipping — Creator, Indie, Studio, Technical, and Custom presets as first-class JSON profiles in `Layouts/`. |
| [ ] | Editor persistence — layout state, shortcut bindings, active theme, and recent files restored on next launch through a `QSettings`-backed sink. |
| [ ] | Headless editor mode — automated import, validation, cook, and content-build runs without a visible UI; uses the same `IUIApplication` surface with a null backend. |

---

## 2. Commands, Shortcuts, and Undo

Every user-visible action in the editor is named through the command
registry. Shortcuts, the command palette, menu entries, and toolbar
buttons are all thin views over the same registry. Undo / redo lives
next to commands because most actions worth undoing are also worth
naming.

| Status | Item |
|--------|------|
| [x] | `CommandRegistry` — stores every named editor command; supports late registration and unregistration by packages. `Commands/CommandRegistry.h` / `.cpp`. |
| [x] | `CommandDispatcher` — invokes handlers with pre-hook veto and post-hook observation. `Commands/CommandDispatcher.h` / `.cpp`. |
| [x] | `CommandPalette` — frameless dialog with live text filter, keyboard navigation, and Enter-to-invoke. Opens on `Ctrl+Shift+P`. `Commands/CommandPalette.h` / `.cpp`. |
| [x] | `ShortcutManager` — maps `KeyChord` (modifier + scancode) to command ids; remappable at runtime; `OnKeyEvent` dispatches immediately. `Commands/ShortcutManager.h` / `.cpp`. |
| [x] | `UndoRedoStack` — `IEditorAction` interface, linear history, configurable depth, `CanUndo` / `CanRedo`, label accessors. `Commands/UndoRedoStack.h` / `.cpp`. |
| [ ] | Macro recorder — sequence of dispatched commands captured into a replayable macro that can be saved and re-bound. |
| [ ] | Command groups and contextual filtering — palette and shortcut maps respect the currently focused panel so the same chord can mean different things in different contexts. |
| [ ] | Conflict detection for shortcut bindings — operator-visible warning when two commands resolve to the same chord, with a deterministic resolution policy. |

---

## 2.5. UI Persona System (Customisability Spectrum)

The editor is the same product whether the user is a 10-year-old
dragging snap-together blocks or a senior tools engineer running a
seven-panel `.uasset`-style workflow. The persona system is what
makes that possible: a single switch — exposed in the welcome screen,
the View menu, and the command palette — flips the theme, the
panel set, the toolbar, the keyboard map, the dock layout, the visual
scripting block style, the inspector density, the gizmo size, the
icon set, and the shortcut hint visibility. Personas are first-class
build artefacts: every persona is a JSON profile that ships with the
editor, can be cloned to a `Custom` slot, and can be shared between
users as a single file drop.

The two anchor personas the engine commits to shipping are described
at the bottom of this section under "Anchor Personas". Every other
persona slides between them on the same axes.

### Architecture

The persona is a value type composed of three orthogonal sub-types,
all framework-free and JSON-serialisable:

- `DensityProfile` — the spacing-and-size scale. Five steps from
  `Cosy` (large fonts, generous padding, kid-friendly hit boxes) to
  `Ultra` (smallest fonts, tightest padding, expert-only).
- `BlockVisualStyle` — how visual scripting nodes draw. Encodes block
  shape (`Stacked` Scratch-style vs. `PinNode` Unreal-style vs.
  `Hybrid`), corner radius, slot shape, palette saturation,
  connection style (orthogonal, bezier, straight), and label scale.
- `UIPersona` — the top-level container that names the persona,
  references a theme id, references a default workspace preset id,
  picks a `DensityProfile` and a `BlockVisualStyle`, and lists which
  panels are visible by default and which toolbar entries are pinned.

A `PersonaRegistry` owns the catalogue (built-in personas plus
user-authored ones loaded from `<workspace>/Personas/`) and
`PersonaActivator` applies a persona by fanning the change out to the
`ThemeRegistry`, the `LayoutSerializer`, the inspector density
service, the visual scripting graph canvas, and the gizmo overlay.
Switching personas mid-session is a first-class flow — the user
should be able to A/B test which UI fits the current task without
restarting the editor.

| Status | Item |
|--------|------|
| [ ] | `DensityProfile` value type — five built-in steps (`Cosy`, `Comfortable`, `Compact`, `Dense`, `Ultra`) plus a custom slot. |
| [ ] | `BlockVisualStyle` value type — `BlockShape` enum (Stacked / PinNode / Hybrid), corner radius, slot shape, saturation multiplier, connector style, label scale, palette swatch size. |
| [ ] | `UIPersona` value type — name, description, theme id, workspace-preset id, density id, block-style id, default-visible panel list, default-toolbar entry list. |
| [ ] | `PersonaRegistry` — owns built-in personas, loads user personas from JSON, exposes `Find`, `GetAll`, `OnPersonaChanged`. |
| [ ] | `PersonaActivator` — fans a `UIPersona` change out to `ThemeRegistry`, `LayoutSerializer`, the inspector density service, the graph canvas, and the gizmo overlay. |
| [ ] | Persona JSON serializer — round-trip with the workspace preset format so a persona file is a single drop. |
| [ ] | Welcome-screen persona picker — first-launch experience asks the user which persona to start with and explains the trade-offs. |
| [ ] | View-menu persona switcher — runtime swap with a one-click "preview" that reverts on cancel. |
| [ ] | Persona delta diagnostics — the editor reports which panels were closed / opened by the switch so the user is not surprised. |
| [ ] | Persona compatibility check — packages can declare a `requiredPersona` or a `recommendedPersona`; the editor warns when a package's recommended persona does not match the active one. |

### Anchor Personas

`BlockyBeginner` — kid-friendly, blocks-first authoring. Inspired by
the look and feel of Scratch.

  - Density: `Cosy` (16/24/32 px spacing, 14 pt body font).
  - BlockVisualStyle: `Stacked` (chunky 10 px-corner blocks, top-bottom
    snap connectors, high-saturation category colours, no exposed
    typed pins, big slot dropdowns).
  - Default panels: Hierarchy, World Viewport, Block Palette, Stage
    (a single graph canvas), Output Console.
  - Toolbar: green Play, red Stop, gold Save. Three buttons total.
  - Keyboard map: minimised — Ctrl+Z / Ctrl+S / Space (run) only.
  - Theme: a friendly light palette with high-contrast category
    colours; the gizmo overlay uses thick lines and large handles.

`ProDense` — professional, multi-panel, dense pro-tools layout.
Inspired by the look and feel of Unreal Engine.

  - Density: `Compact` or `Dense` user-selectable.
  - BlockVisualStyle: `PinNode` (rounded-rectangle nodes with typed
    input / output pins, bezier connections, low-saturation accent
    colours, exposed pin types).
  - Default panels: World Viewport (centre), Hierarchy + Outliner
    tabs, Inspector + Details tabs, Asset Browser, Console + Output
    Log tabs, Profiler, optional Graph Editor in a secondary tab
    group.
  - Toolbar: Play / Pause / Stop / Step + a Build dropdown + a Mode
    selector (Translate / Rotate / Scale / Universal).
  - Keyboard map: full set including viewport WASDQE fly-cam,
    F-frame-selection, Ctrl+Shift+P palette, alt-drag duplicate,
    middle-mouse pan.
  - Theme: dark, low saturation, small accents; gizmo overlay is
    thin-lined and pixel-precise.

The intermediate personas (`IndieBalanced`, `Technical`, `Custom`) sit
between the anchors. The `Custom` slot is the user's own persona,
clonable from any built-in and edited live through the View → Edit
Persona dialogue.

---

## 3. Selection, Theme, and Localisation

| Status | Item |
|--------|------|
| [x] | `SelectionManager` — ordered multi-selection with a `SelectionChanged` signal and primary-id query. `Selection/SelectionManager.h` / `.cpp`. |
| [x] | `EditorTheme` — QSS stylesheet plus `ColorPalette` plus fonts. `Themes/EditorTheme.h`. |
| [x] | `ThemeRegistry` — runtime `Apply()` and `OnThemeChanged` callback; ships Dark, Light, Nord, SolarizedDark, Midnight. `Themes/ThemeRegistry.h` / `.cpp`. |
| [x] | `ColorPalette` — 28 semantic tokens covering backgrounds, foregrounds, accent, status, and gizmo colors. `Themes/ColorPalette.h` / `.cpp`. |
| [x] | Localisation scaffolding — string-table directory under `Localization/`. |
| [ ] | High-DPI rendering pass — every painter-driven panel honours device pixel ratio and the gizmo line widths scale with it. |
| [ ] | Translation pipeline — string-key extraction, missing-key reports, and a per-locale fallback policy. |
| [ ] | Custom theme authoring UI — operator can edit `ColorPalette` slots live and save the result to `Layouts/` as a new preset. |

---

## 4. Core Panels

The default workspace ships these panels. Each one is registered by
the shell at start-up, dockable, themable, and reachable from the
command palette.

| Status | Item |
|--------|------|
| [x] | `HierarchyPanel` — entity tree with live search. `Panels/HierarchyPanel.h` / `.cpp`. |
| [x] | `InspectorPanel` — selection-reactive scroll area. `Panels/InspectorPanel.h` / `.cpp`. |
| [x] | `ConsolePanel` — severity-filtered rich-text log. `Panels/ConsolePanel.h` / `.cpp`. |
| [x] | `AssetBrowserPanel` — folder tree plus asset grid. `Panels/AssetBrowserPanel.h` / `.cpp`. |
| [x] | `WorldViewportPanel` — viewport panel with grid placeholder and full input routing. `Panels/WorldViewportPanel.h` / `.cpp`. |
| [x] | `ProjectBrowserPanel` — project root, recent projects, project metadata. `Panels/ProjectBrowserPanel.h` / `.cpp`. |
| [x] | `CollaborationPanel` — peer presence and live edit indicators. `Panels/CollaborationPanel.h` / `.cpp`. |
| [x] | `ProfilerPanel` — runtime perf counters and frame breakdown. `Panels/ProfilerPanel.h` / `.cpp`. |
| [x] | `GraphViewportPanel` — visual scripting graph host. `Panels/GraphViewportPanel.h` / `.cpp`. |
| [ ] | Live search-and-replace inside the hierarchy. |
| [ ] | Console filter persistence — saved filter sets restored on next launch. |
| [ ] | Asset browser favourites and recent-asset shelf. |

---

## 5. Viewport System

The viewport is the editor's window into the running simulation. The
RHI swap chain blits into a `WorldViewportPanel`-owned surface and the
camera controller, ray-cast, and gizmo overlay all run inside the
panel.

| Status | Item |
|--------|------|
| [ ] | RHI swap-chain blit into `WorldViewportPanel` — present a render target into the panel without flickering on resize. |
| [ ] | Camera controller — orbit, fly, pan, focus-on-selection, ortho top / front / right snap. |
| [ ] | Selection ray-cast — pixel-accurate pick against scene proxies, with multi-select via marquee. |
| [x] | `TransformGizmo` data type — `Gizmos/TransformGizmo.h`. |
| [x] | `BoundsGizmo` — `Gizmos/BoundsGizmo.h`. |
| [x] | `CameraGizmo` — `Gizmos/CameraGizmo.h`. |
| [x] | `SplineGizmo` — `Gizmos/SplineGizmo.h`. |
| [ ] | Gizmo overlay renderer — translate / rotate / scale handles drawn on top of the viewport, snapping support, and undo-aware drag. |
| [ ] | Picking buffer cache — keep a per-frame picking image so repeated drags do not re-rasterise the whole scene. |
| [ ] | Multi-viewport mode — split horizontal / vertical / quad views with independent cameras and modes. |
| [ ] | Stats overlay — frame time, draw calls, visible entities, camera coordinates rendered in-corner. |

---

## 6. Inspector and Property Editing

The inspector turns a selection into editable widgets. The component
editor registry maps each component type to a section factory; numeric,
string, vector, enum, and asset-reference widgets are reusable across
every component.

| Status | Item |
|--------|------|
| [x] | `ComponentEditorRegistry` — type-keyed factory map. `InspectorEditing/ComponentEditorRegistry.h`. **Style debt — needs `/// @file`, section dividers, and the `m_` member prefix per `CodingStandards.md`.** |
| [x] | `InspectorBinder` — connects the active selection to the inspector panel. `InspectorEditing/InspectorBinder.h`. |
| [x] | `PropertyEditorFactory` — primitive-typed widget factory. `InspectorEditing/PropertyEditorFactory.h`. |
| [ ] | Numeric, string, vector, quaternion, colour, and enum widgets — uniform look, drag-to-modify on numeric fields, undo on commit. |
| [ ] | Asset reference picker — drop target plus search popup; respects `AssetKind` filtering. |
| [ ] | Multi-edit support — when several entities are selected, the inspector shows shared values, marks divergent fields, and writes the change to all. |
| [ ] | Component reorder, copy, paste, and reset-to-default. |
| [ ] | Custom inspector API — packages register a per-type editor through `IExtensionContext`. |

---

## 7. Visual Scripting

Visual scripting is the editor's block-authoring surface. Block authors
build graphs of typed nodes; the same gameplay can be authored as a C#
subset and round-trip between the two views. The IR compiler and the
runtime live in `SHARED_ROADMAP.md`; this section tracks the editor
surface only.

| Status | Item |
|--------|------|
| [x] | `GraphDocument` — graph data model (nodes, pins, connections, metadata). `VisualScripting/Graphs/GraphDocument.h`. |
| [x] | `GraphLayout` — layout engine for nodes. `VisualScripting/Graphs/GraphLayout.h`. |
| [x] | `GraphMetadata` — per-graph identity, version, dependency descriptor. `VisualScripting/Graphs/GraphMetadata.h`. |
| [x] | `GraphEditor` — root editor controller. `VisualScripting/Editor/GraphEditor.h`. |
| [x] | `GraphCanvas` — interactive canvas (zoom, pan, marquee, selection). `VisualScripting/Editor/GraphCanvas.h`. |
| [x] | `NodePalette` — searchable palette of registered nodes. `VisualScripting/Editor/NodePalette.h`. |
| [x] | `NodeWidgets` — node visual templates. `VisualScripting/Editor/NodeWidgets.h`. |
| [x] | `NodeGraphToolbar` — graph-view toolbar. `VisualScripting/Editor/NodeGraphToolbar.h`. |
| [x] | `BreakpointPanel` — graph-debugger breakpoint list. `VisualScripting/Editor/BreakpointPanel.h`. |
| [x] | `WatchPanel` — graph-debugger watch values. `VisualScripting/Editor/WatchPanel.h`. |
| [x] | `ExecutionOverlay` — live execution highlights on the graph. `VisualScripting/Editor/ExecutionOverlay.h`. |
| [x] | `CompilePreviewAdapter` — surfaces compiler output inside the canvas. `VisualScripting/Compiler/CompilePreviewAdapter.h`. |
| [x] | `ErrorSurfaceAdapter` — turns compiler diagnostics into pin-level markers. `VisualScripting/Compiler/ErrorSurfaceAdapter.h`. |
| [ ] | Graph editor shell — final wiring of zoom, pan, search, selection, grouping, collapse, and inline diagnostics into the workspace. |
| [ ] | Block authoring v1 — typed pins, node library, compile-to-IR, and graph validation surfaced through the editor. |
| [ ] | Text authoring v1 — C# subset editor that targets the same canonical IR as the block system. |
| [ ] | Block / C# synchronisation — supported gameplay subset moves between block view and text view without losing meaning. |
| [ ] | Editor diagnostics integration — graph errors, type mismatches, missing references, and runtime preview failures surfaced in-place. |
| [ ] | Node grouping and collapse-to-subgraph round-trip. |
| [ ] | Comment frames and rich annotation layer. |

---

## 7.5. Block Authoring (Scratch-style, inside Visual Scripting)

Block coding is **not** a separate subsystem — it lives inside
`VisualScripting/Blocks/` and feeds the same canonical IR the
pin-node editor feeds. The persona system (section 2.5) decides how
the canvas renders the same underlying data: stacked snap-together
blocks for `BlockyBeginner`, typed pin nodes for `ProDense`, both for
`Hybrid`. The data model below is the editor-facing block
representation; the IR lives in `SHARED_ROADMAP.md`.

### Design notes (Scratch parity)

The block authoring model intentionally mirrors Scratch's well-known
mental model so users coming from Scratch transfer skills directly:

- Programs are **stacks of blocks**. A stack starts at a **hat block**
  and runs top-to-bottom until a **cap block** or the end of the
  stack.
- Blocks have one of **seven shapes** that are visible at a glance —
  shape and only shape decides what can connect to what. The shape
  vocabulary is `Hat`, `Stack`, `C-block`, `E-block`, `Cap`,
  `Reporter`, `Boolean`. Inside the editor we call this concept
  `BlockKind` to avoid colliding with the visual `BlockShape` enum
  the persona system uses for canvas rendering style.
- Blocks belong to **eight built-in categories** (Motion, Looks,
  Sound, Events, Control, Sensing, Operators, Variables) plus a ninth
  user-authored category, **My Blocks**. Each category has a single
  brand colour. Extensions register additional categories.
- Blocks have **inline slots** that accept primitive literals
  (number, text, dropdown, colour) **or** Reporter / Boolean
  expressions. Slots are typed but the kid-friendly persona hides
  the type information; the pro persona shows it.
- C-blocks and E-blocks have **mouths** that hold a child stack each
  (one for `If`, two for `If…Else`, etc.).
- Custom blocks (Scratch's "My Blocks") let users define their own
  callable stacks with named parameters; under the hood these
  compile to user-defined IR procedures.

### Architecture

| Status | Item |
|--------|------|
| [ ] | `BlockKind` enum — `Hat`, `Stack`, `CBlock`, `EBlock`, `Cap`, `Reporter`, `Boolean`. Stable string ids per shape. |
| [ ] | `BlockCategory` value type — id, display name, brand colour, built-in factory functions for the eight standard categories. |
| [ ] | `BlockSlotKind` enum — `Number`, `Text`, `Boolean`, `Dropdown`, `Colour`, `Variable`, `Procedure`, `Statement`. |
| [ ] | `BlockSlot` — slot definition: id, label, kind, default literal, dropdown options, accepts-reporter flag. |
| [ ] | `BlockDefinition` — palette template: opcode, category, kind, label fragments interleaved with slots, output type for reporters. |
| [ ] | `BlockInstance` — placed block: opcode reference, per-slot fills (literal value or nested reporter), child stack handles for C/E mouths. |
| [ ] | `BlockStack` — ordered list of `BlockInstance` ids snapping top-to-bottom; the topmost instance is required to be a hat or a stack-with-no-predecessor. |
| [ ] | `BlockScript` — full program: a list of `BlockStack`s, each rooted at a hat block, plus a free-floating "shelf" stack for in-progress blocks the user has not yet attached. |
| [ ] | `BlockLibrary` — registry of `BlockDefinition`s. Built-in registration covers Motion / Looks / Sound / Events / Control / Sensing / Operators / Variables. Extensions register additional opcodes. |
| [ ] | Variables and lists — first-class data with global / sprite / lexical scope; the editor surfaces them in the Variables category. |
| [ ] | Custom blocks (My Blocks) — user-authored procedure definitions with named parameters; round-trip through the canonical IR. |
| [x] | Snap rules — kind-driven adjacency rules enforced at edit time. `BlockSnapRules.h/.cpp` defines `DropTargetKind` (EmptyCanvas / StackTop / StackBottom / StackBetween / Mouth / ExpressionSlot), the `BlockDropTarget` descriptor, fifteen-arm `SnapRejection` enum, and `CanSnap(opcode, target, script, library)` returning a `SnapVerdict`. Every Scratch invariant is enforced (Hat top-only, Cap end-only, expressions only into matching slots, dropdown / colour / statement slots reject expressions, variable getters polymorphic). |
| [x] | Block-to-IR lowering — `BlockToIRLowerer.h/.cpp` emits the canonical `IRProgram` (`BlockIR.h/.cpp`). One entry-point procedure per Hat block; Reporter expressions lower recursively into `IRValue::Call`; control-flow opcodes (`saga.block.control.repeat / .if / .if_else / .forever / .stop_all`) lower into branch / loop / stop statements; empty slots fall back to the definition's default literal. The IR is shared with the future C# subset parser (SHARED_ROADMAP §2). |
| [ ] | IR-to-block lifter — when a graph saved as pins is re-opened in `BlockyBeginner`, it lifts back into a `BlockScript` where the supported subset round-trips losslessly. |
| [ ] | Block palette panel — categorised, scrollable, supports drag-from-palette and the search-as-you-type filter. |
| [ ] | Inline literal editors — number / text / dropdown / colour / boolean toggles styled per the active persona's `BlockVisualStyle`. |

### Anchor Examples

The two anchor block stacks the engine commits to supporting on day
one are:

- **`when ▶ clicked → move 10 steps → turn 15 degrees → say "hi" for 2 seconds`**
  — single hat, three stack blocks, one literal-fill slot per stack.
  Compiles to a four-instruction IR sequence under one entry point.
- **`when key [space] pressed → repeat 10 { if <touching [edge]> { play sound [pop] } move 5 steps }`**
  — single hat, one C-block (`repeat`), one nested E-block-less if,
  one Reporter expression in the if's slot, one play-sound stack
  block, one trailing stack block. Exercises every shape except Cap
  and EBlock.

---

## 8. Scene, Prefab, and Project Editing

| Status | Item |
|--------|------|
| [x] | `SceneEditing/` directory — scaffolding for scene-level authoring (open, save, validate). |
| [x] | `PrefabEditing/` directory — scaffolding for prefab editing and overrides. |
| [x] | `ProjectManager` — project lifecycle. `Project/ProjectManager.h`. |
| [x] | `ProjectIndexer` — content-tree index for fast lookup. `Project/ProjectIndexer.h`. |
| [x] | `RecentProjects` — recent-project history. `Project/RecentProjects.h`. |
| [ ] | Scene document model — undoable mutations, dirty tracking, autosave. |
| [ ] | Prefab override pipeline — instance overrides, propagation rules, and revert-to-prefab. |
| [ ] | Multi-scene editing — load several scenes side-by-side without losing per-scene undo history. |
| [ ] | Project templates — starter content shipped with the editor (empty, third-person, top-down, MMO sandbox). |
| [ ] | Project upgrade tool — runs schema migrations when an older project opens in a newer editor. |

---

## 9. Asset Pipeline (editor side)

The runtime streaming and decoding layer lives in `ROADMAP.md`. The
editor side is the import, cook, validate, and preview flow.

| Status | Item |
|--------|------|
| [x] | `AssetImportManager` — orchestration entry point. `Importers/AssetImportManager.h`. |
| [x] | `AssetPreviewImporter` — produces lightweight previews for the asset browser. `Importers/AssetPreviewImporter.h`. |
| [x] | `BlockGraphImporter` — imports block-graph assets. `Importers/BlockGraphImporter.h`. |
| [x] | `TextScriptImporter` — imports text-script assets. `Importers/TextScriptImporter.h`. |
| [x] | `GraphImportAdapter` — bridges importers and the visual scripting model. `Importers/GraphImportAdapter.h`. |
| [x] | `ContentPipeline` — declarative pipeline definition. `Pipeline/ContentPipeline.h`. |
| [x] | `AssetCooking` — cooking-step definitions. `Pipeline/AssetCooking.h`. |
| [x] | `DerivedDataCache` — content-addressed cache for cooked artifacts. `Pipeline/DerivedDataCache.h`. |
| [ ] | Editor-driven mesh import — `.gltf` / `.glb` flow that calls the engine importer, runs validation, and produces a usable `Render::MeshAsset`. |
| [ ] | Editor-driven texture import — DDS / KTX2 flow with format hints, sRGB awareness, and platform-target overrides. |
| [ ] | Asset thumbnail generation — async, cached, regenerated on source change. |
| [ ] | Cook-on-save toggle — optional automatic cook of the selected asset whenever it is saved. |
| [ ] | Drag-and-drop import from OS file manager. |
| [ ] | Bulk reimport with progress reporting and cancel support. |

---

## 10. Validation and Serialisation

| Status | Item |
|--------|------|
| [x] | `Validation/` directory — scaffolding for project / asset / scene validators. |
| [x] | `Serialization/` directory — scaffolding for editor-side document serialisation. |
| [ ] | Validator registry — validators register a target type and a severity policy; results are surfaced in the console panel. |
| [ ] | Pre-save validation — block save when a fatal validator fails; warn-and-continue on non-fatal. |
| [ ] | Migration policy — assets opened from older project versions migrate automatically and emit a report. |
| [ ] | Round-trip serialisation tests — `serialize(deserialize(x)) == x` enforced for scenes, prefabs, graphs, and project metadata. |

---

## 11. Collaboration

The editor supports multi-user authoring sessions. Every collaboration
subsystem lives behind an interface so the transport, the conflict
resolver, and the presence model can be swapped per deployment.

| Status | Item |
|--------|------|
| [x] | Session lifecycle — `CollaborationSession`, `CollaborationToken`, `SessionHandshake`, `SessionHeartbeat`, `SessionId`, `SessionConfig`. `Collaboration/Session/`. |
| [x] | Client / server bridges — `CollaborationClient`, `ICollaborationClient`, `PeerInfo`, `PeerRegistry`, `CollaborationServerRouter`, `CollaborationServerState`, `ICollaborationServer`. `Collaboration/Client/`, `Collaboration/Server/`. |
| [x] | Authority model — `AuthorityManager`, `AuthorityElection`, `AuthorityHandoff`, `AuthorityMode`, `AuthorityEvent`. `Collaboration/Authority/`. |
| [x] | Lock manager — `LockManager`, `LockToken`, `LockHeartbeat`, `LockPolicy`, `LockEvent`, `LockConflictHandler`. `Collaboration/Locks/`. |
| [x] | Permissions — `PermissionManager`, `PermissionPolicy`, `PermissionEnforcer`, `CollaboratorRole`. `Collaboration/Permissions/`. |
| [x] | Presence — `PresenceManager`, `PresenceEvent`, `PresenceInfo`, `PresenceOverlayData`, `CursorBroadcast`, `SelectionBroadcast`. `Collaboration/Presence/`. |
| [x] | Sync transport — `SyncTransportImpl`, `ISyncTransport`, `SyncConfig`, `EditOperation`, `EditOperationQueue`, `OperationVector`, `OperationalTransform`, `CrdtSceneDocument`, `SyncFlushScheduler`. `Collaboration/Sync/`. |
| [x] | Conflict resolution — `ConflictResolutionPipeline`, `IConflictResolver`, `LastWriteWinsResolver`, `ServerAuthoritativeResolver`, `ThreeWayMerge`, `ConflictStrategy`, `EditConflict`. `Collaboration/ConflictResolution/`. |
| [x] | Merge strategies — `IMergeStrategy`, `HierarchyMerge`, `PropertyMerge`, `SceneMerge`, `MergeResult`. `Collaboration/Merge/`. |
| [x] | Replay and audit — `EditHistory`, `OperationLog`, `IOperationLog`, `OperationLogEntry`, `ReplayController`, `AuditLogger`, `IAuditLogger`, `AuditEvent`, `AuditEventType`, `AuditExporter`, `AuditFilter`. `Collaboration/Replay/`, `Collaboration/Audit/`. |
| [x] | Workspace document model — `CollaborativeWorkspace`, `ICollaborativeWorkspace`, `WorkspaceDocument`, `WorkspaceDocumentState`, `WorkspaceSnapshot`, `WorkspaceSyncPolicy`. `Collaboration/Workspace/`. |
| [ ] | Live cursor and selection overlays inside `WorldViewportPanel` and `GraphViewportPanel`. |
| [ ] | Comment threads and review-mode pass — anchored to entities, components, or graph nodes. |
| [ ] | Offline editing with deferred sync — local edits replay onto the server when the connection comes back. |
| [ ] | Permission revocation propagation — locks held by a revoked collaborator are released automatically. |
| [ ] | Session reconnect with state-resync proof — the client confirms its replayed log produces the same hash as the server before resuming. |

---

## 12. Extensions and Package SDK (editor side)

The editor exposes an extension surface so packages can add panels,
inspectors, menus, shortcuts, importers, and graph nodes. The runtime
side of the package format and the trust model lives in
`SHARED_ROADMAP.md`.

| Status | Item |
|--------|------|
| [x] | `IEditorExtension` — extension interface (`OnLoad` / `OnUnload`). `Extensions/IEditorExtension.h`. |
| [x] | `IExtensionContext` — panel / command / theme registration plus host access. `Extensions/IExtensionContext.h`. |
| [x] | `ExtensionRegistry` — extension catalog. `Extensions/ExtensionRegistry.h`. |
| [x] | `ExtensionHost` — owns loaded extensions and their lifetime. `Extensions/ExtensionHost.h`. |
| [x] | `ExtensionLoader` — discovery, manifest parsing, resolution. `Extensions/ExtensionLoader.h`. |
| [x] | `ExtensionManifest` — package manifest schema. `Extensions/ExtensionManifest.h`. |
| [x] | `ExtensionCommandBridge` and `IExtensionCommand` — extension-contributed commands. `Extensions/ExtensionCommandBridge.h`, `IExtensionCommand.h`. |
| [x] | `ExtensionPanelBridge` and `IExtensionPanel` — extension-contributed panels. `Extensions/ExtensionPanelBridge.h`, `IExtensionPanel.h`. |
| [x] | `ExtensionPoint` — declarative extension-point registration. `Extensions/ExtensionPoint.h`. |
| [x] | `IExtensionSerializer` — extension-side document serialisation hook. `Extensions/IExtensionSerializer.h`. |
| [ ] | Editor tool API completion — packages can add inspectors, menus, shortcuts, and graph tools through a single documented surface. |
| [ ] | Hot-reload of extensions during a session, with a clean unload path. |
| [ ] | Extension settings panel — packages can declare typed settings the user edits inside the editor. |
| [ ] | Sample extension — reference implementation packaged as an external project to exercise the API. |

---

## 13. Headless Editor Mode

| Status | Item |
|--------|------|
| [ ] | Headless `IUIApplication` backend — same `EditorApp` boot path with no Qt window, used for CI cooks and batch validation. |
| [ ] | Batch cook command — runs `ContentPipeline` over a project and exits with a structured status code. |
| [ ] | Batch validate command — runs every registered validator and emits a JSON report. |
| [ ] | Batch import command — pulls a manifest of source assets and produces cooked output. |
| [ ] | Determinism mode — disables timers, animations, and randomness so a CI run produces byte-identical output across machines. |

---

## Layout Map

```
Editor/
  include/SagaEditor/
    Collaboration/                 ← Multi-user editing stack (see section 11)
    Commands/
      CommandRegistry.h
      CommandDispatcher.h
      CommandPalette.h
      ShortcutManager.h
      UndoRedoStack.h
    Docking/
      DockWorkspace.h
      DockLayoutManager.h
      DockTab.h
    Extensions/
      IEditorExtension.h
      IExtensionContext.h
      ExtensionHost.h
      ExtensionRegistry.h
      ExtensionLoader.h
      ExtensionManifest.h
      ExtensionPoint.h
      ExtensionCommandBridge.h
      ExtensionPanelBridge.h
      IExtensionCommand.h
      IExtensionPanel.h
      IExtensionSerializer.h
    Gizmos/
      TransformGizmo.h
      BoundsGizmo.h
      CameraGizmo.h
      SplineGizmo.h
    Host/
      EditorApp.h
      EditorHost.h
    Importers/
      AssetImportManager.h
      AssetPreviewImporter.h
      BlockGraphImporter.h
      GraphImportAdapter.h
      TextScriptImporter.h
    InspectorEditing/
      ComponentEditorRegistry.h
      InspectorBinder.h
      PropertyEditorFactory.h
    Layouts/
      LayoutPreset.h
      LayoutSerializer.h
      WorkspacePreset.h
    Localization/                  ← String tables and locale fallback
    Panels/
      IPanel.h
      HierarchyPanel.h
      InspectorPanel.h
      ConsolePanel.h
      AssetBrowserPanel.h
      WorldViewportPanel.h
      GraphViewportPanel.h
      ProjectBrowserPanel.h
      CollaborationPanel.h
      ProfilerPanel.h
    Pipeline/
      ContentPipeline.h
      AssetCooking.h
      DerivedDataCache.h
    PrefabEditing/                 ← Prefab document model and overrides
    Project/
      ProjectManager.h
      ProjectIndexer.h
      RecentProjects.h
    SceneEditing/                  ← Scene document model and undo
    Selection/
      SelectionManager.h
    Serialization/                 ← Editor-side document serialisation
    Shell/
      EditorShell.h
      ShellCommands.h
      ShellLayout.h
      ShellWindow.h
    Themes/
      EditorTheme.h
      ColorPalette.h
      ThemeRegistry.h
    Toolbars/                      ← Toolbar customisation
    UI/
      IUIApplication.h
      IUIFactory.h
      IUIMainWindow.h
      IUIWidget.h
      Qt/                          ← Concrete Qt backend (only place QtWidgets is included)
    Validation/                    ← Project / asset / scene validators
    VisualScripting/
      Compiler/
        CompilePreviewAdapter.h
        ErrorSurfaceAdapter.h
      Debugger/                    ← Graph debugger UI hooks
      Editor/
        GraphEditor.h
        GraphCanvas.h
        NodeGraphToolbar.h
        NodePalette.h
        NodeWidgets.h
        BreakpointPanel.h
        WatchPanel.h
        ExecutionOverlay.h
      Graphs/
        GraphDocument.h
        GraphLayout.h
        GraphMetadata.h
      Imports/                     ← Graph import adapters
      Nodes/                       ← Built-in node library
      Pins/                        ← Typed pin system
      Preview/                     ← Inline preview rendering
      Runtime/                     ← Editor-side runtime stub for preview
      TypeSystem/                  ← Typed-pin type registry
  src/SagaEditor/                  ← Translation units mirror the include tree
```

---

## Definitions

- **Editor-only**: ships in the editor binary; never linked into a
  shipped game runtime. Examples: `EditorShell`, `CommandPalette`,
  `GraphCanvas`, `AssetImportManager`.
- **Shared**: linked into both the editor and the runtime. Tracked in
  `SHARED_ROADMAP.md`. Examples: the canonical IR, the C# host, the
  package manifest format, the security boundary policy.
- **Runtime-only**: never linked into the editor. Tracked in
  `ROADMAP.md`. Examples: replication, simulation tick, network
  transport, RHI command stream, asset streaming orchestrator.

---

## Where to Start (current focus)

1. Bring `InspectorEditing/ComponentEditorRegistry.h` and the rest of
   `InspectorEditing/` up to `CodingStandards.md` compliance.
2. Land the inspector primitive widgets (numeric, string, vector,
   quaternion, colour, enum) and wire them through
   `PropertyEditorFactory`.
3. Wire `ComponentEditorRegistry` into `InspectorPanel` so that
   selection changes rebuild the panel from registered editors.
4. Begin the viewport system (RHI swap-chain blit + camera controller)
   so the gizmo overlay has somewhere to draw.
