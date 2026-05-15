# SagaEngine — Runtime and Server Foundation Roadmap

> Last updated: 2026-05-15
> Status: Active roadmap
> Target: A production-grade authoritative multiplayer runtime and server foundation for Saga projects.
> Scope: Runtime execution, networking, security, packet protocols, authoritative simulation, replication, prediction, reconciliation, interpolation, zone/shard management, world kernel systems, deterministic runtime support, asset streaming, package/artifact consumption, gameplay graph artifact consumption, script binding consumption, runtime diagnostics, server diagnostics, testing, and deployment-facing runtime/server foundations.

---

## 0. Roadmap Convention

* `[x]` — Shipped. The note after the item names the files, modules, tests, or integration points that represent the completed work and highlights decisions worth preserving.
* `[ ]` — Open. Either unstarted or partially explored; the item describes the finished production state rather than temporary scaffolding.
* Shipped items must name the files, modules, tests, artifact formats, or integration points that represent completed work.
* Open items must describe finished production behavior, not temporary scaffolding.
* Runtime/server code must not include editor-private headers.
* Engine/Core must not include from `Editor/`, `Apps/`, `Saga/`, `Server/`, or tool implementation folders.
* SDE must remain a standalone deterministic data compiler.
* Runtime/server may consume SDE compiled outputs and manifests through explicit artifact/package boundaries.
* Collaboration contracts must be consumed through `SagaShared` or `SagaCollaboration` service APIs, not editor-private paths.
* Runtime asset streaming consumes runtime-ready assets; it does not own editor import/cook workflows.
* Server authority is not optional for MMO gameplay truth.

A runtime that trusts authoring artifacts without validating manifests is not flexible.

It is just optimistic about bugs.

---

## 1. Document Purpose

This document is the source of truth for SagaEngine runtime and server systems.

It covers:

* runtime execution,
* runtime package consumption,
* server package consumption,
* manifest validation,
* networking,
* security,
* packet protocols,
* authoritative simulation,
* client/server replication,
* prediction and reconciliation,
* interpolation,
* zone and shard management,
* world kernel systems,
* concurrency,
* deterministic runtime support,
* gameplay graph artifact consumption,
* script binding artifact consumption,
* asset streaming and residency,
* runtime diagnostics,
* server diagnostics,
* engine/runtime/server test strategy.

It does not own:

* Saga product shell,
* project dashboard UX,
* editor authoring UX,
* final collaboration product workflows,
* SDE compiler internals,
* Forge build frontend behavior,
* Prism code intelligence behavior,
* SagaTools dispatch behavior,
* asset import/cook implementation,
* C# script compiler implementation,
* generated code authoring UX.

SagaEngine provides the runtime/server foundation consumed by Saga and packaged Saga projects.

The engine should not slowly absorb every nearby feature just because it has the word “runtime” nearby.

That is not architecture.

That is gravitational collapse with CMake files.

---

## 2. Companion Documents

| Document                            | Purpose                                                                                                           |
| ----------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| `SAGA_PRODUCT_ROADMAP.md`           | Primary Saga executable, product shell, project lifecycle, mode orchestration, preview/build/publish entry points |
| `EDITOR_ROADMAP.md`                 | Authoring module mounted by Saga                                                                                  |
| `SHARED_ROADMAP.md`                 | Editor/runtime/server/tool shared contracts and artifact descriptors                                              |
| `COLLABORATION_ROADMAP.md`          | Product collaboration, session ownership, shared collaboration boundaries                                         |
| `DependencyGraph.md`                | Dependency ownership and compile-time architecture rules                                                          |
| `TOOLS_ROADMAP.md`                  | Tool ecosystem ownership and boundaries                                                                           |
| `DIAGNOSTICS_ROADMAP.md`            | Runtime reliability and diagnostics roadmap                                                                       |
| `SDE_ROADMAP.md`                    | Standalone deterministic compiler, canonical IR, artifact emission                                                |
| `FORGE_ROADMAP.md`                  | Build, cook, package, and publish-check workflow frontend                                                         |
| `PRISM_ROADMAP.md`                  | Code/artifact intelligence, stale artifact detection, boundary validation                                         |
| `SAGA_GAMEPLAY_GRAPH_ROADMAP.md`    | Gameplay graph artifacts and runtime/server graph consumption                                                     |
| `AUTHORING_AUTHORITY_MODEL.md`      | Authority, execution domains, side effects, replication, persistence, prediction contracts                        |
| `SAGA_SCRIPTING_ROADMAP.md`         | Script artifacts, binding manifests, runtime/server script binding consumption                                    |
| `ASSET_PIPELINE_ROADMAP.md`         | Source asset → cook → manifest → runtime-ready asset pipeline                                                     |
| `BUILD_PUBLISH_PIPELINE_ROADMAP.md` | Build/package/publish gates and package manifest expectations                                                     |
| `ClientReplicationFormalSpec.md`    | Formal client-side replication correctness contract                                                               |
| `AssetStreamingImplementation.md`   | Runtime asset streaming implementation note                                                                       |

---

## 3. Product Architecture Rule

* [x] Define SagaEngine as runtime/server foundation instead of product shell owner.

  Represented by:

  ```txt
  ENGINE_ROADMAP.md
  DependencyGraph.md
  EDITOR_ROADMAP.md
  COLLABORATION_ROADMAP.md
  SAGA_PRODUCT_ROADMAP.md
  ```

  Preserved decision:

  ```txt
  Saga owns product lifecycle.
  SagaEngine owns runtime/server execution foundations.
  ```

* [ ] Keep the primary product executable owned by `Saga`.

  Done means:

  * `Saga` owns the product shell,
  * `Saga` owns project create/open workflows,
  * `Saga` owns product mode switching,
  * engine runtime/server modules are mounted, launched, or orchestrated by Saga where needed,
  * engine code does not define product dashboard behavior,
  * engine code does not own build/publish UI.

