# SagaEngine — Production MMO Engine Roadmap

> Last updated: 2026-05-15
> Status: Active roadmap
> Target: A production-grade authoritative multiplayer runtime and server core.
> Scope: Runtime execution, networking, security, packet protocols, authoritative simulation, replication, zone/shard management, resources, diagnostics, testing, and deployment-facing engine foundations.

---

## 0. Roadmap Convention

- `[x]` — Shipped. The note after the item names the files, modules, or integration points that represent the work and highlights any decisions worth preserving.
- `[ ]` — Open. Either unstarted or partially explored; the item describes the finished production state rather than interim scaffolding.
- Shipped items must name the files, modules, or integration points that represent the completed work.
- Open items must describe the finished state, not temporary scaffolding.
- Runtime/server code must not include editor-private headers.
- Engine/Core must not include from `Editor/`, `Apps/`, `Saga/`, or `Server/`.
- SDE must remain a standalone deterministic data compiler.
- Collaboration contracts must be consumed through `SagaShared` or `SagaCollaboration`, not editor-private paths.

---

## 1. Document Purpose

This document is the source of truth for SagaEngine runtime and server systems.

It covers:

- runtime execution,
- networking,
- security,
- packet protocols,
- authoritative simulation,
- client/server replication,
- prediction and reconciliation,
- interpolation,
- zone and shard management,
- world kernel systems,
- concurrency,
- determinism,
- resource and asset streaming,
- runtime diagnostics,
- server diagnostics,
- engine/runtime test strategy.

It does not own:

- Saga product shell,
- project dashboard UX,
- editor authoring UX,
- final collaboration product workflows,
- SDE compiler internals,
- Forge build frontend behavior,
- Prism code intelligence behavior,
- SagaTools dispatch behavior.

SagaEngine provides the runtime/server foundation consumed by Saga.

The engine should not slowly absorb every nearby feature just because it has the word “core” nearby. That is not architecture. That is gravitational collapse with CMake files.

---

## 2. Companion Documents

| Document | Purpose |
|---|---|
| `SAGA_PRODUCT_ROADMAP.md` | Primary Saga executable, product shell, project lifecycle, and mode orchestration |
| `EDITOR_ROADMAP.md` | Authoring module mounted by Saga |
| `SHARED_ROADMAP.md` | Editor/runtime shared contracts and systems |
| `COLLABORATION_ROADMAP.md` | Product collaboration/session ownership and shared collaboration boundaries |
| `DependencyGraph.md` | Dependency ownership and compile-time architecture rules |
| `TOOLS_ROADMAP.md` | Tool ecosystem ownership and boundaries |
| `DIAGNOSTICS_ROADMAP.md` | SagaDiagnostics runtime reliability roadmap |
| `ClientReplicationFormalSpec.md` | Formal client-side replication correctness contract |
| `AssetStreamingImplementation.md` | Runtime asset streaming implementation note |

---

## 3. Product Architecture Rule

- [x] Define SagaEngine as runtime/server foundation instead of product shell owner.

  Represented by:

  ```txt
  ENGINE_ROADMAP.md
  DependencyGraph.md
  EDITOR_ROADMAP.md
  COLLABORATION_ROADMAP.md
  ```

  Preserved decision:

  ```txt
  Saga owns product lifecycle.
  SagaEngine owns runtime/server execution foundations.
  ```

- [ ] Keep the primary product executable owned by `Saga`.

  Done means:

  - `Saga` owns the product shell,
  - `Saga` owns project create/open workflows,
  - `Saga` owns product mode switching,
  - engine runtime/server modules are mounted, launched, or orchestrated by Saga where needed,
  - engine code does not define product dashboard behavior.

- [ ] Keep development binaries separate from production architecture.

  Done means:

  - standalone runtime/server/editor launchers may exist for testing,
  - development binaries are documented as compatibility/test tools,
  - production user workflow remains `Saga`.

---

## 4. Engine Ownership

- [x] Define engine/runtime ownership boundaries.

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

- [ ] Keep engine ownership focused on reusable runtime foundations.

  Done means engine owns:

  - math,
  - memory utilities,
  - containers,
  - diagnostics primitives,
  - profiling primitives,
  - filesystem abstraction,
  - task scheduling primitives,
  - serialization primitives,
  - runtime resource primitives,
  - ECS/runtime primitives,
  - deterministic runtime support.

- [ ] Prevent Engine/Core from depending upward.

  Forbidden dependency direction:

  ```txt
  Engine/Core → Editor
  Engine/Core → Server
  Engine/Core → Apps
  Engine/Core → Saga product shell
  Engine/Core → SagaCollaboration implementation
  Engine/Core → SDE compiler internals
  ```

---

## 5. Runtime Ownership

- [ ] Keep runtime responsible for game execution.

  Done means runtime owns:

  - game loop,
  - simulation stepping,
  - scene execution,
  - ECS runtime application,
  - runtime resource residency,
  - replication application,
  - prediction,
  - interpolation,
  - reconciliation,
  - rendering integration,
  - runtime diagnostics.

