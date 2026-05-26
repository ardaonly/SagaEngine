# SagaEngine Dependency Graph

> Last updated: 2026-05-15
> Status: Active architecture contract
> Target: A hard dependency and ownership map for Saga product, editor, runtime, server, shared contracts, collaboration, SDE, Forge, Prism, asset pipeline, scripting, and tools.
> Scope: Module ownership, allowed dependency directions, forbidden dependency directions, public/private header boundaries, tool boundaries, artifact/report boundaries, package/runtime boundaries, and CI enforcement expectations.

---

## 0. Document Status

This document is an architecture contract, not a suggestion list.

Every roadmap and implementation plan should respect this dependency graph unless this document is intentionally updated.

Dependency mistakes do not look dangerous at first.

They look like:

```txt
I'll just include this one header.
```

Then six months later the runtime includes editor UI, SDE includes engine headers, Forge knows private compiler internals, and nobody can explain why changing a toolbar breaks a server build.

That is not complexity.

That is dependency rot.

---

## 1. Core Principle

Saga architecture is organized by ownership:

```txt
Saga
  product lifecycle and mode orchestration

SagaEditor
  authoring UX

SagaEngine Runtime
  game/client execution

SagaServer / Server runtime layer
  authoritative multiplayer execution

SagaShared
  neutral contracts only

SagaCollaboration
  collaboration implementation

SDE
  standalone deterministic compiler

Forge
  build workflow frontend/orchestrator

Prism
  code/artifact intelligence analyzer

AssetPipeline
  source asset import/cook/artifact generation

Scripting Toolchain
  script validate/compile/binding generation

SagaTools
  thin tool dispatcher
```

The rule:

```txt
A module may consume contracts and artifacts.
A module must not own another module's implementation.
```

---

## 2. Top-Level Dependency Shape

Preferred high-level dependency graph:

```txt
                    ┌────────────────────┐
                    │        Saga        │
                    │  Product Shell     │
                    └─────────┬──────────┘
                              │
      ┌───────────────────────┼────────────────────────┐
      │                       │                        │
      ▼                       ▼                        ▼
┌──────────────┐       ┌──────────────┐          ┌──────────────┐
│ SagaEditor   │       │ Runtime      │          │ Server       │
│ Authoring UX │       │ Client/Game  │          │ Authority    │
└──────┬───────┘       └──────┬───────┘          └──────┬───────┘
       │                      │                         │
       └──────────────┬───────┴──────────────┬──────────┘
                      ▼                      ▼
              ┌──────────────┐       ┌──────────────────┐
              │ SagaShared   │       │ SagaCollaboration │
              │ Contracts    │       │ Implementation    │
              └──────────────┘       └──────────────────┘

Tools / Build Side:

┌──────────────┐   invokes/reads   ┌──────────────┐
│ Forge        │ ────────────────▶ │ SDE          │
│ Build Flow   │                   │ Compiler     │
└──────┬───────┘                   └──────────────┘
       │
       │ invokes/reads
       ▼
┌──────────────┐
│ Prism        │
│ Analysis     │
└──────────────┘
       ▲
       │ reads manifests/reports/source
       │
┌──────┴────────┐      ┌──────────────────┐      ┌──────────────────┐
│ AssetPipeline │      │ Scripting Tooling│      │ SagaTools        │
│ Import/Cook   │      │ Compile/Bindings │      │ Dispatcher       │
└───────────────┘      └──────────────────┘      └──────────────────┘
```

Important:

```txt
SDE must not depend on Saga modules.
Runtime/server must not depend on tool internals.
SagaShared must not depend upward.
Editor must not own product lifecycle.
Forge must not own compiler/cooker/analyzer internals.
Prism must not mutate/build/cook artifacts.
```

---

## 3. Module Ownership Table

