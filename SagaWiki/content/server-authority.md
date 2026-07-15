---
title: Server authority
status: current
owner: SagaWiki
generated_html: pages/server-authority.html
---

# Server authority

ServerAuthority owns zones, shards, authoritative connection implementation, gameplay command dispatch, movement validation, security policy, ownership, and authoritative simulation decisions. Clients may request and predict; they do not become multiplayer authority.

ConnectionManager and ServerConnection are private implementation classes consumed by ServerAuthority internals. Generic connection and transport contracts belong in Networking only when they are authority-neutral. A dedicated public authority-connection contract should be introduced only when a real external consumer requires one.

## Replication correctness

Where implemented, authority and replication paths preserve command validation, ownership, ordering, stale-input rejection, prediction reconciliation, interpolation, and explicit failure diagnostics. Client requests and predicted state remain subordinate to server decisions. These contracts describe correctness boundaries, not a complete replication or persistent-world product loop.

## Evidence boundary

Focused server and client/server tests demonstrate their named scenarios only. They do not establish production scale, deployment topology, hostile-network security, persistence durability, operations readiness, or service availability.
