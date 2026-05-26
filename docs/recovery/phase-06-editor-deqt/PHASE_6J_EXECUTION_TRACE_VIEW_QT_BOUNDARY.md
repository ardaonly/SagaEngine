# Phase 6J ExecutionTraceView Qt Boundary

> Last updated: 2026-05-25
> Status: Phase 6J bounded implementation slice
> Phase 6: Editor Public API De-Qtification

Phase 6J closes Phase 6I honestly, audits the remaining visual scripting
`QtPanel` cluster, and migrates exactly one proven-safe debugger/trace view.
The selected target is `ExecutionTraceView`.

Full CTest remains unverified.

## Phase 6I Closure

Phase 6I is accepted as:

- second visual scripting leaf pimpl migration
- `BreakpointPanel` public Qt exposure removal
- repeated visual scripting `IPanel` + pimpl/private Qt widget pattern proof
- Phase 6B allowlist reduction

Phase 6I is not accepted as:

- broad visual scripting migration
- `ExecutionTraceView` migration
- `GraphDebuggerView` migration
- `NodePalette` migration
- `NodeLibraryPanel` migration
- `GraphCanvas` migration
- `QtPanel` removal
- `SagaEditorModule` API de-Qtification
- CMake Qt PUBLIC dependency cleanup

Completion evidence:

- `BreakpointPanel` public header is Qt-free.
- `BreakpointPanel` now inherits `IPanel`.
- Its private Qt implementation exists under
  `Editor/src/SagaEditor/UI/Qt/VisualScripting/Editor`.
- The `BreakpointPanel.h` allowlist entry was removed.
- The boundary guard reported 288 scanned public headers, 29 allowed legacy Qt
  exposure findings, and 0 violations.
- `SagaEditorLib`, `SagaArchitectureTests`, focused architecture/editor tests,
  and label inventory were run during Phase 6I.

Phase 6I proves:

- the `IPanel` + pimpl/private Qt widget pattern works for two visual scripting
  leaf panels
- safe leaf visual scripting panels can be migrated without model/debugger
  ownership changes
- Phase 6B allowlist reduction continues to work

Phase 6I does not prove:

- `ExecutionTraceView`, `GraphDebuggerView`, `NodePalette`, or
  `NodeLibraryPanel` are automatically safe
- `GraphCanvas` is ready for migration
- `QtPanel` itself is removable
- `SagaEditorModule` product ABI is clean
- CMake Qt PUBLIC visibility can change

## Remaining Cluster Inventory

| Surface | Public Qt exposure | Public API shape | Non-Qt dependencies | Implementation | Private Qt backend | Ownership state | Tests | Pattern | Risk | Safe now | Next action | Rollback |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `ExecutionTraceView.h` | `QtPanel` include/inheritance before Phase 6J | default constructor, panel id, title, init | none | stub | none before Phase 6J | no trace/session/debugger state in source | placeholder debugger tests only | `IPanel` + private Qt pimpl | Low-medium | Yes | migrate in Phase 6J | restore `QtPanel` header/source and allowlist entry |
| `GraphDebuggerView.h` | `QtPanel` include/inheritance | default constructor, panel id, title, init | none | stub | none | no source ownership, but graph/debugger role implies future coupling risk | placeholder debugger tests only | likely `IPanel` + private Qt pimpl | Medium | Not yet | debugger/model ownership audit | keep allowlist entry |
| `NodePalette.h` | `QtPanel` include/inheritance | default constructor, panel id, title, init | none | stub | none | no source ownership, but node catalog role implies model boundary risk | no direct panel test | likely `IPanel` + private Qt pimpl | Medium | Not yet | node/model ownership audit | keep allowlist entry |
| `NodeLibraryPanel.h` | `QtPanel` include/inheritance | `NodeCategoryBrowser&`, panel id, title, init | `NodeCategoryBrowser` | stub | none | direct browser reference | node model/import tests only | needs browser ownership decision before pimpl | Medium-high | No | `NodeCategoryBrowser` boundary audit | keep allowlist entry |
| `GraphCanvas.h` | `QtPanel` include/inheritance | `GraphDocument&`, panel id, title, init | `GraphDocument` | stub | none | direct graph document reference | graph model tests only | graph view contract split before Qt pimpl | High | No | defer for graph view/model plan | keep allowlist entry |
| `QtPanel.h` | `<QWidget>`, `Q_OBJECT`, `QWidget`, `QtPanel` | legacy Qt-backed panel base | `IPanel` | implemented | public Qt backend | root public Qt debt | boundary guard only | remove or privatize after all inheritors migrate | High | No | keep allowlisted | keep allowlist entry |
| `Apps/Saga/SagaEditorModule.h` | `QMainWindow`, `QStackedWidget` forward declarations | product editor activation API | product project/session types | implemented | product shell Qt API | product ABI leak | product/editor tests | separate mount abstraction plan | Medium-high | No | defer product ABI plan | keep allowlist entry |

Classification after inspection:

- A: `ExecutionTraceView`
- B: `GraphDebuggerView`, `NodePalette`
- C: `NodeLibraryPanel`
- D: `GraphCanvas`

