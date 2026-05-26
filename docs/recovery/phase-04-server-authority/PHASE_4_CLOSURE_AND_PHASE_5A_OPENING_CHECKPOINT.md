# Phase 4 Closure And Phase 5A Opening Checkpoint

> Last updated: 2026-05-25
> Status: Phase 4 closed as Server Authoritative Movement Foundation established
> Decision: open Phase 5A as a report-only Asset Pipeline / Package Runtime Readiness Audit

This checkpoint closes Phase 4 narrowly. It does not claim a complete
server-authoritative multiplayer loop, ReplicationManager movement integration,
accepted-state snapshot serialization, or client reconciliation proof.

Full test health remains unverified.

## Phase 4I Closure Summary

Phase 4I can close as:

- accepted as movement dirty replication intent bridge
- not accepted as ReplicationManager integration
- not accepted as accepted-state snapshot serialization
- not accepted as client reconciliation proof
- not accepted as full end-to-end authoritative multiplayer loop

## Phase 4I Completion Evidence

Confirmed evidence:

- `MovementDirtyReplicationBridge` exists under Server networking/replication ownership.
- `MovementDirtyReplicationBridge` records accepted movement dirty entities as `{ entityId, serverTick }` replication intents.
- The bridge deduplicates pending entity ids and keeps the latest tick until consumed.
- `ZoneServer::TickMovementAuthority` records `AuthoritativeMovementTickReport::dirtyEntityIds` into the bridge.
- `ZoneServer` exposes `ConsumeMovementDirtyReplicationIntentsForTesting()` and `GetPendingMovementDirtyReplicationIntentCountForTesting()`.
- `MovementDirtyReplicationBridgeTests` exists.
- `MovementDirtyReplicationBridgeTests` is registered with `unit;server;replication` labels.
- Focused implementation verification built `SagaServerLib` and `MovementDirtyReplicationBridgeTests`.
- Focused CTest passed for `MovementDirtyReplicationBridgeTests` and `ZoneServerMovementAuthorityTests`.
- `git diff --check` passed during closure inspection.
- Label inventories were checked after Phase 4I:
  - `server`: 10 tests
  - `networking`: 5 tests
  - `replication`: 4 tests
  - `integration`: 3 tests
- Full CTest remains unverified.

## Authority Proof Achieved Across Phase 4

Phase 4 established the server authoritative movement foundation in staged
slices:

- Phase 4A completed the Server Authoritative Movement Audit.
- Phase 4B added `AuthoritativeMovementCore`.
- Phase 4C added `AuthoritativeMovementInputAdapter`.
- Phase 4D added `AuthoritativeMovementCommandIntake`.
- Phase 4E added `ServerPacketNormalizer` and `NormalizedServerPacketView`.
- Phase 4F wired packet normalization into the ZoneServer normalized packet drain.
- Phase 4G added `ActorOwnershipRegistry`.
- Phase 4H wired ZoneServer to `ActorOwnershipRegistry` and `AuthoritativeMovementCommandIntake`.
- Phase 4I added `MovementDirtyReplicationBridge` and ZoneServer dirty movement replication intent recording.

## What Phase 4 Can Claim

Phase 4 may claim:

- server authoritative movement foundation is established.
- registered actor input can mutate server-owned movement state only on tick.
- unknown, malformed, non-input, rejected, and non-mutating cases do not mutate or produce dirty intent.
- accepted movement produces replication intent.
- deterministic focused tests exist for the chain from normalized ZoneServer input to tick-gated movement mutation and dirty intent.

## What Phase 4 Cannot Claim

Phase 4 may not claim:

- full authoritative multiplayer loop is complete.
- `ReplicationManager` consumes movement dirty intents.
- accepted movement state is serialized into snapshots.
- client apply/reconciliation is end-to-end proven.
- `ClientHost` and `ClientNetworkSession` are resolved.
- Runtime/App ownership work is complete.
- full CTest health is verified.

## Remaining Phase 4 Debt

| Debt item | Current state | Risk | Likely future phase |
|---|---|---:|---|
| ReplicationManager component dirty bridge | Movement dirty intent exists, but `ReplicationManager::MarkComponentDirty` is not called | high | Post-Phase 5 or dedicated replication slice |
| Movement component id / transform serialization contract | No stable production contract proven | high | Replication/snapshot follow-up |
| Accepted-state snapshot proof | Not implemented | high | Replication/snapshot follow-up |
| Client snapshot apply/reconciliation proof | Client infrastructure exists, but not proven end to end with ZoneServer authority path | high | MultiplayerSandbox proof |
| `ClientHost` / `ClientNetworkSession` cleanup | Still unresolved from Phase 3 and Phase 4 | medium | Runtime/client ownership follow-up |
| Full integration test health | Full CTest remains unverified | medium | Any release/closure checkpoint |
| End-to-end `MultiplayerSandbox` proof | Future vertical proof only | high | Later gameplay/multiplayer phase |