| Module / Tool         | Owns                                                                                     | Does Not Own                                                                               |
| --------------------- | ---------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------ |
| `Saga`                | Product shell, project lifecycle, mode orchestration, preview/build/publish entry points | Editor internals, runtime internals, compiler internals, build internals                   |
| `SagaEditor`          | Authoring UI, panels, graph UX, asset UX, script UX, diagnostics display                 | Product lifecycle, runtime execution, server authority, SDE/Forge/Prism internals          |
| `SagaEngine Runtime`  | Client/game execution, package consumption, asset streaming, prediction/interpolation    | Editor UI, asset import/cook, build pipeline, SDE internals                                |
| `SagaServer`          | Server authority, session validation, authoritative graph/script execution               | Editor UI, product shell, SDE/Forge/Prism internals                                        |
| `SagaShared`          | Neutral ids, descriptors, manifests, diagnostics payloads                                | Implementation, UI, runtime behavior, compiler/build/analyzer logic                        |
| `SagaCollaboration`   | Sessions, presence, permissions, claims, locks, conflicts, change streams                | Editor UI, product shell, runtime/server authority                                         |
| `SDE`                 | Standalone deterministic compiler, source language, IR, artifacts, diagnostics           | Saga-specific runtime/editor/server behavior                                               |
| `Forge`               | Build orchestration, adapters, build plans, package staging, reports, publish checks     | SDE compiler internals, Prism internals, asset cooker internals, script compiler internals |
| `Prism`               | Source/artifact analysis, stale checks, boundary checks, reports                         | Build execution, compilation, cooking, package staging                                     |
| `AssetPipeline`       | Source asset import/cook, cooked artifacts, asset manifests                              | Runtime streaming cache, editor UI, Forge internals                                        |
| `Scripting Toolchain` | Script validation/compile, binding manifests, generated code metadata                    | Runtime script host, editor script UI, Forge internals                                     |
| `SagaTools`           | Tool registry/dispatch                                                                   | Tool implementation, product shell, build planner                                          |

---

## 4. Allowed Dependency Directions

### 4.1 Product Layer

Allowed:

```txt
Saga → SagaShared
Saga → SagaEditor public module API
Saga → Runtime public preview API
Saga → Server launcher/service boundary
Saga → SagaCollaboration public service API
Saga → Forge command/service boundary
Saga → SDE command/public facade boundary
Saga → Prism command/report boundary
Saga → AssetPipeline service/report boundary
Saga → Scripting service/report boundary
```

Forbidden:

```txt
Saga → Editor private panels
Saga → Runtime private internals
Saga → Server private internals
Saga → SDE parser/AST internals
Saga → Forge planner internals
Saga → Prism internal index database
Saga → AssetPipeline private importer/cooker internals
Saga → Scripting compiler private internals
```

Rationale:

```txt
Saga coordinates product workflow.
Saga does not become the implementation owner of every mode/tool.
```

---

### 4.2 Editor Layer

Allowed:

```txt
SagaEditor → SagaShared
SagaEditor → SagaCollaboration public service API
SagaEditor → SDE command/public facade boundary
SagaEditor → Forge command/report boundary
SagaEditor → Prism report/query boundary
SagaEditor → Runtime preview API boundary
SagaEditor → AssetPipeline service/report boundary
SagaEditor → Scripting service/report boundary
```

Forbidden:

```txt
SagaEditor → Saga product shell internals
SagaEditor → Runtime private internals
SagaEditor → Server private internals
SagaEditor → SDE parser/AST/semantic internals
SagaEditor → Forge planner internals
SagaEditor → Prism internal index database
SagaEditor → AssetPipeline private importer/cooker implementation
SagaEditor → Scripting compiler private implementation
SagaEditor public headers → Qt types
```

Rationale:

```txt
Editor displays and edits authoring state.
Editor does not own product lifecycle, runtime/server truth, or tool internals.
```

---

### 4.3 Runtime Layer

Allowed:

```txt
Runtime → Engine/Core
Runtime → SagaShared contracts
Runtime → package manifests
Runtime → asset manifests
Runtime → graph artifacts
Runtime → script binding manifests
Runtime → authority manifests
Runtime → runtime-ready asset artifacts
Runtime → SagaCollaboration service API only where explicitly approved
```

Forbidden:

```txt
Runtime → SagaEditor
Runtime → Saga product shell internals
Runtime → SDE compiler internals
Runtime → Forge internals
Runtime → Prism internals
Runtime → AssetPipeline implementation
Runtime → Scripting compiler implementation
Runtime → Server private authority implementation except approved boundary
```

Rationale:

```txt
Runtime consumes validated packages/artifacts.
Runtime does not author, compile, cook, index, or package them.
```

---

### 4.4 Server Layer

