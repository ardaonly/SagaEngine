# SagaShared — Shared Contracts Roadmap

> Last updated: 2026-05-14  
> Status: Active roadmap  
> Target: A small, neutral, stable shared contract layer used by Saga, SagaEditor, SagaEngine runtime, SagaServer, SagaCollaboration, and approved tools.  
> Scope: Cross-module contracts, identifiers, descriptors, diagnostics payloads, package metadata, scripting contracts, canonical IR references, replay/profiling event contracts, and editor/runtime integration boundaries.

---

## 0. Roadmap Convention

- `[x]` — Shipped. The note after the item names the files, modules, or integration points that represent the work and highlights any decisions worth preserving.
- `[ ]` — Open. Either unstarted or partially explored; the item describes the finished production state rather than interim scaffolding.
- Shipped items must name the files, modules, or integration points that represent the completed work.
- Open items must describe the finished state, not temporary scaffolding.
- `SagaShared` must remain neutral and small.
- `SagaShared` must not contain implementation ownership.
- `SagaShared` must not depend upward into editor, runtime, server, product shell, collaboration implementation, or tools.
- Shared contracts must be stable enough to survive use by multiple modules.

---

## 1. Document Purpose

This document defines the roadmap for `SagaShared`.

`SagaShared` exists for contracts that genuinely need to be consumed by multiple Saga modules.

A subsystem belongs here only if:

- it has at least one editor consumer,
- it has at least one runtime/server/product/collaboration/tool consumer,
- neither side can own the contract alone without creating a dependency leak.

This roadmap must not become a dumping ground.

If a system is shared only because nobody wants to own it, it does not belong here.

That is not architecture.

That is abandonment with a nicer folder name.

---

## 2. Companion Documents

| Document | Purpose |
|---|---|
| `DependencyGraph.md` | Dependency ownership and compile-time architecture boundaries |
| `SAGA_PRODUCT_ROADMAP.md` | Saga product shell and primary executable behavior |
| `EDITOR_ROADMAP.md` | Authoring module mounted by Saga |
| `ENGINE_ROADMAP.md` | Runtime/server execution and authority |
| `COLLABORATION_ROADMAP.md` | Product collaboration, sessions, presence, permissions, conflicts |
| `TOOLS_ROADMAP.md` | Tool ecosystem ownership and boundaries |
| `SDE_ROADMAP.md` | Deterministic data compiler roadmap |
| `ClientReplicationFormalSpec.md` | Runtime/client replication formal contract |

---

## 3. Ownership Boundary

