---
title: World facade and simulation boundary
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# World facade and simulation boundary

World owns partitioning, streaming coordination, relevance, cells, events, and high-level world lifecycle. Simulation owns mutable ECS-style runtime state and deterministic tick behavior. Public consumers should not need World's internal scheduler or partition graph merely to initialize or inspect a world.

## Narrow public facade

`SagaEngine/World/WorldFacade.h` is the current transitional public shell. It exposes facade-owned configuration, tick options/results, status, statistics, and a narrow command vocabulary. Internal `WorldNode`, `SimCell`, `DomainScheduler`, `EventStream`, `RelevanceGraph`, partition/streaming types, session details, and mutable simulation state remain private implementation.

The facade currently provides bounded lifecycle and accounting behavior. It is not a stable general world SDK, full streaming solution, persistence surface, replication API, or permission to expose internal pointers later.

## Commands, queries, and diagnostics

Future public growth should stay in copied value records: explicit commands, read-only queries, status, and diagnostics-safe summaries. Replication, persistence, rendering interest, and server authority may consume world information through owned bridges, but they do not merge their policies into the facade.

## Simulation truth

Runtime `WorldState` and authoritative simulation state remain separate from scene authoring source. Serialized snapshots and replicated deltas are runtime representations. They do not replace project-owned scene/entity truth or grant a client authority.

## Evidence boundary

Architecture checks can prove the public/private header split, while unit/integration tests prove the specific lifecycle, simulation, serialization, authority, or replication paths they execute. This evidence does not establish shipped persistent worlds.

See [World and simulation contracts](../reference/world-and-simulation-contracts.md) for truth layers, scene activation, cells, relevance, commands/events, cancellation, and evidence.
