# Phase 7C ProjectManager Stateful Workflow

> Last updated: 2026-05-26
> Status: Phase 7C bounded implementation
> Phase 7: Editor Scaffolding Quarantine

Phase 7C replaces one editor-local generated scaffold with deterministic
project session behavior. It does not wire editor menus or product shell
behavior.

Full CTest remains unverified.

## Phase 7B Closure

Phase 7B is accepted as:

- project/workspace workflow ownership map
- product/editor/lab boundary classification
- selection of `ProjectManager` as the first bounded implementation candidate

Phase 7B is not accepted as:

- source behavior change
- editor menu/file-dialog wiring
- product shell behavior change
- SDE startup integration
- editor product readiness
- full CTest health

## Implementation Result

`ProjectManager` now owns editor-local project session state:

- `OpenProject` validates a non-empty existing directory, or a manifest file's
  parent directory.
- `OpenProject` reads `saga.project.json` when present.
- Manifest `displayName` or `name` populates `ProjectInfo::name`.
- Manifest `engineVersion` or `engine` populates `ProjectInfo::engineVersion`.
- A directory without manifest falls back to the directory name.
- `ProjectInfo::rootPath` stores the absolute project root path.
- Invalid or unreadable manifests fail without replacing the current open
  project.
- `CloseProject` clears open state and current project data.
- `IsProjectOpen`, `GetCurrentProject`, and `SetOnProjectChanged` now expose
  real state and callback behavior.

## Stub Reduction

The Phase 7A generated/explicit implementation-stub inventory changed from 115
files under `Editor/src` to 114. The `Project` area changed from 3 stub files
to 2 because `ProjectManager.cpp` is no longer a generated scaffold.

## Tests

Added `Tests/Unit/Editor/ProjectManagerTests.cpp` covering:

- manifest-backed open and metadata projection
- directory open without a manifest
- missing-path rejection without replacing current state
- invalid-manifest rejection
- project-changed callback behavior on open and close

## Non-Goals

Phase 7C does not:

- wire `saga.command.file.open_project`
- add file dialogs
- change `EditorHost` or `EditorShell` ownership
- move `SagaProjectSystem` into Editor
- add an Editor dependency on Apps/Saga
- invoke SDE or SagaPipeline from editor startup
- change product shell behavior
- implement visual scripting, graph model, collaboration, runtime, server, or
  package behavior
- change public Qt boundaries
- prove full CTest health

## Verification

Verification completed for this slice:

- `git diff --check`
- targeted `rg` for `ProjectManager`, `ProjectManagerTests`, Phase 7C,
  project/session ownership, non-goals, and remaining scaffold counts
- `Tools/Forge/bin/forge nix build --target SagaEditorLib --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target SagaUnitTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `build/RelWithDebInfo-0.0.9/bin/SagaUnitTests --gtest_filter=ProjectManagerTests.*`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`

Recommended next slice: Phase 7D diagnostics/dashboard reality pass. Start with
an audit of whether the production dashboard and Problems panel accurately
surface the real project/session readiness now available, and do not broaden
into visual scripting or collaboration behavior.
