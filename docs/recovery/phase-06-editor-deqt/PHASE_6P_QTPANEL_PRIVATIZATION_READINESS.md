# Phase 6P QtPanel Privatization Readiness

> Last updated: 2026-05-26
> Status: Phase 6P readiness slice
> Phase 6: Editor Public API De-Qtification

Phase 6P audits whether `QtPanel` can be removed or privatized. It cannot be
removed yet because `GraphCanvas.h` remains a public inheritor.

Full CTest remains unverified.

## Phase 6O Closure

Phase 6O is accepted as:

- `SagaEditorModule` product Qt ABI audit
- public `QMainWindow` / `QStackedWidget` exposure removal from
  `SagaEditorModule.h`
- bounded `SagaEditorNativeMount` adapter implementation
- Phase 6B allowlist reduction

Phase 6O is not accepted as:

- product shell rewrite
- `GraphCanvas` migration
- `QtPanel` removal
- CMake Qt PUBLIC dependency cleanup
- full CTest health

## Readiness Audit

Remaining public `QtPanel` dependencies:

- `Editor/include/SagaEditor/VisualScripting/Editor/GraphCanvas.h` includes
  `SagaEditor/UI/Qt/QtPanel.h` and inherits `SagaEditor::QtPanel`.
- `Editor/include/SagaEditor/UI/Qt/QtPanel.h` remains public and exposes
  `<QWidget>`, `Q_OBJECT`, `QWidget`, and `QtPanel`.

Decision: do not remove or privatize `QtPanel` in Phase 6P. `QtPanel` must
remain public allowlisted debt until `GraphCanvas` has a safe graph view/model
seam and no public header inherits it.

## Boundary Guard Status

- `EditorQtPublicAbiBoundaryTests` keeps `QtPanel.h` allowlisted.
- `EditorQtPublicAbiBoundaryTests` keeps `GraphCanvas.h` allowlisted.
- No allowlist reduction is safe in this slice.

## Non-Goals

Phase 6P does not:

- remove or privatize `QtPanel`
- migrate `GraphCanvas`
- change `GraphDocument`
- change editor shell, graph model, graph runtime, serializer, importer,
  preview, compiler, validation, pin, or overlay behavior
- change CMake Qt dependency visibility
- prove full CTest health

## Verification

Verification completed for this slice:

- `git diff --check`
- targeted `rg` for `QtPanel`, `GraphCanvas`, remaining public Qt exposure,
  `EditorQtPublicAbiBoundary`, allowlist, and Phase 6P wording
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`

Recommended Phase 6Q: audit `SagaEditorLib` Qt PUBLIC dependency cleanup
readiness. Do not make Qt private while `GraphCanvas.h` and `QtPanel.h` remain
public Qt ABI.
