# SagaEngine Dependency Graph



---

## 0. Document Status

This document is an architecture contract, not a suggestion list.

Every roadmap and implementation plan should respect this dependency graph unless this document is intentionally updated.

Dependency mistakes do not look dangerous at first.

They look like:

```txt
I'll just include this one header.
```



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



Important:



---

## 3. Module Ownership Table



---

## 4. Allowed Dependency Directions

### 4.1 Product Layer

Allowed:



Forbidden:



Rationale:

```txt
Saga coordinates product workflow.
Saga does not become the implementation owner of every mode/tool.
```

---

### 4.2 Editor Layer

Allowed:



Forbidden:



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



Rationale:

```txt
Collaboration owns session/resource state.
Editor/product display and orchestrate it.
```

---

## 5. Tool Dependency Rules



Allowed:



Forbidden:



Rationale:



---

### 5.2 Forge

Allowed:



Forbidden:



Rationale:

```txt
Forge orchestrates.
Forge does not become compiler, cooker, analyzer, or product shell.
```

---

### 5.3 Prism

Allowed:



Forbidden:



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



---

### 7.2 Runtime / Engine Public Headers

Engine public headers:

```txt
Engine/Public/SagaEngine/**
Engine/include/SagaEngine/**
```

Must not include:



---

### 7.3 Server Public Headers

Server public headers must not include:



---

### 7.4 Shared Public Headers

Shared public headers:

```txt
Shared/include/SagaShared/**
```

Must not include any implementation-owner module.

Forbidden:



---

### 7.5 Tool Public Headers

Tool public headers should avoid private internals of other tools.

Examples:



---

## 8. Artifact Boundary Rule

Authoring/build tools produce artifacts.

Runtime/server consume artifacts.

Preferred flow:



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



---





Required:



Forbidden:



---

## 13. Build/Publish Boundary

Forge owns build orchestration and publish readiness reporting.

Allowed:



Forbidden:



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



SagaEngine
  runtime/core engine target, no Editor/Saga product/tool internals

SagaServer
  server authority target, no Editor/product/tool internals

SagaEditor
  editor module target, no product shell ownership, no runtime/server private internals

Saga
  product executable/library, may link public module APIs





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



  Required checks:



* [ ] Complete CMake target dependency checks.



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



The architecture is allowed to be large.

It is not allowed to be tangled.

Large systems can survive if their boundaries are boring, explicit, and enforced.

Tangled systems survive only until the first real production deadline.