- [x] Define `SagaShared` as neutral contract ownership, not implementation ownership.

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
  SagaShared does not own product behavior, editor UI, runtime internals, server authority, collaboration implementation, or tool internals.
  ```

- [ ] Keep `SagaShared` limited to contracts with genuine multi-module consumers.

  Done means every shared contract has:

  - a named owner document,
  - at least two valid consumers,
  - no dependency on private implementation headers,
  - no dependency on UI framework types,
  - no backend connection ownership,
  - no process lifecycle ownership.

- [ ] Add documentation for every shared contract family.

  Done means each shared contract folder explains:

  - what the contract represents,
  - which modules consume it,
  - which module owns implementation,
  - what is explicitly forbidden in that folder.

---

## 4. Belongs Here

- [ ] Keep valid shared systems inside `SagaShared`.

  Valid shared systems include:

  - C# scripting host contracts,
  - canonical IR definitions,
  - editor/runtime compilation pipeline contracts,
  - package manifest format,
  - shared security policy descriptors,
  - profiling/replay event contracts,
  - sample content contracts used by both editor and runtime,
  - neutral collaboration contracts,
  - project/workspace descriptors,
  - diagnostics payloads,
  - artifact references,
  - stable ids.

- [ ] Require every shared system to have neutral data-only contracts unless explicitly justified.

  Done means shared headers primarily contain:

  - ids,
  - descriptors,
  - enums,
  - result types,
  - POD-style data structures,
  - serialization-safe payloads,
  - narrow interfaces where truly required.

---

## 5. Does Not Belong Here

- [ ] Prevent pure editor systems from entering `SagaShared`.

  Forbidden examples:

  - editor panels,
  - editor docking,
  - editor menus,
  - Qt widgets,
  - editor command implementation,
  - inspector implementation,
  - content browser implementation.

- [ ] Prevent pure runtime systems from entering `SagaShared`.

  Forbidden examples:

  - runtime ECS internals,
  - renderer internals,
  - replication apply internals,
  - prediction implementation,
  - interpolation buffers,
  - asset streaming implementation.

- [ ] Prevent server authority systems from entering `SagaShared`.

  Forbidden examples:

  - authoritative simulation policy,
  - shard authority implementation,
  - server persistence services,
  - anti-cheat implementation,
  - packet transport internals.

- [ ] Prevent product shell systems from entering `SagaShared`.

  Forbidden examples:

  - product dashboard widgets,
  - project launcher implementation,
  - recent project registry implementation,
  - Saga mode switching implementation,
  - product-level window ownership.

- [ ] Prevent tools from dumping implementation into `SagaShared`.

  Forbidden examples:

  - SDE compiler internals,
  - Forge build frontend implementation,
  - Prism code graph implementation,
  - SagaTools dispatch internals.

---

## 6. Dependency Rules

### 6.1 Allowed Dependencies

- [ ] Allow product/editor/runtime/server/collaboration/tools to consume shared contracts.

  Allowed examples:

  ```txt
  Saga → SagaShared
  SagaEditor → SagaShared
  Runtime → SagaShared
  Server → SagaShared
  SagaCollaboration → SagaShared
  Forge → SagaShared API where explicitly approved
  Prism → SagaShared API where explicitly approved
  SagaTools → SagaShared API where explicitly approved
  ```

- [ ] Allow `SagaShared` to depend only on approved low-level primitives.

  Allowed examples:

  ```txt
  SagaShared → standard library
  SagaShared → platform-neutral primitive libraries
  SagaShared → approved serialization primitives
  SagaShared → approved diagnostics primitive types
  ```

---

### 6.2 Forbidden Dependencies

- [ ] Prevent `SagaShared` from depending upward.

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
  ```

- [ ] Prevent `SagaShared` from exposing framework-specific UI types.

  Forbidden examples:

  ```cpp
  #include <QWidget>
  #include <QMainWindow>
  #include <QApplication>
  #include <QDockWidget>
  ```

- [ ] Prevent `SagaShared` from owning transport or persistence implementation.

  Forbidden examples:

  ```txt
  Redis client
  database connection
  socket transport
  relay implementation
  cloud session backend
  asset storage backend
  ```

---

## 7. Core Shared Identity Contracts

- [ ] Add stable shared id contracts.

  Done means shared ids exist for:

  - project id,
  - workspace id,
  - session id,
  - participant id,
  - artifact id,
  - resource id,
  - package id,
  - schema id,
  - diagnostic id.

Expected files:

```txt
Shared/include/SagaShared/Ids/ProjectId.hpp
Shared/include/SagaShared/Ids/WorkspaceId.hpp
Shared/include/SagaShared/Ids/SessionId.hpp
Shared/include/SagaShared/Ids/ParticipantId.hpp
Shared/include/SagaShared/Ids/ArtifactId.hpp
Shared/include/SagaShared/Ids/ResourceId.hpp
Shared/include/SagaShared/Ids/PackageId.hpp
Shared/include/SagaShared/Ids/SchemaId.hpp
Shared/include/SagaShared/Ids/DiagnosticId.hpp
```

- [ ] Keep shared ids deterministic and serialization-safe.

  Done means:

  - ids are stable across processes,
  - ids can be serialized,
  - ids can be logged,
  - invalid/null ids are explicit,
  - id comparison is cheap and deterministic.