* [ ] Keep development binaries separate from production architecture.

  Done means:

  * standalone runtime/server/editor launchers may exist for testing,
  * development binaries are documented as compatibility/test tools,
  * production user workflow remains `Saga`,
  * package/manifest validation behavior is shared across dev launchers and product workflows where practical.

---

## 4. Engine Ownership

* [x] Define engine/runtime ownership boundaries.

  Represented by:

  ```txt
  ENGINE_ROADMAP.md
  DependencyGraph.md
  ```

  Preserved decision:

  ```txt
  Engine owns runtime primitives.
  Runtime owns game execution.
  Server owns authority.
  Saga owns product lifecycle.
  Editor owns authoring UX.
  ```

* [ ] Keep engine ownership focused on reusable runtime foundations.

  Done means engine owns:

  * math,
  * memory utilities,
  * containers,
  * diagnostics primitives,
  * profiling primitives,
  * filesystem abstraction,
  * task scheduling primitives,
  * serialization primitives,
  * runtime resource primitives,
  * ECS/runtime primitives,
  * deterministic runtime support,
  * runtime package consumption primitives,
  * runtime manifest validation primitives,
  * network/replication primitives.

* [ ] Prevent Engine/Core from depending upward.

  Forbidden dependency direction:

  ```txt
  Engine/Core → Editor
  Engine/Core → Server
  Engine/Core → Apps
  Engine/Core → Saga product shell
  Engine/Core → SagaCollaboration implementation
  Engine/Core → SDE compiler internals
  Engine/Core → Forge internals
  Engine/Core → Prism internals
  Engine/Core → asset pipeline implementation
  Engine/Core → scripting compiler implementation
  ```

---

## 5. Runtime Ownership

* [ ] Keep runtime responsible for game execution.

  Done means runtime owns:

  * game loop,
  * simulation stepping,
  * scene execution,
  * ECS runtime application,
  * runtime resource residency,
  * asset manifest consumption,
  * client-safe graph artifact loading,
  * client-safe script binding consumption,
  * replication application,
  * prediction,
  * interpolation,
  * reconciliation,
  * rendering integration,
  * runtime diagnostics,
  * package startup validation.

* [ ] Prevent runtime from owning editor/product/tool behavior.

  Done means runtime does not own:

  * editor panels,
  * Qt authoring widgets,
  * product dashboard,
  * project create/open UX,
  * collaboration product lifecycle,
  * SDE compiler internals,
  * Forge build planning,
  * Prism indexing,
  * standalone tool orchestration,
  * asset import/cook implementation,
  * script compile implementation.

Allowed runtime dependencies:

```txt
Runtime → Engine/Core
Runtime → SagaShared contracts
Runtime → SagaCollaboration service API where explicitly approved
Runtime → SDE compiled outputs/manifests
Runtime → packaged runtime artifacts
Runtime → script binding manifests
Runtime → asset manifests
```

Forbidden runtime dependencies:

```txt
Runtime → Editor private headers
Runtime → Editor/include/SagaEditor/Collaboration
Runtime → Product shell internals
Runtime → SDE compiler internals
Runtime → Forge internals
Runtime → Prism internals
Runtime → asset pipeline implementation
Runtime → scripting compiler implementation
Runtime → server-private authority internals except approved API boundary
```

---

## 6. Server Ownership

* [ ] Keep server responsible for authority and multiplayer session policy.

  Done means server owns:

  * authoritative simulation,
  * networking,
  * packet protocol handling,
  * client session state,
  * zone/shard authority,
  * persistence integration,
  * anti-cheat validation,
  * replication source generation,
  * server package consumption,
  * authoritative gameplay graph artifact execution/binding,
  * server script binding consumption,
  * server diagnostics.

* [ ] Prevent server from owning editor/product/tool behavior.

  Done means server does not own:

  * editor UI,
  * product shell,
  * Qt widgets,
  * authoring workflows,
  * tool UI,
  * editor-private collaboration contracts,
  * SDE compiler internals,
  * Forge build pipeline implementation,
  * Prism analysis implementation,
  * asset import/cook implementation,
  * script compiler implementation.

Allowed server dependencies:

```txt
Server → Engine/Core
Server → SagaShared contracts
Server → SagaCollaboration service API where explicitly approved
Server → Backends
Server → SDE compiled outputs/manifests
Server → packaged server artifacts
Server → script binding manifests
Server → asset/data manifests
```

Forbidden server dependencies:

```txt
Server → Editor
Server → Apps/Saga product shell internals
Server → Apps/Editor
Server → Tools/Forge UI/internals
Server → Tools/Prism UI/internals
Server → SDE compiler internals
Server → asset pipeline implementation
Server → scripting compiler implementation
```

---

## 7. Current Runtime/Server Baseline

* [x] Establish working CMake/Conan/Ninja/sccache build path.

  Represented by:

  ```txt
  CMake configuration
  Conan dependency setup
  Ninja build flow
  sccache integration
  ```

  Preserved decision:

  ```txt
  Build pipeline is usable enough to support runtime/server iteration.
  ```

* [x] Establish basic UDP client/server connectivity.

  Represented by:

  ```txt
  Client UDP connection path
  Server UDP listener path
  Handshake flow
  ```

  Preserved decision:

  ```txt
  The runtime network path is active and testable.
  ```

* [x] Establish initial snapshot send/decode/apply path.

  Represented by:

  ```txt
  Server snapshot sender
  Client snapshot decoder
  Snapshot apply pipeline
  ```

  Preserved decision:

  ```txt
  The client can receive and apply replicated state.
  ```

