# Saga Product Shell Workflow Contract

Status: bounded Product Shell ownership and workflow contract.

The Saga Product Shell is a:

```txt
launcher/dashboard/workflow router
```

This document is a contract over existing entry points. The Product Shell owns
a bounded launcher window, project/session models, typed target preparation,
and process handoff. Workflow smokes remain report-backed commands. They do not
implement editor workflow panels, package distribution,
runtime logic, server logic, SagaScript behavior, Visual Blocks
editor UI, collaboration services, cloud workspace, or real-time team editing.

## Role

The Product Shell may route a user through local workflows that already have
CLI or process-level evidence. Its first honest workflow is report-first: every
tool handoff preserves typed arguments, exit classification, report path, and
diagnostics path or diagnostics payload. It does not execute shell command
strings.

The Product Shell must not claim success without a real tool report. A failed
tool remains visible as a failed workflow step. Package preflight failure is a
current limitation, not a hidden pass.

## Non-Roles

The Product Shell is not:

- full editor;
- Visual Blocks editor UI;
- package builder;
- distribution pipeline;
- enterprise workspace server;
- runtime engine;
- C# compiler;

SagaEditor owns future editing and inspection UI. This document does not wire that UI
to the workflow steps below.

## Existing Product Boundary

The repository already contains an `Apps/Saga` product shell boundary:

- `Apps/Saga/SagaProjectSystem.*` creates and opens product shell project
  manifests, tracks recent projects, and manages local-only session labels.
- `Apps/Saga/SagaProductHost.*` prepares target metadata for editor, runtime,
  and server roles without owning those modules.
- `Apps/Saga/SagaProcessService.*` is the only Product-owned `QProcess`
  implementation. It accepts typed Editor, Runtime, Forge, or SagaScript
  identities, an environment-key allowlist, and bounded timeouts; it never
  invokes a command shell.
- `Apps/Saga/SagaProcessLauncher.*` adapts prepared product targets to that
  service.
- `Apps/Saga/SagaSessionModel.*` defines product workspace, target, and
  diagnostic data.
- `Apps/Saga/SagaLauncherWindow.*` owns the bounded launcher UI. Opening a
  project hands off to the external `SagaEditor` executable.
- `Apps/Saga/SagaScriptGate.*`, `Apps/Saga/SagaPackageStaging.*`, and
  `Apps/Saga/SagaPublishReadiness.*` contain product-owned gate or report
  services, but this document does not expand them.

`Apps/Editor` is a thin editor executable host; editor implementation and
inspection live in `Editor/` and `SagaEditorLib`. `Apps/EditorLab` remains a
scenario/development shell and its optional bridge is off by default.

## Workflow Smoke

This document describes a Product Shell workflow smoke over a caller-provided
project, selected profile, and report output path. Exact build directories and
report paths are local evidence details, not architecture truth.

The report contains project metadata, selected profile, workflow step command
references, expected report paths, diagnostics, known limitations, non-claims,
and `verified: false`. It does not execute the referenced workflow tools.
Missing reports and package preflight limitations remain visible.

## Local Workspace Transaction Smoke

This document describes a local workspace transaction smoke over a
caller-provided project, workspace, actor, operation, and report output path.

The report records a read-only local transaction preview over StarterArena. It
does not write durable collaboration metadata, mutate project files, start a
collaboration server, or provide cloud/team synchronization.

## Workflow Contract

The first honest Product Shell workflow is:

1. Open StarterArena.
2. Validate project metadata.
3. Run runtime smoke.
4. Run SagaScript analyze and compile.
5. Generate and read block projection plus safe edit evidence through the
   CLI-only Visual Blocks chain.
6. Show or read diagnostics and reports.
7. Run package preflight when requested.
8. Surface the unsupported server target and other limitations without a
   launch instruction.

The CLI tools remain the source of truth for proof. A Product Shell UI may make
these steps easier to launch and read later, but it may not replace or mask the
underlying reports.

## Open Project

Current project truth for this workflow is:

```txt
samples/StarterArena/StarterArena.sagaproj
```

