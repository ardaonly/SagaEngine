# Engine WorldNode / SimCell Facade Audit

> Last updated: 2026-06-05

This started as the focused report-only audit for the five remaining public
headers in `Engine/Public/SagaEngine/World` after the Partition/Streaming
cluster move. The follow-up migration has now moved that WorldNode/SimCell
cluster private without forwarding headers or CMake/install rule changes.

The original audit conclusion was that the WorldNode/SimCell cluster was
public-internal debt. `WorldNode.h` was a facade candidate, but not a clean
public SDK facade because it exposed server sessions, replication state,
mutable cells, and direct subsystem access.

## Method

Direct includes remain the primary usage signal. Broad symbol searches are used
only as supporting context because the current names overlap with comments and
implementation details.

Read-only inspection commands:

```sh
rg -n '#include .*SagaEngine/World/(DomainScheduler|EventStream|RelevanceGraph|SimCell|WorldNode)\.h' . -g '!build/**' -g '!Build/**'
rg -n '\b(WorldNode|SimCell|DomainScheduler|EventStream|RelevanceGraph|ClientSession|WorldEvent|RelevanceResult|SimDomainType|DomainTickDispatcher)\b' Tests Tools Runtime Server Editor Apps Engine/Private/SagaEngine/World
```

## Direct Include Matrix

| Header | Public World direct includes | Engine private direct includes | Tests/Tools/Runtime/Server/Editor/Apps direct includes |
| --- | --- | --- | --- |
| `SagaEngine/World/DomainScheduler.h` | `WorldNode.h` | `DomainScheduler.cpp` | None |
| `SagaEngine/World/EventStream.h` | `WorldNode.h` | `EventStream.cpp` | None |
| `SagaEngine/World/RelevanceGraph.h` | `WorldNode.h` | `RelevanceGraph.cpp` | None |
| `SagaEngine/World/SimCell.h` | `DomainScheduler.h`, `EventStream.h`, `RelevanceGraph.h`, `WorldNode.h` | `SimCell.cpp`, `SimCell_SpatialLogic.cpp` | None |
| `SagaEngine/World/WorldNode.h` | None | `WorldNode.cpp` | None |

No direct include of these five headers was found from `Tests`, `Tools`,
`Runtime`, `Server`, `Editor`, or `Apps`. The only non-Engine hit in the
supporting symbol search was an `Apps/Server/TestSnapshotServer.h` comment that
mentions future WorldNode wiring; it is not a dependency.

## Symbol Usage Summary

Inside `Engine/Private/SagaEngine/World`, the symbols are used by their matching
implementation files and by `WorldNode.cpp`, which owns the cluster behavior:

- `WorldNode.cpp` constructs and operates `SimCell`, `DomainScheduler`,
  `EventStream`, and `RelevanceGraph`; it also manages `ClientSession`,
  replication snapshot/delta flow, dirty component tracking, entity cell
  membership, and event flushing.
- `DomainScheduler.cpp` implements domain registration, cell registration, and
  tick firing over `SimDomainType` and `SimCellId`.
- `EventStream.cpp` implements event append, replay, snapshot, disk persistence,
  and serialization helpers for `WorldEvent` and `WorldSnapshot`.
- `RelevanceGraph.cpp` implements edge/rule maintenance, rebuild and
  incremental updates, relevance queries, and graph stats.
- `SimCell.cpp` and `SimCell_SpatialLogic.cpp` implement entity membership,
  metrics, split/merge decisions, state transitions, and event buffering.

Outside the Engine private World implementation, no direct include dependency
was found. That supports treating the cluster as public-internal, but does not
make the boundary safe to move directly because the public headers expose each
other through concrete signatures.

## File-Level Decision Matrix

| Header | Classification | Recommended action | Risk | Reason |
| --- | --- | --- | --- | --- |
| `SagaEngine/World/WorldNode.h` | Public facade candidate with server/replication boundary leakage | Create narrow facade first | High | Exposes client sessions, snapshot/delta replication, dirty component tracking, direct subsystem accessors, mutable `SimCell*`, `DomainScheduler&`, `EventStream&`, `RelevanceGraph&`, and `WorldState&`. |
| `SagaEngine/World/SimCell.h` | Internal world simulation detail | Keep public-internal until facade exists | High | Central dependency for `DomainScheduler`, `EventStream`, `RelevanceGraph`, and `WorldNode`; moving it alone would force a cluster move or public forwarding strategy. |
| `SagaEngine/World/DomainScheduler.h` | Internal world simulation detail | Move private after facade exists | Medium | Only exposed through `WorldNode.h`; manages domain tick registration and cell dispatch, not stable SDK API. |
| `SagaEngine/World/EventStream.h` | Internal world persistence/replay detail | Move private after facade exists | Medium | Exposes event log, snapshot, disk persistence, serialization helper surface, and WorldNode-owned event flow. |
| `SagaEngine/World/RelevanceGraph.h` | Internal replication/interest detail | Move private after facade exists | Medium | Exposes interest graph edges/rules/query internals used by WorldNode replication filtering. |