* [x] Establish initial client replication pipeline chain.

  Represented by:

  ```txt
  PacketDemux
  StateMachine
  SnapshotApplyPipeline
  ReplicationApplyBridge
  ReconciliationBuffer
  InterpolationManager
  ```

  Preserved decision:

  ```txt
  Client-side packet demux, state management, snapshot apply, reconciliation, and interpolation are connected.
  ```

* [x] Establish initial server input acknowledgement path.

  Represented by:

  ```txt
  InputCommand receive path
  InputAck send path
  ```

  Preserved decision:

  ```txt
  Server receives client input commands and can acknowledge them.
  ```

* [x] Establish initial world kernel skeleton.

  Represented by:

  ```txt
  SimCell
  DomainScheduler
  EventStream
  RelevanceGraph
  WorldNode
  ```

  Preserved decision:

  ```txt
  World kernel concepts exist but are not yet production-complete.
  ```

---

## 8. Package and Artifact Consumption Roadmap

Runtime/server must consume built artifacts through explicit manifests.

This is the bridge between authoring/build systems and execution systems.

---

### 8.1 Runtime Package Manifest

* [ ] Add runtime package manifest loader.

  Done means runtime can load package manifests containing:

  * package id,
  * package kind,
  * build profile,
  * target platform,
  * runtime compatibility version,
  * asset manifest refs,
  * client-safe graph artifact refs,
  * client-safe script artifact refs,
  * diagnostics metadata refs,
  * package hash.

Expected files:

```txt
Engine/include/SagaEngine/Packages/PackageManifest.hpp
Engine/include/SagaEngine/Packages/PackageManifestLoader.hpp
Engine/src/SagaEngine/Packages/PackageManifestLoader.cpp
```

* [ ] Validate package manifest on runtime startup.

  Done means runtime rejects:

  * missing manifest,
  * unsupported manifest version,
  * wrong package kind,
  * incompatible runtime version,
  * missing required artifact,
  * hash mismatch where configured,
  * server-only executable artifact in client package.

---

### 8.2 Server Package Manifest

* [ ] Add server package manifest validation.

  Done means server validates:

  * server package kind,
  * authoritative graph artifacts,
  * server script artifacts,
  * server data artifacts,
  * persistence/schema refs,
  * asset/data manifests,
  * authority manifests,
  * package compatibility.

Expected files:

```txt
Engine/include/SagaEngine/Server/ServerPackageManifest.hpp
Engine/include/SagaEngine/Server/ServerPackageValidator.hpp
Engine/src/SagaEngine/Server/ServerPackageValidator.cpp
```

* [ ] Reject invalid server package state.

  Done means server does not continue startup with missing/invalid authoritative package data.

---

### 8.3 Artifact Manifest Consumption

* [ ] Add artifact manifest reader.

  Done means runtime/server can consume artifact manifests for:

  * compiled graph artifacts,
  * script artifacts,
  * asset artifacts,
  * schema/data artifacts,
  * generated manifests,
  * diagnostic reports.

Expected files:

```txt
Engine/include/SagaEngine/Artifacts/ArtifactManifest.hpp
Engine/include/SagaEngine/Artifacts/ArtifactRef.hpp
Engine/include/SagaEngine/Artifacts/ArtifactManifestLoader.hpp
Engine/src/SagaEngine/Artifacts/ArtifactManifestLoader.cpp
```

* [ ] Validate artifact hashes and versions.

  Done means runtime/server can reject artifacts with:

  * unknown kind,
  * unsupported version,
  * missing file,
  * hash mismatch,
  * incompatible execution domain,
  * incompatible authority metadata.

---

## 9. Gameplay Graph Artifact Consumption

* [ ] Add compiled gameplay graph artifact loader.

  Done means runtime/server can load compiled graph artifacts produced by SDE/build pipeline without including SDE compiler internals.

Expected files:

```txt
Engine/include/SagaEngine/Graph/CompiledGraphArtifact.hpp
Engine/include/SagaEngine/Graph/CompiledGraphLoader.hpp
Engine/include/SagaEngine/Graph/GraphArtifactRegistry.hpp
Engine/src/SagaEngine/Graph/CompiledGraphLoader.cpp
Engine/src/SagaEngine/Graph/GraphArtifactRegistry.cpp
```

* [ ] Validate graph artifact metadata.

  Done means runtime/server check:

  * graph id,
  * graph kind,
  * schema version,
  * artifact hash,
  * source map ref where applicable,
  * block registry compatibility,
  * execution domain,
  * authority context.

* [ ] Support client-safe graph artifacts.

  Done means runtime can load graph artifacts for:

  * UI/client presentation,
  * visual-only logic,
  * prediction-safe logic,
  * runtime preview,
  * read-only replicated state reactions.

* [ ] Support server-authoritative graph artifacts.

  Done means server can bind or execute graph artifacts for:

  * quest rewards,
  * inventory rules,
  * ability rules,
  * combat rules,
  * zone rules,
  * validation hooks,
  * persistence-affecting gameplay.

* [ ] Reject wrong graph placement.

  Done means runtime/server reject:

  * server-only graph artifact loaded as executable client logic,
  * client-only graph artifact treated as server authority,
  * editor-only graph artifact loaded into runtime/server package,
  * build-only graph artifact loaded into runtime/server package.

Graph authoring may be visual.

Graph execution still has to respect physics, packets, and trust boundaries.

---

## 10. Authority Manifest Consumption

* [ ] Add runtime/server authority manifest model.

  Done means graph/script/package artifacts can include authority manifests describing:

  * authority context,
  * execution domain,
  * side effects,
  * replication effects,
  * persistence effects,
  * prediction safety,
  * security boundary.

Expected files:

```txt
Engine/include/SagaEngine/Authority/AuthorityManifest.hpp
Engine/include/SagaEngine/Authority/AuthorityValidator.hpp
Engine/src/SagaEngine/Authority/AuthorityValidator.cpp
```