Allowed:

```txt
Server → Engine/Core
Server → SagaShared contracts
Server → package manifests
Server → graph artifacts
Server → script binding manifests
Server → authority manifests
Server → server data/asset manifests
Server → backend/service APIs
Server → SagaCollaboration service API only where explicitly approved
```

Forbidden:

```txt
Server → SagaEditor
Server → Saga product shell internals
Server → SDE compiler internals
Server → Forge internals
Server → Prism internals
Server → AssetPipeline implementation
Server → Scripting compiler implementation
Server → Qt UI
```

Rationale:

```txt
Server owns authoritative execution.
Server must not depend on authoring/build/analyzer implementation.
```

---

### 4.5 Shared Layer

Allowed:

```txt
SagaShared → C++ standard library
SagaShared → approved low-level serialization/hash/value primitives
```

Forbidden:

```txt
SagaShared → Saga
SagaShared → SagaEditor
SagaShared → Runtime private internals
SagaShared → Server
SagaShared → SagaCollaboration implementation
SagaShared → SDE internals
SagaShared → Forge internals
SagaShared → Prism internals
SagaShared → AssetPipeline implementation
SagaShared → Scripting implementation
SagaShared → Qt UI
```

Rationale:

```txt
SagaShared is common language, not common implementation.
```

---

### 4.6 Collaboration Layer

Allowed:

```txt
SagaCollaboration → SagaShared contracts
SagaCollaboration → backend/transport adapters
Saga → SagaCollaboration public API
SagaEditor → SagaCollaboration public API
Forge → SagaShared collaboration reports/contracts
```

Forbidden:

```txt
SagaCollaboration → SagaEditor UI
SagaCollaboration → Saga product shell internals
SagaCollaboration → Runtime private internals
SagaCollaboration → Server private internals
SagaCollaboration → SDE internals
SagaCollaboration → Forge planner internals
SagaCollaboration → Prism internals
SagaCollaboration → AssetPipeline implementation
SagaCollaboration → Scripting compiler implementation
```

Rationale:

```txt
Collaboration owns session/resource state.
Editor/product display and orchestrate it.
```

---

## 5. Tool Dependency Rules

### 5.1 SDE

Allowed:

```txt
SDE → standard library / standalone compiler-safe dependencies
External consumers → SDE CLI / public compiler facade / artifacts / manifests
```

Forbidden:

```txt
SDE → Saga
SDE → SagaEngine
SDE → SagaEditor
SDE → SagaServer
SDE → SagaShared
SDE → SagaCollaboration
SDE → Forge
SDE → Prism
SDE → SagaTools
SDE → AssetPipeline implementation
SDE → Scripting implementation
SDE → Qt
```

Rationale:

```txt
SDE must be independently usable outside SagaEngine.
Saga is a consumer, not SDE's owner.
```

---

### 5.2 Forge

Allowed:

```txt
Forge → CMakeAdapter / ConanAdapter
Forge → ToolEnv / EnvProbe
Forge → SDE executable/public facade boundary
Forge → Prism executable/report boundary
Forge → AssetPipeline executable/service boundary
Forge → Scripting compiler executable/service boundary
Forge → SagaShared build/artifact/package/diagnostic contracts where approved
Forge → project manifests / build reports / package manifests
```

Forbidden:

```txt
Forge → Saga product shell internals
Forge → SagaEditor UI
Forge → Runtime private internals
Forge → Server private internals
Forge → SDE parser/AST/semantic internals
Forge → Prism internal database/index implementation
Forge → AssetPipeline private importer/cooker implementation
Forge → Scripting compiler private implementation
Forge → Qt UI
```

Rationale:

```txt
Forge orchestrates.
Forge does not become compiler, cooker, analyzer, or product shell.
```

---

### 5.3 Prism

Allowed:

```txt
Prism → source files
Prism → generated files
Prism → compile_commands.json
Prism → SDE manifests/source maps/dependency manifests
Prism → script binding manifests
Prism → asset/cooked artifact manifests
Prism → package manifests
Prism → Forge build reports
Prism → SagaShared report/diagnostic contracts where approved
```

Forbidden:

