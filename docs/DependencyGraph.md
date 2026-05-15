# SagaEngine Dependency Graph

> Last updated: 2026-05-14  
> Status: Active architecture document  
> Scope: Product, editor, runtime, server, shared contracts, collaboration, tools, and backend dependency boundaries.

---

## 0. Document Purpose

This document defines the allowed dependency direction across SagaEngine, Saga, SagaEditor, SagaServer, SagaShared, SagaCollaboration, SDE, and tool modules.

It exists to prevent accidental architecture drift.

Without this document, every subsystem eventually includes every other subsystem, and then everyone pretends the build graph is “flexible”. It is not. It is just broken with better lighting.

---

## 1. High-Level Product and Runtime Layers

SagaEngine now separates product ownership, editor authoring, runtime execution, server authority, shared contracts, collaboration implementation, standalone tools, and backend services.

The primary user-facing executable is `Saga`.

```txt
                         ┌──────────────────────────────┐
                         │            Saga              │
                         │ Primary Product Executable   │
                         │ Product Shell + Mode Host    │
                         └───────────────┬──────────────┘
                                         │ owns product lifecycle
              ┌──────────────────────────┼──────────────────────────┐
              │                          │                          │
              ▼                          ▼                          ▼
   ┌─────────────────────┐    ┌─────────────────────┐    ┌─────────────────────┐
   │   Editor Module     │    │   Runtime Module    │    │   Server Module     │
   │ authoring surface   │    │ game execution      │    │ authority/session   │
   └──────────┬──────────┘    └──────────┬──────────┘    └──────────┬──────────┘
              │                          │                          │
              └──────────────┬───────────┴──────────────┬───────────┘
                             │                          │
                             ▼                          ▼
                  ┌────────────────────┐      ┌────────────────────┐
                  │    SagaShared      │      │ SagaCollaboration  │
                  │ neutral contracts  │      │ collaboration impl │
                  └─────────┬──────────┘      └─────────┬──────────┘
                            │                           │
                            └──────────────┬────────────┘
                                           ▼
                              ┌────────────────────────┐
                              │      Engine/Core       │
                              │ runtime primitives     │
                              └───────────┬────────────┘
                                          ▼
                              ┌────────────────────────┐
                              │        Backends        │
                              │ DB / Redis / FS / etc. │
                              └────────────────────────┘
```

---

## 2. Product Ownership

`Saga` owns the product-level user experience.

It owns:

- main process lifetime,
- `QApplication`,
- primary main window,
- product shell,
- project selection,
- project creation/opening,
- recent project registry,
- session selection,
- mode switching,
- editor/runtime/server module orchestration,
- product-level diagnostics entry points.

`Saga` is the only primary user-facing executable.

Development compatibility binaries may exist, but they are not the production workflow.

Allowed examples:

```txt
Apps/Saga/
Saga/
Product/
```

Forbidden:

```txt
Editor owns product shell
Runtime owns project dashboard
Server owns user-facing project lifecycle
Tool owns Saga product mode switching
```

---

## 3. Editor Ownership

The editor module owns authoring UI and editor-specific workflows.

The editor may own:

- panels,
- docking,
- inspector,
- hierarchy,
- viewport tools,
- content browser,
- editor commands,
- editor preferences,
- editor diagnostics panels,
- collaboration display panels,
- conflict resolution UI,
- editor-local adapters.

The editor must not own:

- product shell,
- project/session lifecycle truth,
- final collaboration contracts,
- runtime authority,
- server authority,
- global orchestration,
- standalone tool ownership.

Production rule:

```txt
Saga = product host
Editor = authoring module mounted by Saga
```

Development compatibility rule:

```txt
Apps/Editor may exist for local testing only.
It must not become the production product shell.
```

---

## 4. Runtime Ownership

Runtime owns game execution.

The runtime may own:

- game loop,
- simulation stepping,
- scene execution,
- ECS runtime application,
- asset residency,
- replication application,
- prediction,
- interpolation,
- reconciliation,
- rendering integration,
- runtime diagnostics.

The runtime must not own:

- editor panels,
- Qt authoring widgets,
- product dashboard,
- collaboration product lifecycle,
- SDE compiler internals,
- standalone tool orchestration.

Allowed dependency direction:

```txt
Runtime → Engine/Core
Runtime → SagaShared
Runtime → SagaCollaboration service API
Runtime → SDE compiled outputs
```

Forbidden dependency direction:

```txt
Runtime → Editor private headers
Runtime → Editor collaboration headers
Runtime → Product shell internals
Runtime → SDE compiler internals
```

---

## 5. Server Ownership

Server modules own authority and multiplayer session policy.

The server may own:

- authoritative simulation,
- networking,
- packet protocol handling,
- client session state,
- shard/zone authority,
- persistence integration,
- anti-cheat validation,
- replication source generation,
- server diagnostics.

The server must not own:

- editor UI,
- product shell,
- Qt widgets,
- authoring workflows,
- client-only preview policy,
- tool UI,
- editor-private collaboration contracts.

Allowed dependency direction:

```txt
Server → Engine/Core
Server → SagaShared
Server → SagaCollaboration service API
Server → Backends
Server → SDE compiled outputs
```

Forbidden dependency direction:

```txt
Server → Editor
Server → Apps/Saga product shell internals
Server → Apps/Editor
Server → Tools/Forge UI
Server → Tools/Prism UI
```

---

## 6. SagaShared Ownership

`SagaShared` owns neutral contracts and small shared primitives.

It exists because editor, runtime, server, product, and collaboration sometimes need common language.

It must stay small.

Allowed examples:

- session descriptors,
- workspace descriptors,
- participant ids,
- room codes,
- presence snapshots,
- permission grants,
- artifact references,
- stable ids,
- diagnostics primitives,
- protocol-neutral edit operation envelopes,
- shared result/error structures.

Forbidden examples:

- Qt widgets,
- editor UI,
- runtime internals,
- server internals,
- transport implementations,
- persistence engines,
- orchestration logic,
- conflict engines,
- asset import UI,
- product shell state machines,
- tool-specific implementation.

Rule:

```txt
SagaShared contains contracts.
SagaShared does not contain product behavior.
```

If code in `SagaShared` starts needing threads, sockets, databases, UI objects, or lifecycle ownership, it probably does not belong in `SagaShared`.

---

## 7. SagaCollaboration Ownership

`SagaCollaboration` owns collaboration and session implementation.

It may own:

- session lifecycle,
- quick/dev host state,
- quick/dev join state,
- team session state,
- presence,
- permissions,
- claims,
- locks,
- change streams,
- conflict detection,
- reconnect/recovery,
- transport adapters,
- persistence integration,
- service availability state,
- collaboration diagnostics.

The editor consumes collaboration through services and bridges.

The editor does not own the collaboration stack.

Allowed dependency direction:

```txt
Editor → SagaCollaboration API
Runtime → SagaCollaboration API
Server → SagaCollaboration API
Saga → SagaCollaboration API
SagaCollaboration → SagaShared
SagaCollaboration → Engine/Core primitives where appropriate
SagaCollaboration → Backends where appropriate
```

Forbidden dependency direction:

```txt
SagaCollaboration → Editor UI
SagaCollaboration → Editor private panels
SagaCollaboration → Product shell widgets
SagaCollaboration → Runtime-private gameplay systems
SagaCollaboration → Server-private authority internals
```

---

## 8. Deprecated Editor Collaboration Location

The following path is deprecated for final collaboration contracts:

```txt
Editor/include/SagaEditor/Collaboration
```

Rules:

1. Existing code may remain temporarily during migration.
2. New final collaboration contracts must not be added there.
3. Neutral contracts belong in `SagaShared`.
4. Real collaboration implementation belongs in `SagaCollaboration`.
5. Editor may keep UI-facing adapters and panels.
6. Editor must not own product/session truth.

Correct split:

```txt
SagaShared
  neutral contracts

SagaCollaboration
  collaboration services and implementation

SagaEditor
  collaboration UI and editor-facing adapters
```

Incorrect split:

```txt
SagaEditor/Collaboration
  everything dumped here because it was convenient
```

Convenience is not architecture. It is usually future pain wearing a fake mustache.