* [ ] Validate authority manifest at load time.

  Done means runtime/server reject artifacts when:

  * authority manifest is missing for side-effecting artifact,
  * execution domain does not match package kind,
  * server-only artifact is loaded into client execution domain,
  * client-only artifact is used as server authoritative truth,
  * prediction-unsafe artifact is used in predicted context,
  * persistent side effect exists without server authority.

---

## 11. Script Binding Consumption

* [ ] Add script binding manifest loader.

  Done means runtime/server can load script binding manifests produced by scripting build pipeline.

Expected files:

```txt
Engine/include/SagaEngine/Scripting/ScriptBindingManifest.hpp
Engine/include/SagaEngine/Scripting/ScriptBindingRegistry.hpp
Engine/include/SagaEngine/Scripting/IScriptHost.hpp
Engine/src/SagaEngine/Scripting/ScriptBindingRegistry.cpp
```

* [ ] Validate script binding metadata.

  Done means runtime/server check:

  * assembly id,
  * binding id,
  * signature hash,
  * authority context,
  * execution domain,
  * side effects,
  * artifact hash,
  * script runtime compatibility.

* [ ] Load client-safe script artifacts in runtime.

  Done means runtime may bind scripts for:

  * client presentation,
  * visual-only logic,
  * prediction-safe logic,
  * pure helpers,
  * editor/runtime preview where approved.

* [ ] Load server-authoritative script artifacts in server.

  Done means server may bind scripts for:

  * authoritative gameplay rules,
  * validation hooks,
  * persistence-affecting gameplay,
  * quest/inventory/ability/combat logic.

* [ ] Reject unsafe script artifact placement.

  Done means runtime/server reject:

  * server-only script in client execution domain,
  * editor-only script in runtime/server package,
  * test-only script in shipping package,
  * missing or stale binding manifest,
  * unsupported signature hash.

---

## 12. Runtime Asset Streaming and Asset Manifest Consumption

* [x] Establish runtime asset streaming architecture.

  Represented by:

  ```txt
  AssetStreamingImplementation.md
  AssetId
  AssetManifest
  AssetSource
  AssetStreamRequest
  AssetStreamingScheduler
  ResidencyCache
  AssetHandle<T>
  AssetDiagnostic
  ```

  Preserved decision:

  ```txt
  Runtime streaming consumes runtime-ready assets.
  Editor import/cook produces runtime-ready assets.
  ```

* [ ] Add production runtime asset manifest consumption.

  Done means runtime can load asset manifests produced by build/cook/package pipeline and resolve:

  * asset id,
  * asset kind,
  * cooked artifact path,
  * artifact hash,
  * dependencies,
  * memory estimate,
  * streaming group,
  * platform/profile compatibility,
  * version/schema information.

Expected files:

```txt
Engine/include/SagaEngine/Assets/AssetManifest.hpp
Engine/include/SagaEngine/Assets/AssetRegistry.hpp
Engine/include/SagaEngine/Assets/AssetHandle.hpp
Engine/include/SagaEngine/Assets/AssetDiagnostic.hpp
Engine/src/SagaEngine/Assets/AssetManifestLoader.cpp
Engine/src/SagaEngine/Assets/AssetRegistry.cpp
```

* [ ] Reject invalid asset manifests and artifacts.

  Done means runtime reports diagnostics for:

  * missing asset manifest,
  * invalid manifest version,
  * missing cooked artifact,
  * hash mismatch,
  * unsupported asset kind,
  * incompatible platform/profile,
  * dependency missing,
  * memory budget rejection.

* [ ] Keep source asset import out of runtime.

  Done means runtime does not casually import arbitrary source assets in shipping mode.

Runtime is allowed to load assets.

It is not required to become Blender with a socket loop.

---

## 13. Networking Roadmap

### 13.1 Transport Layer

* [x] Provide basic UDP transport for client/server testing.

  Represented by:

  ```txt
  Client UDP path
  Server UDP listener
  Handshake exchange
  ```

* [ ] Harden UDP transport for production multiplayer.

  Done means:

  * packet send/receive is bounded,
  * socket errors are handled,
  * disconnects are detected,
  * reconnect path exists,
  * packet flood is limited,
  * malformed packet input is rejected safely,
  * transport diagnostics are exposed.

* [ ] Add transport abstraction.

  Done means:

  * runtime/server code can use a transport interface,
  * UDP implementation is one backend,
  * tests can use in-memory transport,
  * future relay/cloud transport can be introduced without rewriting replication logic.

Expected files:

```txt
Engine/include/SagaEngine/Network/ITransport.hpp
Engine/include/SagaEngine/Network/TransportEndpoint.hpp
Engine/src/SagaEngine/Network/UdpTransport.cpp
Engine/src/SagaEngine/Network/InMemoryTransport.cpp
```

---

### 13.2 Connection Lifecycle

* [x] Provide initial handshake flow.

  Represented by:

  ```txt
  Client handshake request
  Server handshake response
  Client connected state transition
  ```

* [ ] Add production connection state machine.

  Done means connection states include:

  * disconnected,
  * resolving,
  * connecting,
  * handshaking,
  * connected,
  * resynchronizing,
  * disconnecting,
  * failed.

Expected files:

```txt
Engine/include/SagaEngine/Network/ConnectionState.hpp
Engine/include/SagaEngine/Network/ConnectionStateMachine.hpp
Engine/src/SagaEngine/Network/ConnectionStateMachine.cpp
```

* [ ] Add connection timeout and heartbeat.

  Done means:

  * missed heartbeat is detected,
  * timeout reason is recorded,
  * client receives disconnect reason where possible,
  * server releases stale client state,
  * diagnostics expose heartbeat age and connection quality.

---

### 13.3 Packet Framing

* [ ] Define stable packet envelope.

  Done means every packet includes:

  * protocol version,
  * packet type,
  * sequence number,
  * connection/session id,
  * payload size,
  * validation/checksum field where appropriate.