- [ ] Prevent runtime from owning editor/product behavior.

  Done means runtime does not own:

  - editor panels,
  - Qt authoring widgets,
  - product dashboard,
  - project create/open UX,
  - collaboration product lifecycle,
  - SDE compiler internals,
  - standalone tool orchestration.

Allowed runtime dependencies:

```txt
Runtime → Engine/Core
Runtime → SagaShared
Runtime → SagaCollaboration service API
Runtime → SDE compiled outputs
```

Forbidden runtime dependencies:

```txt
Runtime → Editor private headers
Runtime → Editor/include/SagaEditor/Collaboration
Runtime → Product shell internals
Runtime → SDE compiler internals
```

---

## 6. Server Ownership

- [ ] Keep server responsible for authority and multiplayer session policy.

  Done means server owns:

  - authoritative simulation,
  - networking,
  - packet protocol handling,
  - client session state,
  - zone/shard authority,
  - persistence integration,
  - anti-cheat validation,
  - replication source generation,
  - server diagnostics.

- [ ] Prevent server from owning editor/product/tool behavior.

  Done means server does not own:

  - editor UI,
  - product shell,
  - Qt widgets,
  - authoring workflows,
  - tool UI,
  - editor-private collaboration contracts.

Allowed server dependencies:

```txt
Server → Engine/Core
Server → SagaShared
Server → SagaCollaboration service API
Server → Backends
Server → SDE compiled outputs
```

Forbidden server dependencies:

```txt
Server → Editor
Server → Apps/Saga product shell internals
Server → Apps/Editor
Server → Tools/Forge UI
Server → Tools/Prism UI
```

---

## 7. Current Runtime/Server Baseline

- [x] Establish working CMake/Conan/Ninja/sccache build path.

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

- [x] Establish basic UDP client/server connectivity.

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

- [x] Establish initial snapshot send/decode/apply path.

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

- [x] Establish initial client replication pipeline chain.

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

- [x] Establish initial server input acknowledgement path.

  Represented by:

  ```txt
  InputCommand receive path
  InputAck send path
  ```

  Preserved decision:

  ```txt
  Server receives client input commands and can acknowledge them.
  ```

- [x] Establish initial world kernel skeleton.

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

## 8. Networking Roadmap

### 8.1 Transport Layer

- [x] Provide basic UDP transport for client/server testing.

  Represented by:

  ```txt
  Client UDP path
  Server UDP listener
  Handshake exchange
  ```

- [ ] Harden UDP transport for production multiplayer.

  Done means:

  - packet send/receive is bounded,
  - socket errors are handled,
  - disconnects are detected,
  - reconnect path exists,
  - packet flood is limited,
  - malformed packet input is rejected safely,
  - transport diagnostics are exposed.

- [ ] Add transport abstraction.

  Done means:

  - runtime/server code can use a transport interface,
  - UDP implementation is one backend,
  - tests can use in-memory transport,
  - future relay/cloud transport can be introduced without rewriting replication logic.

Expected files:

```txt
Engine/include/SagaEngine/Network/ITransport.hpp
Engine/include/SagaEngine/Network/TransportEndpoint.hpp
Engine/src/SagaEngine/Network/UdpTransport.cpp
Engine/src/SagaEngine/Network/InMemoryTransport.cpp
```

---

### 8.2 Connection Lifecycle

- [x] Provide initial handshake flow.

  Represented by:

  ```txt
  Client handshake request
  Server handshake response
  Client connected state transition
  ```

- [ ] Add production connection state machine.

  Done means connection states include:

  - disconnected,
  - resolving,
  - connecting,
  - handshaking,
  - connected,
  - resynchronizing,
  - disconnecting,
  - failed.

Expected files:

```txt
Engine/include/SagaEngine/Network/ConnectionState.hpp
Engine/include/SagaEngine/Network/ConnectionStateMachine.hpp
Engine/src/SagaEngine/Network/ConnectionStateMachine.cpp
```

- [ ] Add connection timeout and heartbeat.

  Done means:

  - missed heartbeat is detected,
  - timeout reason is recorded,
  - client receives disconnect reason where possible,
  - server releases stale client state,
  - diagnostics expose heartbeat age and connection quality.

---

### 8.3 Packet Framing

- [ ] Define stable packet envelope.

  Done means every packet includes:

  - protocol version,
  - packet type,
  - sequence number,
  - connection/session id,
  - payload size,
  - validation/checksum field where appropriate.

Expected files:

```txt
Engine/include/SagaEngine/Network/PacketEnvelope.hpp
Engine/include/SagaEngine/Network/PacketType.hpp
Engine/src/SagaEngine/Network/PacketCodec.cpp
```

- [ ] Add packet size and payload validation.

  Done means:

  - oversized packets are rejected,
  - unknown packet types are rejected,
  - truncated packets are rejected,
  - invalid payload size fails safely,
  - diagnostic reason is recorded.