---

## 8. Project and Workspace Contracts

- [ ] Add shared project descriptor contracts.

  Done means project descriptors can describe:

  - project id,
  - display name,
  - project root,
  - project version,
  - package manifest reference,
  - active workspace reference,
  - collaboration mode where applicable,
  - validation state.

Expected files:

```txt
Shared/include/SagaShared/Project/ProjectDescriptor.hpp
Shared/include/SagaShared/Project/ProjectVersion.hpp
Shared/include/SagaShared/Project/ProjectValidationState.hpp
```

- [ ] Add shared workspace descriptor contracts.

  Done means workspace descriptors can describe:

  - workspace id,
  - project id,
  - local root path,
  - active user id,
  - sync state,
  - dirty resource state,
  - active mode hint.

Expected files:

```txt
Shared/include/SagaShared/Workspace/WorkspaceDescriptor.hpp
Shared/include/SagaShared/Workspace/WorkspaceSyncState.hpp
```

- [ ] Keep project/workspace implementation outside `SagaShared`.

  Done means `SagaShared` does not own:

  - project dashboard,
  - recent project registry,
  - project file IO implementation,
  - cloud workspace sync,
  - editor project open UI.

---

## 9. Collaboration Contracts Boundary

- [x] Define collaboration ownership split.

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

