# Server Authority

> Last updated: 2026-05-26

Server authoritative movement is partly implemented. This does not yet make a
complete multiplayer product loop.

## Replication Correctness Boundary

Server owns authoritative gameplay truth where server-authoritative flows
exist. Clients may send requests, predict local motion, render state, reconcile
against server updates, and interpolate remote state, but client-side state is
not authoritative multiplayer truth.

Replication contracts should preserve validation, ordering, stale packet
rejection, reconciliation, interpolation, bounded diagnostics, and explicit
failure behavior. This boundary does not claim production networking, scale,
soak coverage, anti-cheat, or a complete multiplayer product.

## Current State

- Server authoritative movement code exists.
- Actor ownership, command intake, validation, and movement output have focused
  tests and design notes.
- Full server replication/snapshot/reconciliation product loop is incomplete.
- Stress/load/performance coverage is missing.
- Heavy replication behavior is not established.

## Evidence Boundary

Current evidence supports narrow server-authoritative movement, command
validation, actor ownership, and movement output behavior. It does not prove a
complete replication, snapshot, reconciliation, soak, scale, or production
networking loop.

For the longer correctness specification, see the appendix-style
[Client-Side Replication Pipeline formal spec](../internal/architecture-appendices/ClientReplicationFormalSpec.md).
That spec does not by itself prove complete implementation or production
multiplayer readiness.
