# SagaShared — Shared Contracts Roadmap

> Last updated: 2026-05-15
> Status: Active roadmap
> Target: A small, neutral, stable shared contract layer used by Saga, SagaEditor, SagaEngine runtime, SagaServer, SagaCollaboration, SDE/Forge/Prism integrations, asset pipeline, scripting, diagnostics, and approved tools.
> Scope: Cross-module identifiers, descriptors, diagnostics payloads, artifact references, package manifests, project/workspace contracts, collaboration contracts, gameplay graph contracts, authority metadata, scripting contracts, asset/artifact contracts, build/publish contracts, authoring profile contracts, replay/profiling event contracts, and editor/runtime/tool integration boundaries.

---

## 0. Roadmap Convention

* `[x]` — Shipped. The note after the item names the files, modules, tests, or integration points that represent the completed work and highlights decisions worth preserving.
* `[ ]` — Open. Either unstarted or partially explored; the item describes the finished production state rather than temporary scaffolding.
* Shipped shared work must name the contract files, tests, consuming modules, and ownership decisions where practical.
* Open shared work must describe stable contract behavior, not implementation behavior.
* `SagaShared` must remain neutral and small.
* `SagaShared` must not contain implementation ownership.
* `SagaShared` must not depend upward into editor, runtime, server, product shell, collaboration implementation, tools, SDE internals, Forge internals, Prism internals, asset pipeline implementation, or scripting host implementation.
* Shared contracts must be stable enough to survive use by multiple modules.
* Shared contracts must be serialization-safe where used across tools, reports, build artifacts, or process boundaries.

`SagaShared` should be boring.

That is the compliment.

The moment it becomes exciting, someone probably put a subsystem where a contract was supposed to be.

---

## 1. Document Purpose

This document defines the roadmap for `SagaShared`.

`SagaShared` exists for contracts that genuinely need to be consumed by multiple Saga modules.

A contract belongs here only if:

* it has at least two valid consumers,
* the consumers live in different ownership domains,
* no single implementation owner can own the contract without creating an upward or sideways dependency leak,
* the contract can remain neutral and mostly data-oriented,
* the contract does not require UI, runtime execution, transport, persistence, compiler internals, or product lifecycle ownership.

This roadmap must not become a dumping ground.

If a system is shared only because nobody wants to own it, it does not belong here.

That is not architecture.

That is abandonment with a nicer folder name.

Correct model:

```txt
SagaShared
  owns ids, descriptors, enums, result types, diagnostics payloads, artifact refs, manifests, and small neutral interfaces where justified

Owning modules
  own behavior, lifecycle, storage, UI, compiler passes, runtime execution, build planning, analysis, and backend connections
```

Incorrect model:

```txt
SagaShared
  owns half the product because everything needs to include it
```

That path ends with a shared layer that is neither shared nor layered.

---

## 2. Companion Documents

| Document                            | Purpose                                                                                            |
| ----------------------------------- | -------------------------------------------------------------------------------------------------- |
| `DependencyGraph.md`                | Dependency ownership and compile-time architecture boundaries                                      |
| `SAGA_PRODUCT_ROADMAP.md`           | Saga product shell, project lifecycle, mode orchestration, build/publish workflow                  |
| `EDITOR_ROADMAP.md`                 | SagaEditor authoring module mounted by Saga                                                        |
| `ENGINE_ROADMAP.md`                 | Runtime/server execution, authority, replication, asset streaming, diagnostics                     |
| `COLLABORATION_ROADMAP.md`          | Product collaboration, sessions, presence, permissions, claims, locks, conflicts                   |
| `DIAGNOSTICS_ROADMAP.md`            | Runtime diagnostics, structured reports, health/lifetime/resource visibility                       |
| `SDE_ROADMAP.md`                    | Standalone deterministic data compiler roadmap                                                     |
| `FORGE_ROADMAP.md`                  | Build workflow frontend, SDE invocation, asset cook, script compile, package staging               |
| `PRISM_ROADMAP.md`                  | Code/artifact intelligence, dependency graphs, stale artifact detection                            |
| `TOOLS_ROADMAP.md`                  | Tool ecosystem ownership and boundaries                                                            |
| `SAGA_GAMEPLAY_GRAPH_ROADMAP.md`    | Gameplay graph, blocks, graph artifacts, SDE/C#/runtime integration                                |
| `AUTHORING_AUTHORITY_MODEL.md`      | Authority, execution domain, side effects, replication, persistence, prediction, security metadata |
| `SAGA_SCRIPTING_ROADMAP.md`         | C# scripting contracts, block-callable bindings, script artifacts, generated code preview          |
| `ASSET_PIPELINE_ROADMAP.md`         | Asset ids, source assets, cooked artifacts, asset manifests, import/cook/package contracts         |
| `AUTHORING_PERSONAS.md`             | Authoring profile descriptors, panel/block visibility, diagnostics detail levels                   |
| `BUILD_PUBLISH_PIPELINE_ROADMAP.md` | Build profiles, build reports, package manifests, publish readiness contracts                      |
| `ClientReplicationFormalSpec.md`    | Runtime/client replication formal contract                                                         |
| `AssetStreamingImplementation.md`   | Runtime asset streaming implementation note                                                        |

---

## 3. Ownership Boundary

* [x] Define `SagaShared` as neutral contract ownership, not implementation ownership.

  Represented by:

  ```txt
  SHARED_ROADMAP.md
  DependencyGraph.md
  COLLABORATION_ROADMAP.md
  ENGINE_ROADMAP.md
  EDITOR_ROADMAP.md
  ```

  Preserved decision:

  ```txt
  SagaShared owns small neutral contracts.
  SagaShared does not own product behavior, editor UI, runtime internals, server authority, collaboration implementation, compiler internals, build workflow implementation, code intelligence implementation, asset pipeline implementation, or scripting host implementation.
  ```