- [ ] Add protocol version negotiation.

  Done means:

  - client and server compare supported versions,
  - unsupported versions fail clearly,
  - version mismatch produces actionable diagnostics.

---

### 8.4 Reliability Layer

- [ ] Add reliable message channel for required gameplay/session messages.

  Done means:

  - reliable messages are acknowledged,
  - lost reliable messages are retransmitted,
  - retransmission is bounded,
  - duplicate reliable messages are ignored,
  - reliable channel does not block high-frequency unreliable snapshots indefinitely.

- [ ] Keep high-frequency state snapshots unreliable where appropriate.

  Done means:

  - snapshot stream tolerates loss,
  - newest valid snapshot can supersede old snapshots,
  - stale snapshots are rejected,
  - packet loss is visible in diagnostics.

---

## 9. Security Roadmap

### 9.1 Packet Security

- [ ] Validate all network input before use.

  Done means:

  - no packet payload is trusted before decoding validation,
  - malformed packets are rejected,
  - invalid state transitions are rejected,
  - unknown client/session ids are rejected,
  - invalid entity/component ids are rejected.

- [ ] Add rate limits.

  Done means server limits:

  - connection attempts,
  - handshake attempts,
  - input command frequency,
  - oversized packet attempts,
  - invalid packet spam.

- [ ] Add disconnect reasons and security diagnostics.

  Done means disconnect diagnostics can distinguish:

  - timeout,
  - malformed packet,
  - unsupported protocol,
  - unauthorized action,
  - rate limit,
  - server shutdown.

---

### 9.2 Client Trust Boundary

- [ ] Treat clients as untrusted.

  Done means:

  - client input is intent, not authority,
  - server validates movement/action requests,
  - client cannot directly author authoritative world state,
  - client cannot spawn/modify entities without authority,
  - client-side prediction never becomes server truth.

This is multiplayer. Trusting clients is how cheaters get a welcome basket.

---

### 9.3 Authentication Boundary

- [ ] Add authentication/session identity integration point.

  Done means:

  - network connection can bind to an authenticated account/session id,
  - unauthenticated dev sessions are explicitly marked,
  - production server can reject unauthenticated clients,
  - auth failure is reported clearly.

- [ ] Keep auth integration outside low-level transport.

  Done means:

  - transport moves packets,
  - session/auth layer validates identity,
  - game/server policy decides access.

---

## 10. Replication Roadmap

### 10.1 Server Snapshot Generation

- [x] Generate initial server snapshots.

  Represented by:

  ```txt
  Server test entity snapshot path
  Server snapshot send path
  ```

- [ ] Add production snapshot builder.

  Done means snapshots include:

  - stable entity ids,
  - component payloads,
  - tick id,
  - baseline/reference id where applicable,
  - relevance filtering result,
  - serialization version,
  - payload budget result.

Expected files:

```txt
Engine/include/SagaEngine/Replication/Snapshot.hpp
Engine/include/SagaEngine/Replication/SnapshotBuilder.hpp
Engine/src/SagaEngine/Replication/SnapshotBuilder.cpp
```

- [ ] Add snapshot delta compression.

  Done means:

  - server can build deltas against known baselines,
  - client can apply deltas safely,
  - missing baseline triggers resync,
  - invalid delta is rejected without corrupting client state.

---

### 10.2 Client Snapshot Decode and Apply

- [x] Decode and apply initial snapshots on client.

  Represented by:

  ```txt
  PacketDemux
  SnapshotApplyPipeline
  ReplicationApplyBridge
  ```

- [ ] Formalize client-side apply order.

  Done means implementation matches `ClientReplicationFormalSpec.md` for:

  - decode,
  - validation,
  - ordering,
  - state machine transition,
  - snapshot application,
  - reconciliation,
  - interpolation buffer update.

- [ ] Reject stale or invalid snapshots.

  Done means:

  - old tick snapshots are ignored,
  - snapshots from invalid session are rejected,
  - invalid component payload fails safely,
  - diagnostics identify rejection reason.

---

### 10.3 Replication State Machine

- [x] Establish initial client replication state machine.

  Represented by:

  ```txt
  StateMachine
  SnapshotApplyPipeline
  ReconciliationBuffer
  InterpolationManager
  ```

- [ ] Add production replication states.

  Done means states include:

  - idle,
  - connecting,
  - waiting for baseline,
  - synchronized,
  - resynchronizing,
  - disconnected,
  - failed.

Expected files:

```txt
Engine/include/SagaEngine/Replication/ReplicationState.hpp
Engine/include/SagaEngine/Replication/ReplicationStateMachine.hpp
Engine/src/SagaEngine/Replication/ReplicationStateMachine.cpp
```

- [ ] Add state transition diagnostics.

  Done means every replication state transition records:

  - previous state,
  - next state,
  - tick,
  - reason,
  - error code where applicable.

---

### 10.4 Relevance Filtering

- [ ] Add server-side relevance graph integration.

  Done means:

  - entities are filtered per client,
  - distance/visibility/interest rules can be evaluated,
  - bandwidth budget can affect snapshot selection,
  - relevance changes are reflected safely on client.

