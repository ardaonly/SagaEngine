# Phase 6K GraphDebuggerView Qt Boundary

> Last updated: 2026-05-26
> Status: Phase 6K bounded implementation slice
> Phase 6: Editor Public API De-Qtification

Phase 6K audits the next debugger-adjacent visual scripting public `QtPanel`
leak and migrates exactly one proven-safe class: `GraphDebuggerView`.

Full CTest remains unverified.

## Phase 6J Closure

Phase 6J is accepted as:

- `ExecutionTraceView` debugger/trace ownership audit
- `ExecutionTraceView` public Qt exposure removal
- repeated visual scripting `IPanel` + pimpl/private Qt widget pattern proof
- Phase 6B allowlist reduction

Phase 6J is not accepted as:

- `GraphDebuggerView` migration
- node UI migration
- `GraphCanvas` migration
- `QtPanel` removal
- `SagaEditorModule` API de-Qtification
- CMake Qt PUBLIC dependency cleanup
- full CTest health

## Ownership Audit

Current `GraphDebuggerView` evidence:

- Public API was only a default constructor, panel id, title, and init hook.
- The public header had no debugger, session, graph, trace, breakpoint, watch,
  node, or document references.
- The previous implementation was a non-functional stub.
- No current source path wires `GraphDebuggerView` to debugger/session/model
  ownership.
- Existing placeholder debugger tests do not prove future debugger behavior,
  but they also do not expose a current model contract that must be preserved.

Decision: `GraphDebuggerView` is safe for a bounded pimpl migration in this
slice. Do not add debugger behavior.

## Migration Result

Public ABI change:

- `Editor/include/SagaEditor/VisualScripting/Debugger/GraphDebuggerView.h` no
  longer includes `SagaEditor/UI/Qt/QtPanel.h`.
- `GraphDebuggerView` now inherits `IPanel` directly.
- The public header exposes no `QWidget`, `QObject`, `Q_OBJECT`, `QtPanel`, or
  Qt include.
- The public class name and default constructor are preserved.
- The header now exposes `void* GetNativeWidget() const noexcept`.

Private Qt implementation:

- `Editor/src/SagaEditor/UI/Qt/VisualScripting/Debugger/GraphDebuggerView.cpp`
  owns the private Qt widget through `GraphDebuggerView::Impl`.
- `GetNativeWidget()` returns the private `QWidget*` as `void*`.
- `GetPanelId()` returns `saga.panel.visual_scripting.graph_debugger`.
- `GetTitle()` returns `Graph Debugger`.
- `OnInit()` and `OnShutdown()` remain no-ops.
- The placeholder label text is `Graph Debugger`.
- The old
  `Editor/src/SagaEditor/VisualScripting/Debugger/GraphDebuggerView.cpp` is a
  non-defining moved stub to avoid duplicate symbols.

Boundary guard update:

- `EditorQtPublicAbiBoundaryTests` removed only
  `Editor/include/SagaEditor/VisualScripting/Debugger/GraphDebuggerView.h`
  from the allowlist.
- `NodePalette`, `NodeLibraryPanel`, `GraphCanvas`, `QtPanel.h`, and
  `Apps/Saga/SagaEditorModule.h` remain allowlisted.

## Cluster Status After Phase 6K

- `WatchPanel`: migrated in Phase 6H
- `BreakpointPanel`: migrated in Phase 6I
- `ExecutionTraceView`: migrated in Phase 6J
- `GraphDebuggerView`: migrated in Phase 6K
- `NodePalette`: node/model ownership audit required
- `NodeLibraryPanel`: `NodeCategoryBrowser` boundary audit required
- `GraphCanvas`: high-risk `GraphDocument` view debt; defer
- `QtPanel.h`: remains public allowlisted debt
- `SagaEditorModule.h`: remains product ABI debt

## Non-Goals

Phase 6K does not:

- remove Qt
- remove or privatize `QtPanel`
- migrate `NodePalette`, `NodeLibraryPanel`, or `GraphCanvas`
- change `SagaEditorModule`
- rewrite `EditorShell`
- redesign visual scripting graph document, graph runtime, debugging, node
  library, or editor shell ownership
- add debugger/session/model behavior
- change breakpoint storage, watch controller, trace handling, command routing,
  node library, or graph document semantics
- change `SagaEditorLib` Qt dependency visibility
- change runtime/client asset work
- change publish gates
- change collaboration or MultiplayerSandbox work
- prove full CTest health

## Verification

Verification completed for this slice:

- `git diff --check`
- targeted `rg` for `GraphDebuggerView`, `ExecutionTraceView`,
  `BreakpointPanel`, `WatchPanel`, `NodePalette`, `NodeLibraryPanel`,
  `GraphCanvas`, `QtPanel`, `EditorQtPublicAbiBoundary`, allowlist, `IPanel`,
  pimpl, and Phase 6K wording
- `Tools/Forge/bin/forge nix build --target SagaEditorLib --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target SagaArchitectureTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R ArchitectureTests --output-on-failure -j 1`

Recommended Phase 6L: perform a `NodePalette` node/model boundary audit, with
migration only if inspection proves the same stub-level, no-ownership shape.
