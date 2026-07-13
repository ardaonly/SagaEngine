# Saga Product Shell Workflow Contract

Status: bounded Product Shell ownership and typed workflow-status contract.

The Saga Product Shell is a bounded launcher/dashboard/workflow router. It owns
project and session models, typed target preparation, process handoff, and
report aggregation. It does not own editor implementation, runtime logic,
server logic, SagaScript behavior, Visual Blocks editor UI, collaboration
services, cloud workspace, or real-time team editing.

## Role

The Product Shell may route a user through local workflows that already have
tool, process, or report evidence. Process handoff uses typed arguments through
the allowlisted Product process boundary. The workflow-smoke report itself is
different: it records typed action identities, ownership, availability,
status, and expected report locations without embedding or executing an
invocation.

The Product Shell must not claim success without real evidence. Missing reports
remain visible. Package preflight is bounded evidence, not package or
distribution readiness.

## Non-Roles

The Product Shell is not:

- a full editor;
- Visual Blocks editor UI;
- a package builder or distribution pipeline;
- an enterprise workspace server;
- the runtime engine;
- a C# compiler;
- a dedicated-server product host.

SagaEditor owns editing and inspection UI. Generic server execution remains
unsupported, and repository-only server evidence is not a Product Shell action.

## Existing Product Boundary

The repository contains an explicit `Apps/Saga` product boundary:

- `SagaProjectCatalog.*` opens canonical schema-0 `.sagaproj` inputs and treats
  `saga.project.json` only as legacy compatibility.
- `SagaRecentProjectsStore` persists bounded project metadata in the injected
  platform application-config path. `SagaProjectSystem.*` remains a temporary
  package/publish compatibility adapter and is not the launcher catalog.
- `SagaLauncherModel.*`, `SagaLauncherTargets.*`, and
  `SagaLauncherController.*` own typed state, exact target resolution,
  single-action scheduling, and cancellation without Qt widget dependencies.
- `SagaProductHost.*` prepares typed Editor and Runtime targets and returns a
  stable unsupported result for server targets.
- `SagaProcessService.*` is the only Product-owned `QProcess` implementation.
  It accepts allowlisted executable identities, a bounded environment policy,
  and bounded timeouts; it never invokes a shell.
- `SagaProcessLauncher.*` adapts prepared Product targets to that service.
- `SagaSessionModel.*` defines Product workspace, target, and diagnostic data.
- `SagaLauncherWindow.*` renders controller snapshots and dispatches typed
  actions. Project selection and the external `SagaEditor` handoff are separate
  operations.
- `SagaScriptGate.*`, `SagaPackageStaging.*`, and
  `SagaPublishReadiness.*` contain Product-owned gate or report services but do
  not expand the workflow-smoke claim.

`Apps/Editor` is a thin executable host. Editor implementation and project
inspection live in `Editor/` and `SagaEditorLib`. `Apps/EditorLab` remains a
development scenario surface, and its optional bridge is off by default.

## Workflow Smoke Schema 2

The workflow-smoke output is clean schema 2. There is no schema 1 fallback.
Its top-level contract is:

```txt
schemaVersion: 2
tool: Saga
action: workflow-smoke
status
verified: false
project
profile
workflowActions
reportReferences
diagnostics
knownLimitations
nonClaims
```

Every `workflowActions` entry contains:

```txt
id
status
actionKind
owner
expectedReportPath
availability
unavailableReason or diagnosticId when applicable
```

The exact current action identities are:

```txt
project_validation
editor_inspection
runtime_smoke
sagascript_analyze_compile
visual_blocks_cli_chain
package_preflight
known_limitations
```

The report contains no shell text, legacy workflow-step shape, or separate
invocation list. `reportReferences` maps an action identity and observed status
to its expected report path. The report does not execute any action.

## Local Workspace Transaction Smoke

The local workspace transaction smoke is a separate caller-provided project,
workspace, actor, operation, and report-output contract. It records a read-only
local transaction preview. It does not write durable collaboration metadata,
mutate project files, start a collaboration server, or provide cloud/team
synchronization.

## Workflow Actions

### Project Validation

`project_validation` is owned by `sagaproject`. The action records availability,
the current report status, and the expected validation report. Validation
failure remains visible and is never converted into a pass.

### Editor Inspection

`editor_inspection` is owned by `SagaEditor`. Product Shell prepares an external
Editor request and does not link or compile Editor implementation. The action
is a status/report reference; Editor owns inspection behavior and its clean
schema-2 report.

This does not claim a completed dashboard, inspector, scene editor, Visual
Blocks editor, or source editor.

### Runtime Smoke

`runtime_smoke` is owned by `SagaRuntime`. It refers to bounded StarterArena
headless evidence over a caller-selected project and report path. It is not a
generic project-execution or interactive gameplay claim.

### Script Analysis and Compilation

`sagascript_analyze_compile` is owned by `sagascript`. It refers to the selected
source-analysis and artifact-report evidence. Product Shell does not own the
compiler or embed a tool invocation in the workflow-smoke report.

### Visual Blocks CLI Chain

`visual_blocks_cli_chain` is owned by `sagascript` and has `cli_only`
availability. It refers to projection and safe-edit evidence. It does not
describe Visual Blocks editor UI, arbitrary C# roundtrip, or a completed
authoring surface.

### Package Preflight

`package_preflight` is owned by `package-linux-saga` and has `preflight_only`
availability. Its limitation diagnostic states that preflight is not package
or distribution readiness. The action does not change the exact three-app and
three-public-tool package whitelist.

### Known Limitations

`known_limitations` is a Product status reference. It keeps the workflow's
bounded evidence and non-claims visible without turning unavailable product
features or repository development fixtures into launcher actions.

## Unsupported Server Target

The Product Shell may display declarative future server metadata, but target
resolution returns a stable unsupported diagnostic and never constructs a
process request. No server action appears in the schema-2 workflow action set.
Repository-only authority fixtures do not establish a dedicated-server product
executable or generic server support.

## Diagnostics

The Product Shell may read and link to local validation, runtime, Editor,
SagaScript, and package-preflight reports. The workflow-smoke report preserves
typed action identity, observed status, availability, and report path. Missing
reports are a limitation or failure, never implicit success.

## Known Limitations

- Workflow smoke is report-only and does not execute its typed actions.
- The minimum Product Launcher Foundation UI exists; a finished or polished
  dashboard/workflow product is not completed by this milestone.
- Local workspace transaction smoke is read-only and report-only.
- StarterArena has no general launch-profile or generic runtime-execution claim.
- Visual Blocks evidence remains CLI-only.
- Package preflight is not package or distribution readiness.
- Repository-only server evidence is not a Product Shell workflow action.
- Generic server execution and a dedicated-server product executable remain
  unsupported.
- No phase is marked `Verified` by this report.
