# Phase 6N GraphCanvas Boundary Plan

> Last updated: 2026-05-26
> Status: Phase 6N boundary planning slice
> Phase 6: Editor Public API De-Qtification

Phase 6N audits `GraphCanvas` / `GraphDocument` and deliberately does not
migrate `GraphCanvas`.

Full CTest remains unverified.

## Phase 6M Closure

Phase 6M is accepted as:

- `NodeLibraryPanel` / `NodeCategoryBrowser` ownership audit
- `NodeLibraryPanel` public Qt exposure removal
- preservation of the existing `NodeCategoryBrowser&` constructor boundary
- repeated visual scripting `IPanel` + pimpl/private Qt widget pattern proof
- Phase 6B allowlist reduction

Phase 6M is not accepted as:

- `GraphCanvas` migration
- graph view/model boundary resolution
- `QtPanel` removal
- `SagaEditorModule` API de-Qtification
- CMake Qt PUBLIC dependency cleanup
- full CTest health

## Boundary Audit

Current `GraphCanvas` evidence:

- `GraphCanvas` still publicly includes `QtPanel` and inherits `QtPanel`.
- `GraphCanvas` publicly takes `GraphDocument&` and stores it as a direct
  non-owning reference.
- `GraphDocument` is the shared graph model surface for node/link mutation,
  serialization, validation, imports, preview, compiler adapters, pin rendering,
  overlays, and graph editor helpers.
- The current `GraphCanvas` implementation is a stub, but the public
  `GraphDocument&` contract makes this a graph view/model boundary, not a leaf
  UI shell.

Decision: do not migrate `GraphCanvas` in Phase 6N. Keep it allowlisted until a
dedicated graph view contract exists.

## Required Future Seam

A future `GraphCanvas` migration should first define one of these explicit
boundaries:

- a backend-neutral graph canvas view interface that receives a graph view model
  instead of owning graph document behavior
- a graph document adapter that preserves `GraphDocument&` lifetime and mutation
  semantics while hiding Qt in a private implementation
- focused graph view/model tests that prove canvas construction, panel identity,
  document lifetime, and graph mutation semantics remain unchanged

Minimum migration prerequisites:

- document whether `GraphCanvas` is display-only or allowed to mutate
  `GraphDocument`
- preserve or intentionally replace the public `GraphCanvas(GraphDocument&)`
  contract
- prove no graph runtime, serializer, importer, preview, compiler, validation,
  pin, overlay, or editor helper behavior changes
- keep `QtPanel` available until this public header no longer needs it

## Boundary Guard Status

- `EditorQtPublicAbiBoundaryTests` keeps
  `Editor/include/SagaEditor/VisualScripting/Editor/GraphCanvas.h` allowlisted
  for `QtPanelInclude` and `QtPanel`.
- `QtPanel.h` and `Apps/Saga/SagaEditorModule.h` remain allowlisted.
- No CMake Qt visibility cleanup is allowed while these public Qt exposures
  remain.

## Non-Goals

Phase 6N does not:

- migrate `GraphCanvas`
- remove Qt
- remove or privatize `QtPanel`
- change `GraphDocument`
- redesign graph model, graph runtime, graph editor, serializer, importer,
  preview, compiler, validation, pins, overlays, or editor shell ownership
- change `SagaEditorModule`
- change `SagaEditorLib` Qt dependency visibility
- prove full CTest health

## Verification

Verification completed for this slice:

- `git diff --check`
- targeted `rg` for `GraphCanvas`, `GraphDocument`, `QtPanel`,
  `EditorQtPublicAbiBoundary`, allowlist, Phase 6N wording, and remaining debt
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`

Recommended Phase 6O: audit `SagaEditorModule` product Qt ABI and produce a
bounded product adapter plan unless implementation inspection proves a small
non-breaking adapter is safe.
