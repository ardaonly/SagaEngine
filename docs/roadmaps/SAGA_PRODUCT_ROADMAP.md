# Saga Product Roadmap

> Last updated: 2026-05-15
> Status: Proposed active product roadmap
> Target: The primary user-facing Saga executable that owns product lifecycle, project workflow, mode orchestration, preview/build/publish entry points, module mounting, global diagnostics, collaboration entry points, and user-facing integration of editor, runtime, server, SDE, Forge, Prism, asset pipeline, gameplay graph, scripting, and diagnostics systems.
> Scope: Saga product shell, project dashboard, project create/open, workspace resolution, mode host, editor/runtime/server orchestration, preview sessions, build/package/publish workflows, diagnostics routing, collaboration entry points, tool integration boundaries, user profiles, product settings, and failure-state UX.

---

## 0. Roadmap Convention

- `[x]` — Shipped. The note after the item names the files, modules, tests, or integration points that represent completed work.
- `[ ]` — Open. The item describes the finished production state, not temporary scaffolding.
- Shipped product work must include implementation, integration evidence, diagnostics behavior, and clear ownership.
- Open product work must describe observable user-facing behavior, not vague architecture intent.
- `Saga` is the primary user-facing executable.
- `Saga` owns product lifecycle and mode orchestration.
- `Saga` must not become the editor, runtime, server, compiler, build tool, code indexer, asset cooker, or collaboration backend.
- Development compatibility launchers may exist, but they are not the production product workflow.

A product shell that quietly absorbs every nearby subsystem is not a product shell.

It is a blob with a startup screen.

---

## 1. Document Purpose

This document defines the roadmap for `Saga`, the primary product executable.

Saga exists to provide the user-facing product workflow around SagaEngine systems:

- project creation,
- project opening,
- recent project management,
- workspace resolution,
- mode switching,
- editor mounting,
- runtime preview,
- server/dev session orchestration,
- collaboration entry points,
- validation,
- build,
- cook,
- package,
- publish,
- diagnostics routing,
- tool integration.

Saga does not own the implementation internals of those systems.

Correct model:

```txt
Saga
  owns product lifecycle and mode orchestration

SagaEditor
  owns authoring UX

Runtime
  owns game execution

Server
  owns authority/session policy

SDE
  owns deterministic data compilation

Forge
  owns build/cook/package orchestration

Prism
  owns code/artifact intelligence

SagaCollaboration
  owns collaboration services

SagaDiagnostics
  owns runtime diagnostic systems
```

Incorrect model:

```txt
Apps/Editor creates the product shell because it was the first window that worked.
```

That is how prototypes become architectural debt with menu bars.

---

## 2. Companion Documents

| Document | Purpose |
|---|---|
| `DependencyGraph.md` | Dependency direction, ownership boundaries, product/runtime/editor/server split |
| `EDITOR_ROADMAP.md` | SagaEditor authoring module mounted by Saga |
| `ENGINE_ROADMAP.md` | Runtime/server foundations, replication, authority, diagnostics, asset streaming |
| `SHARED_ROADMAP.md` | Neutral contracts and artifact references used across modules |
| `COLLABORATION_ROADMAP.md` | Product collaboration workflows, sessions, presence, permissions, conflicts |
| `DIAGNOSTICS_ROADMAP.md` | Runtime diagnostics, structured reports, crash/lifetime/health visibility |
| `SDE_ROADMAP.md` | Deterministic data compiler, validation, canonical IR, artifacts |
| `FORGE_ROADMAP.md` | Build workflow frontend, validation, cook, package, diagnostics aggregation |
| `PRISM_ROADMAP.md` | Code intelligence, dependency graphs, stale artifact detection |
| `SAGA_GAMEPLAY_GRAPH_ROADMAP.md` | Gameplay graph/block authoring model |
| `AUTHORING_AUTHORITY_MODEL.md` | MMO authority model for graph/block/script authoring |
| `SAGA_SCRIPTING_ROADMAP.md` | C# scripting, block-callable APIs, generated C# preview, script artifacts |
| `ASSET_PIPELINE_ROADMAP.md` | Source asset → import → cook → manifest → runtime streaming pipeline |
| `ClientReplicationFormalSpec.md` | Client replication correctness contract |
| `AssetStreamingImplementation.md` | Implemented runtime asset streaming architecture |

---

## 3. Product Architecture Rule

