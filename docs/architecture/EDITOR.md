# Editor

> Last updated: 2026-05-26

Editor boundaries are being cleaned up, and several Qt-shaped public surfaces
have been reduced or guarded. This is not a finished editor workflow.

## Current State

- Several public panel/module surfaces were migrated away from public Qt-shaped
  contracts.
- `EditorQtPublicAbiBoundaryTests` guards the current public Qt exposure
  baseline.
- `GraphCanvas`, `QtPanel`, and `SagaEditorLib` Qt PUBLIC visibility remain
  open cleanup work.
- Editor scaffolding exists, but the end-to-end authoring workflow is not
  complete.
- Full visual scripting workflow and dashboard behavior are not complete.

## Background

- [Phase 6 editor de-Qt closure](../recovery/phase-06-editor-deqt/PHASE_6R_EDITOR_PUBLIC_API_DEQT_CLOSURE.md)
- [Phase 7 editor scaffolding closure](../recovery/phase-07-editor-scaffolding/PHASE_7_CLOSURE_AND_PHASE_8_OPENING_CHECKPOINT.md)
