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

## Evidence Boundary

Current evidence supports selected public API boundary cleanup and editor
scaffolding. It does not prove a complete authoring workflow, complete visual
scripting UI, complete dashboard behavior, or production editor readiness.