The shell contract may treat this as the selected project path. It must also
surface that StarterArena currently has no launch profiles in the project
manifest and that runtime/server evidence uses bounded smoke entry points
directly.

## Validate Project

Existing validation entry point:

```bash
nix-shell --run "Tools/SagaProjectKit/sagaproject validate --project samples/StarterArena/StarterArena.sagaproj --out /tmp/starter_arena_validate.json"
```

The Product Shell must show the command, process exit status, and
`/tmp/starter_arena_validate.json`. Validation failure remains a failed step.

## Edit

Editing and inspection belong to SagaEditor, not to hidden Product Shell
behavior. Product Shell prepares an external `SagaEditor --workspace ...
--profile ...` request and does not link or compile Editor implementation.

The Product Shell may route to editor mode through the existing product/editor
boundary, but this document does not claim a completed dashboard, inspector,
scene editor, Visual Blocks editor, or source editor.

## Play / Runtime Smoke

Existing runtime smoke evidence is a bounded headless runtime command over a
caller-provided project, smoke frame count, fixed timestep, and report output
path.

The Product Shell must surface the exit status and report path. The smoke path
is a bounded headless proof, not a general client launch or interactive gameplay
claim.

## Script / Blocks

Existing SagaScript analyze and compile entry points:

```bash
Tools/SagaScript/sagascript analyze --source samples/StarterArena/Scripts --out /tmp/starter_arena_sagascript
Tools/SagaScript/sagascript compile --source samples/StarterArena/Scripts --out /tmp/starter_arena_sagascript/Manifests --artifacts-out /tmp/starter_arena_sagascript/Artifacts/Scripts --project-root samples/StarterArena --assembly-name StarterArenaScripts --diagnostics /tmp/starter_arena_sagascript/sagascript_diagnostics.json --json
```

Existing Visual Blocks proof is CLI-only. The supported chain is:

```txt
compatibility-profile -> project-blocks -> plan-block-edit -> apply-block-edit -> analyze -> compile
```

The Product Shell contract may show the generated reports and diagnostics from
that chain. It must not describe this as Visual Blocks editor UI, arbitrary C#
roundtrip, or a completed authoring surface.

## Unsupported Server Target

The Product Shell may display declarative future server metadata, but target
resolution returns a stable unsupported diagnostic and never constructs a
process request. Repository-only server-authority fixtures are not launcher
actions and do not establish a dedicated-server product executable.

## Diagnostics

The workflow dashboard may read and link to local outputs such as:

- `/tmp/*_smoke.json`;
- SagaScript diagnostics reports;
- project validation reports;
- server diagnostics directories.

Diagnostics visibility must preserve source command, exit status, report path,
and failure state. Missing reports are a failure or limitation, not success.

## Package Preflight

Existing package preflight entry point:

```bash
scripts/package-linux-saga
```

This script is preflight-only. It is expected to fail honestly until real
distribution inputs and output layout exist. The Product Shell may expose that
failure as a known limitation when the user requests package preflight, but it
must not treat it as package readiness or distribution readiness.

## Customization Direction

Early Product Shell customization is limited to direction, not implementation:

- profile and view presets;
- panel and workflow visibility;
- personal layout later;
- shared project truth remains strict.

This document does not claim maximum customization, shared customizable workspaces,
or implemented profile editing.

## Known Limitations

- This document is a docs/evidence-only workflow contract.
- This document is a no-UI Product Shell workflow smoke report.
- This document is a no-UI local workspace transaction boundary report.
- No Product Shell dashboard workflow UI is implemented by this milestone.
- No UI wiring is added for validation, smoke, scripting, blocks, diagnostics,
  or package preflight.
- Workflow smoke command entries are references and are not executed by the
  report.
- Local workspace transaction smoke is read-only and report-only.
- StarterArena has no launch profiles in its project manifest.
- Visual Blocks evidence remains CLI-only.
- Package preflight is not package or distribution readiness.
- The shell must never hide failed tools or missing reports.
- No milestone is marked `Verified`.
