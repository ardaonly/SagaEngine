# Phase 6C QtPanel Public ABI Replacement Plan

> Last updated: 2026-05-25
> Status: Phase 6C decision checkpoint
> Phase 6: Editor Public API De-Qtification

Phase 6C closes Phase 6B honestly and decides the replacement pattern for
public `QtPanel` inheritance. It is a docs-only planning slice. No Qt removal,
panel migration, inheritance change, `EditorShell` behavior change, or
`SagaEditorModule` API change is included.

Full CTest remains unverified.

## Phase 6B Closure

Phase 6B is accepted as:

- Editor public header Qt exposure boundary guard
- deterministic public Qt exposure scanner
- allowlisted baseline for known public Qt leaks
- architecture/editor CTest enforcement

Phase 6B is not accepted as:

- Qt removal
- QtPanel public ABI removal
- panel migration
- visual scripting UI migration
- `SagaEditorModule` API de-Qtification
- CMake Qt PUBLIC dependency cleanup
- editor test split

Full CTest remains unverified.

Completion evidence:

- `Tests/Unit/Architecture/EditorQtPublicAbiBoundaryTests.cpp` exists.
- `cmake/modules/SagaTests.cmake` registers `EditorQtPublicAbiBoundaryTests`.
- The focused CTest entry is labelled `unit;architecture;editor`.
- The allowlist covers the current Phase 6A public leaks:
  `QtPanel.h`, four public editor panel headers, seven visual scripting
  panel/view headers, and `Apps/Saga/SagaEditorModule.h`.
- New non-allowlisted public Qt exposure fails the guard.
- Allowed leaks are reported separately from violations.
- `SagaArchitectureTests` and the focused guard passed during Phase 6B
  verification.
- Phase 6A, Phase 6B, roadmap, and test-suite docs were updated.

What Phase 6B proves:

- Public Qt leak growth is guarded.
- Public Qt exposure is measurable and intentional through an exact allowlist.
- Phase 6 can begin migration work without silently expanding Qt public ABI.

What Phase 6B does not prove:

- No `QtPanel` replacement exists yet.
- No panel class has migrated away from `QtPanel`.
- No visual scripting panel or view has migrated.
- `Apps/Saga/SagaEditorModule.h` still exposes product Qt activation ABI.
- `SagaEditorLib` still publishes `Qt6::Core` and `Qt6::Widgets` while public
  headers expose Qt.

## QtPanel Root Cause

`QtPanel` is the root public leak because it lives under
`Editor/include/SagaEditor/UI/Qt/QtPanel.h`, includes `<QWidget>`, uses
`Q_OBJECT`, inherits `QWidget`, and also implements `IPanel`. Any public header
that inherits `QtPanel` becomes a Qt-shaped public ABI surface.

Public `QWidget` inheritance is dangerous because consumers must compile
against Qt headers, respect QObject inheritance layout, and accept Qt widget
lifetime and moc constraints as part of the Editor ABI. `Q_OBJECT` makes this
stronger: it requires moc-shaped metadata and makes a UI backend detail visible
to any consumer of the public header.

The current inheritance chain also explains the CMake boundary:
`SagaEditorLib` cannot safely move `Qt6::Core` and `Qt6::Widgets` from
`PUBLIC` to `PRIVATE` while public headers expose `QWidget`, `Q_OBJECT`, and
`QtPanel`.

`QtPanel` currently provides:

- `IPanel` implementation through a Qt widget object.
- `GetNativeWidget()` returning `this` as a backend-owned native handle.
- A convenient base for Qt-only panels.

The useful public contract is already covered by `IPanel`:

- stable `PanelId`
- human-readable title
- native widget handle as `void*`
- `OnInit`, `OnShutdown`, `OnFocusGained`, and `OnFocusLost`

The replacement should provide no new required public base in Phase 6C.
Public panels should implement `IPanel` directly, own a private `Impl`, and
let the Qt implementation allocate and expose the native `QWidget*` through
`GetNativeWidget()`. This is already proven by clean pimpl panels such as
`ProductionDashboardPanel`, `CustomizeWorkspacePanel`,
`ShortcutPreferencesPanel`, `HierarchyPanel`, `InspectorPanel`,
`ConsolePanel`, `AssetBrowserPanel`, `ProblemsPanel`, and
`WorldViewportPanel`.

What remains in Qt implementation:

- `QWidget`, QObject inheritance, `Q_OBJECT`, moc, layouts, actions, menus, and
  concrete widget trees
- casts from `void*` to `QWidget*`
- private panel widget ownership under `Editor/src/SagaEditor/UI/Qt/Panels/**`
- eventual private or adapter-only use of `QtPanel`

What moves into backend-neutral public contracts:

- panel identity and title
- lifecycle hooks
- layout/docking intent through `UIDockArea`
- shell registration through `EditorShell::RegisterPanel(std::unique_ptr<IPanel>)`

Deferred:

- removing or relocating `QtPanel`
- `SagaEditorModule` product ABI replacement
- `SagaEditorLib` Qt visibility cleanup
- visual scripting graph surface redesign