Expected files:

```txt
Engine/include/SagaEngine/World/RelevanceGraph.hpp
Engine/src/SagaEngine/World/RelevanceGraph.cpp
Engine/include/SagaEngine/Replication/RelevanceFilter.hpp
Engine/src/SagaEngine/Replication/RelevanceFilter.cpp
```

- [ ] Add relevance diagnostics.

  Done means diagnostics can show:

  - why an entity was included,
  - why an entity was excluded,
  - per-client relevant entity count,
  - bandwidth pressure.

---

### 10.5 Prediction and Reconciliation

- [x] Establish initial reconciliation buffer path.

  Represented by:

  ```txt
  ReconciliationBuffer
  InputCommand
  InputAck
  ```

- [ ] Add production client-side prediction.

  Done means:

  - client can predict local controlled entity,
  - input commands are stored by tick,
  - server correction can rewind/replay,
  - prediction errors are smoothed where appropriate,
  - severe divergence is corrected safely.

Expected files:

```txt
Engine/include/SagaEngine/Prediction/PredictionBuffer.hpp
Engine/include/SagaEngine/Prediction/InputCommand.hpp
Engine/include/SagaEngine/Prediction/InputAck.hpp
Engine/src/SagaEngine/Prediction/PredictionBuffer.cpp
```

- [ ] Add reconciliation diagnostics.

  Done means diagnostics expose:

  - correction count,
  - correction magnitude,
  - input delay,
  - last acknowledged input,
  - replay count,
  - severe divergence events.

---

### 10.6 Interpolation

- [x] Establish initial interpolation manager.

  Represented by:

  ```txt
  InterpolationManager
  ```

- [ ] Add production interpolation buffer.

  Done means:

  - remote entities interpolate between snapshots,
  - buffer delay is configurable,
  - missing snapshots are handled,
  - teleport/large correction cases are handled,
  - interpolation does not mutate authoritative state.

Expected files:

```txt
Engine/include/SagaEngine/Interpolation/InterpolationBuffer.hpp
Engine/include/SagaEngine/Interpolation/InterpolationManager.hpp
Engine/src/SagaEngine/Interpolation/InterpolationManager.cpp
```

---

## 11. Authoritative Simulation Roadmap

### 11.1 Server Authority

- [ ] Make server the source of truth for multiplayer simulation.

  Done means:

  - server owns authoritative entity state,
  - clients submit input intents,
  - server validates inputs,
  - server advances simulation ticks,
  - server emits authoritative snapshots.

- [ ] Add deterministic tick loop.

  Done means:

  - fixed tick rate exists,
  - tick id is monotonic,
  - simulation step duration is bounded,
  - tick overrun is recorded,
  - simulation does not depend on wall-clock randomness.

Expected files:

```txt
Engine/include/SagaEngine/Simulation/SimulationTick.hpp
Engine/include/SagaEngine/Simulation/SimulationClock.hpp
Engine/src/SagaEngine/Simulation/SimulationClock.cpp
```

---

### 11.2 Input Processing

- [x] Receive client input commands on server.

  Represented by:

  ```txt
  InputCommand receive path
  InputAck send path
  ```

- [ ] Add validated input command pipeline.

  Done means:

  - input command format is versioned,
  - commands include tick id,
  - commands include controlled actor/entity id,
  - commands are validated against permissions/ownership,
  - invalid commands are rejected with diagnostics.

Expected files:

```txt
Engine/include/SagaEngine/Input/InputCommand.hpp
Engine/include/SagaEngine/Input/InputValidationResult.hpp
Engine/src/SagaEngine/Input/InputCommandValidator.cpp
```

---

### 11.3 Entity Authority

- [ ] Add authority ownership model.

  Done means every networked entity has:

  - authority owner,
  - replication policy,
  - relevance policy,
  - prediction policy where applicable,
  - permission/ownership validation hooks.

- [ ] Add entity spawn/despawn authority.

  Done means:

  - server controls networked spawn/despawn,
  - spawn messages are ordered,
  - despawn is safely applied on client,
  - stale updates for despawned entities are rejected.

---

## 12. World Kernel Roadmap

### 12.1 World Node

- [x] Establish initial `WorldNode` concept.

  Represented by:

  ```txt
  WorldNode
  ```

- [ ] Make `WorldNode` production-ready.

  Done means:

  - world node owns a clear simulation domain,
  - world node has lifecycle states,
  - world node can load/unload cells,
  - world node exposes diagnostics,
  - world node participates in shard/zone ownership.

---

### 12.2 SimCell

- [x] Establish initial `SimCell` concept.

  Represented by:

  ```txt
  SimCell
  ```

- [ ] Make `SimCell` production-ready.

  Done means:

  - cell has stable id,
  - cell has bounds or logical domain,
  - cell has entity membership,
  - cell can be activated/deactivated,
  - cell can migrate between workers/nodes where supported,
  - cell diagnostics are available.

---

### 12.3 DomainScheduler

