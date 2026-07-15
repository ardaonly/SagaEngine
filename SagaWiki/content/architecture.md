---
title: Architecture
status: current
owner: SagaWiki
generated_html: pages/architecture.html
---

# Architecture

Dependencies point toward stable contracts and artifacts; ownership follows runtime, editor, program, tool, sample, and test modules.

## Layer direction

```text
Programs and Editor
        ↓
Runtime modules and public contracts
        ↓
Core, Math, ECS, low-level graphics vocabulary

Offline Tools → manifests, packages, reports → Runtime / ServerAuthority
```

Runtime consumes project and package artifacts. ServerAuthority owns authoritative decisions. Editor owns authoring presentation and commands. Tools own offline validation, compilation, packaging, and reporting. Reports never become source truth merely because they exist.

## Ownership and artifact flow

Reusable module contracts point downward toward Core, Math, ECS, and vendor-neutral runtime vocabulary. Concrete backend managers, editor UI, server policy, and tool implementation stay with their owning modules rather than becoming public engine API by default.

Source-controlled project inputs flow through validation, analysis, compilation, and packaging tools into generated artifacts and manifests consumed by Runtime or ServerAuthority. Diagnostics and tests describe what was checked; they do not replace authored inputs or promote a bounded result into product readiness.

## Aggregate targets

The hard cutover preserves existing exported and aggregate CMake target names. Module CMake files register explicit sources, include roots, and dependencies into those targets. A target-purity decomposition is intentionally deferred.

## Dependency rule

Generic Networking owns transport and protocol primitives. Replication owns client replication, prediction, interpolation, snapshots, interest, and RPC. ServerAuthority owns zones, authoritative connections, gameplay validation, security policy, and server simulation.
