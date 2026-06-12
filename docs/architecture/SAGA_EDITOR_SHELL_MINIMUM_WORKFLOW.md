# Saga Editor Shell Minimum Workflow

> Status: Editor shell report boundary

This document records the minimum editor-shell inspection report boundary and
profile/view preset customization metadata.

The Saga Editor Shell minimum workflow is a report-backed inspection path over
existing project and tool evidence. It is not a full editor workflow.

## Current Surfaces

Real current surfaces used by this milestone:

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

The minimum this document workflow is:

```txt
Open StarterArena metadata
Show project identity/status
Show validation report path/status
Show available workflow actions as shell entries
Show known limitations
Expose links/commands for runtime smoke, script/blocks evidence, server smoke, and package preflight
```

`SagaEditor` exposes this as a no-UI inspection mode over a caller-provided
project and report output path. The exact build directory and output file are
local evidence details, not architecture truth.

The report contains project identity, editor read-model status, project browser
sections, workflow action command/report references, customization metadata,
diagnostics, and known limitations. The mode does not run the tools; it records
the existing proof commands and whether their expected reports are present.

## Boundary

Product Shell routes workflows. SagaEditor owns future project inspection and
editing views. This document only provides an editor-shell inspection/report proof.
This document only adds report-level profile/view preset metadata and read-only
capability flags.

CLI tools remain the source of truth for project validation, runtime smoke,
SagaScript, CLI-only Visual Blocks evidence, server smoke, and package
preflight. The editor shell must not hide failed commands, missing reports, or
known limitations.

## Customization Direction

Early customization remains limited to:

- view and profile presets;
- panel and workflow visibility metadata;
- read-only capability reporting;
- personal editor preferences later;
- strict shared project truth.

See `SAGA_EDITOR_CUSTOMIZATION_MODEL.md` for the current contract report schema and
non-claims. This milestone does not claim maximum customization or completed
profile editing.

## Non-Claims

This document does not implement or claim:

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

No milestone is marked `Verified`.

## Risks

- The shell report could be mistaken for interactive UI.
- Workflow action entries could be mistaken for executed workflow buttons.
- CLI-only Visual Blocks proof could be mistaken for Visual Blocks editor UI.
- Package preflight could be mistaken for distribution readiness.
- Customization direction could be overclaimed as maximum customization.