```txt
Prism → Saga product shell internals
Prism → SagaEditor UI
Prism → Runtime private internals
Prism → Server private internals
Prism → SDE parser/AST/semantic internals
Prism → Forge planner internals
Prism → AssetPipeline private importer/cooker implementation
Prism → Scripting compiler private implementation
Prism → Qt UI
```

Rationale:

```txt
Prism analyzes relationships and reports evidence.
Prism does not build, cook, compile, package, or execute.
```

---

### 5.4 AssetPipeline

Allowed:

```txt
AssetPipeline → standalone importer/cooker dependencies
AssetPipeline → SagaShared asset/artifact contracts where approved
Forge → AssetPipeline command/service boundary
Editor → AssetPipeline service/report boundary
Prism → AssetPipeline manifests/reports
Runtime → cooked asset artifacts/manifests only
```

Forbidden:

```txt
AssetPipeline → Runtime private asset streaming cache
AssetPipeline → SagaEditor UI
AssetPipeline → Saga product shell internals
AssetPipeline → Forge planner internals
AssetPipeline → Prism internals
AssetPipeline → SDE compiler internals
AssetPipeline → Scripting compiler internals
```

Rationale:

```txt
AssetPipeline creates runtime-ready artifacts.
Runtime consumes them.
Editor presents UX around them.
Forge orchestrates them.
```

---

### 5.5 Scripting Toolchain

Allowed:

```txt
ScriptingToolchain → script compiler/analyzer dependencies
ScriptingToolchain → SagaShared scripting/authority/artifact contracts where approved
Forge → ScriptingToolchain command/service boundary
Editor → ScriptingToolchain diagnostics/report boundary
Prism → script source/binding manifests/reports
Runtime/Server → compiled script artifacts/binding manifests only
```

Forbidden:

```txt
ScriptingToolchain → SagaEditor script panel implementation
ScriptingToolchain → Runtime private script host internals
ScriptingToolchain → Server private authority implementation
ScriptingToolchain → Forge planner internals
ScriptingToolchain → Prism internals
ScriptingToolchain → SDE compiler internals
```

Rationale:

```txt
Compiler/toolchain produces script artifacts.
Runtime/server bind and execute validated artifacts.
Editor edits scripts.
```

---

### 5.6 SagaTools

Allowed:

```txt
SagaTools → tool registry metadata
SagaTools → executable dispatch
SagaTools → version/help commands
SagaTools → setup/install scripts
```

Forbidden:

```txt
SagaTools → SDE internals
SagaTools → Forge internals
SagaTools → Prism internals
SagaTools → AssetPipeline internals
SagaTools → Scripting compiler internals
SagaTools → Saga product shell internals
SagaTools → Editor UI
SagaTools → Runtime/Server internals
```

Rationale:

```txt
SagaTools dispatches tools.
It does not implement them.
```

---

## 6. Contract vs Implementation Rule

A contract may move to `SagaShared` only if it is neutral and has multiple real consumers.

Valid `SagaShared` examples:

```txt
AssetId
ArtifactRef
PackageManifest
GraphId
BlockDefinitionDescriptor
AuthorityRequirement
ScriptBindingDescriptor
BuildReport
PublishBlocker
Collaboration ResourceLock
DiagnosticPayload
```

Invalid `SagaShared` examples:

```txt
GraphCompiler
QuestRewardExecutor
QtGraphCanvas
ForgeBuildPlanner
PrismIndexer
TextureImporter
CSharpCompilerHost
RuntimeResidencyCache
ServerInventoryTransactionService
CollaborationConflictEngine
```

Rule:

```txt
Data contracts may be shared.
Behavior needs an owner.
```

---

## 7. Public / Private Header Rules

### 7.1 Editor Public Headers

Public editor headers:

```txt
Editor/include/SagaEditor/**
```

Must not include:

```txt
Qt headers except approved UI abstraction/backend boundary
Runtime private headers
Server private headers
SDE internals
Forge internals
Prism internals
AssetPipeline internals
Scripting compiler internals
Saga product shell internals
```

---

### 7.2 Runtime / Engine Public Headers

Engine public headers:

```txt
Engine/Public/SagaEngine/**
Engine/include/SagaEngine/**
```

Must not include:

```txt
Editor headers
Saga product shell headers
SDE compiler internals
Forge internals
Prism internals
AssetPipeline implementation
Scripting compiler implementation
Qt UI headers
```

