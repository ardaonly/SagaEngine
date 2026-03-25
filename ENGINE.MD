# ENGINE.md

**Project state:** Foundational MMO Engine Core

**Current version:** SagaEngine_v0.0.5

## Summary

SagaEngine is an MMO-first engine foundation with explicit separation between:
- core runtime
- dedicated server runtime
- backend services
- editor tooling
- future platform extensions

The repository does not implement a platform product today.
It defines the engine core and the boundaries required for later platform-grade expansion without entangling platform behavior with core runtime systems.

## Architectural focus

- Authoritative simulation
- Replication and interest management
- Persistence and backend service separation
- Modding and scripting boundaries
- Resource loading and streaming
- Client/server runtime separation

## Current scope

- `Engine/` for core systems
- `Runtime/` for client runtime glue
- `Server/` for dedicated server logic
- `Backends/` for persistence and service infrastructure
- `Editor/` for tooling
- `Tests/` for validation

## Vision

Future higher-level products may be built on top of this foundation, but only through clean boundaries and explicit extension layers.

This file is a human-readable project descriptor.
Tooling and build behavior must rely on manifests and build configuration, not on prose.