- [ ] Define `Saga` as the only primary user-facing executable.

  Done means:

  - users create projects through Saga,
  - users open projects through Saga,
  - users choose authoring/runtime/server modes through Saga,
  - users start preview/build/publish workflows through Saga,
  - users enter editor mode through Saga,
  - standalone editor/runtime/server binaries are clearly marked development/test compatibility launchers.

- [ ] Keep `Apps/Editor` development-only.

  Done means:

  - `Apps/Editor` can remain for local editor iteration,
  - it does not define product architecture,
  - it does not own project lifecycle,
  - it does not own product shell,
  - it does not become the user-facing workflow by accident.

- [ ] Keep `Apps/Runtime` and `Apps/Server` development/test oriented where applicable.

  Done means standalone runtime/server launchers can exist for testing, but production workflow is orchestrated by Saga, package manifests, and approved deployment tools.

---

## 4. Product Ownership

### 4.1 Saga Owns

Saga owns:

- process-level product lifecycle,
- `QApplication` ownership where Qt is used in product mode,
- primary main window,
- product shell,
- project dashboard,
- project create/open flows,
- recent project registry,
- workspace resolution entry point,
- mode selection,
- editor/runtime/server module mounting,
- preview session entry points,
- build/cook/package/publish entry points,
- collaboration entry points,
- global diagnostics routing,
- product-level error UX,
- product settings,
- user profile/persona selection,
- product-level update/migration warnings.

---

### 4.2 Saga Does Not Own

Saga must not own:

- editor panels,
- editor graph UI,
- Qt widget implementation inside editor modules,
- runtime simulation internals,
- server authority implementation,
- network transport internals,
- SDE parser/AST/semantic internals,
- Forge build planner internals,
- Prism indexing internals,
- asset importer/cooker internals,
- C# scripting host internals,
- collaboration backend implementation,
- diagnostic storage internals.

Saga coordinates.

It does not become emperor of all implementation.

---

## 5. Dependency Rules

### 5.1 Allowed Dependencies

Allowed dependency directions:

```txt
Saga → SagaShared contracts
Saga → SagaEditor module API
Saga → Runtime module API
Saga → Server module API / launcher boundary
Saga → SagaCollaboration service API
Saga → SagaDiagnostics product-facing API
Saga → Forge executable/service boundary
Saga → SDE executable/service boundary
Saga → Prism report/query boundary
Saga → Asset pipeline service boundary
Saga → Scripting service boundary
```

---

### 5.2 Forbidden Dependencies

Forbidden dependency directions:

```txt
Saga → editor private panel implementation
Saga → runtime private internals
Saga → server private internals
Saga → SDE compiler internals
Saga → Forge planner internals
Saga → Prism index database internals
Saga → asset importer/cooker private implementation
Saga → scripting host private implementation
Saga → collaboration backend internals
```

Forbidden shortcuts:

```txt
Saga directly parses SDE source.
Saga directly compiles C# scripts.
Saga directly cooks textures/meshes.
Saga directly indexes source symbols.
Saga directly owns collaboration conflict engine.
Saga directly mutates runtime/server private state.
```

---

## 6. Product Shell

- [ ] Add Saga product shell.

  Done means Saga has a primary window/surface that can show:

  - project dashboard,
  - project create/open flows,
  - recent projects,
  - workspace status,
  - installed tools status,
  - diagnostics summary,
  - mode entry points,
  - build/publish status.

Expected files:

```txt
Apps/Saga/main.cpp
Saga/include/Saga/Product/SagaApplication.h
Saga/include/Saga/Product/SagaProductShell.h
Saga/include/Saga/Product/SagaMainWindow.h
Saga/src/Product/SagaApplication.cpp
Saga/src/Product/SagaProductShell.cpp
Saga/src/Product/SagaMainWindow.cpp
```

- [ ] Keep shell backend-neutral where practical.

  Done means product-level public contracts do not expose Qt types unnecessarily, even if the first implementation uses Qt.

---

## 7. Project Lifecycle

### 7.1 Project Create

- [ ] Add production project creation workflow.

  Done means users can create a Saga project with:

  - project name,
  - project path,
  - project template,
  - target profile,
  - initial content option,
  - authoring profile,
  - collaboration mode hint,
  - validation of path and manifest.

- [ ] Generate required project structure.

  Initial structure:

```txt
<Project>/saga.project.json
<Project>/Assets/
<Project>/Scripts/
<Project>/.sde/
<Project>/Generated/
<Project>/Build/
<Project>/Packages/
```

