# Phase 6H Visual Scripting QtPanel Cluster

> Last updated: 2026-05-25
> Status: Phase 6H bounded implementation slice
> Phase 6: Editor Public API De-Qtification

Phase 6H closes Phase 6G honestly, inventories the visual scripting public
`QtPanel` cluster, and migrates exactly one safe leaf panel. The selected
panel is `WatchPanel`.

Full CTest remains unverified.

## Phase 6G Closure

Phase 6G is accepted as:

- CollaborationPanel Qt boundary audit and pimpl migration
- simple editor-panel cluster public Qt exposure cleanup
- repeated `IPanel` + pimpl/private Qt widget pattern proof
- Phase 6B allowlist reduction

Phase 6G is not accepted as:

- visual scripting migration
- `QtPanel` removal
- `SagaEditorModule` API de-Qtification
- CMake Qt PUBLIC dependency cleanup
- collaboration service rewrite

Full CTest remains unverified.

Completion evidence:

- `CollaborationPanel` public header is Qt-free.
- `CollaborationPanel` now inherits `IPanel`.
- Its private Qt implementation exists under
  `Editor/src/SagaEditor/UI/Qt/Panels`.
- The `CollaborationPanel.h` allowlist entry was removed.
- The simple migrated panel set is `GraphViewportPanel`, `ProfilerPanel`,
  `ProjectBrowserPanel`, and `CollaborationPanel`.
- The boundary guard reported 288 scanned public headers, 35 allowed legacy Qt
  exposure findings, and 0 violations.
- `SagaEditorLib`, `SagaArchitectureTests`, focused architecture tests, and
  `UnitTests` passed during Phase 6G verification.

Phase 6G proves:

- the `IPanel` + pimpl pattern works across the simple editor-panel cluster
- collaboration panel migration did not require collaboration service ownership
  changes
- public Qt exposure can be reduced incrementally with guard enforcement

Phase 6G does not prove:

- visual scripting headers are clean
- `QtPanel` itself is removable
- `SagaEditorModule` product ABI is clean
- CMake Qt PUBLIC visibility can change

## Visual Scripting Cluster Inventory

| Header | Current public Qt exposure | Class role | Dependencies | Implementation | Owns graph/debugger/node state | Leaf/simple UI | Tests | Risk | Action |
|---|---|---|---|---|---|---|---|---|---|
| `ExecutionTraceView.h` | `QtPanel` include/inheritance | Execution trace view | Debugger/trace semantics by role | Stub | Not in source, but trace ownership is implied | Partial | Placeholder debugger tests only | Medium | B: audit trace ownership before migration |
| `GraphDebuggerView.h` | `QtPanel` include/inheritance | Graph debugger view | Graph/debugger semantics by role | Stub | Not in source, but debugger ownership is implied | Partial | Placeholder debugger tests only | Medium | B/C: audit and test seam first |
| `WatchPanel.h` | `QtPanel` include/inheritance only | Debugger watch leaf panel | None beyond panel shell | Stub | No | Yes | Placeholder debugger tests only | Low-medium | A: migrate first |
| `BreakpointPanel.h` | `QtPanel` include/inheritance only | Breakpoint leaf panel | None beyond panel shell | Stub | No | Yes | Placeholder debugger tests only | Low-medium | A: next migration candidate |
| `GraphCanvas.h` | `QtPanel` include/inheritance | Graph core view | `GraphDocument&` | Stub | Yes, graph document reference | No | Graph model tests only | High | D: defer |
| `NodePalette.h` | `QtPanel` include/inheritance only | Node palette UI | Node model semantics by role | Stub | Not in source, but node catalog ownership is implied | Partial | No direct tests | Medium | B: audit node palette/model boundary |
| `NodeLibraryPanel.h` | `QtPanel` include/inheritance | Node library panel | `NodeCategoryBrowser&` | Stub | Yes, node browser reference | No | Node model tests only | Medium-high | B: audit browser ownership before migration |

