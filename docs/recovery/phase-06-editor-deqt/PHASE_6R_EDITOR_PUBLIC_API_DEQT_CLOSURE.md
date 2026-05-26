# Phase 6R Editor Public API De-Qtification Closure

> Last updated: 2026-05-26
> Status: Phase 6 closure checkpoint
> Phase 6: Editor Public API De-Qtification

Phase 6R closes Phase 6 as a public API de-Qtification checkpoint with explicit
deferred debt. It does not close SagaEditor as fully Qt-free.

Full CTest remains unverified.

## Phase 6Q Closure

Phase 6Q is accepted as:

- `SagaEditorLib` Qt PUBLIC dependency readiness audit
- explicit CMake Qt visibility cleanup deferral
- confirmation that `SagaEditorLib` still publishes Qt because public Editor
  ABI still exposes `GraphCanvas` / `QtPanel`

Phase 6Q is not accepted as:

- `SagaEditorLib` Qt visibility cleanup
- `QtPanel` removal
- `GraphCanvas` migration
- graph model redesign
- full CTest health

## Completed Public ABI Cleanup

Phase 6 removed public Qt exposure from these bounded surfaces:

- `GraphViewportPanel`
- `ProfilerPanel`
- `ProjectBrowserPanel`
- `CollaborationPanel`
- `WatchPanel`
- `BreakpointPanel`
- `ExecutionTraceView`
- `GraphDebuggerView`
- `NodePalette`
- `NodeLibraryPanel`
- `SagaEditorModule`

Those surfaces now avoid public `QtPanel` inheritance or public Qt-shaped
product activation parameters.

## Remaining Explicit Debt

Remaining public Qt exposure is intentionally not hidden:

- `Editor/include/SagaEditor/VisualScripting/Editor/GraphCanvas.h`
- `Editor/include/SagaEditor/UI/Qt/QtPanel.h`
- `SagaEditorLib` `Qt6::Core` / `Qt6::Widgets` `PUBLIC` CMake visibility

`GraphCanvas` remains deferred because its public constructor takes
`GraphDocument&` and the graph view/model seam is not yet defined. `QtPanel`
must remain public while any public header inherits or includes it.
`SagaEditorLib` must keep Qt public while its public headers require Qt.

## Guard Status

`EditorQtPublicAbiBoundaryTests` is the Phase 6 guard for public Qt exposure.
At closure it allowlists only:

- `Editor/include/SagaEditor/UI/Qt/QtPanel.h`
- `Editor/include/SagaEditor/VisualScripting/Editor/GraphCanvas.h`

This is a clean guarded baseline with explicit debt, not a zero-leak public
Editor API.

## Closure Decision

Phase 6 is closed as:

- public Qt exposure substantially reduced
- remaining public Qt debt explicitly classified
- product editor activation public ABI de-Qtified
- CMake Qt visibility cleanup correctly deferred until public ABI cleanup is
  possible
- focused public ABI guard passing with no unexpected violations

Phase 6 is not closed as:

- complete Qt removal
- zero public Qt exposure
- `GraphCanvas` migration
- `QtPanel` privatization
- `SagaEditorLib` Qt PRIVATE conversion
- visual scripting product readiness
- graph model redesign
- editor product readiness
- full CTest health

## Verification

Verification completed for this checkpoint:

- `git diff --check`
- targeted `rg` for Phase 6R closure wording, remaining allowlist entries,
  public Qt exposure, `GraphCanvas`, `QtPanel`, `SagaEditorModule`,
  `SagaEditorLib`, and CMake Qt visibility
- `Tools/Forge/bin/forge nix build --target SagaArchitectureTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R ArchitectureTests --output-on-failure -j 1`

Recommended Phase 7A: start Editor Scaffolding Quarantine with a report-only
editor scaffold inventory. Do not start broad editor behavior work from this
closure checkpoint.