- [ ] Validate project creation atomically.

  Done means failed project creation does not leave half-valid project directories without a clear recovery path.

---

### 7.2 Project Open

- [ ] Add production project open workflow.

  Done means users can open projects by:

  - explicit path,
  - recent project list,
  - workspace descriptor,
  - collaboration session reference where applicable.

- [ ] Validate project before entering editor/runtime mode.

  Done means Saga checks:

  - project manifest exists,
  - version is supported,
  - required directories are present or recoverable,
  - toolchain availability is known,
  - asset/script/SDE state can be diagnosed,
  - migration warnings are visible.

---

### 7.3 Project Close

- [ ] Add safe project close workflow.

  Done means closing a project:

  - prompts for unsaved editor state where required,
  - stops preview sessions,
  - detaches runtime/server modules safely,
  - flushes relevant diagnostics,
  - returns user to product shell,
  - preserves recent project state.

---

### 7.4 Recent Projects

- [ ] Add recent project registry.

  Done means Saga tracks:

  - project display name,
  - project path,
  - last opened time,
  - last validation status,
  - missing/unavailable state,
  - optional thumbnail/icon.

- [ ] Keep recent project registry product-owned.

  Done means editor/runtime/tools do not own recent project truth.

---

## 8. Workspace Model

- [ ] Add product workspace descriptor.

  Done means an opened workspace can describe:

  - workspace id,
  - project id,
  - local root,
  - active mode,
  - active authoring profile,
  - dirty state summary,
  - validation state,
  - collaboration state,
  - active preview/build state.

- [ ] Add workspace resolver.

  Done means Saga can resolve:

  - project path,
  - project manifest,
  - SDE root,
  - asset roots,
  - script roots,
  - generated output roots,
  - build output roots,
  - package output roots.

Expected files:

```txt
Saga/include/Saga/Workspace/SagaWorkspace.h
Saga/include/Saga/Workspace/SagaWorkspaceResolver.h
Saga/include/Saga/Workspace/SagaWorkspaceState.h
Saga/src/Workspace/SagaWorkspaceResolver.cpp
```

---

## 9. Mode Host

Saga must host product modes instead of letting each subsystem become its own product.

Required modes:

```txt
Dashboard
Editor
RuntimePreview
ServerPreview
Collaboration
BuildAndPublish
Diagnostics
Settings
```

---

### 9.1 Editor Mode

- [ ] Mount SagaEditor inside Saga product shell.

  Done means:

  - Saga owns primary product window,
  - editor mode mounts into a Saga-owned surface,
  - editor mode can unmount cleanly,
  - closing editor mode returns to dashboard or project shell,
  - editor mode does not create a second product lifecycle.

---

### 9.2 Runtime Preview Mode

- [ ] Add runtime preview entry point.

  Done means users can launch runtime preview from Saga with:

  - selected scene/world/package,
  - selected profile,
  - selected client/server mode,
  - diagnostics capture enabled,
  - clear failure reporting.

---

### 9.3 Server Preview Mode

- [ ] Add local server preview workflow.

  Done means Saga can start/stop local dev server sessions for testing:

  - authoritative gameplay graphs,
  - script artifacts,
  - replication,
  - asset/package manifests,
  - diagnostics.

- [ ] Keep production deployment separate.

  Done means local server preview is not silently treated as production deployment.

---

### 9.4 Build and Publish Mode

- [ ] Add build/publish product mode.

  Done means users can run:

  - validate,
  - build,
  - cook,
  - package,
  - publish check,
  - publish/export where supported.

Saga may initiate these workflows.

Forge owns the build orchestration.

---

### 9.5 Diagnostics Mode

- [ ] Add product-level diagnostics view.

  Done means users can inspect:

  - project validation diagnostics,
  - editor diagnostics,
  - SDE diagnostics,
  - Forge build diagnostics,
  - Prism analysis reports,
  - runtime/server diagnostics,
  - asset/script/graph diagnostics.

Saga routes diagnostics.

Saga does not own every diagnostic storage backend.

---

## 10. Authoring Profiles

- [ ] Add product-level authoring profile selection.

  Required profiles:

```txt
Beginner
Intermediate
Advanced
NetworkDeveloper
ToolDeveloper
```