## Replacement Designs

| Design | Goal | Public API shape | Private Qt shape | Complexity | Compatibility | Tests | Risk | Value | Decision |
|---|---|---|---|---|---|---|---|---|---|
| A. Backend-neutral `IPanel` migration | Remove public Qt inheritance using existing contracts. | Public panels inherit `IPanel`, use pimpl, expose `void*` native handle. | Qt widget lives in `Editor/src/SagaEditor/UI/Qt/Panels/**`; `QtPanel` becomes legacy/private later. | Medium | Source break only for code that relied on concrete `QtPanel` inheritance; intended public API becomes cleaner. | Boundary guard, focused panel tests, editor shell fake-window tests. | Medium | High | Chosen. |
| B. New public `EditorPanelBase` | Add default non-Qt panel base if `IPanel` is too sparse. | Public panels inherit new non-Qt base. | Qt widgets still pimpl/private. | Medium-low | Adds new API surface and possible duplication. | Contract tests if behavior is added. | Medium-low | Medium | Defer; `IPanel` is sufficient now. |
| C. Public `PanelHandle` / `PanelDescriptor` | Stop treating public panel classes as widgets. | Panels expose descriptors/handles; shell owns backend implementation. | Backend creates widgets from descriptors. | Medium-high | Larger shell/registry redesign. | Broad shell, layout, extension, composition tests. | Medium-high | High long term | Defer. |
| D. Keep panel names but pimpl QWidget | Preserve concrete panel class names while removing `QtPanel`. | Public concrete panel implements `IPanel` directly. | Private `Impl` owns `QWidget*`. | Medium | Best incremental source compatibility for constructors/names. | Same as A. | Medium | High | Use as migration technique under A. |
| E. Relocate `QtPanel` private now | Remove `QtPanel` from public include root. | No public `QtPanel`. | Private Qt base only. | High now | Breaks current allowlisted headers immediately. | Broad editor build and guard update. | High | High later | Defer until no public header inherits it. |
| F. Public contract plus private `QtPanelAdapter` | Add adapter between `IPanel` and Qt widget hosting. | Public panels implement contract only. | Private adapter wraps/hosts widget. | Medium | Additive after design is proven. | Adapter tests plus one migrated panel. | Medium | Medium-high | Defer until first pimpl migration proves adapter need. |

Rollback for Phase 6C is removing this document and the two docs references.
No code behavior changes are introduced.

## Migration Inventory

| Path | Current public Qt exposure | Role | Replacement pattern | Difficulty | Dependencies | Tests needed | Safe order | Phase 6C action |
|---|---|---|---|---|---|---|---|---|
| `Editor/include/SagaEditor/UI/Qt/QtPanel.h` | `<QWidget>`, `QWidget`, `Q_OBJECT`, `QtPanel` | Legacy Qt panel base | Keep until all users migrate, then move private or delete | High | All allowlisted inheritors | Boundary guard and full editor consumer build | 6 | Defer |
| `Editor/include/SagaEditor/Panels/GraphViewportPanel.h` | `QtPanel` include/inheritance | Editor graph viewport stub | `IPanel` + private Qt pimpl | Low-medium | None visible beyond panel shell registration | Boundary guard, editor shell fake-window or focused panel build | 1 | Defer to 6D |
| `Editor/include/SagaEditor/Panels/ProfilerPanel.h` | `QtPanel` include/inheritance | Editor profiler stub | `IPanel` + private Qt pimpl | Low-medium | None visible | Same as above | 2 | Defer |
| `Editor/include/SagaEditor/Panels/ProjectBrowserPanel.h` | `QtPanel` include/inheritance | Project browser stub | `IPanel` + private Qt pimpl | Low-medium | Product/project model may follow later | Same as above | 2 | Defer |
| `Editor/include/SagaEditor/Panels/CollaborationPanel.h` | `QtPanel` include/inheritance | Collaboration UI panel stub | `IPanel` + private Qt pimpl | Medium | Collaboration service boundary | Collaboration/editor panel smoke after migration | 3 | Defer |
| `Editor/include/SagaEditor/VisualScripting/Debugger/ExecutionTraceView.h` | `QtPanel` include/inheritance | Visual scripting debugger view | `IPanel` + private Qt pimpl | Medium | Visual scripting debugger state | Visual scripting/editor panel tests | 4 | Defer |
| `Editor/include/SagaEditor/VisualScripting/Debugger/GraphDebuggerView.h` | `QtPanel` include/inheritance | Visual scripting debugger view | `IPanel` + private Qt pimpl | Medium | Visual scripting graph/debugger state | Visual scripting/editor panel tests | 4 | Defer |
| `Editor/include/SagaEditor/VisualScripting/Editor/WatchPanel.h` | `QtPanel` include/inheritance | Watch panel | `IPanel` + private Qt pimpl | Medium | Watch controller | Visual scripting/editor panel tests | 4 | Defer |
| `Editor/include/SagaEditor/VisualScripting/Editor/BreakpointPanel.h` | `QtPanel` include/inheritance | Breakpoint panel | `IPanel` + private Qt pimpl | Medium | Breakpoint controller | Visual scripting/editor panel tests | 4 | Defer |
| `Editor/include/SagaEditor/VisualScripting/Editor/NodePalette.h` | `QtPanel` include/inheritance | Node palette | `IPanel` + private Qt pimpl | Medium | Node library/category data | Visual scripting/editor panel tests | 4 | Defer |
| `Editor/include/SagaEditor/VisualScripting/Nodes/NodeLibraryPanel.h` | `QtPanel` include/inheritance | Node library panel | `IPanel` + private Qt pimpl | Medium | `NodeCategoryBrowser&` | Visual scripting/editor panel tests | 4 | Defer |
| `Editor/include/SagaEditor/VisualScripting/Editor/GraphCanvas.h` | `QtPanel` include/inheritance | Graph canvas with `GraphDocument&` | Split graph view contract, then pimpl Qt canvas | Medium-high | `GraphDocument`, graph editor surface | Graph model/view tests plus boundary guard | 5 | Defer |
| `Apps/Saga/SagaEditorModule.h` | `QMainWindow`, `QStackedWidget` forward declarations | Product editor activation API | Separate product mount contract or private Qt adapter | Medium-high | Product shell and same-process editor mode | Product/editor mode tests | 7 | Defer |

