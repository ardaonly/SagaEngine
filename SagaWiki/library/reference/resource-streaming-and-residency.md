---
title: Resource streaming and residency
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Resource streaming and residency

This reference carries forward the durable design knowledge from the former asset-streaming implementation note and reconciles it with the 0.0.11 `Assets`, `Resources`, Render, Audio, and Diagnostics owners. It is intentionally more detailed than the overview page because streaming bugs usually occur at the seams between identity, bytes, residency, typed decoding, and consumer lifetime.

## Ownership

`Engine/Source/Runtime/Assets` owns manifest and artifact/package record contracts. `Engine/Source/Runtime/Resources` owns runtime identity resolution, registry bootstrap, byte sources, virtual content access, streaming requests, scheduling, budgets, residency, and generic typed streamer coordination. Render and Audio own domain interpretation and device-specific lifetime. Diagnostics consumes snapshots and events without taking control of resource lifetime.

The boundary can be summarized as:

```text
Assets manifests
  -> Resources identity and registry
  -> IAssetSource or virtual file system
  -> StreamingManager / ResourceStreamingPipeline
  -> ResourceManager residency
  -> domain decoder and device upload
  -> Render, Audio, Script, or other consumer
```

Editor import is outside this runtime path. It can produce validated/cooked payloads and manifests, but it must not add editor dependencies to `Resources` or teach a runtime worker to invoke an authoring importer as a transparent fallback.

## Stable identity

An `AssetId` is the compact runtime identity used in hot paths. A project-facing asset key is the readable canonical key used in manifests and diagnostics. A source or virtual path says where the bytes can be requested. These are related values, not interchangeable strings.

Identity resolution must be deterministic for the same validated inputs. An unknown key produces an explicit failure. Collision detection occurs before registration. Invalid or sentinel identifiers never become valid entries merely because a path exists.

Code should pass identities rather than repeatedly carrying native paths through gameplay and rendering. Paths change when content is cooked, packaged, mounted, or redirected; stable identity lets those storage decisions change without rewriting every consumer.

### Identity invariants

The invalid/sentinel id is permanently reserved. Registration rejects it. A valid id maps to no more than one canonical key within the active registry generation, and one canonical key resolves to one id. Rebuilding a registry from identical validated inputs produces identical mappings.

An id is meaningful only in the identity domain/generation that produced it. Persisting an incidental hash-table index or process-local pointer is forbidden. If the numeric `AssetId` is derived from a key, collision detection remains mandatory; “hashes usually do not collide” is not a correctness rule.

Diagnostic display prefers the canonical key plus id. The key helps humans; the id helps correlate runtime events. Neither is used as an unchecked native path.

## Registry contract

`AssetRegistry` is the lookup boundary between a stable `AssetId` and the minimal information needed to request bytes. Entries can include the canonical key, kind, current runtime/cooked path, flags, approximate byte size, and LOD availability. The registry is deliberately not a source-control database, dependency graph, collaboration store, or virtual filesystem.

Registry construction happens during controlled bootstrap. Once active, lookup should be predictable and cheap. If the implementation permits mutation for editor reload or tests, that mutation must remain an explicit mode with synchronization and collision rules; normal runtime readers must not observe half-applied batches.

`AssetManifestRegistryAdapter` provides a preflight/plan boundary. It resolves a manifest entry to a planned registry entry, attaches structured diagnostics, and then performs insertion only when the plan is acceptable. This avoids registering the first entries and discovering a fatal collision halfway through the file.

Registry entry flags can express startup-critical, streamable, pinned/default-resident, or other supported policy. A flag changes scheduling/residency behavior only when the current owner implements it; unknown bits are rejected or ignored according to a versioned compatibility rule. Approximate size can guide admission but never replaces the measured payload size after load.

Bootstrap has three explicit phases:

1. load and validate manifests/identity mappings;
2. plan all registry entries and detect duplicate key/id/path conflicts without mutation;
3. commit the accepted batch, then publish the registry as ready.

Readers never see a registry between phase two and three. Reload/editor modes either build a new immutable generation and swap it or use a separately synchronized mutation contract.

## Virtual content access

`IVirtualFileSystem` addresses content with normalized absolute virtual paths. A mounted backend receives a mount-relative path. The concrete VFS resolves the longest matching mount and returns a structured status, bytes, and diagnostic context.

