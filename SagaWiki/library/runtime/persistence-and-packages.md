---
title: Persistence, assets, and packages
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Persistence, assets, and packages

Persistence, Assets, Resources, and packaging tools solve different parts of the content path. Persistence owns storage contracts, Assets owns asset identity and loading concerns, Resources owns runtime resource lifetime, and SagaPackager stages distributable inputs from a validated project manifest.

## Interface before backend

Public persistence contracts should describe operations, configuration, results, and ownership without requiring pqxx, hiredis, or Redis implementation types. Concrete database adapters are implementation details unless a deliberate public API decision says otherwise.

`PostgreSQLImpl.h` and `RedisImpl.h`, together with their implementation sources, live under Persistence `Private/Backends`. Public Persistence contracts remain vendor-neutral, and architecture checks prevent pqxx, hiredis, and Redis implementation types from leaking into installed headers.

Likewise, package consumers should rely on manifest and package contracts rather than the layout of an individual staging directory. A generated package is derived output and must be traceable to its source inputs.

## Asset source and runtime streaming

Project-owned asset source manifests record canonical asset identity, ownership, paths, and references. SagaProjectKit inventories those manifests and reports missing owners and non-canonical generated artifacts; the report does not become asset truth itself.

At runtime, Resources owns lookup, registry integration, streaming requests, residency, and bounded streaming policy. Runtime streaming consumes runtime-ready metadata; it does not own editor import UX, source-format normalization, cooking policy, package staging, or publishing. Development-only fallback paths should be explicit and diagnostics-visible.

## Project and package manifests

`.sagaproj` schema version `0` is the current validated project-manifest contract. SagaProjectKit validates it; SagaPackager consumes it. Package profiles may narrow included artifacts, but they do not redefine project identity.

Optional governance reports accepted by packaging are externally produced inputs. SagaPackager does not produce those reports and no bundled policy workflow should be inferred from the flag.

## Capability limit

Storage adapters and serialization foundations do not establish a shipped persistent-world service, migration system, backup policy, operational database topology, or compatibility guarantee for future schemas. Those claims require their own implementation and evidence.

Detailed asset/package startup rules are in [Runtime lifecycle, assets, and packages](../reference/runtime-lifecycle-assets-and-packages.md). Byte streaming and residency are in [Resource streaming and residency](../reference/resource-streaming-and-residency.md).

Current storage interfaces, backpressure, migration, cache/presence/pub-sub, event-log evidence, and backend debt are in [Persistence contracts](../reference/persistence-contracts.md). Producer-side identity and manifest rules are in [Asset pipeline and manifest generation](../reference/asset-pipeline-and-manifest-generation.md).