- [ ] Add neutral collaboration contracts under `SagaShared`.

  Done means shared collaboration contracts include:

  - participant id,
  - session id,
  - room code,
  - session descriptor,
  - workspace descriptor reference,
  - presence snapshot,
  - permission grant,
  - resource claim,
  - resource lock,
  - collaboration diagnostic.

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
Shared/include/SagaShared/Collaboration/CollaborationDiagnostic.hpp
```

- [ ] Keep collaboration implementation out of `SagaShared`.

  Forbidden in `SagaShared`:

  - session lifecycle implementation,
  - host/join state machines,
  - transport adapters,
  - backend sync,
  - conflict engine,
  - lock manager implementation,
  - permission service implementation,
  - reconnect/recovery implementation.

- [ ] Prevent new final collaboration contracts from being added under editor-private headers.

  Deprecated path:

  ```txt
  Editor/include/SagaEditor/Collaboration
  ```

  Correct target:

  ```txt
  Shared/include/SagaShared/Collaboration
  ```

---

## 10. Diagnostics Contracts

- [ ] Add shared diagnostics payload contracts.

  Done means diagnostics can be represented without depending on editor/runtime/server implementation.

Expected files:

```txt
Shared/include/SagaShared/Diagnostics/DiagnosticSeverity.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticCode.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticMessage.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticLocation.hpp
Shared/include/SagaShared/Diagnostics/DiagnosticPayload.hpp
```

- [ ] Support resource-linked diagnostics.

  Done means diagnostics can reference:

  - project,
  - workspace,
  - resource,
  - artifact,
  - schema,
  - source range,
  - runtime entity where safe.

- [ ] Keep diagnostics display outside `SagaShared`.

  Done means `SagaShared` does not own:

  - Problems panel,
  - log panel,
  - UI rendering,
  - clickable navigation behavior,
  - editor diagnostics service implementation.

---

## 11. Artifact Reference Contracts

- [ ] Add shared artifact reference contracts.

  Done means artifacts can be referenced by:

  - artifact id,
  - artifact kind,
  - content hash,
  - schema id,
  - source resource id,
  - build/cook status,
  - diagnostic summary.

Expected files:

```txt
Shared/include/SagaShared/Artifacts/ArtifactId.hpp
Shared/include/SagaShared/Artifacts/ArtifactKind.hpp
Shared/include/SagaShared/Artifacts/ArtifactRef.hpp
Shared/include/SagaShared/Artifacts/ArtifactHash.hpp
Shared/include/SagaShared/Artifacts/ArtifactStatus.hpp
```

- [ ] Support SDE artifact references without depending on SDE internals.

  Done means shared contracts can reference:

  - compiled graph artifacts,
  - generated code artifacts,
  - schema artifacts,
  - validation artifacts.

  But `SagaShared` does not include:

  ```txt
  Tools/SystemDefinitionEngine/src/**
  SDE compiler AST internals
  SDE parser internals
  SDE optimizer internals
  ```

---

## 12. Package Manifest Contracts

- [ ] Add shared package manifest format.

  Done means package manifests describe:

  - package id,
  - package name,
  - package version,
  - dependencies,
  - resources,
  - artifacts,
  - build profiles,
  - validation requirements.

Expected files:

```txt
Shared/include/SagaShared/Packages/PackageId.hpp
Shared/include/SagaShared/Packages/PackageManifest.hpp
Shared/include/SagaShared/Packages/PackageDependency.hpp
Shared/include/SagaShared/Packages/PackageVersion.hpp
```

- [ ] Add manifest validation result contracts.

  Done means validation results include:

  - severity,
  - code,
  - message,
  - affected package,
  - affected dependency,
  - affected resource,
  - recoverability flag.

Expected files:

```txt
Shared/include/SagaShared/Packages/PackageValidationResult.hpp
```

- [ ] Keep package loading/cooking implementation outside `SagaShared`.

  Done means implementation belongs to owning systems:

  - editor import/cook UX,
  - runtime asset registry,
  - build tools,
  - package services.

---

## 13. Scripting Contracts

- [ ] Add shared scripting host contracts.

  Done means editor/runtime can agree on:

  - script id,
  - script language,
  - script entry point,
  - script compile status,
  - script diagnostic payload,
  - script artifact reference.

Expected files:

```txt
Shared/include/SagaShared/Scripting/ScriptId.hpp
Shared/include/SagaShared/Scripting/ScriptLanguage.hpp
Shared/include/SagaShared/Scripting/ScriptDescriptor.hpp
Shared/include/SagaShared/Scripting/ScriptCompileResult.hpp
```

- [ ] Add C# scripting contract boundary where needed.

  Done means shared contracts describe:

  - assembly reference,
  - type reference,
  - method reference,
  - compile diagnostics,
  - runtime binding descriptor.

Expected files:

```txt
Shared/include/SagaShared/Scripting/CSharpAssemblyRef.hpp
Shared/include/SagaShared/Scripting/CSharpTypeRef.hpp
Shared/include/SagaShared/Scripting/CSharpBindingDescriptor.hpp
```

- [ ] Keep scripting runtime implementation outside `SagaShared`.

  Forbidden in `SagaShared`:

  - C# runtime host implementation,
  - compiler process ownership,
  - editor script UI,
  - runtime script VM internals,
  - hot reload implementation.

---

## 14. Canonical IR Contracts

- [ ] Add canonical IR reference contracts.

  Done means editor/runtime/tools can reference canonical IR without importing compiler internals.

Expected files:

```txt
Shared/include/SagaShared/IR/IRSchemaId.hpp
Shared/include/SagaShared/IR/IRNodeId.hpp
Shared/include/SagaShared/IR/IRGraphRef.hpp
Shared/include/SagaShared/IR/IRArtifactRef.hpp
```

- [ ] Add IR validation result contracts.

  Done means IR validation can report:

  - schema mismatch,
  - missing node,
  - invalid edge,
  - invalid type,
  - invalid artifact reference,
  - compile-blocking state.

Expected files:

```txt
Shared/include/SagaShared/IR/IRValidationResult.hpp
```

- [ ] Keep IR compiler/optimizer internals outside `SagaShared`.

  Forbidden in `SagaShared`:

  - parser AST,
  - optimizer passes,
  - code generation internals,
  - compiler execution pipeline,
  - SDE private graph structures.

---

## 15. Profiling and Replay Event Contracts

- [ ] Add shared profiling event contracts.

  Done means profiling events can describe:

  - event id,
  - event name,
  - category,
  - timestamp,
  - duration,
  - thread id,
  - scope id,
  - metadata payload.

Expected files:

```txt
Shared/include/SagaShared/Profiling/ProfileEvent.hpp
Shared/include/SagaShared/Profiling/ProfileCategory.hpp
Shared/include/SagaShared/Profiling/ProfileMetadata.hpp
```

- [ ] Add replay event contracts.

  Done means replay events can describe:

  - replay stream id,
  - tick id,
  - event kind,
  - actor id,
  - payload hash,
  - deterministic ordering key.

Expected files:

```txt
Shared/include/SagaShared/Replay/ReplayEvent.hpp
Shared/include/SagaShared/Replay/ReplayStreamId.hpp
Shared/include/SagaShared/Replay/ReplayTick.hpp
```

- [ ] Keep profiling/replay execution outside `SagaShared`.

  Done means runtime/editor/server own:

  - event collection,
  - storage,
  - visualization,
  - replay runner,
  - deterministic simulation verification.

---

## 16. Security Policy Contracts

- [ ] Add shared security policy descriptors.

  Done means shared contracts can describe:

  - permission id,
  - role id,
  - capability id,
  - access result,
  - denial reason.

Expected files:

```txt
Shared/include/SagaShared/Security/PermissionId.hpp
Shared/include/SagaShared/Security/RoleId.hpp
Shared/include/SagaShared/Security/CapabilityId.hpp
Shared/include/SagaShared/Security/AccessResult.hpp
Shared/include/SagaShared/Security/AccessDenialReason.hpp
```

- [ ] Keep enforcement implementation outside `SagaShared`.

  Done means enforcement belongs to:

  - Saga product services,
  - SagaCollaboration permission service,
  - server authority,
  - backend/auth services,
  - editor UI consumers.

`SagaShared` may define the words.

It must not become the police, court, prison, and paperwork department.

---

## 17. Serialization Contracts

- [ ] Define shared serialization-safe data rules.

  Done means shared contracts:

  - avoid raw owning pointers,
  - avoid framework-specific types,
  - define version fields where needed,
  - define invalid/default states,
  - can be serialized deterministically where required.

- [ ] Add versioned contract support where needed.

  Done means contracts that cross process/network/file boundaries include:

  - schema version,
  - compatibility policy,
  - migration notes,
  - validation result.

Expected files:

```txt
Shared/include/SagaShared/Serialization/SchemaVersion.hpp
Shared/include/SagaShared/Serialization/VersionedPayload.hpp
Shared/include/SagaShared/Serialization/CompatibilityResult.hpp
```

- [ ] Keep serialization implementation narrow.

  Done means `SagaShared` may define payload structures and version fields, but does not own every serializer implementation by default.

---

## 18. Error and Result Types

- [ ] Add shared result/error contracts.

  Done means common operations can return:

  - success/failure,
  - error code,
  - readable message,
  - recoverability flag,
  - optional diagnostic reference.

Expected files:

```txt
Shared/include/SagaShared/Result/ResultCode.hpp
Shared/include/SagaShared/Result/OperationResult.hpp
Shared/include/SagaShared/Result/FailureReason.hpp
```

- [ ] Keep result types boring and stable.

  Done means result types do not contain:

  - UI callbacks,
  - exception-only behavior,
  - backend handles,
  - implementation-specific objects,
  - transport-specific state.

Boring shared contracts are good.

Exciting shared contracts usually mean someone smuggled a subsystem into a header.

---

## 19. Runtime/Editor Compilation Pipeline Contracts

- [ ] Add shared compilation pipeline contracts.

  Done means editor/runtime/tools can agree on:

  - compile request descriptor,
  - compile artifact reference,
  - compile status,
  - compile diagnostics,
  - dependency references.

Expected files:

```txt
Shared/include/SagaShared/Compilation/CompileRequest.hpp
Shared/include/SagaShared/Compilation/CompileStatus.hpp
Shared/include/SagaShared/Compilation/CompileResult.hpp
Shared/include/SagaShared/Compilation/CompileDependency.hpp
```

- [ ] Keep compilation execution outside `SagaShared`.

  Done means actual compile execution belongs to:

  - SDE,
  - asset cook pipeline,
  - scripting compiler integration,
  - Forge/tooling workflows,
  - runtime artifact loaders.

---

## 20. Tool Consumption Boundary

- [ ] Allow tools to consume stable shared contracts only when explicitly approved.

  Valid examples:

  - Forge reading package manifest contracts,
  - Prism reading stable source/artifact references,
  - SagaTools dispatching based on stable tool descriptors,
  - SDE emitting artifact references compatible with shared contracts.

- [ ] Prevent tools from turning `SagaShared` into a tool implementation dependency hub.

  Forbidden examples:

  ```txt
  SagaShared owns Forge build execution
  SagaShared owns Prism graph database
  SagaShared owns SagaTools command dispatch
  SagaShared owns SDE compiler internals
  ```

---

## 21. SDE Boundary

- [x] Define SDE as standalone and independent.

  Represented by:

  ```txt
  SDE_ROADMAP.md
  SHARED_ROADMAP.md
  ENGINE_ROADMAP.md
  DependencyGraph.md
  ```

  Preserved decision:

  ```txt
  SDE produces deterministic artifacts.
  SagaShared may define artifact references and diagnostics payloads.
  SDE must not depend on SagaShared unless explicitly allowed by a future architecture decision.
  ```

- [ ] Keep SDE outputs consumable without forcing SDE to depend on Saga modules.

  Done means SDE can emit:

  - artifact refs,
  - hashes,
  - schema ids,
  - generated artifacts,
  - diagnostics payloads,

  through stable output formats.

- [ ] Prevent reverse dependency from SDE to Saga modules.

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
  ```

This may look strict.

Good.

SDE is a compiler, not a social butterfly.

---

## 22. Public Header Rules

- [ ] Keep all `SagaShared` public headers implementation-neutral.

  Done means public headers do not include:

  - editor private headers,
  - runtime private headers,
  - server private headers,
  - product shell headers,
  - collaboration implementation headers,
  - tool implementation headers.

- [ ] Keep public headers lightweight.

  Done means headers avoid:

  - heavy template machinery unless justified,
  - global state,
  - backend clients,
  - thread ownership,
  - event loops,
  - UI framework types.

- [ ] Add include scanner for forbidden dependencies.

  Required checks:

  ```txt
  Shared/include/SagaShared/** must not include Editor/**
  Shared/include/SagaShared/** must not include Runtime/private/**
  Shared/include/SagaShared/** must not include Server/**
  Shared/include/SagaShared/** must not include SagaCollaboration/**
  Shared/include/SagaShared/** must not include Apps/Saga/**
  Shared/include/SagaShared/** must not include Tools/**
  ```

---

## 23. CMake Rules

- [ ] Keep `SagaShared` low in the dependency graph.

  Correct examples:

  ```cmake
  target_link_libraries(SagaEditor
      PUBLIC
          SagaShared
  )

  target_link_libraries(SagaCollaboration
      PUBLIC
          SagaShared
  )

  target_link_libraries(SagaRuntime
      PUBLIC
          SagaShared
  )
  ```

- [ ] Prevent upward CMake dependencies.

  Bad examples:

  ```cmake
  target_link_libraries(SagaShared
      PUBLIC
          SagaEditor
          SagaCollaboration
          SagaServer
          Saga
  )
  ```

- [ ] Add CMake dependency validation.

  Done means CI fails if `SagaShared` links upward into product/editor/runtime/server/collaboration/tool implementation targets.

---

## 24. Testing Requirements

- [ ] Add shared contract serialization tests.

  Required coverage:

  - ids,
  - descriptors,
  - diagnostics payloads,
  - artifact references,
  - package manifests,
  - collaboration contracts.

- [ ] Add compatibility tests for versioned contracts.

  Required coverage:

  - current version decode,
  - older version decode where supported,
  - unsupported version rejection,
  - missing required fields,
  - invalid enum values.

- [ ] Add dependency boundary tests.

  Required checks:

  - `SagaShared` does not include Qt,
  - `SagaShared` does not include editor private headers,
  - `SagaShared` does not include server private headers,
  - `SagaShared` does not include SagaCollaboration implementation,
  - `SagaShared` does not include SDE internals.

---

## 25. Migration Plan

- [ ] Audit existing shared-like contracts.

  Done means every candidate contract is classified as:

  - valid shared contract,
  - editor-owned,
  - runtime-owned,
  - server-owned,
  - collaboration-owned,
  - tool-owned,
  - obsolete.

- [ ] Move neutral contracts into `SagaShared`.

  Candidates:

  - ids,
  - descriptors,
  - diagnostics payloads,
  - artifact references,
  - package metadata,
  - permission descriptors,
  - presence snapshots.

- [ ] Move implementation out of `SagaShared`.

  Done means implementation is moved to the correct owner:

  - product behavior to `Saga`,
  - authoring UI to `SagaEditor`,
  - runtime behavior to `SagaEngine`,
  - authority to `SagaServer`,
  - collaboration implementation to `SagaCollaboration`,
  - compiler behavior to SDE,
  - tool behavior to the owning tool.

- [ ] Freeze invalid shared additions.

  Done means new shared additions require explicit ownership justification.

- [ ] Add CI checks after migration.

  Done means dependency violations fail automatically instead of relying on humans remembering architecture rules, which history suggests is not a plan.

---

## 26. Non-Goals

This roadmap does not own:

- product shell behavior,
- project dashboard implementation,
- editor panels,
- editor docking,
- editor command implementation,
- runtime simulation,
- runtime replication internals,
- server authority,
- collaboration session implementation,
- conflict engine implementation,
- SDE compiler internals,
- Forge build frontend,
- Prism code intelligence implementation,
- SagaTools command dispatch,
- backend infrastructure.

Related ownership:

| Area | Owner |
|---|---|
| Product shell | `Saga` |
| Authoring UI | `SagaEditor` |
| Runtime systems | `SagaEngine` |
| Server authority | `SagaServer` |
| Collaboration implementation | `SagaCollaboration` |
| SDE compiler | `SDE` |
| Tool ecosystem | `TOOLS_ROADMAP.md` |

---

## 27. Production Definition of Done

- [ ] `SagaShared` contains only neutral contracts and small shared primitives.

- [ ] Every shared contract has at least two valid consumers.

- [ ] Public shared headers do not expose Qt, editor, runtime, server, product shell, collaboration implementation, or tool internals.

- [ ] Collaboration contracts are split correctly between `SagaShared` and `SagaCollaboration`.

- [ ] Project/workspace descriptors are available without pulling in product shell implementation.

- [ ] Diagnostics payloads are reusable across editor/runtime/server/tooling.

- [ ] Artifact references support SDE/runtime/editor workflows without importing SDE internals.

- [ ] Package manifest contracts are stable and versioned.

- [ ] Scripting and IR contracts are shared without owning compiler/runtime implementation.

- [ ] Security policy descriptors exist without enforcement implementation living in `SagaShared`.

- [ ] Serialization/versioning rules are documented and tested.

- [ ] CI enforces include and CMake dependency boundaries.

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

Tools
  own standalone workflows
```

`SagaShared` should be boring.

That is the compliment.

The moment it becomes exciting, someone probably put a subsystem where a contract was supposed to be.