Expected files:

```txt
Engine/include/SagaEngine/Network/PacketEnvelope.hpp
Engine/include/SagaEngine/Network/PacketType.hpp
Engine/src/SagaEngine/Network/PacketCodec.cpp
```

* [ ] Add packet size and payload validation.

  Done means:

  * oversized packets are rejected,
  * unknown packet types are rejected,
  * truncated packets are rejected,
  * invalid payload size fails safely,
  * diagnostic reason is recorded.

* [ ] Add protocol version negotiation.

  Done means:

  * client and server compare supported versions,
  * unsupported versions fail clearly,
  * version mismatch produces actionable diagnostics.

---

### 13.4 Reliability Layer

* [ ] Add reliable message channel for required gameplay/session messages.

  Done means:

  * reliable messages are acknowledged,
  * lost reliable messages are retransmitted,
  * retransmission is bounded,
  * duplicate reliable messages are ignored,
  * reliable channel does not block high-frequency unreliable snapshots indefinitely.

* [ ] Keep high-frequency state snapshots unreliable where appropriate.

  Done means:

  * snapshot stream tolerates loss,
  * newest valid snapshot can supersede old snapshots,
  * stale snapshots are rejected,
  * packet loss is visible in diagnostics.

---

## 14. Security Roadmap

### 14.1 Packet Security

* [ ] Validate all network input before use.

  Done means:

  * no packet payload is trusted before decoding validation,
  * malformed packets are rejected,
  * invalid state transitions are rejected,
  * unknown client/session ids are rejected,
  * invalid entity/component ids are rejected.

* [ ] Add rate limits.

  Done means server limits:

  * connection attempts,
  * handshake attempts,
  * input command frequency,
  * oversized packet attempts,
  * invalid packet spam,
  * client request graph invocation frequency where applicable.

* [ ] Add disconnect reasons and security diagnostics.

  Done means disconnect diagnostics can distinguish:

  * timeout,
  * malformed packet,
  * unsupported protocol,
  * unauthorized action,
  * rate limit,
  * invalid graph/script request,
  * server shutdown.

---

### 14.2 Client Trust Boundary

* [ ] Treat clients as untrusted.

  Done means:

  * client input is intent, not authority,
  * server validates movement/action requests,
  * client cannot directly author authoritative world state,
  * client cannot spawn/modify entities without authority,
  * client-side prediction never becomes server truth,
  * client-executed graph/script artifacts cannot commit authoritative state.

This is multiplayer.

Trusting clients is how cheaters get a welcome basket.

---

### 14.3 Authentication Boundary

* [ ] Add authentication/session identity integration point.

  Done means:

  * network connection can bind to an authenticated account/session id,
  * unauthenticated dev sessions are explicitly marked,
  * production server can reject unauthenticated clients,
  * auth failure is reported clearly.

* [ ] Keep auth integration outside low-level transport.

  Done means:

  * transport moves packets,
  * session/auth layer validates identity,
  * game/server policy decides access.

---

### 14.4 Client Request Validation Boundary

* [ ] Add server-validated request boundary for graph/script actions.

  Done means client requests route through:

  ```txt
  Client request
      ↓
  packet/session validation
      ↓
  identity/permission validation
      ↓
  gameplay precondition validation
      ↓
  authoritative graph/script invocation where approved
  ```

* [ ] Reject direct client invocation of authoritative graph/script logic.

  Done means client artifacts cannot call `GiveItem`, `CommitQuestReward`, `WritePersistentState`, or equivalent server-only actions directly.

---

## 15. Replication Roadmap

### 15.1 Server Snapshot Generation

* [x] Generate initial server snapshots.

  Represented by:

  ```txt
  Server test entity snapshot path
  Server snapshot send path
  ```

* [ ] Add production snapshot builder.

  Done means snapshots include:

  * stable entity ids,
  * component payloads,
  * tick id,
  * baseline/reference id where applicable,
  * relevance filtering result,
  * serialization version,
  * payload budget result.

Expected files:

```txt
Engine/include/SagaEngine/Replication/Snapshot.hpp
Engine/include/SagaEngine/Replication/SnapshotBuilder.hpp
Engine/src/SagaEngine/Replication/SnapshotBuilder.cpp
```

* [ ] Add snapshot delta compression.

  Done means:

  * server can build deltas against known baselines,
  * client can apply deltas safely,
  * missing baseline triggers resync,
  * invalid delta is rejected without corrupting client state.

---

### 15.2 Client Snapshot Decode and Apply

* [x] Decode and apply initial snapshots on client.

  Represented by:

  ```txt
  PacketDemux
  SnapshotApplyPipeline
  ReplicationApplyBridge
  ```

* [ ] Formalize client-side apply order in implementation.

  Done means implementation matches `ClientReplicationFormalSpec.md` for:

  * decode,
  * validation,
  * ordering,
  * state machine transition,
  * snapshot application,
  * reconciliation,
  * interpolation buffer update.

* [ ] Reject stale or invalid snapshots.

  Done means:

  * old tick snapshots are ignored,
  * snapshots from invalid session are rejected,
  * invalid component payload fails safely,
  * diagnostics identify rejection reason.

---

### 15.3 Replication State Machine

* [x] Establish initial client replication state machine.

  Represented by:

  ```txt
  StateMachine
  SnapshotApplyPipeline
  ReconciliationBuffer
  InterpolationManager
  ```

* [ ] Add production replication states.

  Done means states include:

  * idle,
  * connecting,
  * handshaking,
  * waiting for baseline,
  * synchronized,
  * resynchronizing,
  * disconnected,
  * failed.

Expected files:

```txt
Engine/include/SagaEngine/Replication/ReplicationState.hpp
Engine/include/SagaEngine/Replication/ReplicationStateMachine.hpp
Engine/src/SagaEngine/Replication/ReplicationStateMachine.cpp
```

