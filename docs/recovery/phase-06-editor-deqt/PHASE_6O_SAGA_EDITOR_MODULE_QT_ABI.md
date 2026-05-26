# Phase 6O SagaEditorModule Qt ABI

> Last updated: 2026-05-26
> Status: Phase 6O bounded implementation slice
> Phase 6: Editor Public API De-Qtification

Phase 6O audits the Saga product editor activation ABI and removes the public
Qt-shaped `SagaEditorModule` header dependency with a narrow native mount
adapter.

Full CTest remains unverified.

## Phase 6N Closure

Phase 6N is accepted as:

- `GraphCanvas` / `GraphDocument` boundary audit
- explicit `GraphCanvas` migration deferral
- remaining graph view/model seam requirements
- unchanged boundary guard allowlist for `GraphCanvas`

Phase 6N is not accepted as:

- `GraphCanvas` migration
- graph model redesign
- `QtPanel` removal
- `SagaEditorModule` API de-Qtification
- CMake Qt PUBLIC dependency cleanup
- full CTest health

## Product ABI Audit

Current `SagaEditorModule` evidence before Phase 6O:

- `SagaEditorModule.h` forward-declared `QMainWindow` and `QStackedWidget`.
- `Activate` accepted `QMainWindow&` and `QStackedWidget&`.
- `Shutdown` accepted the same Qt references but did not use them.
- The only production caller is `Apps/Saga/SagaApp.cpp`.
- `SagaEditorModule.cpp` already mounts through
  `SagaEditor::QtUIMainWindow(void*, void*)`, so a backend-neutral native mount
  adapter can preserve behavior without rewriting `EditorShell`.

Decision: perform a bounded adapter implementation. Do not rewrite the product
shell.

## Migration Result

Public ABI change:

- `Apps/Saga/SagaEditorModule.h` no longer forward-declares `QMainWindow` or
  `QStackedWidget`.
- The header introduces `SagaEditorNativeMount` with `void* mainWindow` and
  `void* centralStack`.
- `SagaEditorModule::Activate` now accepts `SagaEditorNativeMount`.
- `SagaEditorModule::Shutdown` no longer takes unused native window arguments.

Implementation:

- `Apps/Saga/SagaEditorModule.cpp` validates the native mount handles before
  editor activation.
- The implementation still mounts through `SagaEditor::QtUIMainWindow`, keeping
  Qt in product/editor implementation code rather than the public product
  header.
- `Apps/Saga/SagaApp.cpp` builds the native mount from its existing
  `QMainWindow` and `QStackedWidget` objects.

Boundary guard update:

- `EditorQtPublicAbiBoundaryTests` removed only `Apps/Saga/SagaEditorModule.h`
  from the allowlist.
- `GraphCanvas.h` and `QtPanel.h` remain allowlisted.

## Remaining Public Qt Debt

- `Editor/include/SagaEditor/VisualScripting/Editor/GraphCanvas.h`
- `Editor/include/SagaEditor/UI/Qt/QtPanel.h`

## Non-Goals

Phase 6O does not:

- remove Qt from the Saga product implementation
- rewrite `SagaApp`
- rewrite `EditorShell`
- change editor mode product flow
- migrate `GraphCanvas`
- remove or privatize `QtPanel`
- change `SagaEditorLib` Qt dependency visibility
- prove full CTest health

## Verification

Verification completed for this slice:

- `git diff --check`
- targeted `rg` for `SagaEditorModule`, `SagaEditorNativeMount`,
  `QMainWindow`, `QStackedWidget`, `GraphCanvas`, `QtPanel`,
  `EditorQtPublicAbiBoundary`, allowlist, and Phase 6O wording
- `Tools/Forge/bin/forge nix build --target SagaProductLib --build=build/RelWithDebInfo-0.0.9-sde --jobs=1`
- `Tools/Forge/bin/forge nix build --target SagaArchitectureTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R ArchitectureTests --output-on-failure -j 1`

The non-SDE build tree does not define `SagaProductLib`; the product compile
check uses the existing SDE-enabled build tree.

Recommended Phase 6P: audit `QtPanel` privatization readiness. Do not remove
`QtPanel` while `GraphCanvas.h` remains a public inheritor.