- [x] Establish initial `DomainScheduler` skeleton.

  Represented by:

  ```txt
  DomainScheduler
  ```

- [ ] Implement production domain scheduling.

  Done means:

  - simulation domains are scheduled deterministically where required,
  - dependencies between domains are respected,
  - overloaded domains are detected,
  - scheduling metrics are recorded,
  - domain failures do not silently corrupt world state.

---

### 12.4 EventStream

- [x] Establish initial `EventStream` concept.

  Represented by:

  ```txt
  EventStream
  ```

- [ ] Implement production event stream.

  Done means:

  - events are ordered,
  - event payloads are validated,
  - event memory usage is bounded,
  - event replay is possible where required,
  - invalid events are rejected safely.

---

### 12.5 RelevanceGraph

- [x] Establish initial `RelevanceGraph` concept.

  Represented by:

  ```txt
  RelevanceGraph
  ```

- [ ] Integrate relevance graph with replication.

  Done means:

  - relevance graph feeds snapshot builder,
  - per-client relevance is computed,
  - entity enter/leave relevance events are generated,
  - diagnostics explain relevance decisions.

---

## 13. Zone and Shard Management

### 13.1 Zone Model

- [ ] Define zone ownership model.

  Done means each zone has:

  - zone id,
  - authority owner,
  - world bounds or logical scope,
  - active entity set,
  - client interest set,
  - migration boundaries,
  - diagnostics state.

Expected files:

```txt
Engine/include/SagaEngine/World/ZoneId.hpp
Engine/include/SagaEngine/World/ZoneDescriptor.hpp
Engine/include/SagaEngine/World/ZoneAuthority.hpp
```

- [ ] Add zone lifecycle.

  Done means zones can be:

  - created,
  - loaded,
  - activated,
  - deactivated,
  - unloaded,
  - migrated where supported.

---

### 13.2 Shard Model

- [ ] Define shard ownership model.

  Done means each shard has:

  - shard id,
  - world partition set,
  - server authority owner,
  - active player set,
  - persistence scope,
  - diagnostics state.

Expected files:

```txt
Engine/include/SagaEngine/Shard/ShardId.hpp
Engine/include/SagaEngine/Shard/ShardDescriptor.hpp
Engine/include/SagaEngine/Shard/ShardManager.hpp
```

- [ ] Add shard boot and shutdown flow.

  Done means:

  - shard can start cleanly,
  - shard can reject invalid configuration,
  - shard can shutdown gracefully,
  - connected clients receive shutdown reason,
  - state flush/persistence hooks are called.

---

### 13.3 Cross-Zone Movement

- [ ] Add cross-zone entity transfer.

  Done means:

  - entity can move between zones,
  - authority transfer is explicit,
  - replication relevance updates correctly,
  - client does not observe duplicate authority,
  - failed transfer rolls back or fails safely.

---

## 14. ECS and Component Runtime

### 14.1 Component Registration

- [ ] Add production component registration.

  Done means:

  - components have stable type ids,
  - serializers are registered,
  - replication metadata is registered,
  - default construction is supported,
  - invalid component type ids are rejected.

Expected files:

```txt
Engine/include/SagaEngine/ECS/ComponentTypeId.hpp
Engine/include/SagaEngine/ECS/ComponentRegistry.hpp
Engine/src/SagaEngine/ECS/ComponentRegistry.cpp
```

This is shipping-critical.

If component registration is missing, ECS apply cannot be trusted. Pretending otherwise would be adorable if it were not a direct path to broken replicated entities.

---

### 14.2 Entity Lifecycle

- [ ] Add production entity lifecycle.

  Done means entities support:

  - create,
  - destroy,
  - activate,
  - deactivate,
  - serialize,
  - replicate,
  - validate.

Expected files:

```txt
Engine/include/SagaEngine/ECS/EntityId.hpp
Engine/include/SagaEngine/ECS/EntityRegistry.hpp
Engine/src/SagaEngine/ECS/EntityRegistry.cpp
```

---

### 14.3 Component Apply

- [ ] Add validated replicated component apply.

  Done means:

  - component payload type is validated,
  - component serializer exists,
  - payload size is bounded,
  - apply order is deterministic,
  - invalid payload fails without corrupting entity state.

Expected files:

```txt
Engine/include/SagaEngine/Replication/ComponentApplyContext.hpp
Engine/src/SagaEngine/Replication/ComponentApply.cpp
```

---

## 15. Resources and Asset Streaming

### 15.1 Runtime Asset Streaming

- [x] Document implemented runtime asset streaming system.

  Represented by:

  ```txt
  docs/implementation-notes/AssetStreamingImplementation.md
  ```

  Preserved decision:

  ```txt
  Runtime streaming is implemented enough to document as an implementation note, not an active roadmap.
  ```

- [ ] Keep runtime asset streaming lean.

  Done means runtime streaming owns:

  - async priority-based asset loading,
  - reference-counted residency caching,
  - platform memory budgets,
  - file-backed asset sources,
  - asset registry manifests,
  - typed mesh and texture streaming wrappers.

