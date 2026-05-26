# Phase 6L NodePalette Qt Boundary

> Last updated: 2026-05-26
> Status: Phase 6L bounded implementation slice
> Phase 6: Editor Public API De-Qtification

Phase 6L audits the node palette/model boundary and migrates exactly one
proven-safe public `QtPanel` leak: `NodePalette`.

Full CTest remains unverified.

## Phase 6K Closure

Phase 6K is accepted as:

- `GraphDebuggerView` debugger/model ownership audit
- `GraphDebuggerView` public Qt exposure removal
- repeated visual scripting `IPanel` + pimpl/private Qt widget pattern proof
- Phase 6B allowlist reduction

Phase 6K is not accepted as:

- node UI migration
- `NodeLibraryPanel` migration
- `GraphCanvas` migration
- `QtPanel` removal
- `SagaEditorModule` API de-Qtification
- CMake Qt PUBLIC dependency cleanup
- full CTest health

## Ownership Audit

Current `NodePalette` evidence:

- Public API was only a default constructor, panel id, title, and init hook.
- The public header had no node category, node library, graph document, command,
  model, or editor selection references.
- The previous implementation was a non-functional stub.
- No current source path wires `NodePalette` to node catalog/model ownership.
- No direct panel tests exist, but no current node/model contract is exposed by
  the panel.

Decision: `NodePalette` is safe for a bounded pimpl migration in this slice.
Do not add node catalog behavior.

## Migration Result

Public ABI change:

- `Editor/include/SagaEditor/VisualScripting/Editor/NodePalette.h` no longer
  includes `SagaEditor/UI/Qt/QtPanel.h`.
- `NodePalette` now inherits `IPanel` directly.
- The public header exposes no `QWidget`, `QObject`, `Q_OBJECT`, `QtPanel`, or
  Qt include.
- The public class name and default constructor are preserved.
- The header now exposes `void* GetNativeWidget() const noexcept`.

Private Qt implementation:

- `Editor/src/SagaEditor/UI/Qt/VisualScripting/Editor/NodePalette.cpp` owns the
  private Qt widget through `NodePalette::Impl`.
- `GetNativeWidget()` returns the private `QWidget*` as `void*`.
- `GetPanelId()` returns `saga.panel.visual_scripting.node_palette`.
- `GetTitle()` returns `Node Palette`.
- `OnInit()` and `OnShutdown()` remain no-ops.
- The placeholder label text is `Node Palette`.
- The old `Editor/src/SagaEditor/VisualScripting/Editor/NodePalette.cpp` is a
  non-defining moved stub to avoid duplicate symbols.

Boundary guard update:

- `EditorQtPublicAbiBoundaryTests` removed only
  `Editor/include/SagaEditor/VisualScripting/Editor/NodePalette.h` from the
  allowlist.
- `NodeLibraryPanel`, `GraphCanvas`, `QtPanel.h`, and
  `Apps/Saga/SagaEditorModule.h` remain allowlisted.

## Cluster Status After Phase 6L

- `WatchPanel`: migrated in Phase 6H
- `BreakpointPanel`: migrated in Phase 6I
- `ExecutionTraceView`: migrated in Phase 6J
- `GraphDebuggerView`: migrated in Phase 6K
- `NodePalette`: migrated in Phase 6L
- `NodeLibraryPanel`: `NodeCategoryBrowser` boundary audit required
- `GraphCanvas`: high-risk `GraphDocument` view debt; defer
- `QtPanel.h`: remains public allowlisted debt
- `SagaEditorModule.h`: remains product ABI debt

## Non-Goals

Phase 6L does not:

- remove Qt
- remove or privatize `QtPanel`
- migrate `NodeLibraryPanel` or `GraphCanvas`
- change `SagaEditorModule`
- rewrite `EditorShell`
- redesign visual scripting graph document, graph runtime, debugging, node
  library, or editor shell ownership
- add node palette, node catalog, model, or command behavior
- change node import, node category browser, graph document, or graph canvas
  semantics
- change `SagaEditorLib` Qt dependency visibility
- change runtime/client asset work
- change publish gates
- prove full CTest health

## Verification

Verification completed for this slice:

- `git diff --check`
- targeted `rg` for `NodePalette`, `NodeLibraryPanel`, `GraphCanvas`,
  `QtPanel`, `EditorQtPublicAbiBoundary`, allowlist, `IPanel`, pimpl, and
  Phase 6L wording
- `Tools/Forge/bin/forge nix build --target SagaEditorLib --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target SagaArchitectureTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R ArchitectureTests --output-on-failure -j 1`

Recommended Phase 6M: audit `NodeLibraryPanel` and `NodeCategoryBrowser`, with
migration only if the browser reference can be preserved without changing node
library ownership.