---

### 7.3 Server Public Headers

Server public headers must not include:

```txt
Editor headers
Saga product shell internals
SDE compiler internals
Forge internals
Prism internals
AssetPipeline implementation
Scripting compiler implementation
Qt UI headers
```

---

### 7.4 Shared Public Headers

Shared public headers:

```txt
Shared/include/SagaShared/**
```

Must not include any implementation-owner module.

Forbidden:

```txt
Saga
SagaEditor
Runtime private internals
Server
SagaCollaboration implementation
SDE internals
Forge internals
Prism internals
AssetPipeline implementation
Scripting implementation
Qt UI
```

---

### 7.5 Tool Public Headers

Tool public headers should avoid private internals of other tools.

Examples:

```txt
Forge public headers must not include SDE parser/AST internals.
Prism public headers must not include Forge build planner internals.
AssetPipeline public headers must not include runtime private asset cache.
Scripting toolchain public headers must not include editor script panel classes.
```

---

## 8. Artifact Boundary Rule

Authoring/build tools produce artifacts.

Runtime/server consume artifacts.

Preferred flow:

```txt
SDE source
  → SDE artifacts/manifests/source maps

C# script source
  → script assemblies/binding manifests

Source assets
  → cooked runtime-ready artifacts/asset manifests

Forge
  → package manifests/build reports/publish reports

Prism
  → analysis reports

Runtime/Server
  → load package manifests and validated artifacts
```

Forbidden flow:

```txt
Runtime calls SDE compiler internals.
Server asks Forge planner what to execute.
Editor passes graph canvas nodes directly to runtime as authority.
Prism regenerates stale code.
Forge cooks texture by including AssetPipeline private importer.
```

---

## 9. Saga Product Boundary

`Saga` is allowed to orchestrate modes and tool workflows.

`Saga` must not become an implementation owner for all systems.

Allowed:

```txt
Saga starts editor mode.
Saga starts runtime preview.
Saga starts server preview.
Saga invokes Forge build workflow.
Saga displays publish blockers.
Saga shows collaboration status.
```

Forbidden:

```txt
Saga compiles SDE source directly using private compiler passes.
Saga cooks assets directly.
Saga indexes source like Prism.
Saga owns editor panels.
Saga owns runtime/server authority state.
Saga stores collaboration locks directly.
```

---

## 10. Editor Boundary

`SagaEditor` owns authoring UX.

Allowed:

```txt
Editor displays graph/source/assets/scripts.
Editor invokes validation/compile/cook/build through service/tool boundaries.
Editor shows diagnostics.
Editor shows collaboration state.
Editor provides authoring workflows.
```

Forbidden:

```txt
Editor owns project dashboard truth.
Editor owns product lifecycle.
Editor owns server authority.
Editor owns SDE compiler internals.
Editor owns Forge build planner.
Editor owns Prism analyzer.
Editor owns asset cooker internals.
Editor owns C# compiler internals.
Editor owns collaboration session truth.
```

---

## 11. Runtime/Server Boundary

Runtime/server consume validated package state.

Allowed:

```txt
Runtime loads client package manifest.
Runtime loads asset manifests.
Runtime loads client-safe graph/script artifacts.
Server loads server package manifest.
Server loads authoritative graph/script artifacts.
Server validates authority manifests.
Runtime/server validate artifact hashes/versions.
```

Forbidden:

```txt
Runtime imports arbitrary source assets in shipping mode.
Runtime compiles SDE source.
Runtime compiles C# scripts.
Server trusts client graph execution as authority.
Server asks editor collaboration panel for lock state.
Runtime/server include Forge or Prism internals.
```

---

## 12. SDE Standalone Rule

SDE must remain independently usable outside Saga.

Required:

```txt
SDE builds without SagaEngine repository dependencies.
SDE has non-Saga examples.
SDE has standalone CLI.
SDE can be packaged independently.
SDE consumes schema packages, not Saga module headers.
Saga-specific meaning is expressed by schemas/adapters/consumers.
```

Forbidden:

```txt
SDE requires SagaShared to build.
SDE includes SagaEngine runtime headers.
SDE hardcodes Saga inventory/quest/server semantics in core.
SDE depends on Forge for compilation.
SDE depends on Prism for analysis.
```

