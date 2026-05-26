# Phase 7A Editor Scaffolding Inventory

> Last updated: 2026-05-26
> Status: Phase 7A report-only inventory
> Phase 7: Editor Scaffolding Quarantine

Phase 7A inventories editor scaffolding, placeholders, test placeholders, and
success-stub patterns. It does not change behavior.

Full CTest remains unverified.

## Phase 6 Closure

Phase 6 is accepted only as an Editor public API de-Qtification checkpoint with
explicit deferred `GraphCanvas`, `QtPanel`, and `SagaEditorLib` Qt PUBLIC
dependency debt.

Phase 6 is not accepted as:

- complete Qt removal
- zero public Qt exposure
- graph model redesign
- visual scripting product readiness
- editor product readiness
- full CTest health

## Inventory Method

Targeted searches used:

- `rg` for `TODO`, `FIXME`, `placeholder`, `scaffold`, `stub`, `mock`, `demo`,
  `sample`, `temporary`, `return true`, `return success`, `not implemented`,
  `coming soon`, and `dummy`
- `rg -l` for `implementation stub`, `Pimpl scaffolding`, and `/* stub */`
  under `Editor/src`
- source review of editor shell, editor host, project manager, production
  dashboard, Qt panels, EditorLab adapters, and editor tests

## Measured Scaffold Surface

Observed generated or explicit implementation-stub files under `Editor/src`:
115.

Breakdown by top-level editor area:

| Area | Stub files |
|---|---:|
| Collaboration | 41 |
| VisualScripting | 32 |
| Gizmos | 4 |
| Validation | 4 |
| Extensions | 3 |
| Importers | 3 |
| Localization | 3 |
| Pipeline | 3 |
| PrefabEditing | 3 |
| Project | 3 |
| SceneEditing | 3 |
| Serialization | 3 |
| Toolbars | 3 |
| WorldEditing | 3 |
| Layouts | 2 |
| Shell | 1 |
| Themes | 1 |

Observed placeholder/TODO/not-wired files across editor source, headers, and
editor tests: 25.

## Classification

Production-path stubs:

- `Editor/src/SagaEditor/Project/ProjectManager.cpp`: `OpenProject` returns
  `false`, `IsProjectOpen` returns `false`, and `GetCurrentProject` returns a
  static default project.
- `Editor/src/SagaEditor/UI/Qt/Panels/AssetBrowserPanel.cpp`: refresh clears
  the asset grid and has TODOs for file enumeration and asset opening.
- `Editor/src/SagaEditor/UI/Qt/Panels/WorldViewportPanel.cpp`: camera orbit,
  zoom, and fly-cam controls are documented as TODO / placeholder behavior.
- `Editor/src/SagaEditor/UI/Qt/Panels/HierarchyPanel.cpp`: selection-change
  repopulation is not wired.
- `Editor/src/SagaEditor/Collaboration/Server/CollaborationServer.cpp`:
  networking transport is explicitly not wired.

High-risk future scaffolds:

- Collaboration implementation stubs span sync, permissions, server, client,
  replay, authority, conflict resolution, merge, presence, workspace, session,
  and locks. These should not be converted in one editor workflow slice.
- Visual scripting implementation stubs span graph canvas/document/layout,
  graph editor, compiler adapters, imports, runtime bridges, preview, pins,
  nodes, and debugger controllers. Phase 6 already deferred graph view/model
  ownership here.
- Pipeline, serialization, validation, importers, prefab, world editing, and
  scene editing contain generated stub bodies and should be handled by owned
  feature slices, not by broad scaffold cleanup.

Test-only placeholders:

- `Tests/Unit/Editor/GraphCompilationTests.cpp`
- `Tests/Unit/Editor/UnsupportedSyntaxTests.cpp`
- `Tests/Unit/Editor/PinConnectionTests.cpp`
- `Tests/Unit/Editor/NodeValidationTests.cpp`
- `Tests/Unit/Editor/RoundTripTests.cpp`
- `Tests/Unit/Editor/GraphDebuggerTests.cpp`

Lab/test scaffolding:

- EditorLab deterministic and host scenario adapters use mock extension and
  success-result flows for scenario harness testing. These should be reviewed
  separately from product editor workflows.

Currently real or partially real editor surfaces:

- `EditorHost` initializes registries, settings, diagnostics, customization,
  composition session, extension host, and editor engine bridge.
- `EditorShell` registers default panels, shell commands, persona/profile
  commands, composition-driven layout, diagnostics, and panel visibility.
- `ProductionDashboardPanel` displays live editor host data for engine bridge,
  profile, persona, and customization status.
- `ProblemsPanel` is wired to the diagnostics service through the shell.
- Customization, profile, persona, diagnostics, notification, command, and
  composition subsystems have focused tests and more concrete behavior than
  the generated stub clusters.

## Candidate For Phase 7B

Recommended Phase 7B focus: map ownership for one real project/session
workflow centered on opening or resolving a project/workspace and surfacing
readiness in the editor shell or dashboard.

Reason:

- `ProjectManager` is a narrow, obvious production-path stub.
- The public API is small: open, close, current project, open-state, and change
  callback.
- It connects naturally to the Phase 7 acceptance target: open project,
  validate readiness, show dashboard/diagnostics, and persist session state.
- It does not require visual scripting, collaboration, graph model, SDE
  compiler startup, runtime/server behavior, or broad panel rewrites.

## Non-Goals

Phase 7A does not:

- change source behavior
- reduce stub count
- rewrite editor shell
- implement project opening
- implement visual scripting, graph canvas, or graph model behavior
- implement collaboration product behavior
- invoke SDE or SagaPipeline from editor startup
- change public Qt boundaries
- prove full CTest health

## Verification

Verification completed for this inventory:

- `git diff --check`
- targeted `rg` scaffold inventory over `Editor`, `Apps/Editor`,
  `Apps/EditorLab`, `Apps/Saga`, `Tools/SagaEditorComposition`,
  `Tests/Unit/Editor`, and `Tests/Unit/Architecture`
- targeted `rg` for Phase 7A roadmap and iteration-note wording
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`

Recommended Phase 7B: produce an editor workflow ownership map for the project
/ workspace session path and select one bounded implementation slice.