- [ ] Keep editor import/cook workflows outside runtime streaming internals.

  Done means:

  - editor import UX is editor/tool owned,
  - asset cooking pipeline is separate from runtime streaming,
  - runtime consumes cooked/validated assets,
  - runtime does not become a broad third-party asset importer.

---

### 15.2 Resource Budgets

- [ ] Add production resource budget system.

  Done means:

  - memory budgets exist per platform/profile,
  - streaming priority respects budget pressure,
  - eviction policy is deterministic enough to debug,
  - budget violations produce diagnostics.

Expected files:

```txt
Engine/include/SagaEngine/Resources/ResourceBudget.hpp
Engine/include/SagaEngine/Resources/ResidencyCache.hpp
Engine/src/SagaEngine/Resources/ResidencyCache.cpp
```

---

### 15.3 Asset Registry

- [ ] Add production asset registry integration.

  Done means:

  - runtime assets have stable ids,
  - asset metadata can be loaded,
  - missing assets fail clearly,
  - invalid asset hashes are detected,
  - cooked asset manifests are supported.

Expected files:

```txt
Engine/include/SagaEngine/Assets/AssetId.hpp
Engine/include/SagaEngine/Assets/AssetManifest.hpp
Engine/include/SagaEngine/Assets/AssetRegistry.hpp
Engine/src/SagaEngine/Assets/AssetRegistry.cpp
```

---

## 16. SDE Boundary

- [x] Define SDE as standalone deterministic data compiler.

  Represented by:

  ```txt
  ENGINE_ROADMAP.md
  SHARED_ROADMAP.md
  DependencyGraph.md
  SDE_ROADMAP.md
  ```

  Preserved decision:

  ```txt
  Engine/runtime may consume SDE outputs.
  Engine must not make SDE depend on engine headers.
  ```

- [ ] Consume SDE outputs through explicit runtime integration points.

  Done means runtime/server can consume:

  - compiled graph outputs,
  - artifact references,
  - schema ids,
  - stable hashes,
  - generated artifacts,
  - diagnostic payloads.

- [ ] Prevent invalid SDE output from publishing runtime state.

  Done means:

  - failed SDE compile blocks unsafe preview/publish,
  - invalid artifact references fail clearly,
  - runtime does not silently load invalid graph artifacts.

Forbidden dependency direction:

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

---

## 17. Collaboration Boundary

- [x] Define collaboration as product/session capability, not runtime gameplay policy.

  Represented by:

  ```txt
  COLLABORATION_ROADMAP.md
  DependencyGraph.md
  ENGINE_ROADMAP.md
  ```

- [ ] Allow runtime/server to consume collaboration services only through stable APIs.

  Done means:

  - runtime/server may consume `SagaShared` contracts,
  - runtime/server may consume `SagaCollaboration` service APIs,
  - runtime/server never include editor-private collaboration headers.

Allowed:

```txt
Runtime/Server → SagaShared contracts
Runtime/Server → SagaCollaboration service APIs
```

Forbidden:

```txt
Runtime/Server → Editor/include/SagaEditor/Collaboration
Runtime/Server → editor UI
Runtime/Server → product shell internals
```

- [ ] Keep runtime multiplayer policy separate from editor collaboration policy.

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

Shared primitives may exist.

Shared policy should not be blindly merged, unless the plan is to create one bug that can ruin two systems at once.

---

## 18. Diagnostics and Telemetry

### 18.1 Runtime Diagnostics

- [ ] Add runtime diagnostics service.

  Done means diagnostics can report:

  - simulation tick timing,
  - replication state,
  - packet loss,
  - interpolation buffer health,
  - prediction corrections,
  - resource budget pressure,
  - asset loading failures.

Expected files:

```txt
Engine/include/SagaEngine/Diagnostics/RuntimeDiagnostic.hpp
Engine/include/SagaEngine/Diagnostics/IRuntimeDiagnostics.hpp
Engine/src/SagaEngine/Diagnostics/RuntimeDiagnostics.cpp
```

---

### 18.2 Server Diagnostics

- [ ] Add server diagnostics service.

  Done means diagnostics can report:

  - active clients,
  - connection states,
  - packet rates,
  - invalid packet counts,
  - input rejection counts,
  - simulation tick health,
  - snapshot send budget,
  - shard/zone load.

Expected files:

```txt
Server/include/SagaServer/Diagnostics/ServerDiagnostic.hpp
Server/include/SagaServer/Diagnostics/IServerDiagnostics.hpp
Server/src/SagaServer/Diagnostics/ServerDiagnostics.cpp
```

---

### 18.3 Network Debug Views

- [ ] Add network debugging output.

  Done means developers can inspect:

  - connection state,
  - round-trip time,
  - packet loss,
  - packet type counts,
  - reliable resend count,
  - snapshot age,
  - last acknowledged input.

---

## 19. Persistence and Backend Integration

- [ ] Define backend access boundaries.

  Done means:

  - server accesses persistence through interfaces,
  - runtime client does not own database connections,
  - engine core does not know backend schemas,
  - backend failures are surfaced through diagnostics.

