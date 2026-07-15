---
title: Runtime foundations: Core, Math, ECS, and Simulation
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Runtime foundations: Core, Math, ECS, and Simulation

Core, Math, ECS, and Simulation form the lower part of the current runtime dependency graph. They are separate owners: Core supplies process-local services and lifecycle vocabulary, Math supplies engine value types, ECS supplies component-oriented storage and execution contracts, and Simulation supplies authoritative ticking plus the physics and animation surfaces currently assigned to that module. This page describes the checked-in public surface; it does not declare every public header to be a frozen external SDK.

## Core owns mechanisms, not domain policy

Core contains the general mechanisms used by higher modules. Its public headers currently cover application lifetime, module and subsystem registration, events, jobs and task graphs, time, profiling, allocation helpers, logging, crash capture, and low-level memory accounting.

`IModule`, `ModuleRegistry`, and `ModuleManager` define process-local module lifecycle. A composition owner may use them to register and order modules, but individual domain modules still own their startup policy. A module manager must not become a second location for Render, Scripting, World, or networking behavior.

`EventBus` provides in-process publication. It is not a durable message broker, a replication channel, or an editor collaboration transport. Event payload ownership and lifetime must remain explicit at the caller boundary.

`JobSystem` and `TaskGraph` provide bounded work scheduling and dependency ordering. They do not make arbitrary domain state thread-safe. The owner submitting work remains responsible for borrows, mutation, cancellation, deterministic joins, and shutdown ordering.

Core also contains `Logger`, sinks, an asynchronous log queue, rotation, `CrashHandler`, and `MemoryTracker`. These are low-level collection and process-support mechanisms. Runtime Diagnostics owns structured diagnostic values, operational aggregation, health and reliability reports. A Core log line or crash callback can feed Diagnostics, but the two modules are not interchangeable.

## Math values and conventions

Engine-facing code uses SagaEngine Math types instead of leaking a math vendor through unrelated public contracts. The current surface includes `Vec3`, `Quat`, `Mat4`, `Transform`, scalar/constants helpers, SIMD helpers, and an explicit GLM bridge. Vendor conversion is visible at the Math boundary rather than implicit throughout ECS, networking, or rendering headers.

The rendering coordinate convention remains right-handed: +X right, +Y up, and forward along -Z, with zero-to-one depth and counter-clockwise front faces. That convention protects interchange between transforms, cameras, scene data, and rendering; it does not mean every simulation value must use a floating-point presentation type.

`DeterministicMath` and `DeterministicVectors` provide fixed-point scalar, vector, and quaternion values for deterministic paths. Crossing from fixed values to ordinary floating-point values is an explicit boundary because repeated conversion can reintroduce drift. Deterministic types reduce one source of variation; they do not by themselves prove identical behavior across every compiler, platform, physics backend, script, or unordered container.

## ECS identity and storage

The ECS public surface separates entity identity, component metadata, storage organization, queries, systems, and observation:

| Area | Current contract |
| --- | --- |
| Entity | A handle carries identity and version so a recycled slot does not silently validate an old reference. |
| Components | Registration assigns component metadata; serializer registration is a separate concern. |
| Archetypes | Component sets organize storage and migration when an entity's component set changes. |
| Queries | Consumers request matching component sets instead of taking ownership of storage internals. |
| Systems | Systems express work over ECS state; parallel execution remains subject to declared access safety. |
| Change tracking | Observers and partial dirty masks make bounded changes visible to interested owners. |

Component identifiers, serialization identifiers, and network field identifiers are related only when a deliberate mapping says so. A C++ type name is not a persistent asset or wire identity. Registration collisions and missing registrations must fail visibly rather than selecting an arbitrary type.

Parallel system execution does not permit two systems to mutate the same state without coordination. Scheduling evidence must name the access model it proves. A successful parallel unit test is not evidence that all component callbacks, serializers, or third-party code are thread-safe.

## Simulation truth and ticks

`WorldState` is the current behavior-state container used by the Simulation surface. Focused tests cover entity creation and destruction, generation changes on recycled identifiers, component add/remove/query behavior, deterministic serialization, and state hashing. These tests prove the named in-process contracts, not a shipped persistent-world store.

`SimulationTick` converts elapsed time into bounded fixed updates and exposes interpolation alpha. Its spiral-of-death clamp is a local scheduling safeguard. It must be observable when real time cannot be fully simulated; silently changing the authority timeline would hide an overload condition.

The authority surface registers systems and advances them in a defined order. Determinism evidence records and verifies state hashes over a bounded history. A hash match is useful evidence for the state included in the hash. It does not prove that omitted state, external I/O, floating-point vendor code, or asynchronous callbacks were deterministic.

`WorldPartitionSystem` currently records entity cell and zone membership, reports zone transitions, and supports bounded radius queries. It is not the owner of source scene schemas, streaming storage, network interest, or a production sharding service. Those concerns meet at explicit adapters described in [World and simulation contracts](world-and-simulation-contracts.md).

## Physics and animation placement

Physics and animation headers currently live under Simulation. The public surface includes physics types, a physics world/engine, animation clips, skeletons, and pose evaluation. This placement records current source ownership; it does not assert that the APIs are feature-complete or permanently frozen.

Authoritative gameplay must decide explicitly which physics results enter deterministic state. A visual pose, interpolated transform, or backend contact callback is not automatically simulation truth. Animation assets and authored skeleton data remain source/artifact concerns until loaded and validated; evaluated poses are derived runtime values.

## Failure and shutdown rules

Foundation services are commonly initialized early and destroyed late. That makes failure behavior important:

- registration must reject duplicate or incompatible identities;
- scheduled work must be joined or cancelled before its state disappears;
- event subscribers must not survive the objects they call;
- logging and diagnostics should remain available long enough to report cleanup failure;
- allocators and trackers must distinguish invalid release from a successful no-op;
- partial startup must unwind in reverse dependency order.

These rules are ownership constraints, not a claim that every current call site has completed hardening.

## Evidence and review boundary

Cross-cutting tests under `Tests` currently carry most evidence; the per-module `Tests` directories for these owners remain placeholders. Simulation unit coverage is detailed, while architecture tests protect public/private and dependency boundaries. When a foundation contract changes, add focused unit evidence for accepted and rejected behavior and architecture evidence if dependency visibility changes.

Reviewers should ask whether a change adds a general mechanism or smuggles domain policy into a foundation module, whether a supposedly deterministic value crosses into floating point or unordered work, and whether a public type is a deliberate contract rather than a moved implementation header.