* [ ] Add state transition diagnostics.

  Done means every replication state transition records:

  * previous state,
  * next state,
  * tick,
  * reason,
  * error code where applicable.

---

### 15.4 Relevance Filtering

* [ ] Add server-side relevance graph integration.

  Done means:

  * entities are filtered per client,
  * distance/visibility/interest rules can be evaluated,
  * bandwidth budget can affect snapshot selection,
  * relevance changes are reflected safely on client.

Expected files:

```txt
Engine/include/SagaEngine/World/RelevanceGraph.hpp
Engine/src/SagaEngine/World/RelevanceGraph.cpp
Engine/include/SagaEngine/Replication/RelevanceFilter.hpp
Engine/src/SagaEngine/Replication/RelevanceFilter.cpp
```

* [ ] Add relevance diagnostics.

  Done means diagnostics can show:

  * why an entity was included,
  * why an entity was excluded,
  * per-client relevant entity count,
  * bandwidth pressure.

---

### 15.5 Prediction and Reconciliation

* [x] Establish initial reconciliation buffer path.

  Represented by:

  ```txt
  ReconciliationBuffer
  InputCommand
  InputAck
  ```

* [ ] Add production client-side prediction.

  Done means:

  * client can predict local controlled entity,
  * input commands are stored by tick,
  * server correction can rewind/replay,
  * prediction errors are smoothed where appropriate,
  * severe divergence is corrected safely.

Expected files:

```txt
Engine/include/SagaEngine/Prediction/PredictionBuffer.hpp
Engine/include/SagaEngine/Prediction/InputCommand.hpp
Engine/include/SagaEngine/Prediction/InputAck.hpp
Engine/include/SagaEngine/Prediction/PredictionError.hpp
Engine/src/SagaEngine/Prediction/PredictionBuffer.cpp
```

* [ ] Validate prediction-safe graph/script usage.

  Done means runtime rejects prediction contexts that attempt to execute artifacts marked:

  * prediction unsafe,
  * persistent write,
  * authoritative write,
  * server-only,
  * editor-only.

---

### 15.6 Interpolation

* [x] Establish initial interpolation manager path.

  Represented by:

  ```txt
  InterpolationManager
  SnapshotApplyPipeline integration
  ```

* [ ] Add production interpolation buffers.

  Done means:

  * remote entity state samples are buffered,
  * sample ordering is validated,
  * interpolation delay is configurable,
  * missing samples are handled gracefully,
  * diagnostics expose interpolation health.

Expected files:

```txt
Engine/include/SagaEngine/Interpolation/InterpolationBuffer.hpp
Engine/include/SagaEngine/Interpolation/InterpolationManager.hpp
Engine/src/SagaEngine/Interpolation/InterpolationManager.cpp
```

---

## 16. Authoritative Simulation Roadmap

* [ ] Add authoritative simulation loop.

  Done means server simulation:

  * advances by explicit ticks,
  * processes validated inputs,
  * applies authoritative gameplay logic,
  * mutates authoritative ECS/world state,
  * emits replication source state,
  * records diagnostics.

Expected files:

```txt
Engine/include/SagaEngine/Server/AuthoritativeSimulation.hpp
Engine/include/SagaEngine/Server/ServerTick.hpp
Engine/src/SagaEngine/Server/AuthoritativeSimulation.cpp
```

* [ ] Add server gameplay request pipeline.

  Done means server requests:

  * decode safely,
  * validate session/client identity,
  * validate permissions,
  * validate gameplay preconditions,
  * invoke approved graph/script artifact,
  * emit diagnostics and replication events.

* [ ] Add deterministic tick diagnostics.

  Done means server can report:

  * tick duration,
  * input count,
  * graph/script invocation count,
  * replication output size,
  * persistence operations,
  * slow systems.

---

## 17. World Kernel and Zone/Sharding Roadmap

* [x] Establish initial world kernel skeleton.

  Represented by:

  ```txt
  SimCell
  DomainScheduler
  EventStream
  RelevanceGraph
  WorldNode
  ```

* [ ] Add production world kernel ownership model.

  Done means world kernel supports:

  * world nodes,
  * simulation cells,
  * domain scheduling,
  * event streams,
  * relevance graphs,
  * authority ownership,
  * diagnostics.

* [ ] Add zone authority model.

  Done means zones can define:

  * zone id,
  * owning server/shard,
  * active clients,
  * relevant entities,
  * migration/handoff policy,
  * diagnostics.

* [ ] Add shard/session policy integration.

  Done means server can manage:

  * shard id,
  * session id,
  * player assignment,
  * zone assignment,
  * reconnect/handoff state,
  * shard diagnostics.

Expected files:

```txt
Engine/include/SagaEngine/World/WorldKernel.hpp
Engine/include/SagaEngine/World/ZoneAuthority.hpp
Engine/include/SagaEngine/World/ShardId.hpp
Engine/include/SagaEngine/World/ZoneId.hpp
Engine/src/SagaEngine/World/WorldKernel.cpp
Engine/src/SagaEngine/World/ZoneAuthority.cpp
```

---

## 18. Persistence Integration Roadmap

* [ ] Add server persistence integration boundary.

  Done means server can integrate with persistence systems through approved backend/service boundaries.

Expected files:

```txt
Engine/include/SagaEngine/Persistence/IPersistenceService.hpp
Engine/include/SagaEngine/Persistence/PersistenceRequest.hpp
Engine/include/SagaEngine/Persistence/PersistenceResult.hpp
```

* [ ] Add persistence transaction model for gameplay graph/script execution.

  Done means server-authoritative graph/script operations can declare and execute:

  * transaction start,
  * staged writes,
  * commit,
  * rollback/failure,
  * diagnostics.

