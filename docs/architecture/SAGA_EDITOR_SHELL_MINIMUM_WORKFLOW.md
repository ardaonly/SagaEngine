# Saga Editor Shell Minimum Workflow

Phase 22 status is `Implemented-Unverified`.

The Saga Editor Shell minimum workflow is a report-backed inspection path over
existing project and tool evidence. It is not a full editor workflow.

## Current Surfaces

Real current surfaces used by this phase:

- `Apps/Saga` owns the Product Shell boundary, including project create/open,
  target preparation, process launch, local session metadata, SagaScript gate,
  package staging, publish readiness, and same-process editor mounting.
- `Apps/Editor` owns the standalone `SagaEditor` launcher.
- `Apps/EditorLab` owns scenario/development shell flows.
- `Editor` contains `EditorApp`, `EditorHost`, `EditorShell`, profile/layout
  plumbing, panels, and read-only authoring models such as
  `TechnicalPreviewProjectView` and `ProjectBrowserWorkflowView`.
- `Tools/SagaProjectKit/sagaproject validate` remains project validation truth.
- `Tools/SagaScript/sagascript` remains script and CLI-only block evidence
  truth.
- StarterArena provides `StarterArena.sagaproj`, scene metadata, script source,
  diagnostics/report folder conventions, and sample documentation.

## Minimum Workflow

The minimum Phase 22 workflow is:

```txt
Open StarterArena metadata
Show project identity/status
Show validation report path/status
Show available workflow actions as shell entries
Show known limitations
Expose links/commands for runtime smoke, script/blocks evidence, server smoke, and package preflight
```

`SagaEditor` exposes this as a no-UI inspection mode:

```bash
build/RelWithDebInfo-0.0.9/bin/SagaEditor --inspect-project samples/StarterArena/StarterArena.sagaproj --editor-shell-report /tmp/starter_arena_editor_shell_report.json
```

The report contains project identity, editor read-model status, project browser
sections, workflow action command/report references, diagnostics, and known
limitations. The mode does not run the tools; it records the existing proof
commands and whether their expected reports are present.

## Boundary

Product Shell routes workflows. SagaEditor owns future project inspection and
editing views. Phase 22 only provides an editor-shell inspection/report proof.

CLI tools remain the source of truth for project validation, runtime smoke,
SagaScript, CLI-only Visual Blocks evidence, server smoke, and package
preflight. The editor shell must not hide failed commands, missing reports, or
known limitations.

## Customization Direction

Early customization remains limited to:

- view and profile presets;
- panel and workflow visibility metadata;
- personal layout later;
- strict shared project truth.

This phase does not claim maximum customization or completed profile editing.

## Non-Claims

Phase 22 does not implement or claim:

- full editor;
- full editor dashboard;
- Visual Blocks editor UI;
- drag/drop editing;
- in-place C# editing;
- package builder;
- distribution pipeline;
- collaboration server;
- runtime engine changes;
- server changes;
- SagaScript changes;
- SDE changes;
- StarterArena gameplay changes;
- CSharpScriptHost changes.

No phase is marked `Verified`.

## Risks

- The shell report could be mistaken for interactive UI.
- Workflow action entries could be mistaken for executed workflow buttons.
- CLI-only Visual Blocks proof could be mistaken for Visual Blocks editor UI.
- Package preflight could be mistaken for distribution readiness.
- Customization direction could be overclaimed as maximum customization.