Path rules are security and determinism rules, not convenience formatting:

- virtual paths use one normalized separator convention;
- `.` and `..` traversal is rejected or normalized before backend access according to the contract;
- a path must not escape the mounted native root;
- empty and relative virtual paths are not silently interpreted through the process working directory;
- mount collisions and invalid prefixes return explicit mount status;
- native directory, memory, archive, or future remote backends obey the same logical read contract.

The in-memory backend is deterministic test infrastructure. The native-directory backend supports loose development content. Their existence does not imply a CDN client, signed archive implementation, or production remote delivery service.

## Asset source boundary

`IAssetSource` answers the byte-fetch question independently of scheduling and residency. `FileAssetSource` and `VirtualFileAssetSource` are concrete sources. A source receives a validated registry entry/request and returns bytes or a classified failure. It does not decide whether another consumer already holds the asset, how long bytes stay resident, or how a texture is uploaded to a GPU.

Separating source from scheduler makes deterministic tests possible: a memory source can model success, absence, delay, corrupt bytes, or bounded failure without touching the native filesystem. It also prevents packaging choices from leaking into the public consumer contract.

## Request shape

A streaming request identifies at least the asset, expected kind, priority, and requested quality/LOD where relevant. It may also carry cancellation state and bounded diagnostic correlation. The request does not carry an arbitrary callback that can mutate runtime state from a worker thread without an ownership rule.

Priority expresses scheduling importance, not correctness. Critical startup content can be scheduled ahead of speculative content, but low-priority content remains required if the calling contract says it is required. Priority must not alter identity validation or allow one request to read outside the mounted content boundary.

LOD is a request bounded by the registry and source. Zero or a designated default can mean best/current policy; other values request a tier. The source reports what it actually returned. Consumers must not assume that a requested tier exists merely because the integer was accepted by an API.

The request handle follows an observable state model:

| State | Meaning | Allowed next states |
| --- | --- | --- |
| Pending | Accepted/queued or active; no payload is readable | Ready, Failed, Cancelled |
| Ready | A validated neutral payload is published | Terminal |
| Failed | A classified failure and diagnostics are published | Terminal unless an explicit new request is made |
| Cancelled | The request will not publish a usable payload | Terminal |

Terminal state is published once. A timeout observer can decide to cancel or abandon its borrow, but it does not rewrite the shared handle to failure while a worker still owns publication.

## Streaming manager

`StreamingManager` accepts work, orders it, invokes the configured source/decoding stage, and publishes terminal request state. A request handle exposes pending, success, cancellation, and failure without requiring the caller to block a frame thread.

Durable scheduler rules include:

- no request is processed before its identity and registry entry validate;
- a higher priority affects queue order but does not starve lower classes forever;
- cancellation is observable and safe whether work is queued, active, or already terminal;
- worker failure becomes a terminal result with a diagnostic rather than an abandoned pending handle;
- shutdown stops admission, resolves or cancels queued work, joins owned workers, and only then destroys source state;
- callbacks or published payloads cross to their consuming thread through a declared boundary.

Thread count and queue implementation are configuration details. Correctness must not depend on the accidental timing of a specific number of workers.

### Queue discipline

Priority queues need stable tie-breaking, normally request sequence. Without it, identical fixtures can complete in a different order on every run and make diagnostics hard to reproduce. Starvation prevention can age queued work or reserve service across priority classes, but it must remain bounded and testable.

Admission validates queue capacity. When full, the manager returns backpressure/rejection rather than allocating indefinitely. Critical startup policy can reserve capacity or fail startup; it does not bypass total memory bounds.

Cancellation removes queued work when possible. Active I/O can observe a cancellation token; if the source cannot cancel, completion is discarded and published as cancelled without exposing the bytes to the consumer. The request object remains alive until the worker releases it, preventing use-after-free.

### Result publication

The worker builds a complete result off to the side, including status, payload, actual byte count, selected LOD, and diagnostic. It then publishes under the handle's synchronization with release/acquire or lock semantics that make the bytes visible before readiness becomes true. A caller must not see `Ready` and an incompletely constructed vector.

Callbacks, if supported, run in a documented context. A worker callback cannot call arbitrary gameplay/editor code under the queue lock. Prefer a completion queue consumed by the domain owner.