Expected files:

```txt
Server/include/SagaServer/Persistence/IPersistenceBackend.hpp
Server/include/SagaServer/Persistence/PlayerStateRecord.hpp
Server/src/SagaServer/Persistence/PersistenceService.cpp
```

- [ ] Add authoritative state persistence hooks.

  Done means:

  - shard/server can flush state,
  - player state can be saved,
  - world state can be snapshotted where required,
  - persistence failure has safe fallback behavior.

---

## 20. Runtime Preview Integration

- [ ] Support runtime preview as a consumer of engine runtime.

  Done means:

  - editor can start runtime preview through approved service boundary,
  - runtime preview does not include editor-private implementation,
  - preview can report diagnostics back to editor,
  - stopping preview restores safe authoring state.

- [ ] Support multiplayer preview test sessions.

  Done means:

  - editor/product can launch local client/server test sessions,
  - runtime/server use stable descriptors,
  - server-private headers do not leak into editor,
  - network/session diagnostics are visible.

---

## 21. Concurrency and Tasking

- [ ] Add production task scheduler boundaries.

  Done means:

  - runtime jobs are scheduled through engine task primitives,
  - server simulation work can be partitioned,
  - resource loading jobs are bounded,
  - diagnostics expose queue depth and job timing.

Expected files:

```txt
Engine/include/SagaEngine/Tasks/TaskScheduler.hpp
Engine/include/SagaEngine/Tasks/TaskHandle.hpp
Engine/src/SagaEngine/Tasks/TaskScheduler.cpp
```

- [ ] Define thread ownership for replication pipeline.

  Done means:

  - network receive thread ownership is explicit,
  - decode/apply thread ownership is explicit,
  - simulation thread mutation rules are explicit,
  - cross-thread handoff is bounded and validated.

- [ ] Add data race protection tests for runtime/server critical paths.

---

## 22. Determinism and Simulation Correctness

- [ ] Define deterministic runtime constraints.

  Done means:

  - simulation tick order is explicit,
  - deterministic systems avoid wall-clock dependency,
  - random sources are seeded and controlled,
  - floating-point risk areas are documented,
  - replay can validate important deterministic behavior.

- [ ] Add replayable simulation test path.

  Done means:

  - recorded input stream can be replayed,
  - simulation result can be hashed,
  - divergence can be detected,
  - replay diagnostics identify first divergent tick where possible.

Expected files:

```txt
Engine/include/SagaEngine/Replay/ReplayStream.hpp
Engine/include/SagaEngine/Replay/ReplayRunner.hpp
Engine/src/SagaEngine/Replay/ReplayRunner.cpp
```

---

## 23. Testing Roadmap

### 23.1 Unit Tests

- [ ] Add unit tests for packet codec.

  Required coverage:

  - valid packet decode,
  - invalid packet type,
  - oversized packet,
  - truncated packet,
  - version mismatch.

- [ ] Add unit tests for replication state machine.

  Required coverage:

  - valid transitions,
  - invalid transitions,
  - stale snapshot rejection,
  - resync transition,
  - failure transition.

- [ ] Add unit tests for component registration and apply.

  Required coverage:

  - valid component type,
  - missing serializer,
  - invalid payload,
  - deterministic apply order.

---

### 23.2 Integration Tests

- [ ] Add client/server connection integration test.

  Done means:

  - server starts,
  - client connects,
  - handshake completes,
  - client receives snapshot,
  - client disconnects cleanly.

- [ ] Add replication integration test.

  Done means:

  - server changes authoritative state,
  - snapshot is sent,
  - client applies update,
  - interpolation receives state,
  - invalid snapshot is rejected.

- [ ] Add prediction/reconciliation integration test.

  Done means:

  - client sends inputs,
  - server acknowledges inputs,
  - client reconciles correction,
  - final state matches authority.

---

### 23.3 Load and Soak Tests

- [ ] Add server load test harness.

  Done means:

  - simulated clients can connect,
  - packet rate can be configured,
  - snapshot rate can be configured,
  - server tick health is measured,
  - packet loss/latency can be simulated.

- [ ] Add long-running soak test.

  Done means:

  - server runs for extended duration,
  - memory growth is tracked,
  - connection churn is simulated,
  - resource loading is exercised,
  - diagnostics are collected.

---

## 24. CI and Architecture Enforcement

- [ ] Add forbidden include scanner.

  Required checks:

```txt
Engine/** must not include Editor/**
Engine/** must not include Apps/**
Engine/** must not include Saga product shell internals/**
Engine/** must not include Server/private/**
Runtime/** must not include Editor/include/SagaEditor/Collaboration/**
Server/** must not include Editor/include/SagaEditor/Collaboration/**
SDE/** must not include SagaEngine/**
```

- [ ] Add CMake dependency validation.

  Bad examples:

```txt
EngineCore → SagaEditor
EngineCore → Apps/Saga
EngineCore → SagaCollaboration implementation
SagaShared → Engine runtime internals
SDE → SagaEngine
```

