# Editor

> Last updated: 2026-05-26

Editor public Qt exposure is reduced and guarded, and editor scaffolding has a
quarantine foundation. This is not full editor product readiness.

## Current State

- Several public panel/module surfaces were migrated away from public Qt-shaped
  contracts.
- `EditorQtPublicAbiBoundaryTests` guards the current public Qt exposure
  baseline.
- `GraphCanvas`, `QtPanel`, and `SagaEditorLib` Qt PUBLIC visibility remain
  explicit debt.
- Editor scaffolding and workflow ownership were classified.
- Full editor product readiness, full visual scripting workflow readiness, and
  complete dashboard readiness are not proven.

## Evidence

- [Phase 6 editor de-Qt closure](../recovery/phase-06-editor-deqt/PHASE_6R_EDITOR_PUBLIC_API_DEQT_CLOSURE.md)
- [Phase 7 editor scaffolding closure](../recovery/phase-07-editor-scaffolding/PHASE_7_CLOSURE_AND_PHASE_8_OPENING_CHECKPOINT.md)
