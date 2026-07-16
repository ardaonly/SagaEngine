---
title: Networking, replication, and authority
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Networking, replication, and authority

Networking transports data, Replication selects and applies replicated state, and ServerAuthority owns authoritative world decisions. Keeping these responsibilities separate prevents a connection helper from silently becoming the simulation authority.

## Authority boundary

The authoritative path validates client identity, packet shape, ownership, timing, and command semantics before changing simulation state. Movement input is intent; the authority decides whether and how it affects an actor. Replication then communicates accepted state through its own dirty-state and interest contracts.

ServerAuthority owns zone/shard and connection behavior specific to authoritative service composition. Generic Networking should not become the home of server-only managers merely because they use sockets.

## Network chaos policy

The Networking module contains a `NetworkChaosLayer` contract and focused unit evidence. Chaos behavior is an explicit test or diagnostic policy: delay, loss, duplication, and reordering must be configured deterministically enough to reproduce a run. It must not be silently enabled in ordinary runtime paths.

The durable lesson from the historical chaos document is fault-injection ownership, not its old target name. After the cutover, Networking owns the contract; tests prove only the configured transformations they exercise.

## Client replication pipeline

Client replication validates and orders untrusted network input before applying it. Full snapshots establish an authoritative baseline; deltas apply only to the matching baseline. Out-of-order work is bounded by count and age, stale or duplicate input is rejected, and a missing baseline requests resynchronization instead of applying a patch to the wrong world state. Prediction reconciliation and interpolation consume the resulting authoritative state without becoming authority themselves.

These invariants are represented by current Replication types and focused integration evidence. The detailed internal class graph may evolve; bounded memory, deterministic apply order, stale-input rejection, and separation from editor collaboration remain the durable contract.

## Current evidence boundary

Unit and integration tests cover pieces of ownership, command intake, zone-server behavior, dirty replication, interest, diagnostics, and client/server interaction. This is not a claim of a production networking service, internet-scale operation, shipped persistent community worlds, or a hardened dedicated-server product.

The bounded server harness defaults to `127.0.0.1` and rejects public bind addresses. `TransportFactory::CreateUdp` names the only implemented transport directly. No dynamic secure-channel surface is presented as wired packet security; production authentication, encryption, key management, and hostile-network deployment remain explicit non-claims.

See [Replication, networking, and server authority](../reference/replication-networking-and-authority.md) for the formal receive/apply pipeline, state machine, sequence, baseline/delta, prediction, interest, memory/rate, chaos, and authority rules.