* [ ] Reject persistence writes outside server authority.

  Done means runtime/server validation prevents:

  * client-only persistent writes,
  * predicted persistent writes,
  * missing transaction policy,
  * invalid schema version writes.

---

## 19. Diagnostics Roadmap

* [ ] Add runtime/server structured diagnostics.

  Done means diagnostics can report:

  * package manifest errors,
  * artifact loading errors,
  * graph artifact validation errors,
  * script binding errors,
  * asset manifest errors,
  * network errors,
  * replication errors,
  * prediction/reconciliation errors,
  * simulation tick warnings,
  * persistence failures,
  * security/rate-limit issues.

Expected files:

```txt
Engine/include/SagaEngine/Diagnostics/RuntimeDiagnostic.hpp
Engine/include/SagaEngine/Diagnostics/RuntimeDiagnosticSink.hpp
Engine/include/SagaEngine/Diagnostics/ServerDiagnostic.hpp
Engine/src/SagaEngine/Diagnostics/RuntimeDiagnostic.cpp
Engine/src/SagaEngine/Diagnostics/ServerDiagnostic.cpp
```

* [ ] Add diagnostics source mapping to artifacts.

  Done means runtime/server diagnostics can reference:

  * package id,
  * artifact id,
  * graph id,
  * script binding id,
  * asset id,
  * entity id where safe,
  * connection/session id,
  * tick id.

* [ ] Add health and metrics reporting.

  Done means runtime/server can expose:

  * frame/tick timing,
  * packet rates,
  * invalid packet count,
  * replication bandwidth,
  * asset streaming pressure,
  * memory usage,
  * graph/script invocation metrics,
  * persistence latency.

---

## 20. Testing Strategy

### 20.1 Dependency Boundary Tests

* [ ] Add compile/include boundary tests.

  Required checks:

  * Engine/Core does not include Editor,
  * Runtime does not include Editor private headers,
  * Server does not include Editor,
  * Runtime/server do not include SDE compiler internals,
  * Runtime/server do not include Forge/Prism internals,
  * Runtime/server do not include asset pipeline implementation,
  * Runtime/server do not include scripting compiler implementation.

---

### 20.2 Package and Artifact Tests

* [ ] Add package manifest validation tests.

  Required coverage:

  * valid client package loads,
  * valid server package loads,
  * missing manifest rejected,
  * unsupported version rejected,
  * wrong package kind rejected,
  * missing artifact rejected,
  * hash mismatch rejected.

* [ ] Add graph artifact tests.

  Required coverage:

  * client-safe graph loads in runtime,
  * server graph loads in server,
  * server-only graph rejected in client runtime,
  * editor-only graph rejected in runtime/server,
  * invalid authority manifest rejected.

* [ ] Add script binding tests.

  Required coverage:

  * valid client script binding loads,
  * valid server script binding loads,
  * missing binding manifest rejected,
  * signature mismatch rejected,
  * server-only script rejected in client runtime.

* [ ] Add asset manifest tests.

  Required coverage:

  * valid texture/mesh artifact loads,
  * missing artifact diagnostic,
  * hash mismatch diagnostic,
  * unsupported asset kind diagnostic,
  * memory budget diagnostic.

---

### 20.3 Networking and Replication Tests

* [ ] Add transport tests.

  Required coverage:

  * UDP send/receive,
  * in-memory transport,
  * malformed packet rejection,
  * oversized packet rejection,
  * disconnect/timeout behavior.

* [ ] Add replication formal-spec tests.

  Required coverage:

  * handshake transition,
  * waiting baseline state,
  * stale snapshot rejection,
  * delta missing baseline rejection,
  * correction/reconciliation flow,
  * interpolation buffer update.

---

### 20.4 Security Tests

* [ ] Add server trust-boundary tests.

  Required coverage:

  * client request cannot mutate inventory directly,
  * client request cannot spawn authoritative entity directly,
  * invalid graph/script request rejected,
  * rate limit enforced,
  * unauthenticated production session rejected.

---

### 20.5 Simulation and Persistence Tests

* [ ] Add authoritative simulation tests.

  Required coverage:

  * validated input processing,
  * authoritative graph invocation,
  * authoritative script invocation,
  * replication output generation,
  * diagnostics emission.

* [ ] Add persistence boundary tests.

  Required coverage:

  * server transaction commit,
  * server transaction rollback,
  * client persistent write rejected,
  * predicted persistent write rejected,
  * schema mismatch rejected.

---

## 21. MVP Vertical Slice

The first runtime/server vertical slice should connect package consumption, graph/script/asset artifacts, and authoritative multiplayer behavior.

Required scenario:

```txt
MMO Starter local client/server preview
```

Required contents:

* one client package,
* one server package,
* one asset manifest with one cooked texture,
* one server-only quest reward graph artifact,
* one C# pure helper binding manifest,
* one client UI/presentation graph or script,
* one server authoritative quest completion request path.

Required behavior:

1. Runtime loads client package manifest.
2. Runtime loads asset manifest and texture artifact through runtime streaming.
3. Runtime rejects any server-only executable artifact in client package.
4. Server loads server package manifest.
5. Server loads authoritative quest reward graph artifact.
6. Server loads C# helper binding manifest.
7. Client sends quest completion request as intent.
8. Server validates request.
9. Server executes authoritative graph/script path.
10. Server mutates authoritative state and persistence transaction.
11. Server emits replicated state.
12. Client receives snapshot/update.
13. Client applies authoritative result through replication pipeline.
14. Diagnostics identify package/artifact/request/replication state.

This slice proves the new architecture is not just a documentation constellation.

It proves authoring artifacts can actually reach runtime/server safely.

---

## 22. Non-Goals

SagaEngine runtime/server must not become:

* Saga product shell,
* editor authoring UX,
* SDE compiler,
* Forge build frontend,
* Prism code intelligence engine,
* asset importer/cooker,
* C# script compiler,
* collaboration product workflow owner,
* backend administration UI,
* product dashboard,
* general-purpose tool dispatcher.

