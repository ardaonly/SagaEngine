---
title: Runtime
status: current
owner: SagaWiki
generated_html: pages/runtime.html
---

# Runtime

SagaRuntime owns runtime application startup, command-line handling, project/package bootstrap, and runtime-host integration. Samples own scenario-specific source. Runtime may consume Assets, Resources, Scripting, UI, Replication, World, Render, RHI, Input, Audio, ECS, Simulation, Diagnostics, Math, and Core contracts; it does not own editor authoring or offline package production.

## Startup and content boundary

Runtime startup validates selected project and package inputs, registers runtime-ready asset and resource manifests, and establishes the owned service and application lifecycle. Invalid, missing, or stale consumption inputs must remain explicit failures. Source discovery, import, cook, package staging, publish checks, and authoring UI remain offline-tool or editor responsibilities rather than runtime ownership.

## Graphics coordinates

Saga uses a right-handed coordinate system: +X is right, +Y is up, and forward is `-Z`. Normalized device depth is zero-to-one, and counter-clockwise winding defines front faces. Backend-specific conversion belongs at the private graphics adapter boundary and must preserve this public convention.

## Networking and replication

Networking owns generic transports, packets, reliability, rate and bandwidth behavior. Replication owns snapshots, RPC, interest, prediction, interpolation, client application, and related wire state. Server-only connection implementations do not live under generic Networking.

## Persistence

Persistence exposes SagaEngine-owned database and event-sourcing contracts. Concrete PostgreSQL and Redis implementations are backend implementation details; public contracts must not expose their vendor types.