---

## 13. Build/Publish Boundary

Forge owns build orchestration and publish readiness reporting.

Allowed:

```txt
Forge invokes SDE.
Forge invokes script compiler.
Forge invokes AssetPipeline.
Forge invokes Prism.
Forge stages packages.
Forge writes manifests/reports.
Forge reports publish blockers.
```

Forbidden:

```txt
Forge directly implements SDE semantic passes.
Forge directly compiles C# by owning compiler internals.
Forge directly imports/cooks textures/meshes/audio internally.
Forge directly indexes code like Prism.
Forge launches product/editor UI as build logic.
```

---

## 14. Analysis Boundary

Prism owns analysis and reporting.

Allowed:

```txt
Prism reads source files.
Prism reads generated files.
Prism reads manifests/reports.
Prism calculates hashes.
Prism emits stale/boundary/package reports.
```

Forbidden:

```txt
Prism regenerates stale files.
Prism builds packages.
Prism compiles SDE source using SDE internals.
Prism cooks assets.
Prism compiles C#.
Prism mutates package manifests by default.
```

---

## 15. Collaboration Boundary

SagaCollaboration owns collaboration truth.

Allowed:

```txt
Saga starts/join/leaves sessions through collaboration API.
Editor displays participants/claims/locks/conflicts.
Forge reads collaboration status for publish gates.
Shared defines neutral collaboration contracts.
```

Forbidden:

```txt
Editor panel stores final lock truth.
Saga product shell stores conflict engine state.
Forge resolves conflicts automatically during publish check.
Runtime/server derive authority from editor collaboration state.
Authoring profile grants collaboration permission.
```

---

## 16. Package and Manifest Boundary

Manifests are the legal bridge between authoring/build/tooling and runtime/server.

Required manifests:

```txt
SDE artifact manifest
SDE dependency manifest
SDE source map
script binding manifest
asset manifest
artifact manifest
client package manifest
server package manifest
build report
publish report
Prism reports
```

Rules:

```txt
Runtime/server consume manifests and artifacts.
Forge generates/stages manifests.
Prism reads and checks manifests.
Editor displays manifest/report diagnostics.
Saga displays summary/publish blockers.
```

Forbidden:

```txt
Runtime/server guess project state from source folders.
Editor UI state becomes package truth.
Tool terminal output is the only report.
Generated artifacts lack origin metadata.
Package manifests omit hashes/versions.
```

---

## 17. CMake / Target Boundary Expectations

Target-level expectations:

```txt
SagaShared
  lowest shared contract layer

SDE::Core
  independent package target, no Saga dependency

SagaEngine
  runtime/core engine target, no Editor/Saga product/tool internals

SagaServer
  server authority target, no Editor/product/tool internals

SagaEditor
  editor module target, no product shell ownership, no runtime/server private internals

Saga
  product executable/library, may link public module APIs

Forge
  tool target, no SDE/Prism/AssetPipeline/Scripting private internals

Prism
  tool target, no Forge/SDE private internals

AssetPipeline
  tool/service target, no runtime private cache or editor UI

ScriptingToolchain
  compiler/tool target, no editor UI or runtime private host internals

SagaTools
  dispatcher target, no tool private internals
```

Bad sign:

```txt
A target needs half the repository in include_directories just to compile.
```

That usually means architecture has already been bypassed.

---

## 18. CI Enforcement

* [ ] Complete include-boundary test coverage.

  Partial enforcement exists in `ArchitectureTests`.
  `Tests/Unit/Architecture/PublicPrivateBoundaryTests.cpp` covers selected
  public/private include boundaries, and recent SDE cleanup added explicit
  guards that `Engine/Public` does not expose SDE includes or types through
  `Tests/Unit/Architecture/CMakeTargetBoundaryTests.cpp`.

  Required checks:

```txt
Editor public headers do not include Qt except approved boundary.
Editor does not include server/runtime/tool private internals.
Runtime/server do not include editor/tool/compiler internals.
SagaShared does not include implementation-owner modules.
SDE does not include Saga modules.
Forge does not include SDE/Prism/AssetPipeline/Scripting private internals.
Prism does not include SDE/Forge private internals.
AssetPipeline does not include runtime private cache internals.
ScriptingToolchain does not include editor UI/runtime private host internals.
```