Recommended migration order:

1. Migrate one simple non-visual-scripting stub panel first:
   `GraphViewportPanel` unless implementation inspection selects
   `ProfilerPanel` as lower risk.
2. Migrate remaining simple editor panel stubs:
   `ProfilerPanel` and `ProjectBrowserPanel`.
3. Migrate `CollaborationPanel` after confirming service/UI ownership.
4. Migrate visual scripting leaf panels/views:
   `ExecutionTraceView`, `GraphDebuggerView`, `WatchPanel`,
   `BreakpointPanel`, `NodePalette`, and `NodeLibraryPanel`.
5. Migrate `GraphCanvas` after the graph view/model boundary is explicit.
6. Remove or privatize `QtPanel` after no public header inherits/includes it.
7. Replace `SagaEditorModule` product Qt activation ABI in a separate product
   boundary slice.
8. Audit `SagaEditorLib` Qt `PUBLIC` visibility only after the public headers
   are clean.

## Phase 6C Slice Decision

| Candidate | Goal | Bundle contents | Behavior | Tests | Risk | Value | Decision |
|---|---|---|---|---|---|---|---|
| A. QtPanel Public ABI Replacement Plan | Decide replacement and migration order. | Architecture docs plus roadmap/notes updates. | Docs-only | `git diff --check`, targeted `rg`, label inventory, optional guard run | Low | High | Chosen |
| B. Backend-neutral `EditorPanelBase` or descriptor | Add a new migration target. | New public header and tests. | Additive API | Build and focused tests | Medium-low | Medium | Defer because `IPanel` is enough |
| C. First low-risk panel pimpl migration | Prove the pattern in code. | One panel header/source migration. | Behavior-preserving code change | Build, guard, focused editor tests | Medium | High | Defer to 6D |
| D. Visual scripting QtPanel dependency plan | Plan cluster cleanup. | Docs-only visual scripting plan. | Docs-only | Docs checks | Low | Medium | Defer until base panel pattern is applied once |
| E. `SagaEditorModule` product ABI plan | Isolate product Qt API debt. | Docs-only product boundary plan. | Docs-only | Product label inventory | Low | Medium | Defer |
| F. QtPanel private adapter skeleton | Prepare private adapter. | Private Qt adapter scaffold. | Additive code | Build and guard | Medium | Medium | Defer until adapter need is proven |

Phase 6C chooses Candidate A.

Recommended Phase 6D: First Low-Risk Panel pimpl Migration.

Default Phase 6D target: `GraphViewportPanel`, with `ProfilerPanel` as fallback
if implementation inspection shows it is simpler. Phase 6D must migrate exactly
one panel, update the Phase 6B allowlist, and prove the guard fails on no new
Qt exposure.

## Non-Goals

Phase 6C does not:

- remove Qt
- remove or privatize `QtPanel`
- migrate any panel
- migrate visual scripting UI
- change `SagaEditorModule`
- change `EditorShell`
- change `SagaEditorLib` Qt dependency visibility
- split editor tests
- rewrite SDE customization
- rewrite collaboration UI
- implement UI/document `AssetKind`
- change runtime/client asset work
- change publish gates
- change MultiplayerSandbox
- prove full CTest health

## Verification

Planned verification for this docs-only slice:

- `git diff --check`
- targeted `rg` for Phase 6C wording, replacement decision, migration order,
  non-goals, and Phase 6D recommendation
- CTest label inventory for `architecture`, `editor`, `ui`, `product`,
  `collaboration`, and `integration`
- optional cheap guard regression:
  `SagaArchitectureTests --gtest_filter=EditorQtPublicAbiBoundaryTests.*`

Full CTest remains unverified.