The runtime/server foundation should execute validated artifacts.

It should not create, author, compile, cook, package, and publish them too.

---

## 23. Risk Register

### 23.1 Risk: Runtime Starts Importing Source Assets

Mitigation:

* runtime consumes cooked/runtime-ready artifacts,
* asset import/cook remains in asset pipeline/Forge/editor tooling,
* shipping runtime rejects source-only assets.

---

### 23.2 Risk: Server Trusts Client Graph/Script Requests

Mitigation:

* client request is intent only,
* server validates session/identity/permissions/preconditions,
* authoritative graph/script invocation happens only on server,
* invalid request emits diagnostics and can disconnect/rate-limit.

---

### 23.3 Risk: Runtime Loads Wrong Artifact Domain

Mitigation:

* package manifests declare artifact destination,
* authority manifests declare execution domain,
* runtime/server validate package kind and artifact metadata at startup.

---

### 23.4 Risk: SDE/Forge/Prism Internals Leak Into Runtime

Mitigation:

* consume manifests/artifacts/reports only,
* enforce include boundary checks,
* keep compiler/build/analyzer internals out of runtime/server.

---

### 23.5 Risk: Script Host Bypasses Authority Model

Mitigation:

* script binding manifests include authority metadata,
* runtime/server validate binding manifests,
* server request flow controls authoritative invocation,
* client package rejects server-only script artifacts.

---

### 23.6 Risk: Package Validation Is Treated As Optional

Mitigation:

* runtime/server startup validates manifests,
* invalid required package state fails startup,
* diagnostics identify exact missing/incompatible artifacts.

---

## 24. Suggested File Targets

Expected package/artifact files:

```txt
Engine/include/SagaEngine/Packages/PackageManifest.hpp
Engine/include/SagaEngine/Packages/PackageManifestLoader.hpp
Engine/include/SagaEngine/Packages/PackageValidator.hpp
Engine/include/SagaEngine/Artifacts/ArtifactManifest.hpp
Engine/include/SagaEngine/Artifacts/ArtifactManifestLoader.hpp
Engine/src/SagaEngine/Packages/PackageManifestLoader.cpp
Engine/src/SagaEngine/Packages/PackageValidator.cpp
Engine/src/SagaEngine/Artifacts/ArtifactManifestLoader.cpp
```

Expected graph/authority files:

```txt
Engine/include/SagaEngine/Graph/CompiledGraphArtifact.hpp
Engine/include/SagaEngine/Graph/CompiledGraphLoader.hpp
Engine/include/SagaEngine/Graph/GraphArtifactRegistry.hpp
Engine/include/SagaEngine/Authority/AuthorityManifest.hpp
Engine/include/SagaEngine/Authority/AuthorityValidator.hpp
Engine/src/SagaEngine/Graph/CompiledGraphLoader.cpp
Engine/src/SagaEngine/Graph/GraphArtifactRegistry.cpp
Engine/src/SagaEngine/Authority/AuthorityValidator.cpp
```

Expected scripting files:

```txt
Engine/include/SagaEngine/Scripting/IScriptHost.hpp
Engine/include/SagaEngine/Scripting/ScriptBindingManifest.hpp
Engine/include/SagaEngine/Scripting/ScriptBindingRegistry.hpp
Engine/include/SagaEngine/Scripting/ScriptRuntimeConfig.hpp
Engine/src/SagaEngine/Scripting/ScriptBindingRegistry.cpp
```

Expected asset files:

```txt
Engine/include/SagaEngine/Assets/AssetManifest.hpp
Engine/include/SagaEngine/Assets/AssetRegistry.hpp
Engine/include/SagaEngine/Assets/AssetHandle.hpp
Engine/include/SagaEngine/Assets/AssetDiagnostic.hpp
Engine/src/SagaEngine/Assets/AssetManifestLoader.cpp
Engine/src/SagaEngine/Assets/AssetRegistry.cpp
```

Expected network/replication files:

```txt
Engine/include/SagaEngine/Network/ITransport.hpp
Engine/include/SagaEngine/Network/ConnectionStateMachine.hpp
Engine/include/SagaEngine/Network/PacketEnvelope.hpp
Engine/include/SagaEngine/Replication/SnapshotBuilder.hpp
Engine/include/SagaEngine/Replication/ReplicationStateMachine.hpp
Engine/include/SagaEngine/Prediction/PredictionBuffer.hpp
Engine/include/SagaEngine/Interpolation/InterpolationManager.hpp
```

Expected server/world files:

```txt
Engine/include/SagaEngine/Server/AuthoritativeSimulation.hpp
Engine/include/SagaEngine/Server/ServerPackageValidator.hpp
Engine/include/SagaEngine/World/WorldKernel.hpp
Engine/include/SagaEngine/World/ZoneAuthority.hpp
Engine/include/SagaEngine/Persistence/IPersistenceService.hpp
```

---

## 25. Decision Summary

Preserve these decisions:

```txt
SagaEngine owns runtime/server execution foundations.
Saga owns product lifecycle.
Editor owns authoring UX.
SDE owns deterministic source compilation.
Forge owns build/cook/package orchestration.
Prism owns code/artifact analysis.
Runtime/server consume packaged artifacts and manifests.
Runtime streaming consumes runtime-ready assets only.
Server authority remains mandatory for MMO gameplay truth.
Client input is intent, not truth.
Client prediction is temporary and correctable.
Graph/script artifacts must carry authority/execution metadata.
Runtime/server validate package/artifact manifests before execution.
Server invokes authoritative graph/script behavior only after request validation.
Runtime/server must not include editor/tool/compiler private internals.
```

The engine should be the place where validated artifacts become execution.

Not the place where every authoring, build, and product workflow goes to avoid having a real owner.