---

## 9. SagaCollaborationCore Ownership

`SagaCollaborationCore` is optional.

It should exist only if the collaboration implementation grows large enough to justify a lower-level split.

It may eventually own:

- protocol state machines,
- replication internals,
- transport adapters,
- persistent sync engines,
- audit event storage,
- conflict resolution internals,
- distributed authority rules.

Product, editor, runtime, and server code should consume `SagaCollaboration`, not reach directly into `SagaCollaborationCore`.

Allowed direction:

```txt
SagaCollaboration → SagaCollaborationCore
```

Forbidden direction:

```txt
Editor → SagaCollaborationCore
Runtime → SagaCollaborationCore
Server → SagaCollaborationCore
Saga → SagaCollaborationCore
```

---

## 10. Engine/Core Ownership

Engine/Core owns reusable runtime primitives.

It may own:

- math,
- memory utilities,
- containers,
- logging,
- profiling,
- task scheduling primitives,
- filesystem abstraction,
- resource management primitives,
- serialization primitives,
- ECS primitives,
- runtime diagnostics.

It must not own:

- editor UI,
- product shell,
- server-specific policy,
- collaboration product lifecycle,
- standalone tool UX,
- SDE compiler internals.

Allowed direction:

```txt
Engine/Core → platform/backends only when abstracted
```

Forbidden direction:

```txt
Engine/Core → Editor
Engine/Core → Server
Engine/Core → Apps
Engine/Core → Tools
Engine/Core → Saga product shell
Engine/Core → SagaCollaboration implementation
```

Engine/Core must stay below product, editor, runtime, server, collaboration, and tools.

---

## 11. Backend Ownership

Backends provide infrastructure.

Examples:

- filesystem,
- databases,
- Redis,
- object storage,
- authentication services,
- telemetry sinks,
- build caches,
- package registries.

Backends should be accessed through narrow interfaces.

Allowed users:

```txt
Server
SagaCollaboration
Product services
Tool services where explicitly needed
```

Forbidden pattern:

```txt
Random editor panel directly owns backend connection
Runtime gameplay code directly knows database schema
Shared contract layer opens sockets
```

---

## 12. SDE Ownership

SDE is a standalone deterministic data compiler.

SDE must not include headers from:

- Saga,
- SagaEngine,
- SagaEditor,
- SagaServer,
- SagaShared,
- SagaCollaboration,
- Forge,
- Prism,
- SagaTools.

SDE may depend on:

- standard library,
- its own internal modules,
- explicitly approved third-party parser/serialization libraries,
- platform-neutral filesystem utilities if kept isolated.

Saga/editor/runtime/server may consume SDE outputs through explicit integration points.

Allowed output examples:

- compiled graph artifacts,
- schema ids,
- stable hashes,
- artifact references,
- validation diagnostics,
- generated code,
- generated manifests.

Forbidden dependency:

```txt
SDE → Engine headers
SDE → Editor headers
SDE → Runtime headers
SDE → Server headers
SDE → SagaShared headers
SDE → SagaCollaboration headers
```

Correct integration:

```txt
SDE produces artifacts.
Saga/Editor/Runtime/Server consume artifacts.
```

Incorrect integration:

```txt
SDE imports the engine and becomes a secret subsystem.
```

---

## 13. Tool Ownership

Tools remain outside engine/editor/server ownership.

The tool ecosystem is split as follows:

```txt
Tools/SDE
  deterministic data compiler

Tools/Forge
  build workflow frontend

Tools/Prism
  code intelligence and graph builder

Tools/SagaTools
  thin command dispatcher

docs/roadmaps/TOOLS_ROADMAP.md
  ecosystem index only
```

Tools may consume stable contracts or generated outputs where explicitly approved.

Tools must not become hidden owners of product runtime behavior.

Forbidden:

```txt
Forge owns runtime build policy
Prism owns compiler truth
SagaTools owns product lifecycle
SDE includes editor/runtime/server headers
```

---

## 14. Allowed Dependency Matrix

Legend:

- `YES` means allowed.
- `API` means allowed only through stable public API/contracts.
- `NO` means forbidden.
- `OUT` means artifact/output consumption only.