## WorldNode Facade Analysis

`SagaEngine/World/WorldNode.h` currently exposes:

- lifecycle: `Init`, `Shutdown`, `Tick`;
- cell management: `GetOrCreateCell`, `GetCell`, `RemoveCell`, `Cells`;
- entity management: `SpawnEntity`, `DespawnEntity`, `MoveEntity`,
  `GetEntityCell`;
- client/session management: `AddClientSession`, `RemoveClientSession`,
  `GetClientSession`;
- replication: `ReplicateToClient`, dirty tracking, component enumeration;
- event/domain wiring: `AppendEvent`, `RegisterDomain`, `SetDomainDispatcher`;
- direct subsystem accessors: `Scheduler`, `Events`, `Relevance`,
  `WorldStateRef`.

This is not a clean public SDK facade yet. The current header exposes
`ClientSession`, snapshot/delta replication behavior, dirty component tracking,
mutable `SimCell*`, `DomainScheduler&`, `EventStream&`, `RelevanceGraph&`, and
`WorldState&`. These are server and replication implementation details, not a
stable external World API.

A later facade should keep only lifecycle, tick, configuration, stats, and a
minimal command/query surface. It should hide cells, scheduler, event stream,
relevance graph, session internals, and replication details behind private
implementation.

## SimCell Cluster Dependency Analysis

`SagaEngine/World/SimCell.h` is not a stable SDK concept yet. It contains:

- partition identity and helpers through `SimCellId`, `SimCellFootprint`,
  `CoordFromWorld`, and `DistanceSqCoordToWorldPoint`;
- lifecycle and state through `SimCellState` and `SetState`;
- domain registration through `SimDomainType`, `SimDomainMask`, and
  `RegisterDomains`;
- metrics and thresholds through `SimCellMetrics`, `SimCellThresholds`,
  `UpdateMetrics`, `ShouldSplit`, and `ShouldMerge`;
- split/merge execution through `ExecuteSplit` and `ExecuteMergeInto`;
- event buffering through `SimCellEvent`, `AppendEvent`, and `DrainEvents`;
- entity membership through `AddEntity`, `RemoveEntity`, `HasEntity`, and
  `Entities`.

It is the dependency anchor for `SagaEngine/World/DomainScheduler.h`,
`SagaEngine/World/EventStream.h`, `SagaEngine/World/RelevanceGraph.h`, and
`SagaEngine/World/WorldNode.h`. Moving `SimCell.h` directly would require
moving the full cluster or adding forwarding/public surrogate types for the
public signatures that still expose cells, cell IDs, domain types, event cell
IDs, and relevance filtering inputs.

## Recommendation

Choose Option B: create a narrow World facade first, then move the full cluster
private.

Option A, moving `DomainScheduler`, `EventStream`, `RelevanceGraph`, and
`SimCell` private while keeping current `WorldNode.h` public, is too risky
because current `WorldNode.h` directly exposes those types in public methods,
callbacks, accessors, and member-visible return types.

Option C, keeping all five public-internal temporarily, is safe but does not
advance the boundary after the Partition/Streaming move.

Completed follow-up sequence after this audit:

1. Designed the narrow facade contract in
   [Engine World public facade contract](ENGINE_WORLD_PUBLIC_FACADE_CONTRACT.md).
2. Added `SagaEngine/World/WorldFacade.h` as a transitional public shell with
   an architecture guard.
3. Moved `WorldNode`, `SimCell`, `DomainScheduler`, `EventStream`, and
   `RelevanceGraph` private after the public facade no longer depended on their
   concrete types.

Follow-up status: `SagaEngine/World/WorldFacade.h` now exists as a transitional
public shell with an architecture guard. The WorldNode/SimCell cluster is now
private implementation surface.

## Status

Migrated Private. This audit records the original WorldNode/SimCell boundary
problem and the completed Option B follow-up, but it does not claim a stable
SDK/API or full Engine public API cleanup.