* [ ] Keep `SagaShared` limited to contracts with genuine multi-module consumers.

  Done means every shared contract has:

  * a named owner document,
  * at least two valid consumers,
  * no dependency on private implementation headers,
  * no dependency on UI framework types,
  * no backend connection ownership,
  * no process lifecycle ownership,
  * no compiler/runtime/build/analyzer implementation logic.

* [ ] Add documentation for every shared contract family.

  Done means each shared contract folder explains:

  * what the contract represents,
  * which modules consume it,
  * which module owns implementation,
  * what is explicitly forbidden in that folder,
  * serialization/versioning expectations where applicable.

---

## 4. Belongs Here

* [ ] Keep valid shared systems inside `SagaShared`.

  Valid shared contract families include:

  * stable ids,
  * project/workspace descriptors,
  * diagnostics payloads,
  * artifact references,
  * package manifest contracts,
  * build/publish report contracts,
  * authoring profile descriptors,
  * gameplay graph descriptors,
  * block definition descriptors,
  * authority metadata descriptors,
  * scripting artifact/binding descriptors,
  * asset/artifact metadata descriptors,
  * neutral collaboration contracts,
  * replay/profiling event contracts,
  * shared security policy descriptors,
  * editor/runtime compilation pipeline descriptors,
  * canonical IR references,
  * sample content descriptors used by both editor and runtime.

* [ ] Require every shared system to have neutral data-only contracts unless explicitly justified.

  Done means shared headers primarily contain:

  * ids,
  * descriptors,
  * enums,
  * flags,
  * result types,
  * small value types,
  * POD-style structures,
  * serialization-safe payloads,
  * narrow interfaces where truly required.

* [ ] Require every non-data shared interface to have explicit justification.

  Done means narrow shared interfaces must document:

  * why a data contract is insufficient,
  * who implements it,
  * who consumes it,
  * what ownership it does not imply.

---

## 5. Does Not Belong Here

### 5.1 Pure Editor Systems

* [ ] Prevent pure editor systems from entering `SagaShared`.

  Forbidden examples:

  * editor panels,
  * editor docking,
  * editor menus,
  * Qt widgets,
  * editor command implementation,
  * inspector implementation,
  * content browser implementation,
  * graph canvas implementation,
  * script editor implementation,
  * asset import UI.

---

### 5.2 Pure Runtime Systems

* [ ] Prevent pure runtime systems from entering `SagaShared`.

  Forbidden examples:

  * runtime ECS internals,
  * renderer internals,
  * replication apply internals,
  * prediction implementation,
  * interpolation buffers,
  * asset streaming implementation,
  * runtime residency cache,
  * simulation tick scheduler implementation.

---

### 5.3 Server Authority Systems

* [ ] Prevent server authority systems from entering `SagaShared`.

  Forbidden examples:

  * authoritative simulation implementation,
  * shard authority implementation,
  * server persistence services,
  * anti-cheat implementation,
  * packet transport internals,
  * server request validation implementation,
  * database transaction implementation.

---

### 5.4 Product Shell Systems

* [ ] Prevent product shell systems from entering `SagaShared`.

  Forbidden examples:

  * product dashboard widgets,
  * project launcher implementation,
  * recent project registry implementation,
  * Saga mode switching implementation,
  * product-level window ownership,
  * product lifecycle state machines.

---

### 5.5 Collaboration Implementation

* [ ] Prevent collaboration implementation from entering `SagaShared`.

  Forbidden examples:

  * session lifecycle implementation,
  * host/join state machines,
  * transport adapters,
  * backend sync,
  * conflict engine,
  * lock manager implementation,
  * permission service implementation,
  * reconnect/recovery implementation.

---

### 5.6 Tool Internals

* [ ] Prevent tools from dumping implementation into `SagaShared`.

  Forbidden examples:

  * SDE compiler internals,
  * SDE AST/parser/semantic analyzer,
  * Forge build planner implementation,
  * Forge cache internals,
  * Prism source indexer,
  * Prism symbol graph implementation,
  * SagaTools dispatch internals,
  * asset cooker/importer implementation,
  * script compiler implementation.

---

## 6. Dependency Rules

### 6.1 Allowed Dependencies

* [ ] Allow product/editor/runtime/server/collaboration/tools to consume shared contracts.

  Allowed examples:

  ```txt
  Saga → SagaShared
  SagaEditor → SagaShared
  Runtime → SagaShared
  Server → SagaShared
  SagaCollaboration → SagaShared
  Forge → SagaShared contracts where explicitly approved
  Prism → SagaShared contracts where explicitly approved
  SagaTools → SagaShared contracts where explicitly approved
  Asset pipeline tools → SagaShared contracts where explicitly approved
  Scripting toolchain/services → SagaShared contracts where explicitly approved
  ```

* [ ] Allow `SagaShared` to depend only on approved low-level primitives.

  Allowed examples:

  ```txt
  SagaShared → C++ standard library
  SagaShared → platform-neutral primitive libraries
  SagaShared → approved serialization primitives
  SagaShared → approved hash/value primitives
  ```

---

### 6.2 Forbidden Dependencies