| From / To | Saga | Editor | Runtime | Server | SagaShared | SagaCollaboration | Engine/Core | Backends | SDE | Tools |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Saga | YES | API | API | API | YES | API | API | API | OUT | API |
| Editor | NO | YES | API | NO | YES | API | API | NO | OUT | API |
| Runtime | NO | NO | YES | NO | YES | API | YES | API | OUT | NO |
| Server | NO | NO | API | YES | YES | API | YES | API | OUT | NO |
| SagaShared | NO | NO | NO | NO | YES | NO | API | NO | NO | NO |
| SagaCollaboration | NO | NO | NO | NO | YES | YES | API | API | OUT | NO |
| Engine/Core | NO | NO | NO | NO | NO | NO | YES | API | NO | NO |
| SDE | NO | NO | NO | NO | NO | NO | NO | NO | YES | NO |
| Forge | NO | NO | NO | NO | API | NO | NO | API | OUT | YES |
| Prism | NO | NO | NO | NO | API | NO | NO | NO | OUT | YES |
| SagaTools | NO | NO | NO | NO | API | NO | NO | NO | OUT | YES |

---

## 15. Hard Dependency Rules

These rules are compile-time architecture constraints.

- `Engine/` must not include from `Server/`, `Apps/`, `Saga/`, or `Editor/`.
- `Server/` must not include from `Apps/` or `Editor/`.
- `Editor/` must not include server-private headers.
- Runtime/server modules must not include editor-private collaboration headers.
- New final collaboration contracts must not be added under `Editor/include/SagaEditor/Collaboration`.
- Shared contracts must stay neutral and small.
- Collaboration implementation must not be dumped into `SagaShared`.
- SDE must remain independent of SagaEngine and all product/editor/server headers.
- Tools must remain outside engine/editor/server ownership.
- Product shell must remain owned by `Saga`.
- Development launchers must not define production architecture.

---

## 16. Include Rules

Use public headers only across module boundaries.

Allowed pattern:

```cpp
#include <SagaShared/Session/SessionDescriptor.hpp>
#include <SagaCollaboration/Service/CollaborationService.hpp>
#include <SagaEngine/Core/Diagnostics/DiagnosticSink.hpp>
```

Forbidden pattern:

```cpp
#include "Editor/src/SagaEditor/Panels/CollaborationPanel.cpp"
#include "Server/src/Internal/AuthorityState.hpp"
#include "Apps/Saga/MainWindowPrivate.hpp"
#include "Tools/SystemDefinitionEngine/src/Internal/CompilerGraph.hpp"
```

Private headers must stay private.

If another module needs a private type, create a narrow public contract instead of exporting implementation guts like some kind of dependency crime scene.

---

## 17. CMake Target Rules

Each major module should expose explicit public/private dependencies.

Example:

```cmake
target_link_libraries(SagaEditor
    PUBLIC
        SagaShared
        SagaCollaboration
    PRIVATE
        SagaEngineCore
)
```

Bad example:

```cmake
target_link_libraries(SagaShared
    PUBLIC
        SagaEditor
        SagaServer
        SagaCollaboration
)
```

`SagaShared` must not depend upward.

---

## 18. Public API Stability Rules

Public APIs should be stable, boring, and explicit.

Allowed public API qualities:

- small structs,
- stable ids,
- explicit ownership,
- no hidden global state,
- no UI types unless the module is UI-owned,
- no direct backend handles,
- no private implementation leakage.

Forbidden public API qualities:

- raw editor widget ownership,
- database connection handles,
- thread ownership hidden behind innocent-looking structs,
- product shell callbacks inside runtime contracts,
- server authority internals exposed to editor.

---

## 19. Runtime Multiplayer vs Editor Collaboration

Runtime multiplayer and editor collaboration may share primitives.

They must not share policy blindly.

Runtime/game policy optimizes for:

- low latency,
- server authority,
- prediction,
- reconciliation,
- relevance filtering,
- bandwidth control.

Editor collaboration policy optimizes for:

- correctness,
- visibility,
- permissions,
- edit ownership,
- conflict handling,
- safe publishing.