* [ ] Complete CMake target dependency checks.

  Partial enforcement exists in
  `Tests/Unit/Architecture/CMakeTargetBoundaryTests.cpp`.
  Current checks cover target-link boundaries for `SagaEngine`,
  runtime/server targets, asset pipeline, editor composer/tool boundaries,
  related tool ownership checks, and the SDE cleanup guard that `SagaEngine`
  must not link SDE, tool, editor, or product targets.

  Done means CI can reject all forbidden target links.

* [ ] Add Prism boundary validation.

  Required command concept:

```txt
prism boundaries --profile ci
```

* [ ] Add dependency graph report artifact.

  Done means CI can output:

```txt
Build/Reports/dependency_boundary_report.json
```

---

## 19. Migration Priorities

### 19.1 Fix Product/Editor Boundary

* [ ] Ensure `Saga` is primary product executable.
* [ ] Mark `Apps/Editor` as development compatibility launcher.
* [ ] Move product lifecycle out of editor-owned code.

### 19.2 Fix Shared Contracts

* [ ] Move neutral contracts into `SagaShared`.
* [ ] Remove implementation from `SagaShared` if present.
* [ ] Add contract versioning/serialization tests.

### 19.3 Fix SDE Independence

* [ ] Ensure SDE has no Saga module dependency.
* [ ] Add non-Saga sample project.
* [ ] Add standalone package tests.

### 19.4 Fix Tool Boundaries

* [ ] Forge invokes tools through adapters.
* [ ] Prism reads manifests/reports only.
* [ ] AssetPipeline and Scripting toolchain get explicit ownership.

### 19.5 Fix Runtime/Server Artifact Boundary

* [ ] Runtime/server consume manifests/artifacts.
* [ ] Runtime/server stop depending on authoring/build/tooling internals.

---

## 20. Non-Goals

This document does not define implementation details for:

* graph compiler algorithms,
* runtime replication internals,
* editor panel UI layout,
* asset importer internals,
* C# compiler internals,
* collaboration backend transport,
* package file format internals,
* product dashboard UX.

This document defines who may depend on whom.

It is a guardrail document.

Not a feature roadmap.

---

## 21. Risk Register

### 21.1 Risk: Convenience Includes Become Architecture

Mitigation:

* CI include-boundary tests,
* Prism boundary checks,
* narrow public APIs,
* manifest/report boundaries.

---

### 21.2 Risk: SagaShared Becomes Dumping Ground

Mitigation:

* contracts only,
* no implementation ownership,
* every shared contract needs multiple consumers,
* owner document required.

---

### 21.3 Risk: SDE Becomes Saga-Locked

Mitigation:

* no Saga dependencies,
* standalone packaging,
* non-Saga examples,
* schema package model.

---

### 21.4 Risk: Forge Becomes Mega Tool

Mitigation:

* adapters only,
* reports/manifests only,
* no compiler/cooker/analyzer internals.

---

### 21.5 Risk: Runtime/Server Depend on Build Tools

Mitigation:

* package/artifact manifests,
* startup validation,
* no tool internals in runtime/server.

---

### 21.6 Risk: Editor Owns Too Much

Mitigation:

* editor owns authoring UX only,
* Saga owns product lifecycle,
* SagaCollaboration owns collaboration truth,
* tools own compiler/build/analyzer/import behavior.

---

## 22. Decision Summary

Preserve these rules:

```txt
Saga owns product lifecycle.
SagaEditor owns authoring UX.
Runtime owns client/game execution.
Server owns authority.
SagaShared owns neutral contracts only.
SagaCollaboration owns collaboration implementation.
SDE is standalone deterministic compiler.
Forge is build workflow frontend.
Prism is analysis/reporting tool.
AssetPipeline owns source asset import/cook.
ScriptingToolchain owns script compile/binding generation.
SagaTools is thin dispatcher.
Runtime/server consume packaged artifacts/manifests.
Tools communicate through CLIs, adapters, manifests, reports, and shared contracts.
Private implementation headers do not cross ownership boundaries.
```

The architecture is allowed to be large.

It is not allowed to be tangled.

Large systems can survive if their boundaries are boring, explicit, and enforced.

Tangled systems survive only until the first real production deadline.
