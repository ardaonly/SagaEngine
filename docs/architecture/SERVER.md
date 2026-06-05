# Server Authority

> Last updated: 2026-05-26

Server authoritative movement is partly implemented. This does not yet make a
complete multiplayer product loop.

## Current State

- Server authoritative movement code exists.
- Actor ownership, command intake, validation, and movement output have focused
  tests and design notes.
- Full server replication/snapshot/reconciliation product loop is incomplete.
- Stress/load/performance coverage is missing.
- Heavy replication behavior is not established.

## Background

- [Phase 4 closure](../recovery/phase-04-server-authority/PHASE_4_CLOSURE_AND_PHASE_5A_OPENING_CHECKPOINT.md)
- [Server movement audit](../recovery/phase-04-server-authority/PHASE_4A_SERVER_AUTHORITATIVE_MOVEMENT_AUDIT.md)