- [x] Decouple `SagaEngine` public headers from `SagaServer` packet types.

  Represented by:

```txt
Engine/include/SagaEngine/Networking/NetworkTypes.h
Engine/include/SagaEngine/Networking/Packet.h
Engine/include/SagaEngine/Networking/Replication/SnapshotBuilder.h
Shared/include/SagaShared/Replication/SnapshotContracts.hpp
Tools/scripts/check_engine_server_boundary.py
```

  Preserved rule:

  ```txt
  SagaEngine public headers must not require Server/include.
  ```

- [ ] Add protocol compatibility tests to CI.

  Done means incompatible packet/protocol changes fail loudly instead of becoming runtime archaeology.

---

## 25. Deployment-Facing Server Roadmap

- [ ] Add server configuration model.

  Done means server config supports:

  - bind address,
  - port,
  - tick rate,
  - max clients,
  - shard id,
  - logging level,
  - persistence backend config,
  - development/production mode.

Expected files:

```txt
Server/include/SagaServer/Config/ServerConfig.hpp
Server/src/SagaServer/Config/ServerConfigLoader.cpp
```

- [ ] Add graceful shutdown.

  Done means:

  - server stops accepting new clients,
  - connected clients receive shutdown reason,
  - state flush hooks run,
  - sockets close cleanly,
  - shutdown diagnostics are emitted.

- [ ] Add health endpoint or health reporter.

  Done means deployment systems can inspect:

  - process health,
  - active clients,
  - tick health,
  - shard state,
  - backend state.

---

## 26. Migration Plan

- [ ] Move roadmap ownership language to Saga product architecture.

  Done means engine docs no longer imply editor binary or engine owns product lifecycle.

- [ ] Move runtime correctness contracts into formal specs.

  Done means long-term invariants live in:

```txt
docs/specs/replication/ClientReplicationFormalSpec.md
```

  while roadmap progress remains in:

```txt
ENGINE_ROADMAP.md
```

- [ ] Move implementation history out of active roadmaps.

  Done means asset streaming implementation history lives in:

```txt
docs/implementation-notes/AssetStreamingImplementation.md
```

- [ ] Extract shared contracts away from runtime-private/editor-private locations.

  Done means neutral contracts live in:

```txt
SagaShared
```

- [ ] Keep collaboration implementation out of engine runtime policy.

  Done means collaboration implementation lives in:

```txt
SagaCollaboration
```

  and runtime/server consume only stable APIs where needed.

---

## 27. Non-Goals

This roadmap does not own:

- Saga product shell,
- project dashboard UX,
- editor docking/panels,
- editor asset import UX,
- final collaboration implementation,
- collaboration UI,
- SDE compiler internals,
- Forge build frontend behavior,
- Prism code intelligence behavior,
- SagaTools command dispatch behavior,
- marketing/product packaging.

Related ownership:

| Area | Owner |
|---|---|
| Product shell | `Saga` |
| Authoring UI | `SagaEditor` |
| Shared contracts | `SagaShared` |
| Collaboration implementation | `SagaCollaboration` |
| SDE compiler | `SDE` |
| Tool ecosystem | `TOOLS_ROADMAP.md` |
| Runtime/server systems | `SagaEngine` / `SagaServer` |

---

## 28. Production Definition of Done

- [ ] Engine/Core has no upward dependencies into product/editor/server/tool ownership.

- [ ] Server is authoritative for multiplayer simulation.

- [ ] Client/server networking is hardened against malformed input.

- [ ] Packet protocol is versioned, validated, and diagnosed.

- [ ] Replication supports snapshots, deltas, relevance, stale rejection, and resync.

- [ ] Client prediction and reconciliation are production-safe.

- [ ] Interpolation is stable and diagnostic-friendly.

- [ ] World kernel systems are production-ready, not skeleton-only.

- [ ] Zone/shard ownership is explicit.

- [ ] ECS component registration and replicated apply are reliable.

- [ ] Runtime asset streaming is bounded by platform budgets.

- [ ] SDE outputs are consumed through explicit integration points only.

- [ ] Runtime/server consume collaboration services only through stable APIs.

- [ ] Diagnostics exist for runtime, server, network, replication, resource, and simulation failures.

- [ ] Unit, integration, load, and soak tests cover critical runtime/server behavior.

- [ ] CI enforces forbidden dependency directions.

---

## 29. Final Architecture Rule

The engine architecture should remain:

```txt
Saga
  owns product lifecycle and primary executable

SagaEditor
  owns authoring UX

SagaEngine / Runtime
  owns game execution and runtime primitives

SagaServer
  owns multiplayer authority and server policy

SagaShared
  owns neutral contracts

SagaCollaboration
  owns collaboration implementation

SDE
  owns deterministic data compilation

Tools
  own standalone workflows
```

Engine code should be boring, stable, and difficult to misuse.

That sounds unglamorous because it is.

It is also how engines survive longer than one heroic refactor and three optimistic TODO comments.