## Resource streaming pipeline

`ResourceStreamingPipeline` coordinates registry lookup, request planning, source access, and result publication. It is useful when the caller needs a complete request lifecycle rather than a raw source read. The pipeline reports stage-specific errors: unknown identity is different from missing bytes; missing bytes are different from decode failure; decode failure is different from cancellation.

A pipeline stage should not silently change the requested asset kind. If a mesh path contains texture-like bytes, validation fails. Typed wrappers can verify headers/schema before constructing domain objects. Generic infrastructure remains independent of Render and Audio by transporting bytes and neutral metadata.

The stage result vocabulary distinguishes:

- invalid request or unknown id;
- registry kind mismatch;
- no source/mount for the path;
- source missing or I/O failure;
- cancelled before/during source work;
- corrupt or incompatible payload;
- typed decode failure;
- publication/internal invariant failure.

This granularity lets a required startup asset stop the process, an optional render material select a fallback, and a transient development source failure become retryable without conflating all three.

## Residency manager

`ResourceManager` sits above streaming and prevents duplicate work. It owns a map keyed by `AssetId`, returns `ResidencyHandle` values, and keeps a shared streaming handle/payload per entry. Multiple callers acquiring the same identity reuse the same pending or completed entry rather than scheduling identical loads.

A residency handle is a borrow with lifetime semantics:

- default or failed handles are invalid;
- a valid handle can still be pending;
- readiness and terminal status are observed before reading bytes;
- copying a handle increments the logical reference count;
- moving transfers the borrow without incrementing;
- reset or destruction releases the reference;
- a byte pointer or span is valid only while the handle keeps the entry alive.

Consumers must not cache a raw pointer beyond the handle. This rule is more important than the concrete container used by the manager.

## No duplicate streaming

On the first acquisition, the manager creates an entry and submits one streaming request. A second acquisition for the same effective asset while the request is pending attaches to that entry. When the request completes, all handles observe the same terminal state and payload.

The effective key must account for any distinction that produces incompatible payloads. If two LOD requests can resolve to different bytes, the manager either normalizes them to one chosen residency policy or includes the tier in its key. It must not label one payload as another tier to preserve a simplistic map.

Failed entries need a deliberate retry policy. Immediate implicit retry on every acquire can turn one missing file into an I/O storm. Permanent validation failure, transient source failure, cancellation, and stale/reload replacement may require different behavior and diagnostics.

The find-or-create operation is atomic. Two threads that miss simultaneously cannot both submit. One installs the pending entry; the other increments/reuses it. Submission failure either removes/marks the installed entry consistently or publishes one shared failed result.

If the cache key includes `AssetId`, kind, and LOD/generation policy, equality/hash use all of them. If the manager intentionally stores one best payload per id, it documents how a lower/higher LOD acquire upgrades, reuses, or creates another domain-owned resource.

## Reference-counted residency

Reference count answers “is a consumer currently borrowing this entry?” It does not by itself decide immediate destruction. An entry whose count reaches zero becomes an eviction candidate. Retaining it for a bounded time enables a later request to reuse bytes without another source round trip.

Entries with active borrows are not evicted. An asset can temporarily push resident bytes above the soft budget if all existing entries are referenced; correctness takes precedence over pretending the metric never exceeds a limit. The over-budget state is visible in diagnostics so the owner can tune content or budgets.

## LRU eviction

Zero-reference entries enter an LRU structure ordered by release/use policy. When admission or explicit maintenance sees budget pressure, eviction begins with the coldest eligible entries and stops after sufficient bytes are released. Reacquiring an LRU entry removes it from the candidate list and reuses its payload.

Eviction must update the residency map, byte accounting, LRU links, and any terminal request ownership consistently under the manager's synchronization. A stale LRU iterator or an entry present in two lists is an invariant failure, not a harmless statistic mismatch.

`EvictAll` affects zero-reference entries. It is useful for shutdown, deterministic tests, and explicit editor reload. It must not invalidate active borrows by force.

## Budget model

The byte budget is a soft residency ceiling rather than a promise of exact physical memory usage. Compressed bytes, decoded CPU objects, allocator overhead, staging buffers, and GPU resources can have different sizes. The generic manager accounts for the bytes it owns; domain owners expose additional budgets for their allocations.

