# Phase 6M NodeLibraryPanel Qt Boundary

> Last updated: 2026-05-26
> Status: Phase 6M bounded implementation slice
> Phase 6: Editor Public API De-Qtification

Phase 6M audits the `NodeLibraryPanel` / `NodeCategoryBrowser` boundary and
migrates exactly one public `QtPanel` leak: `NodeLibraryPanel`.

Full CTest remains unverified.

## Phase 6L Closure

Phase 6L is accepted as:

- `NodePalette` node/model ownership audit
- `NodePalette` public Qt exposure removal
- repeated visual scripting `IPanel` + pimpl/private Qt widget pattern proof
- Phase 6B allowlist reduction

Phase 6L is not accepted as:

- `NodeLibraryPanel` migration
- `GraphCanvas` migration
- `QtPanel` removal
- `SagaEditorModule` API de-Qtification
- CMake Qt PUBLIC dependency cleanup
- full CTest health

## Ownership Audit

Current `NodeLibraryPanel` / `NodeCategoryBrowser` evidence:

- `NodeLibraryPanel` publicly depends on `NodeCategoryBrowser&`.
- `NodeCategoryBrowser` is a Qt-free data/container boundary with category
  entry registration, listing, and search APIs.
- The previous `NodeLibraryPanel` implementation was a non-functional stub.
- No current source path wires `NodeLibraryPanel` to node import, command,
  graph document, graph canvas, or editor selection behavior.
- The migration can preserve the existing browser reference without changing
  node library ownership.

Decision: `NodeLibraryPanel` is safe for a bounded pimpl migration in this
slice because the existing browser reference is preserved and no node library
behavior is added.

## Migration Result

Public ABI change:

- `Editor/include/SagaEditor/VisualScripting/Nodes/NodeLibraryPanel.h` no
  longer includes `SagaEditor/UI/Qt/QtPanel.h`.
- `NodeLibraryPanel` now inherits `IPanel` directly.
- The public header exposes no `QWidget`, `QObject`, `Q_OBJECT`, `QtPanel`, or
  Qt include.
- The public `NodeLibraryPanel(NodeCategoryBrowser&)` constructor is preserved.
- The direct browser reference member is hidden behind `NodeLibraryPanel::Impl`.
- The header now exposes `void* GetNativeWidget() const noexcept`.

Private Qt implementation:

- `Editor/src/SagaEditor/UI/Qt/VisualScripting/Nodes/NodeLibraryPanel.cpp`
  owns the private Qt widget through `NodeLibraryPanel::Impl`.
- `NodeLibraryPanel::Impl` stores the existing non-owning
  `NodeCategoryBrowser&`.
- `GetNativeWidget()` returns the private `QWidget*` as `void*`.
- `GetPanelId()` returns `saga.panel.visual_scripting.node_library`.
- `GetTitle()` returns `Node Library`.
- `OnInit()` and `OnShutdown()` remain no-ops.
- The placeholder label text is `Node Library`.
- The old `Editor/src/SagaEditor/VisualScripting/Nodes/NodeLibraryPanel.cpp`
  is a non-defining moved stub to avoid duplicate symbols.

Boundary guard update:

- `EditorQtPublicAbiBoundaryTests` removed only
  `Editor/include/SagaEditor/VisualScripting/Nodes/NodeLibraryPanel.h` from the
  allowlist.
- `GraphCanvas`, `QtPanel.h`, and `Apps/Saga/SagaEditorModule.h` remain
  allowlisted.

## Cluster Status After Phase 6M

- `WatchPanel`: migrated in Phase 6H
- `BreakpointPanel`: migrated in Phase 6I
- `ExecutionTraceView`: migrated in Phase 6J
- `GraphDebuggerView`: migrated in Phase 6K
- `NodePalette`: migrated in Phase 6L
- `NodeLibraryPanel`: migrated in Phase 6M
- `GraphCanvas`: high-risk `GraphDocument` view debt; defer
- `QtPanel.h`: remains public allowlisted debt
- `SagaEditorModule.h`: remains product ABI debt

## Non-Goals

Phase 6M does not:

- remove Qt
- remove or privatize `QtPanel`
- migrate `GraphCanvas`
- change `SagaEditorModule`
- rewrite `EditorShell`
- redesign visual scripting graph document, graph runtime, debugging, node
  library, or editor shell ownership
- add node library, import, category browser, model, or command behavior
- change node import, node category browser, graph document, or graph canvas
  semantics
- change `SagaEditorLib` Qt dependency visibility
- change runtime/client asset work
- change publish gates
- prove full CTest health

## Verification

Verification completed for this slice:

- `git diff --check`
- targeted `rg` for `NodeLibraryPanel`, `NodeCategoryBrowser`, `GraphCanvas`,
  `QtPanel`, `EditorQtPublicAbiBoundary`, allowlist, `IPanel`, pimpl, and
  Phase 6M wording
- `Tools/Forge/bin/forge nix build --target SagaEditorLib --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target SagaArchitectureTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R ArchitectureTests --output-on-failure -j 1`

Recommended Phase 6N: produce a `GraphCanvas` / `GraphDocument` boundary plan.
Do not migrate `GraphCanvas` until graph view/model ownership is explicit.