Decision: choose first visual scripting leaf panel pimpl migration because
`WatchPanel` is clearly stub-level and has no public graph, debugger, or node
model dependency. Do not migrate `GraphCanvas` or the full cluster.

## Migration Result

Public ABI change:

- `Editor/include/SagaEditor/VisualScripting/Editor/WatchPanel.h` no longer
  includes `SagaEditor/UI/Qt/QtPanel.h`.
- `WatchPanel` now inherits `IPanel` directly.
- The public header exposes no `QWidget`, `QObject`, `Q_OBJECT`, `QtPanel`, or
  Qt include.
- The public class name and default constructor are preserved.
- The header now exposes the backend-neutral native handle method:
  `void* GetNativeWidget() const noexcept`.

Private Qt implementation:

- `Editor/src/SagaEditor/UI/Qt/VisualScripting/Editor/WatchPanel.cpp` owns the
  private Qt widget through `WatchPanel::Impl`.
- `GetNativeWidget()` returns the private `QWidget*` as `void*`.
- `GetPanelId()` returns `saga.panel.visual_scripting.watch`.
- `GetTitle()` returns `Watch`.
- `OnInit()` and `OnShutdown()` remain no-ops.
- The old `Editor/src/SagaEditor/VisualScripting/Editor/WatchPanel.cpp` is a
  non-defining moved stub to avoid duplicate symbols.

Boundary guard update:

- `EditorQtPublicAbiBoundaryTests` removed only
  `Editor/include/SagaEditor/VisualScripting/Editor/WatchPanel.h` from the
  allowlist.
- All other visual scripting public Qt leaks remain allowlisted.
- `QtPanel.h` remains temporary allowlisted debt.

## Remaining Public Qt Leaks

Still allowlisted after Phase 6H:

- `Editor/include/SagaEditor/UI/Qt/QtPanel.h`
- `Editor/include/SagaEditor/VisualScripting/Debugger/ExecutionTraceView.h`
- `Editor/include/SagaEditor/VisualScripting/Debugger/GraphDebuggerView.h`
- `Editor/include/SagaEditor/VisualScripting/Editor/BreakpointPanel.h`
- `Editor/include/SagaEditor/VisualScripting/Editor/GraphCanvas.h`
- `Editor/include/SagaEditor/VisualScripting/Editor/NodePalette.h`
- `Editor/include/SagaEditor/VisualScripting/Nodes/NodeLibraryPanel.h`
- `Apps/Saga/SagaEditorModule.h`

## Non-Goals

Phase 6H does not:

- remove Qt
- remove or privatize `QtPanel`
- migrate the full visual scripting cluster
- migrate `GraphCanvas`
- change `SagaEditorModule`
- rewrite `EditorShell`
- redesign visual scripting graph document, graph runtime, debugging, node
  library, or editor shell ownership
- change `SagaEditorLib` Qt dependency visibility
- rewrite collaboration services
- implement UI/document `AssetKind`
- change runtime/client asset work
- change publish gates
- change MultiplayerSandbox
- prove full CTest health

## Verification

Verification completed for this slice:

- `Tools/Forge/bin/forge nix build --target SagaEditorLib --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target SagaArchitectureTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R ArchitectureTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R '^UnitTests$' --output-on-failure -j 1`
- `git diff --check`
- targeted `rg` for `WatchPanel`, visual scripting cluster, `QtPanel`,
  architecture allowlist, Phase 6H, `IPanel`, and pimpl wording
- CTest label inventory for `architecture`, `editor`, `ui`, `product`,
  `collaboration`, and `integration`

`EditorQtPublicAbiBoundaryTests` reported 288 scanned public headers, 32
allowed legacy Qt exposure findings, and 0 violations.

No focused behavior test was added. Constructing the migrated panel creates a
Qt widget and the current safe unit seams do not provide a narrow
QApplication-backed panel construction target.

Recommended Phase 6I: migrate `BreakpointPanel` with the same pattern if Phase
6H verification stays clean; otherwise stop for a debugger/watch model boundary
audit.