Budget inputs should be platform/profile configuration. Zero has an explicit meaning, such as disabling eviction for a test, rather than an accidental divide-by-zero or “unlimited unless another branch says otherwise.” Snapshot output includes resident bytes, configured budget, entry count, LRU candidates, total acquisitions, hits, misses, and evictions.

Diagnostics can derive hit ratio and pressure, but report generation must not mutate the cache or hold its lock while performing slow I/O.

### Admission under pressure

On a miss the manager estimates incoming bytes from the registry when known, collects eligible cold entries, and evicts until the expected post-admission state is within the soft ceiling or no candidates remain. Unknown size is admitted and measured on completion. If actual size is larger, a later maintenance/acquire pass evicts other eligible entries and exposes over-budget state.

Pinned/startup-critical entries are not selected by ordinary LRU eviction. Their bytes still count. A profile whose pinned set exceeds budget is a configuration/content diagnostic, not a reason to lie about accounting.

Temporary request/source/decode/staging bytes are reported by the owner that allocates them. The residency budget applies to retained payloads. Render texture/mesh pools have separate budgets because GPU representation can differ radically from source bytes.

## Typed consumers

Generic residency returns neutral payload bytes. A mesh streamer validates and interprets mesh data for Render; a texture streamer validates texture data and selects appropriate mip/upload behavior; Audio owns audio decode and device buffers. This keeps `Resources` from depending upward on every consumer domain.

Typed wrappers should preserve the underlying identity and diagnostic correlation. Decode errors identify the asset key/id, kind, source path when safe, expected schema/format, and failing stage. They should not reinterpret corrupt data as an empty successful asset.

## Mesh and texture behavior

Mesh streaming can use manifest-declared LOD availability and request the tier selected by render interest. The generic pipeline retrieves the payload; Render validates vertex/index layout, creates native-neutral engine resources, and lets private RHI/Diligent code own device objects.

Texture streaming separates source/cooked payload from mip residency and GPU upload. Receiving a full encoded image as CPU bytes does not prove mip streaming. Upload completion does not mean the CPU residency entry can be destroyed unless the Render contract says the device resource is independent. The lifetime handoff is explicit.

Runtime import of arbitrary authoring formats remains a non-goal for shipped behavior. Development fixtures can exercise supported importers, but cooked artifact consumption is the stable distribution direction.

## Dependency handling

The registry is intentionally not a full dependency graph. A material may depend on textures, a scene on meshes, or a script package on assemblies. The owning domain interprets those relationships and issues additional resource acquisitions. This avoids teaching the generic streaming layer the semantics of every asset kind.

Dependency fan-out must remain bounded. Cycles, repeated dependencies, and partial failure are diagnosed by the owner. A top-level handle becoming ready does not imply all domain dependencies are ready unless its typed contract defines readiness that way.

For a material-like typed asset, the domain owner can plan dependency acquisitions first, deduplicate by effective key, and publish the typed asset only after required dependencies are ready. Optional dependencies can bind canonical fallback resources. On failure, all temporary borrows are released deterministically.

The asset/artifact manifest may record dependency identity/freshness for startup and package validation. The runtime registry remains minimal. Do not duplicate a mutable dependency graph inside every layer.

## Failure behavior

Failures are classified so callers can choose correct behavior:

| Failure | Owner response |
| --- | --- |
| Invalid asset id/key | Reject before source access; report identity diagnostic |
| Missing registry entry | Reject request; do not glob for a similarly named file |
| Unsafe or invalid path | Reject at adapter/VFS boundary |
| Source not found | Terminal missing-source result; optional consumer may use declared fallback |
| I/O failure | Terminal source failure with bounded system context |
| Corrupt or wrong-kind payload | Typed validation/decode failure; never publish as ready |
| Budget pressure | Evict eligible entries, expose pressure; never evict active borrow |
| Cancellation | Terminal cancellation; no late publication into destroyed owner |
| Shutdown | Stop admission and resolve in-flight ownership before destruction |
| Stale development artifact | Reject or request explicit rebuild/reload; do not silently overwrite source |

Fallback resources are domain-owned and visible. Render may substitute a known fallback texture or mesh; Audio may choose silence. The result still records the original failure. Fallback must not turn a required startup artifact into reported success without noting degradation.

## Threading and publication

