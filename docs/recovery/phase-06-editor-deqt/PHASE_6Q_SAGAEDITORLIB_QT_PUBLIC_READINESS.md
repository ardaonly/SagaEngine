# Phase 6Q SagaEditorLib Qt PUBLIC Readiness

> Last updated: 2026-05-26
> Status: Phase 6Q readiness slice
> Phase 6: Editor Public API De-Qtification

Phase 6Q audits whether `SagaEditorLib` can stop publishing Qt through its
public CMake interface. It cannot yet, because public Editor headers still
expose Qt through the deferred `GraphCanvas` / `QtPanel` debt.

Full CTest remains unverified.

## Phase 6P Closure

Phase 6P is accepted as:

- `QtPanel` privatization readiness audit
- explicit `QtPanel` privatization deferral
- boundary guard confirmation that only `GraphCanvas.h` and `QtPanel.h`
  remain allowlisted as public Qt debt

Phase 6P is not accepted as:

- `QtPanel` removal
- `GraphCanvas` migration
- graph model redesign
- CMake Qt PUBLIC dependency cleanup
- full CTest health

## Readiness Audit

Current `SagaEditorLib` CMake evidence:

- `SagaEditorLib` publishes `Editor/include` as a public include directory.
- `SagaEditorLib` links `Qt6::Core` and `Qt6::Widgets` as `PUBLIC`.
- `Editor/include/SagaEditor/VisualScripting/Editor/GraphCanvas.h` still
  includes `SagaEditor/UI/Qt/QtPanel.h` and inherits `SagaEditor::QtPanel`.
- `Editor/include/SagaEditor/UI/Qt/QtPanel.h` still exposes Qt through
  `<QWidget>`, `Q_OBJECT`, `QWidget`, and `QtPanel`.

Decision: do not convert `SagaEditorLib`'s Qt links from `PUBLIC` to
`PRIVATE` in Phase 6Q. Qt remains part of the observed public build interface
while the public `GraphCanvas` ABI requires `QtPanel`.

## Boundary Guard Status

- `EditorQtPublicAbiBoundaryTests` keeps `QtPanel.h` allowlisted.
- `EditorQtPublicAbiBoundaryTests` keeps `GraphCanvas.h` allowlisted.
- No allowlist reduction is safe in this slice.
- No CMake visibility change is safe in this slice.

## Non-Goals

Phase 6Q does not:

- change `SagaEditorLib` Qt dependency visibility
- remove or privatize `QtPanel`
- migrate `GraphCanvas`
- change `GraphDocument`
- change editor shell, graph model, graph runtime, serializer, importer,
  preview, compiler, validation, pin, or overlay behavior
- prove full CTest health

## Verification

Verification completed for this slice:

- `git diff --check`
- targeted `rg` for `SagaEditorLib`, `Qt6::Core`, `Qt6::Widgets`,
  `GraphCanvas`, `QtPanel`, `EditorQtPublicAbiBoundary`, allowlist, and Phase
  6Q wording
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`

Recommended Phase 6R: close Phase 6 honestly as a public API de-Qtification
checkpoint with remaining `GraphCanvas`, `QtPanel`, and `SagaEditorLib` Qt
PUBLIC dependency debt explicitly deferred. Do not claim full public Qt removal
or full CTest health.