* [ ] Prevent `SagaShared` from depending upward.

  Forbidden:

  ```txt
  SagaShared → Saga
  SagaShared → SagaEditor
  SagaShared → SagaEngine runtime internals
  SagaShared → SagaServer
  SagaShared → SagaCollaboration implementation
  SagaShared → SDE compiler internals
  SagaShared → Forge implementation
  SagaShared → Prism implementation
  SagaShared → SagaTools implementation
  SagaShared → Asset pipeline implementation
  SagaShared → Scripting host/compiler implementation
  ```

* [ ] Prevent `SagaShared` from exposing framework-specific UI types.

  Forbidden examples:

  ```cpp
  #include <QWidget>
  #include <QMainWindow>
  #include <QApplication>
  #include <QDockWidget>
  ```

* [ ] Prevent `SagaShared` from owning transport or persistence implementation.

  Forbidden examples:

  ```txt
  Redis client
  database connection
  socket transport
  relay implementation
  cloud session backend
  asset storage backend
  package registry backend
  ```

---

## 7. Core Shared Identity Contracts

* [ ] Add stable shared id contracts.

  Done means shared ids exist for:

  * project id,
  * workspace id,
  * session id,
  * participant id,
  * resource id,
  * artifact id,
  * package id,
  * schema id,
  * diagnostic id,
  * graph id,
  * block id,
  * script project id,
  * script assembly id,
  * asset id,
  * build id,
  * publish report id,
  * authoring profile id.

Expected files:

```txt
Shared/include/SagaShared/Ids/ProjectId.hpp
Shared/include/SagaShared/Ids/WorkspaceId.hpp
Shared/include/SagaShared/Ids/SessionId.hpp
Shared/include/SagaShared/Ids/ParticipantId.hpp
Shared/include/SagaShared/Ids/ResourceId.hpp
Shared/include/SagaShared/Ids/ArtifactId.hpp
Shared/include/SagaShared/Ids/PackageId.hpp
Shared/include/SagaShared/Ids/SchemaId.hpp
Shared/include/SagaShared/Ids/DiagnosticId.hpp
Shared/include/SagaShared/Ids/GraphId.hpp
Shared/include/SagaShared/Ids/BlockId.hpp
Shared/include/SagaShared/Ids/ScriptProjectId.hpp
Shared/include/SagaShared/Ids/ScriptAssemblyId.hpp
Shared/include/SagaShared/Ids/AssetId.hpp
Shared/include/SagaShared/Ids/BuildId.hpp
Shared/include/SagaShared/Ids/PublishReportId.hpp
Shared/include/SagaShared/Ids/AuthoringProfileId.hpp
```

* [ ] Keep shared ids deterministic and serialization-safe.

  Done means:

  * ids are stable across processes,
  * ids can be serialized,
  * ids can be logged,
  * invalid/null ids are explicit,
  * id comparison is cheap and deterministic,
  * ids do not require implementation owner headers.

---

## 8. Project and Workspace Contracts

* [ ] Add shared project descriptor contracts.

  Done means project descriptors can describe:

  * project id,
  * display name,
  * project root,
  * project version,
  * package manifest reference,
  * active workspace reference,
  * default authoring profile,
  * collaboration mode where applicable,
  * validation state.

Expected files:

```txt
Shared/include/SagaShared/Project/ProjectDescriptor.hpp
Shared/include/SagaShared/Project/ProjectVersion.hpp
Shared/include/SagaShared/Project/ProjectValidationState.hpp
Shared/include/SagaShared/Project/ProjectRoot.hpp
```

* [ ] Add shared workspace descriptor contracts.

  Done means workspace descriptors can describe:

  * workspace id,
  * project id,
  * local root path,
  * active user id,
  * active mode hint,
  * active authoring profile,
  * sync state,
  * dirty resource state,
  * validation state.

Expected files:

```txt
Shared/include/SagaShared/Workspace/WorkspaceDescriptor.hpp
Shared/include/SagaShared/Workspace/WorkspaceSyncState.hpp
Shared/include/SagaShared/Workspace/WorkspaceValidationState.hpp
```

* [ ] Keep project/workspace implementation outside `SagaShared`.

  Done means `SagaShared` does not own:

  * project dashboard,
  * project file IO implementation,
  * recent project registry,
  * cloud workspace sync,
  * editor project open UI,
  * product mode switching.

---

## 9. Resource Reference Contracts

* [ ] Add shared resource reference contracts.

  Done means cross-system references can point to:

  * project resources,
  * workspace resources,
  * source files,
  * graph resources,
  * script files,
  * asset metadata,
  * cooked artifacts,
  * SDE source ranges,
  * package manifest entries,
  * runtime entities where safe.

Expected files:

```txt
Shared/include/SagaShared/Resources/ResourceId.hpp
Shared/include/SagaShared/Resources/ResourceKind.hpp
Shared/include/SagaShared/Resources/ResourceRef.hpp
Shared/include/SagaShared/Resources/SourceRange.hpp
Shared/include/SagaShared/Resources/SourceLocation.hpp
```

* [ ] Support resource-linked diagnostics.

  Done means diagnostics can navigate to resources without depending on editor implementation.

---

## 10. Collaboration Contracts Boundary

* [x] Define collaboration ownership split.

  Represented by:

  ```txt
  COLLABORATION_ROADMAP.md
  SHARED_ROADMAP.md
  DependencyGraph.md
  ```

  Preserved decision:

  ```txt
  SagaShared may contain neutral collaboration contracts.
  SagaCollaboration owns collaboration implementation.
  SagaEditor owns collaboration UI.
  ```