- [ ] Route profiles into editor/tooling behavior.

  Done means profile affects:

  - visible editor panels,
  - block palette visibility,
  - generated code visibility,
  - graph authority detail level,
  - diagnostics verbosity,
  - build strictness defaults,
  - collaboration display defaults.

- [ ] Keep profiles as product settings, not separate engines.

  Done means beginner mode and advanced mode use the same underlying project/artifact/runtime architecture.

A beginner mode that forks the engine is not accessibility.

It is future migration pain.

---

## 11. Project Templates

- [ ] Add project template system.

  Initial templates:

```txt
Empty Project
MMO Starter
2D Online Prototype
3D Multiplayer Prototype
Editor Tooling Plugin
Dedicated Server Experiment
```

- [ ] Template generation must be explicit.

  Done means templates can create:

  - project manifest,
  - starter `.sde` files,
  - starter scripts,
  - starter assets,
  - starter scenes/graphs,
  - Forge config,
  - package profile defaults.

- [ ] Templates must not hide invalid architecture.

  Done means starter templates use the same authority/build/asset pipeline as real projects.

---

## 12. Tool Integration

### 12.1 SDE Integration

- [ ] Add product-level SDE validation/compile entry points.

  Done means Saga can trigger or route:

  - SDE validate,
  - SDE compile,
  - SDE inspect,
  - SDE diagnostics display,
  - artifact status display.

Saga does not parse SDE source directly.

---

### 12.2 Forge Integration

- [ ] Add product-level build/cook/package entry points.

  Done means Saga can launch Forge workflows and display:

  - build plan summary,
  - running step state,
  - progress where available,
  - diagnostics,
  - artifact/package outputs,
  - failure summaries.

Forge remains the build workflow owner.

---

### 12.3 Prism Integration

- [ ] Add product-level analysis entry points.

  Done means Saga can show or trigger:

  - boundary validation,
  - stale artifact analysis,
  - generated-code origin reports,
  - dependency graph reports,
  - code intelligence summaries.

Prism remains the analysis owner.

---

### 12.4 SagaTools Integration

- [ ] Treat SagaTools as dispatcher, not product shell.

  Done means SagaTools may dispatch tools, but it does not own product workflow or mode switching.

---

## 13. Gameplay Graph Integration

- [ ] Route graph authoring through Saga product state.

  Done means Saga understands graph validation/build status at product level without owning graph UI.

- [ ] Expose graph validation status in product shell.

  Done means the dashboard/build views can show:

  - graph compile status,
  - graph artifact status,
  - authority validation status,
  - generated code freshness,
  - publish-blocking graph diagnostics.

---

## 14. Scripting Integration

- [ ] Route script build status through Saga product state.

  Done means Saga can show:

  - script compile status,
  - script artifact status,
  - block binding status,
  - hot reload availability,
  - script authority diagnostics,
  - publish-blocking script diagnostics.

- [ ] Keep scripting UX available from editor mode.

  Done means script editing belongs to editor authoring surfaces, while product shell shows high-level project/script health.

---

## 15. Asset Pipeline Integration

- [ ] Route asset pipeline status through Saga product state.

  Done means Saga can show:

  - asset import status,
  - cook status,
  - stale artifact status,
  - missing asset references,
  - package asset completeness,
  - publish-blocking asset diagnostics.

- [ ] Keep import authoring in editor mode.

  Done means Saga product shell can start workflows and show status, but editor owns detailed content browser/import/inspector UX.

---

## 16. Collaboration Integration

- [ ] Add product-level collaboration entry points.

  Done means users can:

  - start quick collaboration,
  - join by room code,
  - open team collaboration project,
  - see session state,
  - leave session safely,
  - recover from reconnect/failure states.

- [ ] Keep collaboration implementation in `SagaCollaboration`.

  Done means Saga consumes collaboration services and routes UI state, but does not own presence/locks/conflict engine internals.

- [ ] Show collaboration state in product shell and editor.

  Done means users can see:

  - active session,
  - participants summary,
  - permissions summary,
  - active conflicts,
  - blocked publish/build states caused by collaboration locks/conflicts.

---

## 17. Diagnostics Integration

- [ ] Add product-level diagnostic routing.

  Done means Saga can collect/display diagnostics from:

  - project validation,
  - editor,
  - SDE,
  - Forge,
  - Prism,
  - asset pipeline,
  - scripting,
  - runtime,
  - server,
  - collaboration.

- [ ] Standardize diagnostic severity behavior.

  Required severity levels:

```txt
Info
Warning
Error
Fatal
PublishBlocking
```

- [ ] Add diagnostic navigation.

  Done means clicking diagnostics can navigate to:

  - graph node,
  - script source,
  - SDE source range,
  - asset source/metadata,
  - build step,
  - runtime/server report,
  - collaboration conflict record.

---

## 18. Preview Workflow

- [ ] Add editor preview workflow.

  Done means users can preview project state using editor-preview artifacts and receive diagnostics when artifacts are stale/missing.

- [ ] Add client/server preview workflow.

  Done means users can start a local client/server preview session with:

  - selected build profile,
  - graph/script/SDE artifacts,
  - cooked assets,
  - runtime manifests,
  - server authority validation,
  - replication diagnostics.

- [ ] Reject preview with invalid required artifacts.

  Done means preview fails clearly instead of running stale/random project state.

---

## 19. Build Workflow

- [ ] Add product-facing build workflow.

  Done means users can run build from Saga and see:

  - build profile,
  - build plan summary,
  - active step,
  - logs/diagnostics,
  - artifact output summary,
  - failure reason.

- [ ] Support required profiles.

```txt
editor-preview
dev-client
dev-server
shipping-client
shipping-server
test
```

- [ ] Keep Forge as build orchestrator.

  Done means Saga triggers and displays Forge workflow state; Forge owns plan and step execution.

---

## 20. Publish Workflow

- [ ] Add publish check workflow.

  Done means Saga can run publish readiness checks for:

  - project manifest,
  - SDE artifacts,
  - gameplay graph artifacts,
  - script artifacts,
  - asset artifacts,
  - package manifests,
  - authority validation,
  - collaboration locks/conflicts,
  - diagnostics severity threshold.

- [ ] Add publish blocking state.

  Done means user-facing publish state can explain exactly why publish is blocked.

Example:

```txt
Publish blocked:
- 2 server-only graph artifacts staged in client package
- 1 stale cooked texture artifact
- 3 script compile errors
- 1 unresolved collaboration conflict
```

- [ ] Add publish/export action where supported.

  Done means successful publish produces package outputs and reports through approved build/package systems.

A publish button that does not understand graph/script/asset authority state is not a publish button.

It is a roulette wheel with branding.

---

## 21. Product Settings

- [ ] Add product settings model.

  Done means Saga can store settings for:

  - default authoring profile,
  - default build profile,
  - tool paths where needed,
  - diagnostics verbosity,
  - editor layout preference reference,
  - collaboration display preference,
  - preview configuration,
  - recent project behavior.

- [ ] Separate user settings from project settings.

  Done means project state does not silently absorb machine-local user preferences unless explicitly intended.

---

## 22. Project Manifest

- [ ] Define `saga.project.json` or equivalent project manifest.

  Done means project manifest describes:

  - project id,
  - display name,
  - version,
  - engine/tool compatibility,
  - SDE root,
  - asset roots,
  - script roots,
  - generated roots,
  - build profiles reference,
  - package profiles reference,
  - collaboration mode hint,
  - default authoring profile.

- [ ] Validate project manifest.

  Done means invalid manifest blocks project open/build/publish with actionable diagnostics.

---

## 23. Module Mounting

- [ ] Define module mounting API.

  Done means Saga can mount/unmount:

  - editor mode,
  - runtime preview,
  - server preview,
  - diagnostics view,
  - collaboration view.

Expected files:

```txt
Saga/include/Saga/Modes/IMode.h
Saga/include/Saga/Modes/ModeHost.h
Saga/include/Saga/Modes/ModeDescriptor.h
Saga/src/Modes/ModeHost.cpp
```

- [ ] Require deterministic unmount.

  Done means leaving a mode releases resources, flushes diagnostics, and returns to safe product state.

---

## 24. Error and Failure UX

- [ ] Add product-level failure state model.

  Done means failures have:

  - source system,
  - severity,
  - user-readable message,
  - technical details,
  - suggested action,
  - related diagnostic ids,
  - retry/recover options where safe.

- [ ] Avoid vague failure messages.

Forbidden messages:

```txt
Failed.
Something went wrong.
Build error.
Invalid project.
```

Preferred messages:

```txt
SDE compile failed: 3 graph authority errors in QuestReward.sde.
Forge package failed: stale cooked artifact terrain_albedo.texture.
Runtime preview failed: missing asset manifest for dev-client profile.
```