Allowed shared primitives:

- stable ids,
- ordered event envelopes,
- canonical hashing,
- diagnostics payloads,
- participant identity,
- bounded memory policies.

Forbidden shared policy merge:

```txt
Gameplay replication policy == editor collaboration policy
```

They are related problems, not the same problem.

---

## 20. Product Mode Boundaries

Saga may host multiple modes.

Expected modes:

- project dashboard,
- editor mode,
- runtime preview mode,
- server/session mode,
- collaboration/session management mode,
- diagnostics mode.

Mode ownership:

```txt
Saga owns mode switching.
Each mounted module owns its internal behavior.
```

Forbidden:

```txt
Editor directly replaces Saga product shell.
Runtime directly opens product dashboard.
Server directly owns user-facing mode switching.
```

---

## 21. Migration Rules

During migration from older structure:

1. Do not move everything at once.
2. Move contracts before implementation.
3. Add compatibility adapters only when necessary.
4. Mark deprecated include paths clearly.
5. Prevent new code from using deprecated ownership paths.
6. Delete adapters after consumers migrate.
7. Update CMake dependencies immediately after moving ownership.

Deprecated collaboration path:

```txt
Editor/include/SagaEditor/Collaboration
```

Target split:

```txt
SagaShared
SagaCollaboration
SagaEditor UI adapters
```

Deprecated editor-as-product assumption:

```txt
Apps/Editor is the product.
```

Target product model:

```txt
Saga is the product.
Editor is a mounted authoring module.
```

---

## 22. CI Enforcement

Architecture rules should eventually be enforced by CI.

Recommended checks:

- forbidden include path scanner,
- forbidden CMake dependency scanner,
- public/private header boundary checker,
- deprecated include usage checker,
- tool dependency boundary checker,
- SDE independence checker,
- editor-private collaboration include checker.

Example forbidden include scan:

```txt
Runtime/** must not include Editor/**
Server/** must not include Editor/**
Engine/** must not include Apps/**
Engine/** must not include Server/**
SagaShared/** must not include Editor/**
SagaShared/** must not include SagaCollaboration/**
SDE/** must not include Saga*/**
```

`Tools/scripts/check_engine_server_boundary.py` enforces zero `SagaServer`
references in public `SagaEngine` headers. Engine-owned networking primitives
live under `SagaEngine/Networking`; data-only replication wire contracts live
under `SagaShared/Replication`.

The goal is not bureaucracy.

The goal is preventing future humans, in their natural chaos, from casually turning the architecture into soup.

---

## 23. Documentation Ownership

Related active documents:

| Document | Owner | Purpose |
|---|---|---|
| `DependencyGraph.md` | Architecture | Dependency direction and ownership boundaries |
| `COLLABORATION_ROADMAP.md` | Product collaboration | Collaboration UX, sessions, permissions, presence, conflicts |
| `EDITOR_ROADMAP.md` | SagaEditor | Authoring module roadmap |
| `ENGINE_ROADMAP.md` | SagaEngine | Runtime/server roadmap |
| `SHARED_ROADMAP.md` | SagaShared | Neutral shared contracts and shared editor/runtime systems |
| `TOOLS_ROADMAP.md` | Tool ecosystem | Tool ownership index |
| `SDE_ROADMAP.md` | SDE | Deterministic data compiler |
| `FORGE_ROADMAP.md` | Forge | Build workflow frontend |
| `PRISM_ROADMAP.md` | Prism | Code intelligence |
| `SAGATOOLS_ROADMAP.md` | SagaTools | Thin tool dispatcher |

---

## 24. Summary

The intended architecture is:

```txt
Saga owns the product.
Editor owns authoring UX.
Runtime owns game execution.
Server owns authority.
SagaShared owns neutral contracts.
SagaCollaboration owns collaboration implementation.
Engine/Core owns runtime primitives.
SDE owns deterministic data compilation.
Tools own standalone workflows.
Backends provide infrastructure.
```

Anything that violates this should be treated as architecture debt, not creative freedom.

Architecture is not there to make folders pretty.

It is there so the project can still be understood after more than three days of development and one emotionally unstable refactor.