* [ ] Add neutral collaboration contracts under `SagaShared`.

  Done means shared collaboration contracts include:

  * participant id,
  * session id,
  * room code,
  * session descriptor,
  * workspace descriptor reference,
  * presence snapshot,
  * permission grant,
  * resource claim,
  * resource lock,
  * conflict descriptor,
  * collaboration diagnostic,
  * collaboration change envelope.

Expected files:

```txt
Shared/include/SagaShared/Collaboration/ParticipantId.hpp
Shared/include/SagaShared/Collaboration/SessionId.hpp
Shared/include/SagaShared/Collaboration/RoomCode.hpp
Shared/include/SagaShared/Collaboration/SessionDescriptor.hpp
Shared/include/SagaShared/Collaboration/PresenceSnapshot.hpp
Shared/include/SagaShared/Collaboration/PermissionGrant.hpp
Shared/include/SagaShared/Collaboration/ResourceClaim.hpp
Shared/include/SagaShared/Collaboration/ResourceLock.hpp
Shared/include/SagaShared/Collaboration/ConflictDescriptor.hpp
Shared/include/SagaShared/Collaboration/CollaborationDiagnostic.hpp
Shared/include/SagaShared/Collaboration/CollaborationChangeEnvelope.hpp
```

* [ ] Keep collaboration implementation out of `SagaShared`.

  Forbidden in `SagaShared`:

  * session lifecycle implementation,
  * host/join state machines,
  * transport adapters,
  * backend sync,
  * conflict engine,
  * lock manager implementation,
  * permission service implementation,
  * reconnect/recovery implementation.

* [ ] Prevent new final collaboration contracts from being added under editor-private headers.

  Deprecated path:

  ```txt
  Editor/include/SagaEditor/Collaboration
  ```

  Correct target:

  ```txt
  Shared/include/SagaShared/Collaboration
  ```

---

## 11. Diagnostics Contracts

* [x] Add shared diagnostics payload contracts.

  Done means diagnostics can be represented without depending on editor/runtime/server/tool implementation.

  Shipped in `0.0.8-dev.6` as neutral value contracts plus shared unit coverage:

  ```txt
  Shared/include/SagaShared/Diagnostics/DiagnosticSeverity.hpp
  Shared/include/SagaShared/Diagnostics/DiagnosticCode.hpp
  Shared/include/SagaShared/Diagnostics/DiagnosticLocation.hpp
  Shared/include/SagaShared/Diagnostics/DiagnosticPayload.hpp
  Shared/include/SagaShared/Diagnostics/DiagnosticSource.hpp
  Shared/include/SagaShared/Diagnostics/DiagnosticCategory.hpp
  Shared/include/SagaShared/Diagnostics/DiagnosticSummary.hpp
  Tests/Unit/Shared/DiagnosticContractsTests.cpp
  ```

Expected files:

```txt
Shared/include/SagaShared/Diagnostics/DiagnosticSeverity.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticCode.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticLocation.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticPayload.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticSource.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticCategory.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticSummary.hpp
```

* [ ] Support resource-linked diagnostics.

  Done means diagnostics can reference:

  * project,
  * workspace,
  * resource,
  * artifact,
  * package,
  * graph,
  * graph node/pin/edge,
  * script source range,
  * asset metadata/source,
  * schema,
  * build step,
  * runtime entity where safe.

* [x] Add diagnostic categories used by new authoring/build systems.

  Required category families:

  ```txt
  Project.*
  Workspace.*
  Editor.*
  Graph.*
  Authority.*
  Script.*
  Asset.*
  SDE.*
  Build.*
  Package.*
  Publish.*
  Collaboration.*
  Runtime.*
  Server.*
  Prism.*
  Profile.*
  ```

* [ ] Keep diagnostics display outside `SagaShared`.

  Done means `SagaShared` does not own:

  * Problems panel,
  * log panel,
  * UI rendering,
  * clickable navigation behavior,
  * editor diagnostics service implementation,
  * runtime diagnostic storage,
  * Forge diagnostic aggregation implementation.

---

## 12. Artifact Reference Contracts

* [x] Add shared artifact reference contracts.

  Done means artifacts can be referenced by:

  * artifact id,
  * artifact kind,
  * content hash,
  * schema id,
  * source resource id,
  * build/cook status,
  * diagnostic summary,
  * profile/target compatibility,
  * package membership.

Expected files:

```txt
Shared/include/SagaShared/Artifacts/ArtifactId.hpp
Shared/include/SagaShared/Artifacts/ArtifactKind.hpp
Shared/include/SagaShared/Artifacts/ArtifactRef.hpp
Shared/include/SagaShared/Artifacts/ArtifactHash.hpp
Shared/include/SagaShared/Artifacts/ArtifactStatus.hpp
Shared/include/SagaShared/Artifacts/ArtifactManifest.hpp
Shared/include/SagaShared/Artifacts/ArtifactDependency.hpp
```

  Shipped in `0.0.8-dev.6` as data-only artifact ids, hashes, refs, dependencies, status, kind, and manifests with no SDE includes.

* [x] Support SDE artifact references without depending on SDE internals.

  Done means shared contracts can reference:

  * compiled graph artifacts,
  * canonical IR artifacts,
  * generated code artifacts,
  * schema artifacts,
  * validation artifacts,
  * dependency manifests.

  But `SagaShared` does not include:

  ```txt
  Tools/SystemDefinitionEngine/src/**
  SDE compiler AST internals
  SDE parser internals
  SDE optimizer internals
  ```

* [x] Support asset/script/build artifact refs.

  Done means artifact contracts can represent:

  * cooked asset artifacts,
  * script assemblies,
  * script binding manifests,
  * generated C# artifacts,
  * package manifests,
  * build reports.