A product shell that cannot explain failure is just a loading screen with anxiety.

---

## 25. Testing Strategy

### 25.1 Product Shell Tests

- [ ] Add product shell tests.

  Required coverage:

  - app starts,
  - dashboard loads,
  - recent projects render,
  - project open failure displays diagnostics,
  - mode switch works,
  - mode unmount works.

---

### 25.2 Project Lifecycle Tests

- [ ] Add project lifecycle tests.

  Required coverage:

  - create project,
  - open project,
  - close project,
  - invalid manifest,
  - missing directories,
  - recent registry update,
  - migration warning path.

---

### 25.3 Tool Integration Tests

- [ ] Add tool integration tests.

  Required coverage:

  - SDE validate route,
  - Forge build route,
  - Prism stale route,
  - diagnostics display,
  - failed tool invocation produces product-level error.

---

### 25.4 Mode Host Tests

- [ ] Add mode host tests.

  Required coverage:

  - mount editor mode,
  - unmount editor mode,
  - mount diagnostics mode,
  - runtime preview failure cleanup,
  - server preview failure cleanup.

---

### 25.5 Publish Gate Tests

- [ ] Add publish readiness tests.

  Required coverage:

  - valid publish check,
  - graph authority failure blocks publish,
  - script compile failure blocks publish,
  - stale asset blocks publish,
  - collaboration conflict blocks publish,
  - package manifest mismatch blocks publish.

---

## 26. MVP Vertical Slice

The first Saga product vertical slice should connect project open, editor mode, validation, preview, and publish readiness.

Required scenario:

```txt
Open MMO Starter project → edit quest reward graph → validate → build editor-preview → run local client/server preview → publish check
```

Required behavior:

1. User opens Saga.
2. Product dashboard shows recent projects and create/open actions.
3. User opens MMO Starter project.
4. Saga resolves workspace and validates project manifest.
5. Saga mounts editor mode.
6. User edits quest reward graph in editor.
7. SDE validates graph and emits diagnostics.
8. Forge builds editor-preview profile.
9. Asset/script/graph artifacts are staged.
10. Saga starts local client/server preview.
11. Server executes authoritative quest reward path.
12. Client displays replicated result.
13. Saga runs publish check.
14. Publish check reports either ready state or exact blocking diagnostics.

This slice proves Saga is a product shell instead of a pile of tools with a window.

---

## 27. Non-Goals

Saga product shell must not become:

- the editor implementation,
- the runtime implementation,
- the server implementation,
- the SDE compiler,
- the Forge build planner,
- the Prism indexer,
- the asset cooker,
- the scripting host,
- the collaboration backend,
- a dumping ground for systems without ownership.

Saga should answer:

```txt
What is the user doing, in which project, in which mode, with what validation state?
```

It should not answer:

```txt
How does every subsystem internally work?
```

---

## 28. Risk Register

### 28.1 Risk: Apps/Editor Becomes Product Shell

Mitigation:

- keep Apps/Editor development-only,
- product users enter editor through Saga,
- document and test Saga mode mounting,
- avoid adding product workflows to editor launcher.

---

### 28.2 Risk: Saga Absorbs Tool Internals

Mitigation:

- use service/executable boundaries,
- consume reports/artifacts/contracts,
- forbid direct parser/build/indexer/cooker internals.

---

### 28.3 Risk: Product UX Hides Build/Validation Reality

Mitigation:

- show diagnostics clearly,
- preserve technical details,
- expose exact publish blockers,
- do not collapse all failures into generic messages.

---

### 28.4 Risk: Beginner Profile Forks Architecture

Mitigation:

- use one project/artifact/runtime model,
- hide complexity through profiles,
- preserve advanced expansion/inspection paths,
- avoid separate beginner-only runtime semantics.

---

### 28.5 Risk: Preview Runs Stale Project State

Mitigation:

- preview requires valid editor-preview artifacts,
- stale artifacts produce diagnostics,
- runtime preview checks manifests,
- Saga displays exact stale/missing state.

---

## 29. Suggested File Targets

Expected product files:

```txt
Apps/Saga/main.cpp
Saga/include/Saga/Product/SagaApplication.h
Saga/include/Saga/Product/SagaProductShell.h
Saga/include/Saga/Product/SagaMainWindow.h
Saga/include/Saga/Product/SagaProductConfig.h
Saga/src/Product/SagaApplication.cpp
Saga/src/Product/SagaProductShell.cpp
Saga/src/Product/SagaMainWindow.cpp
Saga/src/Product/SagaProductConfig.cpp
```

