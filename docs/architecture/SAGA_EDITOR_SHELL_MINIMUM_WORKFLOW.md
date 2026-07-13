# Saga Editor Shell Minimum Workflow

> Status: Editor shell report boundary

This document records the minimum editor-shell inspection report boundary and
profile/view preset customization metadata.

The Saga Editor Shell minimum workflow is a report-backed inspection path over
existing project and tool evidence. It is not a full editor workflow.

## Current Surfaces

Real current surfaces used by this milestone:

- `Apps/Saga` owns canonical project selection, recent state, typed target and
  action orchestration, report/distribution views, SagaScript gates, package
  staging, and publish checks. Selecting a project does not launch the Editor;
  a separate explicit action hands off to the external `SagaEditor` host.
- `Apps/Editor` is the thin standalone `SagaEditor` host: process bootstrap,
  Qt static-plugin registration, and delegation only.
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
Show available workflow actions as typed entries
Show known limitations
Expose bounded report paths for runtime smoke, script/blocks evidence, and package preflight
```

`SagaEditor` exposes this as a no-UI inspection mode over a caller-provided
project and report output path. The exact build directory and output file are
local evidence details, not architecture truth.

The clean schema-2 report contains project identity, editor read-model status,
project browser sections, typed action ownership/availability, customization
metadata, diagnostics, and limitations. It has no schema-1 fallback, deprecated
command field, `command: null`, or shell command string. The mode does not run
the tools.

## Boundary

Product Shell routes workflows. SagaEditor owns future project inspection and
editing views. This document only provides an editor-shell inspection/report proof.
This document only adds report-level profile/view preset metadata and read-only
capability flags.

CLI tools remain the source of truth for project validation, bounded runtime
smoke, SagaScript, CLI-only Visual Blocks evidence, and package preflight. The
editor shell must not hide missing reports or known limitations. Repository-only
server evidence is intentionally not emitted as an Editor action.

The Product Launcher does not expose project creation, raw process arguments,
or generic Runtime/server/world/cloud execution. Its bounded Editor action
passes the selected project root across the existing external process boundary;
Editor implementation remains entirely in `SagaEditorLib`.

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
- StarterArena gameplay changes;
- CSharpScriptHost changes.

No milestone is marked `Verified`.

## Risks

- The shell report could be mistaken for interactive UI.
- Workflow action entries could be mistaken for executed workflow buttons.
- CLI-only Visual Blocks proof could be mistaken for Visual Blocks editor UI.
- Package preflight could be mistaken for distribution readiness.
- Customization direction could be overclaimed as maximum customization.