---

## 13. Package Manifest Contracts

* [x] Add shared package manifest format.

  Done means package manifests describe:

  * package id,
  * package kind,
  * package name,
  * package version,
  * build profile,
  * target platform,
  * dependencies,
  * resources,
  * artifacts,
  * asset manifests,
  * script manifests,
  * graph manifests,
  * validation requirements,
  * compatibility metadata.

Expected files:

```txt
Shared/include/SagaShared/Packages/PackageId.hpp
Shared/include/SagaShared/Packages/PackageKind.hpp
Shared/include/SagaShared/Packages/PackageManifest.hpp
Shared/include/SagaShared/Packages/PackageDependency.hpp
Shared/include/SagaShared/Packages/PackageVersion.hpp
Shared/include/SagaShared/Packages/PackageValidationResult.hpp
Shared/include/SagaShared/Packages/PackageCompatibility.hpp
```

  Shipped in `0.0.8-dev.6` as data-only package manifest, package id/kind/version, dependency, compatibility, and validation result contracts.

* [x] Add manifest validation result contracts.

  Done means validation results include:

  * severity,
  * code,
  * message,
  * affected package,
  * affected dependency,
  * affected resource,
  * affected artifact,
  * recoverability flag.

* [x] Keep package loading/cooking implementation outside `SagaShared`.

  Done means package staging and manifest generation implementation belongs to Forge/package systems, not shared contracts.

---

## 14. Build and Publish Contracts

* [x] Add shared build profile contracts.

  Done means build profiles can describe:

  * profile id,
  * display name,
  * target platform,
  * output package kind,
  * validation strictness,
  * SDE options reference,
  * graph validation policy,
  * script compile policy,
  * asset cook policy,
  * package policy.

Expected files:

```txt
Shared/include/SagaShared/Build/BuildProfile.hpp
Shared/include/SagaShared/Build/BuildConfiguration.hpp
Shared/include/SagaShared/Build/TargetPlatform.hpp
Shared/include/SagaShared/Build/BuildProfileId.hpp
```

* [x] Add shared build report contracts.

  Done means build reports can represent:

  * build id,
  * profile,
  * target platform,
  * step summaries,
  * diagnostics summary,
  * artifact outputs,
  * cache decisions,
  * result status.

Expected files:

```txt
Shared/include/SagaShared/Build/BuildId.hpp
Shared/include/SagaShared/Build/BuildStatus.hpp
Shared/include/SagaShared/Build/BuildStepKind.hpp
Shared/include/SagaShared/Build/BuildStepResult.hpp
Shared/include/SagaShared/Build/BuildReport.hpp
```

* [x] Add shared publish readiness contracts.

  Done means publish readiness can represent:

  * ready,
  * ready with warnings,
  * blocked,
  * failed,
  * unknown,
  * blocker list,
  * affected resources,
  * affected artifacts,
  * suggested actions.

Expected files:

```txt
Shared/include/SagaShared/Publish/PublishReadiness.hpp
Shared/include/SagaShared/Publish/PublishStatus.hpp
Shared/include/SagaShared/Publish/PublishBlocker.hpp
Shared/include/SagaShared/Publish/PublishBlockerKind.hpp
Shared/include/SagaShared/Publish/PublishReport.hpp
```

* [x] Keep build orchestration outside `SagaShared`.

  Done means `SagaShared` does not own build planner, step execution, tool invocation, cache implementation, or publishing backend.

  Shipped in `0.0.8-dev.6` as data-only build profile/report and publish readiness/report contracts. Forge orchestration, manifest/report writers, JSON schemas, and publish gates remain outside `SagaShared`.

---

## 15. Gameplay Graph Contracts

* [ ] Add shared gameplay graph contracts.

  Done means graph contracts can represent:

  * graph id,
  * graph kind,
  * graph resource descriptor,
  * graph artifact ref,
  * graph source location,
  * node id,
  * pin id,
  * edge id,
  * block id,
  * block definition descriptor,
  * block palette descriptor,
  * graph diagnostic payload.

Expected files:

```txt
Shared/include/SagaShared/Graph/GraphId.hpp
Shared/include/SagaShared/Graph/GraphKind.hpp
Shared/include/SagaShared/Graph/GraphResourceDescriptor.hpp
Shared/include/SagaShared/Graph/GraphArtifactRef.hpp
Shared/include/SagaShared/Graph/GraphNodeId.hpp
Shared/include/SagaShared/Graph/GraphPinId.hpp
Shared/include/SagaShared/Graph/GraphEdgeId.hpp
Shared/include/SagaShared/Graph/BlockId.hpp
Shared/include/SagaShared/Graph/BlockDefinitionDescriptor.hpp
Shared/include/SagaShared/Graph/BlockPaletteDescriptor.hpp
Shared/include/SagaShared/Graph/GraphDiagnostic.hpp
```

* [ ] Keep graph implementation outside `SagaShared`.

  Forbidden in `SagaShared`:

  * graph editor canvas,
  * graph layout engine,
  * SDE graph parser/compiler passes,
  * runtime graph execution implementation,
  * graph conflict engine implementation,
  * C# code generation implementation.

* [ ] Support graph references across editor/runtime/server/tools.

  Done means graph contracts can be used by:

  * editor graph UI,
  * SDE diagnostics,
  * Forge build reports,
  * Prism stale/generated origin reports,
  * runtime/server artifact manifests,
  * collaboration resource claims/conflicts.

---

## 16. Authority Metadata Contracts