Expected project/workspace files:

```txt
Saga/include/Saga/Project/SagaProjectDescriptor.h
Saga/include/Saga/Project/SagaProjectManifest.h
Saga/include/Saga/Project/SagaProjectService.h
Saga/include/Saga/Project/RecentProjectRegistry.h
Saga/include/Saga/Workspace/SagaWorkspace.h
Saga/include/Saga/Workspace/SagaWorkspaceResolver.h
Saga/include/Saga/Workspace/SagaWorkspaceState.h
Saga/src/Project/SagaProjectService.cpp
Saga/src/Project/RecentProjectRegistry.cpp
Saga/src/Workspace/SagaWorkspaceResolver.cpp
```

Expected mode files:

```txt
Saga/include/Saga/Modes/IMode.h
Saga/include/Saga/Modes/ModeHost.h
Saga/include/Saga/Modes/ModeDescriptor.h
Saga/include/Saga/Modes/EditorModeAdapter.h
Saga/include/Saga/Modes/RuntimePreviewMode.h
Saga/include/Saga/Modes/DiagnosticsMode.h
Saga/src/Modes/ModeHost.cpp
Saga/src/Modes/EditorModeAdapter.cpp
Saga/src/Modes/RuntimePreviewMode.cpp
Saga/src/Modes/DiagnosticsMode.cpp
```

Expected tool integration files:

```txt
Saga/include/Saga/Tools/SdeService.h
Saga/include/Saga/Tools/ForgeService.h
Saga/include/Saga/Tools/PrismService.h
Saga/include/Saga/Tools/ToolDiagnosticAdapter.h
Saga/src/Tools/SdeService.cpp
Saga/src/Tools/ForgeService.cpp
Saga/src/Tools/PrismService.cpp
Saga/src/Tools/ToolDiagnosticAdapter.cpp
```

Expected build/publish files:

```txt
Saga/include/Saga/Build/BuildProfile.h
Saga/include/Saga/Build/BuildRequest.h
Saga/include/Saga/Build/BuildStatus.h
Saga/include/Saga/Publish/PublishCheck.h
Saga/include/Saga/Publish/PublishBlocker.h
Saga/include/Saga/Publish/PublishReport.h
Saga/src/Build/BuildStatus.cpp
Saga/src/Publish/PublishCheck.cpp
Saga/src/Publish/PublishReport.cpp
```

Expected diagnostics files:

```txt
Saga/include/Saga/Diagnostics/ProductDiagnostic.h
Saga/include/Saga/Diagnostics/DiagnosticRouter.h
Saga/include/Saga/Diagnostics/DiagnosticNavigation.h
Saga/src/Diagnostics/DiagnosticRouter.cpp
Saga/src/Diagnostics/DiagnosticNavigation.cpp
```

Expected project manifest:

```txt
<Project>/saga.project.json
```

Example concept:

```json
{
  "projectId": "example-mmo-starter",
  "displayName": "Example MMO Starter",
  "projectVersion": "0.1.0",
  "sdeRoot": ".sde",
  "assetRoots": ["Assets"],
  "scriptRoots": ["Scripts"],
  "generatedRoot": "Generated",
  "buildRoot": "Build",
  "packageRoot": "Packages",
  "defaultAuthoringProfile": "Advanced",
  "defaultBuildProfile": "editor-preview"
}
```

---

## 30. Decision Summary

Preserve these decisions:

```txt
Saga is the primary user-facing executable.
Saga owns product lifecycle and mode orchestration.
Saga owns QApplication/product window in production Qt mode.
SagaEditor is mounted by Saga as an authoring module.
Apps/Editor is development compatibility, not product workflow.
Runtime and Server are orchestrated/mounted/launched through product workflows where needed.
SDE, Forge, Prism remain standalone/tool-owned systems.
Saga consumes artifacts, reports, diagnostics, and service APIs.
Saga must explain validation/build/publish failure clearly.
Publish must understand graph, authority, script, asset, collaboration, and package state.
Beginner/advanced modes are profiles over one architecture, not separate engines.
```

Saga's job is to make the whole engine feel like one coherent product without destroying the ownership boundaries that make the engine survivable.

If Saga succeeds, users see a product.

If Saga fails, users see a folder full of impressive tools and ask which one is the engine.