Registry bootstrap is controlled before normal concurrent reads. Streaming queues and request state are synchronized by their owner. Residency mutation is guarded so map, reference count, LRU, and byte accounting change atomically from the manager's point of view.

Worker threads may perform source I/O and neutral decode. They do not mutate ECS, world authority, editor documents, or native graphics state directly. Results are published through handles/queues and consumed on the domain's allowed thread. GPU upload follows the RHI/Render submission policy; gameplay mutation follows the simulation thread policy.

Avoid holding a global resource lock across I/O, decoding, callbacks, or GPU work. The manager records intent/state under a short critical section, performs slow work through the streaming owner, and publishes terminal state safely.

### Locking invariants

Under the residency mutex:

- every map entry owns one consistent refcount and effective key;
- `inLru` is true exactly when the entry has zero borrows and a valid list iterator;
- an entry occurs at most once in the LRU list;
- resident byte totals equal the retained terminal-ready entries accounted by the manager;
- eviction removes map/list/accounting in one critical operation;
- handle copy/release cannot race an entry deletion.

Destructors should not call arbitrary logging, source, or callback behavior while the lock is held. Collect diagnostic data under lock and emit afterward.

## Development hot reload

Hot reload is an explicit development mode. A changed file or artifact produces a new candidate payload. The candidate validates before replacing the active version. Outstanding handles either continue to reference the old immutable entry or receive a documented generation change; they never observe a vector being mutated underneath a reader.

Reload coalesces repeated file notifications and refuses stale results. If generation `N+1` finishes before an older generation `N` request, `N` must not overwrite the newer resident value. Diagnostics record generation/source hash and the reason a result was discarded.

Hot reload does not make the runtime an importer and does not write authoritative source. It is a consumer-side replacement protocol.

## Diagnostics alignment

Streaming diagnostics are useful when they preserve stage and ownership. At minimum, counters and events can cover:

- queue depth by priority and oldest wait age;
- submitted, completed, failed, and cancelled requests;
- source bytes read and decode time where measurable;
- cache hit, miss, reuse-while-pending, and eviction counts;
- resident bytes, budget, active references, and LRU candidates;
- fallback use and top failure identifiers;
- shutdown cancellations and late-result prevention;
- per-kind or per-owner attribution without unbounded labels.

Memory/resource snapshots distinguish generic resident bytes from typed CPU allocations, GPU allocations, and temporary staging. Adding unlike numbers into one “memory used” field is misleading.

## Tests

Deterministic unit tests should cover identity collision, invalid id, registry plan atomicity, VFS path normalization, longest-prefix mounts, missing file, memory source success/failure, priority ordering, cancellation, shutdown, repeated acquire reuse, copy/move/reset reference behavior, active-entry non-eviction, LRU order, budget pressure, failed request retry policy, and concurrent acquisition.

Integration tests join manifest loading, registry bootstrap, VFS/source, streaming, residency, and a typed consumer with small fixtures. Diagnostics tests assert stable identifiers and bounded fields. Render/GPU tests separately prove device upload and lifetime; those are not required to prove the platform-neutral registry contract.

Tests must not depend on sleeps where a barrier, deterministic source, or explicit pump can express progress. A timing-sensitive test can conceal races on a fast machine and fail randomly under load.

Property/invariant tests can generate acquire/release/cancel/complete/evict sequences and assert accounting after every step. Concurrency tests coordinate two misses on the same id, cancellation during source work, shutdown with queued work, and handle destruction during publication. Leak/lifetime tests finish with no active workers and expected registry/residency state.

## Non-claims

This contract does not claim a production CDN, signed remote content delivery, transparent runtime import of all authoring formats, unlimited world-scale streaming, complete mip/geometry virtualization, or zero-stall behavior. It does not claim that the current public surface is final. It records the ownership and correctness rules supported by the current modules and focused evidence.

## Change checklist

Before changing streaming or residency:

- identify whether the change belongs to Assets, Resources, or a typed consumer;
- keep identity, path, source, scheduling, residency, decode, and device lifetime distinct;
- state the effective cache key and retry policy;
- preserve active-handle lifetime and atomic accounting;
- classify every new terminal failure;
- keep slow work outside central locks;
- expose bounded diagnostics;
- add deterministic tests for success, failure, cancellation, concurrency, and shutdown;
- do not revive editor import or old repository ownership inside the runtime.