* [ ] Add shared authority metadata contracts.

  Done means contracts exist for:

  * authority context,
  * execution domain,
  * side effects,
  * replication effects,
  * persistence effects,
  * prediction safety,
  * security boundary,
  * authority validation result,
  * authority diagnostic payload.

Expected files:

```txt
Shared/include/SagaShared/Authority/AuthorityContext.hpp
Shared/include/SagaShared/Authority/AuthorityRequirement.hpp
Shared/include/SagaShared/Authority/ExecutionDomain.hpp
Shared/include/SagaShared/Authority/SideEffect.hpp
Shared/include/SagaShared/Authority/ReplicationEffect.hpp
Shared/include/SagaShared/Authority/PersistenceEffect.hpp
Shared/include/SagaShared/Authority/PredictionSafety.hpp
Shared/include/SagaShared/Authority/SecurityBoundary.hpp
Shared/include/SagaShared/Authority/AuthorityValidationResult.hpp
Shared/include/SagaShared/Authority/AuthorityDiagnostic.hpp
```

* [ ] Add graph/block authority metadata descriptors.

  Done means graph/block descriptors can carry authority metadata without depending on graph editor or runtime/server implementation.

Expected files:

```txt
Shared/include/SagaShared/Graph/GraphAuthorityMetadata.hpp
Shared/include/SagaShared/Graph/BlockAuthorityMetadata.hpp
Shared/include/SagaShared/Graph/GraphArtifactAuthorityManifest.hpp
Shared/include/SagaShared/Graph/GraphArtifactDestination.hpp
```

* [ ] Keep authority enforcement outside `SagaShared`.

  Done means `SagaShared` does not own:

  * server validation implementation,
  * runtime authority application,
  * anti-cheat,
  * persistence transaction logic,
  * replication source generation,
  * SDE validation passes.

Shared authority contracts describe rules and metadata.

They do not enforce the universe.

---

## 17. Scripting Contracts

* [ ] Add shared scripting contracts.

  Done means scripting contracts can describe:

  * script project id,
  * script assembly ref,
  * script artifact ref,
  * script binding descriptor,
  * block-callable descriptor,
  * script compile result,
  * script diagnostic,
  * script source location,
  * script authority metadata,
  * generated code origin.

Expected files:

```txt
Shared/include/SagaShared/Scripting/ScriptProjectId.hpp
Shared/include/SagaShared/Scripting/ScriptAssemblyRef.hpp
Shared/include/SagaShared/Scripting/ScriptArtifactRef.hpp
Shared/include/SagaShared/Scripting/ScriptBindingDescriptor.hpp
Shared/include/SagaShared/Scripting/BlockCallableDescriptor.hpp
Shared/include/SagaShared/Scripting/ScriptCompileResult.hpp
Shared/include/SagaShared/Scripting/ScriptDiagnostic.hpp
Shared/include/SagaShared/Scripting/ScriptSourceLocation.hpp
Shared/include/SagaShared/Scripting/ScriptAuthorityMetadata.hpp
Shared/include/SagaShared/Scripting/GeneratedCodeOrigin.hpp
```

* [ ] Keep scripting implementation outside `SagaShared`.

  Forbidden in `SagaShared`:

  * Roslyn/compiler implementation,
  * Mono/.NET host internals,
  * script VM implementation,
  * editor script panels,
  * runtime script execution engine,
  * hot reload implementation,
  * filesystem watchers.

* [ ] Support graph/C# bridge contracts.

  Done means shared contracts can represent:

  * generated C# artifact origin,
  * C# binding descriptors,
  * block-callable metadata,
  * non-reversible conversion marker,
  * source map refs.

---

## 18. Asset and Runtime Resource Contracts

* [ ] Add shared asset contracts.

  Done means asset contracts can describe:

  * asset id,
  * asset kind,
  * asset ref,
  * asset metadata,
  * asset dependency,
  * import status,
  * cook status,
  * asset diagnostic,
  * asset manifest descriptor,
  * cooked artifact descriptor.

Expected files:

```txt
Shared/include/SagaShared/Assets/AssetId.hpp
Shared/include/SagaShared/Assets/AssetKind.hpp
Shared/include/SagaShared/Assets/AssetRef.hpp
Shared/include/SagaShared/Assets/AssetMetadata.hpp
Shared/include/SagaShared/Assets/AssetDependency.hpp
Shared/include/SagaShared/Assets/AssetImportStatus.hpp
Shared/include/SagaShared/Assets/AssetCookStatus.hpp
Shared/include/SagaShared/Assets/AssetDiagnostic.hpp
Shared/include/SagaShared/Assets/AssetManifestDescriptor.hpp
Shared/include/SagaShared/Artifacts/CookedArtifactDescriptor.hpp
```

* [ ] Keep asset pipeline implementation outside `SagaShared`.

  Forbidden in `SagaShared`:

  * importers,
  * file format parsers,
  * texture compression implementation,
  * mesh optimization implementation,
  * audio conversion implementation,
  * runtime asset cache,
  * editor asset panels,
  * build/cook orchestration.

* [ ] Support runtime asset manifest consumption.

  Done means shared asset contracts can be consumed by:

  * editor content browser,
  * Forge asset cook/package reports,
  * runtime asset registry/streaming,
  * server package validation,
  * Prism stale artifact analysis,
  * SDE asset reference validation outputs.

---

## 19. Authoring Profile Contracts

* [ ] Add shared authoring profile contracts.

  Done means authoring profile contracts can describe:

  * profile id,
  * display name,
  * description,
  * default layout ref,
  * visible panels,
  * hidden panels,
  * block palettes,
  * diagnostics detail level,
  * scripting visibility,
  * authority visibility,
  * build detail visibility,
  * collaboration detail visibility,
  * dangerous action prompt policy.