## Phase 4 Closure Decision

Close Phase 4 as Server Authoritative Movement Foundation established.

Do not close Phase 4 as full server-authoritative multiplayer loop complete.

The closure claim is intentionally foundation-level: Phase 4 now proves
ownership, normalized input intake, tick-gated server mutation, rejection
behavior, and dirty replication intent. It does not prove snapshot output or
client-visible reconciliation.

## Phase 5A Opening Decision

Phase 5 may open now only as Phase 5A: Asset Pipeline / Package Runtime
Readiness Audit.

Phase 5A is report-only. It must not begin as:

- a broad asset pipeline rewrite
- a package format rewrite
- a Runtime asset service migration
- a publish gate implementation
- a broad CMake boundary hardening slice
- a UI/editor asset workflow implementation

## Phase 5A Audit Scope

Phase 5A should inspect and report on:

- package manifest path and current package manifest loader ownership
- asset manifest path and current asset manifest loader ownership
- runtime asset registry bootstrap path
- asset identity manifest path and id allocation ownership
- current `Tools/AssetPipeline` ownership, including `AssetIdentityGenerator`
- content/package example readiness, including current `Content/UI` assets
- future `MultiplayerSandbox` asset and package needs
- Runtime/server package consumption boundaries
- publish gate relationship and what must remain deferred
- current CMake/test target leakage around AssetPipeline, Runtime, Product, and package tests

Concrete repo areas to inspect:

- `Tools/AssetPipeline`
- `Engine/Private/SagaEngine/Resources`
- `Engine/Public/SagaEngine/Assets`
- `Engine/Private/SagaEngine/Packages`
- `Runtime/include` and `Runtime/src`
- `Apps/Runtime`
- `Content`
- `cmake/modules/SagaTargets.cmake`
- `cmake/modules/SagaTests.cmake`
- `docs/AssetStreamingImplementation.md`
- `docs/DependencyGraph.md`
- `docs/roadmaps/ASSET_PIPELINE_ROADMAP.md`
- `docs/roadmaps/BUILD_PUBLISH_PIPELINE_ROADMAP.md`

## Phase 5A Outputs

The Phase 5A audit should produce:

- asset/package ownership inventory
- runtime package consumption map
- asset identity and manifest flow map
- first narrow Phase 5B implementation candidate
- deterministic test strategy for that candidate
- verification plan
- unresolved risks and explicit non-goals

The first likely implementation candidate should remain small, probably one UI
document asset through pipeline-owned discovery/cook output, package staging,
manifest registration, and Runtime loading proof.

No implementation should happen in Phase 5A unless separately approved after
the audit.

## Verification Summary

Commands run for the Phase 4 closure decision:

- `git status --short`
- `git diff --check`
- `git diff --name-only`
- `git diff --stat`
- targeted `rg` over Phase 4I, replication, snapshot, Runtime, ClientHost, and test terms
- targeted `rg` over asset/package/runtime pipeline terms
- `ctest --test-dir build/RelWithDebInfo-0.0.9-sde -N -L server`
- `ctest --test-dir build/RelWithDebInfo-0.0.9-sde -N -L networking`
- `ctest --test-dir build/RelWithDebInfo-0.0.9-sde -N -L replication`
- `ctest --test-dir build/RelWithDebInfo-0.0.9-sde -N -L integration`

Results recorded for closure:

- Worktree is broadly dirty with unrelated tracked and untracked changes.
- `git diff --check` passed.
- Phase 4I bridge files and ZoneServer integration are present.
- Phase 4I focused tests and CMake target are present.
- Label inventories include the new focused replication bridge target.
- Architecture suite was not run.
- Full CTest remains unverified.

## Decision Gate

Phase 4 is closed only as: Server Authoritative Movement Foundation established.

Phase 5A is open only as: report-only Asset Pipeline / Package Runtime
Readiness Audit.

Do not revise Phase 4 before moving on unless new evidence contradicts the
ZoneServer authority shell or movement dirty replication intent bridge.