## Candidate Decision

Phase 6J candidates:

- `ExecutionTraceView` ownership audit: low risk, useful, but current source
  already proves no debugger/session/model ownership.
- `ExecutionTraceView` pimpl migration: medium value and bounded because the
  type is default-constructed, dependency-free, and stub-level.
- `GraphDebuggerView` ownership audit: useful after trace cleanup, but should
  not precede the lower-risk trace view.
- `NodePalette` / `NodeLibraryPanel` node boundary audit: useful after
  debugger/trace direction is settled.
- `GraphCanvas` high-risk boundary plan: important, but explicitly deferred.
- `SagaEditorModule` product ABI plan: separate product boundary work.
- Visual scripting cluster checkpoint only: safe but too passive because
  `ExecutionTraceView` is migratable.

Decision: migrate `ExecutionTraceView` in Phase 6J. Do not add trace/debugger
behavior.

## Migration Result

Public ABI change:

- `Editor/include/SagaEditor/VisualScripting/Debugger/ExecutionTraceView.h` no
  longer includes `SagaEditor/UI/Qt/QtPanel.h`.
- `ExecutionTraceView` now inherits `IPanel` directly.
- The public header exposes no `QWidget`, `QObject`, `Q_OBJECT`, `QtPanel`, or
  Qt include.
- The public class name and default constructor are preserved.
- The header now exposes `void* GetNativeWidget() const noexcept`.

Private Qt implementation:

- `Editor/src/SagaEditor/UI/Qt/VisualScripting/Debugger/ExecutionTraceView.cpp`
  owns the private Qt widget through `ExecutionTraceView::Impl`.
- `GetNativeWidget()` returns the private `QWidget*` as `void*`.
- `GetPanelId()` returns `saga.panel.visual_scripting.execution_trace`.
- `GetTitle()` returns `Execution Trace`.
- `OnInit()` and `OnShutdown()` remain no-ops.
- The placeholder label text is `Execution Trace`.
- The old
  `Editor/src/SagaEditor/VisualScripting/Debugger/ExecutionTraceView.cpp` is a
  non-defining moved stub to avoid duplicate symbols.

Boundary guard update:

- `EditorQtPublicAbiBoundaryTests` removed only
  `Editor/include/SagaEditor/VisualScripting/Debugger/ExecutionTraceView.h`
  from the allowlist.
- `GraphDebuggerView`, `NodePalette`, `NodeLibraryPanel`, `GraphCanvas`,
  `QtPanel.h`, and `Apps/Saga/SagaEditorModule.h` remain allowlisted.

## Cluster Status After Phase 6J

- `WatchPanel`: migrated in Phase 6H
- `BreakpointPanel`: migrated in Phase 6I
- `ExecutionTraceView`: migrated in Phase 6J
- `GraphDebuggerView`: graph/debugger ownership audit required
- `NodePalette`: node/model ownership audit required
- `NodeLibraryPanel`: `NodeCategoryBrowser` boundary audit required
- `GraphCanvas`: high-risk `GraphDocument` view debt; defer
- `QtPanel.h`: remains public allowlisted debt
- `SagaEditorModule.h`: remains product ABI debt

## Non-Goals

Phase 6J does not:

- remove Qt
- remove or privatize `QtPanel`
- migrate the full visual scripting cluster
- migrate `GraphDebuggerView`, `NodePalette`, `NodeLibraryPanel`, or
  `GraphCanvas`
- change `SagaEditorModule`
- rewrite `EditorShell`
- redesign visual scripting graph document, graph runtime, debugging, node
  library, or editor shell ownership
- add trace/debugger/session/model behavior
- change graph runtime, breakpoint storage, watch controller, command routing,
  node library, or graph document semantics
- change `SagaEditorLib` Qt dependency visibility
- change runtime/client asset work
- change publish gates
- change collaboration or MultiplayerSandbox work
- prove full CTest health

## Verification

Verification completed for this slice:

- `git diff --check`
- targeted `rg` for `ExecutionTraceView`, `GraphDebuggerView`,
  `BreakpointPanel`, `WatchPanel`, `NodePalette`, `NodeLibraryPanel`,
  `GraphCanvas`, `QtPanel`, `EditorQtPublicAbiBoundary`, allowlist, `IPanel`,
  pimpl, and Phase 6J wording
- `Tools/Forge/bin/forge nix build --target SagaEditorLib --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target SagaArchitectureTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R ArchitectureTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R '^UnitTests$' --output-on-failure -j 1`
- CTest label inventory for `architecture`, `editor`, `ui`, `product`,
  `collaboration`, and `integration`

`EditorQtPublicAbiBoundaryTests` reported 288 scanned public headers, 26
allowed legacy Qt exposure findings, and 0 violations.

Label inventory reported architecture 2, editor 2, ui 0, product 2,
collaboration 0, and integration 3.

Recommended Phase 6K: perform a `GraphDebuggerView` debugger/model boundary
audit, with migration only if inspection proves the same stub-level,
no-ownership shape.