Expected files:

```txt
Shared/include/SagaShared/Authoring/AuthoringProfileId.hpp
Shared/include/SagaShared/Authoring/AuthoringProfileDescriptor.hpp
Shared/include/SagaShared/Authoring/AuthoringPersona.hpp
Shared/include/SagaShared/Authoring/DiagnosticsDetailLevel.hpp
Shared/include/SagaShared/Authoring/PanelVisibilityRule.hpp
Shared/include/SagaShared/Authoring/BlockPaletteVisibilityRule.hpp
Shared/include/SagaShared/Authoring/DangerousActionPolicy.hpp
```

* [ ] Keep profile UI implementation outside `SagaShared`.

  Done means `SagaShared` does not own:

  * layout renderer,
  * editor panel visibility implementation,
  * onboarding UI,
  * profile switching UI,
  * permission enforcement.

* [ ] Keep profiles separate from permissions.

  Done means shared contracts can reference both, but profile does not imply permission.

---

## 20. Canonical IR Reference Contracts

* [ ] Add canonical IR reference contracts.

  Done means shared contracts can reference IR artifacts without owning IR compiler internals.

Expected files:

```txt
Shared/include/SagaShared/IR/IRSchemaId.hpp
Shared/include/SagaShared/IR/IRArtifactRef.hpp
Shared/include/SagaShared/IR/IRGraphRef.hpp
Shared/include/SagaShared/IR/IRVersion.hpp
Shared/include/SagaShared/IR/IRValidationResult.hpp
```

* [ ] Keep actual IR generation outside `SagaShared`.

  Done means SDE owns deterministic IR generation; runtime/server/tools consume references/manifests.

* [ ] Support validation result references.

  Done means IR validation results can identify:

  * schema mismatch,
  * missing node,
  * invalid edge,
  * invalid type,
  * invalid artifact reference,
  * compile-blocking state.

---

## 21. Security Policy Descriptor Contracts

* [ ] Add shared security policy descriptors.

  Done means shared descriptors can represent:

  * security boundary,
  * trust level,
  * permission requirement,
  * artifact domain,
  * restricted API category,
  * shipping/profile restrictions.

Expected files:

```txt
Shared/include/SagaShared/Security/SecurityBoundary.hpp
Shared/include/SagaShared/Security/TrustLevel.hpp
Shared/include/SagaShared/Security/PermissionRequirement.hpp
Shared/include/SagaShared/Security/RestrictedApiCategory.hpp
Shared/include/SagaShared/Security/SecurityPolicyDescriptor.hpp
```

* [ ] Keep enforcement outside `SagaShared`.

  Done means `SagaShared` does not own:

  * permission service,
  * auth service,
  * sandbox implementation,
  * server validation policy,
  * build rejection logic.

---

## 22. Replay and Profiling Event Contracts

* [ ] Add neutral replay/profiling event contracts where multi-module use is proven.

  Done means contracts can describe:

  * event id,
  * event timestamp,
  * source system,
  * resource refs,
  * diagnostic refs,
  * payload kind,
  * stable serialization.

Expected files:

```txt
Shared/include/SagaShared/Profiling/ProfileEvent.hpp
Shared/include/SagaShared/Profiling/ProfileEventKind.hpp
Shared/include/SagaShared/Replay/ReplayEvent.hpp
Shared/include/SagaShared/Replay/ReplayEventKind.hpp
```

* [ ] Keep profilers/replay engines outside `SagaShared`.

  Done means actual capture, storage, rendering, replay execution, and analysis live in owning systems.

---

## 23. Serialization and Versioning Rules

* [ ] Define shared serialization rules.

  Done means shared contracts document:

  * stable enum representation,
  * optional field behavior,
  * unknown field behavior,
  * version fields,
  * hash fields where applicable,
  * forward/backward compatibility policy.

* [ ] Define contract versioning policy.

  Done means every persisted or cross-process contract has:

  * version identifier,
  * migration behavior or rejection behavior,
  * compatibility tests,
  * diagnostic behavior for unsupported versions.

* [ ] Avoid exposing third-party serialization types in public contracts unless approved.

  Done means public shared headers should not casually expose implementation dependencies.

---

## 24. Testing Strategy

### 24.1 Contract Compile Tests

* [ ] Add shared contract compile tests.

  Required coverage:

  * all public shared headers compile standalone,
  * shared headers do not require editor/runtime/server/product private headers,
  * shared headers do not expose Qt,
  * shared headers do not include SDE/Forge/Prism internals.

---

### 24.2 Serialization Tests

* [ ] Add serialization tests.

  Required coverage:

  * ids,
  * descriptors,
  * diagnostics payloads,
  * artifact references,
  * package manifests,
  * collaboration contracts,
  * graph contracts,
  * authority metadata,
  * scripting contracts,
  * asset contracts,
  * build/publish reports,
  * authoring profiles.

---

### 24.3 Compatibility Tests

* [ ] Add compatibility tests for versioned contracts.

  Required coverage:

  * current version decode,
  * older version decode where supported,
  * unsupported version rejection,
  * missing required fields,
  * invalid enum values,
  * unknown optional fields.

---

### 24.4 Dependency Boundary Tests

* [ ] Add dependency boundary tests.

  Required checks:

  * `SagaShared` does not include Qt,
  * `SagaShared` does not include editor private headers,
  * `SagaShared` does not include runtime private headers,
  * `SagaShared` does not include server private headers,
  * `SagaShared` does not include product shell headers,
  * `SagaShared` does not include SagaCollaboration implementation,
  * `SagaShared` does not include SDE internals,
  * `SagaShared` does not include Forge internals,
  * `SagaShared` does not include Prism internals,
  * `SagaShared` does not include asset pipeline implementation,
  * `SagaShared` does not include scripting host/compiler implementation.

