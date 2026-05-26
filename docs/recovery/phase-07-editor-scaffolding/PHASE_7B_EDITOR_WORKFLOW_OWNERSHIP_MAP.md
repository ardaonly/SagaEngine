# Phase 7B Editor Workflow Ownership Map

> Last updated: 2026-05-26
> Status: Phase 7B report-only ownership map
> Phase 7: Editor Scaffolding Quarantine

Phase 7B maps one bounded editor project/workspace workflow before
implementation. It does not change behavior.

Full CTest remains unverified.

## Phase 7A Closure

Phase 7A is accepted as:

- editor scaffold inventory
- measured implementation-stub surface
- classification of production-path stubs, high-risk future scaffolds,
  test-only placeholders, lab/test scaffolding, and real/partial behavior
- recommendation to map the project/workspace session workflow

Phase 7A is not accepted as:

- stub-count reduction
- project workflow implementation
- editor product readiness
- visual scripting readiness
- collaboration readiness
- full CTest health

## Ownership Boundaries

Product layer ownership:

- `Apps/Saga/SagaProjectSystem` owns Saga product project manifest creation,
  open validation, recent-project persistence, and local session labels.
- `Apps/Saga/SagaApp` owns product-shell file dialogs, create/open project UI,
  recent-project list UI, and product/editor view switching.
- `Apps/Saga/SagaWorkspaceResolver` owns SDE-backed workspace resolution and
  validation for product startup/workspace selectors.
- `Apps/Saga/SagaEditorModule` owns same-process product-to-editor activation,
  close-project command replacement, project menu addition, and product SDE
  compile command handling.

Editor layer ownership:

- `EditorHost` owns editor services, workspace definition consumption,
  diagnostics, composition, customization, profile/persona registries,
  notification, command, extension, and runtime bridge services.
- `EditorShell` owns editor menus, command registration, panel registration,
  layout, composition projection, and profile/persona visibility application.
- `ProductionDashboardPanel` displays live editor host state but does not own
  product project opening.
- `ProjectManager` is an editor-local project/session scaffold. It is not
  currently wired into `EditorHost`, `EditorShell`, or the Saga product shell.

Lab/test ownership:

- EditorLab scenario adapters own deterministic scenario playback and mock
  project-load operations. They are not product project-opening behavior.

## Current Workflow Flow

Current product path:

1. `SagaProductShellWidget` creates or opens a project with
   `SagaProjectSystem`.
2. `SagaMainWindow::EnterEditorMode` creates a `SagaPreparedProjectSession`.
3. `SagaEditorModule::Activate` translates the product session into an
   `EditorWorkspaceDefinition`.
4. `EditorHost::Init` consumes the prepared workspace root and initializes
   editor services.
5. `EditorShell::Init` registers default panels and commands.
6. `SagaEditorModule` configures project panels, registers product commands,
   and sets initial status text.

Current editor-only gaps:

- `ProjectManager::OpenProject` always returns `false`.
- `ProjectManager::IsProjectOpen` always returns `false`.
- `ProjectManager::GetCurrentProject` returns a static default project.
- `ProjectManager::SetOnProjectChanged` ignores callbacks.
- `saga.command.file.open_project` is a shell command with no handler in the
  standalone editor shell.

## Phase 7C Selection

Recommended Phase 7C implementation: make `ProjectManager` a real editor-local
stateful project session component with deterministic unit tests.

The bounded implementation should:

- validate a non-empty existing directory
- populate `ProjectInfo` from `saga.project.json` when available
- fall back to the directory name when no manifest display name is available
- store an absolute root path
- expose correct open/closed state
- fire the project-changed callback on open and close
- avoid product-layer dependencies
- avoid SDE compiler, SagaPipeline, visual scripting, collaboration, editor
  shell, product shell, and Qt changes

Do not wire the standalone editor `Open Project` menu command in Phase 7C. That
requires UI/file-dialog ownership and should follow after the project manager
contract has deterministic tests.

## Non-Goals

Phase 7B does not:

- change source behavior
- implement project opening
- wire editor menus or file dialogs
- move `SagaProjectSystem` into Editor
- make Editor depend on Apps/Saga
- invoke SDE or SagaPipeline from editor startup
- change product shell behavior
- implement visual scripting, graph model, collaboration, runtime, server, or
  package behavior
- change public Qt boundaries
- prove full CTest health

## Verification

Verification completed for this map:

- `git diff --check`
- targeted `rg` for `ProjectManager`, project/workspace/session flow,
  `SagaProjectSystem`, `SagaWorkspaceResolver`, `SagaEditorModule`,
  `EditorHost`, `EditorShell`, `EditorWorkspaceDefinition`, shell project
  commands, Phase 7B wording, and Phase 7C recommendation
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`

Recommended Phase 7C: implement only the `ProjectManager` stateful open/close
workflow with focused editor unit tests.
