# Phase 7D Diagnostics Dashboard Reality

> Last updated: 2026-05-26
> Status: Phase 7D audit-only reality pass
> Phase 7: Editor Scaffolding Quarantine

Phase 7D audits whether the production dashboard and Problems panel surface
real editor readiness data after the `ProjectManager` scaffold replacement. It
does not change UI behavior.

Full CTest remains unverified.

## Phase 7C Closure

Phase 7C is accepted as:

- one bounded scaffold behavior replacement
- `ProjectManager` stateful open/close implementation
- focused `ProjectManagerTests`
- measured editor stub inventory reduction from 115 to 114 files

Phase 7C is not accepted as:



## Reality Audit

Real diagnostic surfaces:

- `EditorDiagnosticsService` supports add, upsert, remove, clear,
  clear-source, replace-source, severity filtering, and subscriptions.
- `EditorHost` owns one shared diagnostics service and exposes it through
  `GetEditorDiagnosticsService`.
- `EditorHost::Init` maps editor composition diagnostics into the shared
  service under `editor.composition`.
- Workspace and shortcut customization sessions receive the shared diagnostics
  service and can publish customization diagnostics.
- `ProblemsPanel` subscribes to the shared diagnostics service, shows severity,
  source, code, message, location, and publish-blocking tooltips, and can clear
  diagnostics through the service.
- `EditorShell::RegisterDefaultPanels` wires `ProblemsPanel` to the shared
  diagnostics service.

Real dashboard surfaces:



Current gaps:



## Classification

Real:

- shared diagnostics service
- Problems panel diagnostics subscription/display
- composition/customization diagnostics bridge into the shared service
- production dashboard host/profile/persona/customization status

Partial:



Deferred:

- dashboard workspace/project rows
- dashboard diagnostic summary rows
- product compile diagnostics bridge into `EditorDiagnosticsService`
- standalone editor Open Project command wiring

## Recommended Next Step

Recommended next slice: do not implement another behavior slice until a Phase 7
checkpoint decides whether Phase 7 needs a second bounded implementation.

If a second bounded Phase 7 implementation is accepted later, the safest
candidate is a read-only dashboard reality slice:



## Non-Goals

Phase 7D does not:



## Verification

Verification completed for this audit:

- `git diff --check`
- targeted `rg` for `ProductionDashboardPanel`, `ProblemsPanel`,
  `EditorDiagnosticsService`, `EditorHost`, `EditorShell`,
  `EditorWorkspaceDefinition`, `ProjectManager`, product compile diagnostics,
  Phase 7D wording, and non-goals
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`

Stop condition: the next Phase 7 step would choose between a second bounded UI
implementation and a Phase 7 closure checkpoint. That is a scope decision after
the accepted one-workflow implementation.