---

## 25. Migration Plan

* [ ] Audit existing shared-like contracts.

  Done means every candidate contract is classified as:

  * valid shared contract,
  * product-owned,
  * editor-owned,
  * runtime-owned,
  * server-owned,
  * collaboration-owned,
  * SDE-owned,
  * Forge-owned,
  * Prism-owned,
  * asset-pipeline-owned,
  * scripting-owned,
  * obsolete.

* [ ] Move neutral contracts into `SagaShared`.

  Candidates:

  * ids,
  * descriptors,
  * diagnostics payloads,
  * artifact references,
  * package metadata,
  * permission descriptors,
  * presence snapshots,
  * graph refs,
  * authority metadata,
  * script binding descriptors,
  * asset metadata descriptors,
  * build/publish reports,
  * authoring profile descriptors.

* [ ] Move implementation out of `SagaShared`.

  Done means implementation is moved to the correct owner:

  * product behavior to `Saga`,
  * authoring UI to `SagaEditor`,
  * runtime behavior to `SagaEngine`,
  * authority to `SagaServer`,
  * collaboration implementation to `SagaCollaboration`,
  * compiler behavior to SDE,
  * build orchestration to Forge,
  * code intelligence to Prism,
  * asset import/cook behavior to asset pipeline owner,
  * scripting host/compiler behavior to scripting owner.

* [ ] Freeze invalid shared additions.

  Done means new shared additions require explicit ownership justification.

* [ ] Add CI checks after migration.

  Done means dependency violations fail automatically instead of relying on humans remembering architecture rules, which history suggests is not a plan.

---

## 26. Non-Goals

This roadmap does not own:

* product shell behavior,
* project dashboard implementation,
* editor panels,
* editor docking,
* editor command implementation,
* graph editor canvas implementation,
* script editor implementation,
* asset import UI,
* runtime simulation,
* runtime replication internals,
* runtime asset streaming implementation,
* server authority,
* anti-cheat implementation,
* persistence services,
* collaboration session implementation,
* conflict engine implementation,
* SDE compiler internals,
* Forge build frontend implementation,
* Prism code intelligence implementation,
* SagaTools command dispatch,
* asset importer/cooker implementation,
* scripting host/compiler implementation,
* backend infrastructure.

Related ownership:

| Area                             | Owner                     |
| -------------------------------- | ------------------------- |
| Product shell                    | `Saga`                    |
| Authoring UI                     | `SagaEditor`              |
| Runtime systems                  | `SagaEngine` / Runtime    |
| Server authority                 | `SagaServer`              |
| Collaboration implementation     | `SagaCollaboration`       |
| SDE compiler                     | `SDE`                     |
| Build workflow frontend          | `Forge`                   |
| Code/artifact intelligence       | `Prism`                   |
| Tool ecosystem                   | `TOOLS_ROADMAP.md`        |
| Asset import/cook implementation | Asset pipeline owner/tool |
| Scripting host/compiler          | Scripting owner/toolchain |

---

## 27. Production Definition of Done

* [ ] `SagaShared` contains only neutral contracts and small shared primitives.

* [ ] Every shared contract has at least two valid consumers.

* [ ] Public shared headers do not expose Qt, editor, runtime, server, product shell, collaboration implementation, tool internals, asset pipeline implementation, or scripting implementation.

* [ ] Collaboration contracts are split correctly between `SagaShared` and `SagaCollaboration`.

* [ ] Project/workspace descriptors are available without pulling in product shell implementation.

* [ ] Diagnostics payloads are reusable across editor/runtime/server/tooling.

* [ ] Artifact references support SDE/runtime/editor/build workflows without importing SDE internals.

* [ ] Package manifest contracts are stable and versioned.

* [ ] Gameplay graph contracts are shared without owning graph editor/compiler/runtime implementation.

* [ ] Authority metadata contracts exist without enforcement implementation living in `SagaShared`.

* [ ] Scripting and IR contracts are shared without owning compiler/runtime implementation.

* [ ] Asset contracts support import/cook/runtime relationships without owning importer/cooker/runtime cache implementation.

* [ ] Build/publish contracts support product/editor/tool reports without owning build orchestration.

* [ ] Authoring profile contracts support profile-driven UX without owning editor layout implementation.

* [ ] Security policy descriptors exist without enforcement implementation living in `SagaShared`.

* [ ] Serialization/versioning rules are documented and tested.

* [ ] CI enforces include and CMake dependency boundaries.

---

## 28. Final Architecture Rule

The shared architecture should remain:

```txt
SagaShared
  owns neutral contracts and small shared primitives

Saga
  owns product lifecycle

SagaEditor
  owns authoring UI

SagaEngine / Runtime
  owns game execution

SagaServer
  owns authority

SagaCollaboration
  owns collaboration implementation

SDE
  owns deterministic data compilation

Forge
  owns build orchestration

Prism
  owns code/artifact intelligence

Asset Pipeline
  owns import/cook behavior

Scripting
  owns script compile/runtime binding behavior

Tools
  own standalone workflows
```

`SagaShared` should provide common language.

It should not become the place where systems go to avoid having an owner.

If a shared contract needs threads, sockets, Qt widgets, compiler passes, server policy, asset decoders, build graph execution, or runtime object ownership, it is probably not a shared contract.
