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

- editor menu/file-dialog wiring
- `EditorHost` or `EditorShell` ownership change
- product shell behavior change
- SDE startup integration
- editor product readiness
- full CTest health

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

- `ProductionDashboardPanel` displays editor engine bridge state, runtime role,
  engine version, commit, active profile, active persona, SDE customization
  availability, and customization source.
- `EditorShell` refreshes the dashboard after persona/profile changes and when
  the production dashboard command is invoked.

Current gaps:

- `ProductionDashboardPanel` does not display the prepared
  `EditorWorkspaceDefinition` id, root, layout preset, initial profile, or SDE
  validation flag.
- `ProductionDashboardPanel` does not display shared diagnostic counts or
  publish-blocking diagnostic counts.
- `ProjectManager` is now real but remains editor-local and not integrated into
  `EditorHost`, `EditorShell`, dashboard, or Problems.
- `SagaEditorModule::ValidateAndCompile` logs product SDE compile diagnostics
  to Console and status text, but does not publish them into
  `EditorDiagnosticsService`; therefore the Problems panel does not show those
  product compile diagnostics.
- The standalone `saga.command.file.open_project` shell command remains a stub
  and is intentionally not wired in Phase 7D.

## Classification

Real:

- shared diagnostics service
- Problems panel diagnostics subscription/display
- composition/customization diagnostics bridge into the shared service
- production dashboard host/profile/persona/customization status

Partial:

- production dashboard readiness, because it lacks workspace/project readiness
  and diagnostics counts
- product SDE compile feedback, because it is visible in Console/status but not
  in Problems

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

- add prepared workspace id/root/SDE validation rows to
  `ProductionDashboardPanel`
- optionally add diagnostic count rows from `EditorDiagnosticsService`
- do not wire file dialogs
- do not invoke SDE or SagaPipeline from editor startup
- do not change product compile behavior

## Non-Goals

Phase 7D does not:

- change source behavior
- add dashboard rows
- bridge product compile diagnostics into Problems
- wire `saga.command.file.open_project`
- add file dialogs
- change product shell behavior
- move `SagaProjectSystem` into Editor
- invoke SDE or SagaPipeline from editor startup
- implement visual scripting, graph model, collaboration, runtime, server, or
  package behavior
- change public Qt boundaries
- prove full CTest health

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
