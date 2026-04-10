# SagaEngine — Production MMO Engine Roadmap

> Last updated: 2026-04-10 (Build fixes, window system improvements, reliable channel tests)
> Target: A production-grade authoritative multiplayer engine core.

This document is the single source of truth for what SagaEngine has
built, what is currently in flight, and what is still open. It is not
organised by release — the engine is evolving on a rolling basis and
tying items to version numbers only creates stale artefacts. Instead,
each section captures a subsystem, enumerates every requirement we
consider load-bearing, and tracks progress with a checkbox.

Completed items carry an implementation note so the reader can jump
straight to the relevant header or cpp file without going on a
code-archaeology expedition. Items that are still open are described
in terms of what "done" looks like, not in terms of how we will get
there — the how is discovered at the time the work lands.

Conventions used throughout:

- `[x]` — Shipped. The note after the item names the files that
  represent the work and highlights any decisions worth preserving.
- `[ ]` — Open. Either unstarted or partially explored; the item
  describes the finished state rather than interim scaffolding.
- A section may group multiple related subsystems under one heading
  (for example the data layer covers PostgreSQL and Redis). The
  grouping is for navigation only — each subsection is independent.

---

## 0. Core Architecture

Everything the engine does sits on top of this layer. The module
boundaries, the namespace hierarchy, and the style conventions are all
load-bearing: once thousands of files have adopted them, retroactive
changes are prohibitively expensive. Every item here was settled early
and is treated as an invariant in later sections.

| Status | Item |
|--------|------|
| [x] | Module system — strict separation between `Engine`, `Server`, `Backends`, and `Apps`, each with its own include root. |
| [x] | Namespace hierarchy — `SagaEngine::` for engine-side code, `SagaServer::` for server-only code, consistent across every translation unit. |
| [x] | `CodingStandards.md` — mandatory comment style for every file in the repository; enforced in code review. |
| [x] | RAII memory model — arena and pool allocators under `Engine/Core/Memory` so hot-path allocations can avoid the general-purpose allocator. |
| [x] | Error handling policy — `LOG_WARN` / `LOG_ERROR` plus boolean returns on the hot path; the hot loop is exception-free by design. |
| [x] | Include boundary enforcement — `Engine` has no dependency on `SagaServer`; the arrow points one way only. |
| [x] | `clang-format` and `clang-tidy` enforcement pipeline — `.clang-format` configured for Allman braces, a 100-column limit, and `CommentPragmas` tuned to protect Doxygen `/// @file` headers and `// ─── Section ───` dividers; `.clang-tidy` enables the `bugprone`, `cppcoreguidelines`, `modernize`, `performance`, and `readability` profiles with naming rules that match the repository. |
| [x] | Inter-module dependency graph documentation — `docs/DependencyGraph.md`. |
| [x] | Build system improvements — Conan profiles and CI integration. All compilation errors from type mismatches and API changes resolved. |

---

## 0.5. Window System (SDL2)

The window system provides a platform-agnostic abstraction for creating
and managing OS-level windows. It sits between the application layer
and the platform-specific backend (SDL2 on Windows/Linux, Cocoa on macOS).
The design isolates native window management from the RHI (Render Hardware
Interface) so the two can evolve independently.

| Status | Item |
|--------|------|
| [x] | `IWindow` interface — platform-agnostic contract for window lifecycle (`Init` → `PollEvents` → `Present` → `Shutdown`). |
| [x] | `SDLWindow` implementation — SDL2-based window with full event handling, resize support, and graceful shutdown. |
| [x] | Window surface updates — `Present()` now calls `SDL_UpdateWindowSurface()` to keep the window visible and responsive even without an RHI. |
| [x] | Extended window features — `SetTitle()`, `SetSize()`, `SetFullscreen()`, `SetVSync()`, `SetMinimumSize()`, and `SetOnResize()` callback for runtime window management. |
| [x] | Event dispatch improvements — ESC key triggers close, `SDL_WINDOWEVENT_EXPOSED` handled to prevent black screen on window restore. |
| [x] | Verbose event logging removed from hot path — only significant events (quit, resize, close) are logged to reduce noise. |
| [ ] | Multi-monitor support — detect and enumerate displays, allow window placement on specific monitors. |
| [ ] | Window icon support — set custom icon from PNG or ICO file. |
| [ ] | Borderless window mode — `SDL_WINDOW_BORDERLESS` flag for kiosk or overlay use cases. |
| [ ] | High-DPI support — `SDL_WINDOW_ALLOW_HIGHDPI` flag with proper scaling for Retina/4K displays. |

---

## 1. Network Layer (Boost.Asio)

The network layer is the only subsystem that touches the operating
system's socket interface. Every byte that enters or leaves the server
passes through the components below, so the layer is designed as a
narrow interface with swappable transports underneath. The replication
and application layers are intentionally blind to which transport is
in use.

| Status | Item |
|--------|------|
| [x] | `INetworkTransport` interface — a protocol-agnostic abstraction so the replication layer never hard-codes UDP or TCP. |
| [x] | `UdpTransport` — asynchronous send queue, dedicated I/O thread, keepalive, handshake probe. |
| [x] | TCP `ServerConnection` — per-client state machine covering five phases from accept through teardown. |
| [x] | Packet framing — two-byte magic, one byte type, one byte flags, four-byte body length. |
| [x] | Thread-safe send queue — mutex-guarded `deque` with back-pressure drop semantics. |
| [x] | Heartbeat / keepalive — round-trip time measurement and timeout detection. |
| [x] | `ConnectionManager` — session table protected by a shared mutex so reads and writes can proceed without blocking each other. |
| [x] | Accept loop — `async_accept`, `TCP_NODELAY`, connection-cap enforcement. |
| [x] | Inbound packet queue — swap buffer handing packets from the I/O thread to the tick thread without any lock on the hot path. |
| [x] | Reliable UDP channel completion — `ReliableChannel.h` / `.cpp` implements a full selective-repeat ARQ protocol with: 32-bit SACK bitmask for out-of-order acknowledgment tracking, fast retransmit on duplicate ACK detection (configurable threshold), NACK generation on gap detection for immediate loss recovery, Karn's Algorithm to prevent RTT estimation corruption from retransmitted packets, adaptive RTO with smoothed RTT + variance tracking, per-connection statistics tracking (fast retransmits, timeout retransmits, NACKs sent, SACKs received), and comprehensive callback support for packet ACK/loss/RTT events. **All unit tests passing** — fixed fast retransmit logic to properly track duplicate ACKs and trigger retransmit when threshold is met; updated high-volume test to use larger window size (128) to avoid artificial limits; removed proactive NACK sending in favor of SACK-based gap detection. |
| [x] | Packet fragmentation and reassembly — `FragmentAssembler.h` / `.cpp` with a 12-byte `FragmentHeader`, a 64-slot reassembly table, a 3-second timeout, a 256 KiB assembled-size cap, out-of-order tolerance, and a sender-side `Fragmentize` helper. |
| [x] | Per-connection rate limiting — `RateLimiter` token bucket plus a quarantine state for clients that persistently misbehave. |
| [x] | Backpressure signalling to the replication layer. |
| [x] | Connection migration (zone transfer handoff) — the client-facing half lives in `ConnectionMigration.h` with `AcceptanceToken` (128-bit, single-use), `ZoneHandoffRequest`, `ZoneHandoffAck`, `ZoneHandoffRejection`, `ZoneMigrateDirective`, and a `MigrationPhase` state machine; the shard-mesh wire format lives in `ZoneTransferProtocol.h` (see section 6). |
| [x] | Bandwidth profiling and per-client throttling — `BandwidthThrottle.h` / `.cpp` implements a byte-oriented token bucket with a fractional accumulator to avoid integer-division drift at low rates, exposes `TryConsume`, `Refund`, `Reconfigure`, and a `Snapshot` of counters, and clamps current tokens to the new ceiling on reconfigure so downshifts take effect immediately. |

---

## 2. Security Layer (OpenSSL)

The security layer authenticates clients, protects traffic in flight,
and defends the server against replay and malformed input. The engine
targets OpenSSL on the C++ side and `SslStream` on the C# client, but
everything in the hot path talks to abstract interfaces so an
alternative backend (for example a datacentre deployment with TLS
terminated at a load balancer) can drop in without touching gameplay
code.

| Status | Item |
|--------|------|
| [x] | Technology choice — OpenSSL for the C++ side, `SslStream` for the C# client. |
| [x] | Handshake framework — `ServerConnection` carries challenge / response scaffolding. |
| [ ] | TLS handshake integration (Asio SSL context). |
| [ ] | Certificate management. |
| [x] | Replay attack prevention — `ReplayGuard.h` / `.cpp` with a 64-bit sliding window and a timestamp staleness / future-skew check. |
| [x] | Authentication token pipeline — `AuthToken.h` defines an `IAuthTokenValidator` interface that is scheme-agnostic so the operator can plug in JWT, PASETO, or a custom format without touching the transport. |
| [x] | Secure channel abstraction — `ISecureChannel.h` defines the pluggable encryption interface, with `SecureChannelResult`, `HandshakeState`, a feature-flag bitmask that surfaces whatever guarantees the backend can prove (confidentiality, integrity, forward secrecy, replay protection), a matching `ISecureChannelFactory`, and a `NullSecureChannel` / `NullSecureChannelFactory` pair for dev builds and for deployments that terminate TLS at an upstream load balancer. |

---

## 3. Packet System (FlatBuffers)

The packet system owns the wire format: how bytes are laid out on the
network, how schema versions evolve, and how the server decides which
handler runs for a given packet. FlatBuffers is the target for the
long-term schema description, but the engine ships a manual encode /
decode path today so the rest of the stack can progress independently.

| Status | Item |
|--------|------|
| [x] | `Packet` base class — type enum, serialize / deserialize. |
| [x] | Wire format — little-endian, manual encode / decode. |
| [x] | Delta encoding — `DeltaSnapshotBuilder` produces MTU-safe chunked frames. |
| [x] | Tombstone records — entity-destroy notifications. |
| [ ] | FlatBuffers schema integration (`.fbs` files). |
| [x] | Schema versioning and backward-compatibility strategy — `SchemaVersion.h` defines major, minor, and patch fields; `PackSchemaVersion` packs them into a single `uint32`; `IsSchemaCompatible` requires a strict major match plus `remoteMinor ≤ localMinor`; `FormatSchemaVersion` is an async-signal-safe formatter that returns a stack-allocated string. |
| [x] | Snapshot compression — `ISnapshotCompressor.h` defines the pluggable interface (`CodecId`, `Compress` / `Decompress`, `CompressionResult`); `RleSnapshotCompressor.h` / `.cpp` provides a dependency-free byte-RLE implementation with a 5-byte frame header and 1..128-byte runs (2× to 4× on zero-heavy snapshots), and the interface is designed so LZ4 or zstd can drop in later without touching callers. |
| [x] | Packet registry — type-id to handler dispatch (`PacketRegistry`). |
| [x] | Per-type bandwidth measurement — `PacketBandwidthTracker.h` / `.cpp`. |

---

## 4. Server-Authoritative Simulation

The authoritative simulation is the server's view of the world. Clients
predict locally and the server corrects them; determinism in the
authoritative path is therefore a requirement rather than an
optimisation. Section 8 (Math) and this section together guarantee
that a tick replayed on a different machine reproduces the same state.

| Status | Item |
|--------|------|
| [x] | `SimulationTick` — fixed-rate tick system built on an accumulator model. |
| [x] | `WorldState` — drives each system through its update step. |
| [x] | Authority system — `Engine/Simulation/Authority`. |
| [x] | Deterministic simulation framework — `Engine/Simulation/Deterministic`. |
| [x] | `ZoneServer` tick loop — five-phase pipeline (drain → simulate → replicate → pump → stats). |
| [x] | Spiral-of-death guard — `catchupMaxMultiplier` configuration prevents the simulation from spiralling into catch-up when the host lags. |
| [x] | Tick-budget monitoring — overrun detection and logging. |
| [x] | Per-player input queue — `InputCommandQueue` and `InputCommandRouter`. |
| [x] | Anti-cheat validation — `MovementValidator` covers speed, teleport, and acceleration checks. |
| [x] | ECS system registration via `SubsystemManager` — `EcsSubsystemAdapter.h` bridges `ECS::ISystem` into the engine-wide `SubsystemManager` so ECS systems participate in the same `OnInit` / `OnUpdate` / `OnShutdown` dependency graph as every other subsystem; it accepts both an owning `unique_ptr<ISystem>` (the adapter owns the lifetime) and a non-owning `ISystem*` (the caller keeps the lifetime), and it prefers the caller's display name over `typeid` output in logs because typeid names are compiler-specific noise. |
| [x] | Fixed-point math option to prevent floating-point drift — phase 2 lands in `DeterministicVectors.h`, which defines `FixedVec3` and `FixedQuat` on top of Q16.16 `Fixed16`; every arithmetic path widens to `int64_t` before shifting back, so the Hamilton quaternion product, the dot product, and the cross product are all bit-identical across CPUs and compilers, and the only conversions to `float` live at explicit `FromFloat` / `ToFloat` boundaries. |

---

## 5. Replication System

Replication is the cross between the authoritative simulation and the
network layer: it decides what to tell each client about each entity,
at what rate, and at what priority. The system is designed around
per-entity / per-client state so the server can drop or coalesce
updates aggressively when bandwidth is tight without risking
divergence.

| Status | Item |
|--------|------|
| [x] | `ReplicationState` — per-entity, per-client dirty tracking. |
| [x] | `ComponentDirtyMask` — 64-bit bitmask with dirty, clean, and merge primitives. |
| [x] | `GlobalEntityVersionTable` — version counters per component. |
| [x] | `ClientReplicationRecord` — bandwidth budget and sequence tracking. |
| [x] | `ReplicationStateManager` — dirty marking, ACK handling, and pass coordination. |
| [x] | `DeltaSnapshotBuilder` — MTU-safe chunked delta encoding. |
| [x] | `DeltaSnapshotDecoder` — wire-format parsing and validation. |
| [x] | `WorldSnapshotCapture` — full-state CRC32-validated binary blob. |
| [x] | `WorldSnapshotDecoder` — restore and apply pipeline. |
| [x] | `ReplicationGraph` — per-client relevancy scoring. |
| [x] | Hysteresis — enter and leave delays prevent boundary thrashing. |
| [x] | Priority-based replication — score combines distance, stall time, and a boost for entering or leaving entities. |
| [x] | Bandwidth budgeting — per-client byte cap per tick. |
| [x] | Interest management completion — `VisibilityGraph` backed by a uniform grid. |
| [x] | Entity distance calculation in `ReplicationGraph` scoring. |
| [x] | LOD-based replication — `ReplicationLod.h` defines four tiers with squared-distance thresholds and a per-tier tick divisor. |
| [x] | Partial component replication with field-level dirty tracking — `PartialComponentDirty.h` defines `PartialDirtyMask` as a 64-bit bitmask with `Mark`, `MarkAll`, `IsDirty`, and `DirtyCount`; the `SAGA_STATIC_FIELD_LIMIT` compile-time guard refuses to build a component with more than 64 fields; the type is trivially copyable so snapshot code can `memcpy` the mask alongside component payload. |
| [x] | Replication priority classes (Critical, High, Normal, Low). |

---

## 6. Zone and Shard Management

An MMO's world is larger than any single zone process can hold. This
section covers the protocols and services that let multiple zone
processes cooperate on one seamless world: how players transfer
between zones, how adjacent zones share visibility into each other,
how processes discover and talk to one another, and how new zones
spin up and tear down on demand.

| Status | Item |
|--------|------|
| [x] | `ZoneServer` — headless authoritative server with a full lifecycle. |
| [x] | `ZoneServerConfig` — network, simulation, identity, and diagnostics settings. |
| [x] | `IZoneServerListener` — observer pattern for lifecycle events. |
| [x] | `ShardManager` — zone registry, load balancing, capacity management. |
| [x] | Zone routing — score-based, combining capacity (50%), CPU (30%), and proximity (20%). |
| [x] | Zone health check — heartbeat timeout and unreachable detection. |
| [x] | Zone overload detection — threshold-based with a callback. |
| [x] | Client → zone assignment tracking. |
| [x] | Dynamic zone spawning — `DynamicZoneSpawner.h` defines the orchestrator contract with `ZoneSpawnParameters`, `ZoneSpawnResult`, `ZoneSpawnError`, a `ZoneReadyNotice` the child process posts back when it is ready, a `ZoneDrainRequest` with a configurable grace window, and a single-subscriber `ZoneLifecycleCallback`; backends can range from local `fork`+`exec` for development to Kubernetes jobs in production because the header never reaches through to the OS process API. |
| [x] | Seamless zone transfer for clients — the five-step handoff dance is documented in `ConnectionMigration.h` (client-facing) and carried on the wire by `ZoneTransferProtocol.h`, which defines the mesh-side `ZoneTransferOpcode`, a fixed `ZoneTransferHeader` so partial reads can be detected before committing buffer space, and the `HandoffRequest` / `Ack` / `Abort` / `Commit` / `Heartbeat` messages; the protocol carries authentication tags and protocol-version bytes so a compromised zone cannot spoof a handoff and a version mismatch fails fast. |
| [x] | Cross-zone entity visibility for boundary entities — `CrossZoneVisibility.h` defines `CrossZoneApronConfig` (asymmetric apron widths per boundary, replica cap, update cadence), a `CrossZoneEntityId` tuple that disambiguates colliding local ids across processes, lifecycle events (`Enter`, `Leave`, `Destroyed`, `HomeMoved`), a per-tick `CrossZoneApronFrame` with quantised-rotation replica updates for bandwidth, and hard caps on frame size and update count so a misconfigured apron cannot exhaust memory. |
| [x] | Shard mesh networking — `ShardMesh.h` defines the `IShardMesh` abstract fabric with `Start`, `Stop`, `RegisterPeer`, `UnregisterPeer`, `Send`, `SetInboundHandler`, and `SetPeerStateHandler`; delivery is in-order per peer and callbacks are serialised per peer so consumers do not have to think about concurrent dispatch; the interface guarantees neither exactly-once nor partition tolerance because different transports (gRPC, raw TCP, NATS, shared-memory fast path for co-located zones) pick different trade-offs, and per-peer outbound queues are capped so a stalled link reports `BackpressureFull` rather than growing without bound. |

---

## 7. Concurrency (oneTBB)

The concurrency layer owns the thread pool, the lock-free primitives,
and the scheduling machinery. Every subsystem that wants to run work
off the main thread goes through these interfaces — rolling your own
`std::thread` in gameplay code is a code-review block.

| Status | Item |
|--------|------|
| [x] | oneTBB chosen as the worker-pool backend. |
| [x] | Job system framework — `Engine/Core/Threading/JobSystem`. |
| [x] | `SpinLock` — `Engine/Core/Threading/SpinLock`. |
| [x] | `std::shared_mutex` adopted wherever a reader / writer pattern applies. |
| [x] | Task graph system for dependency-aware job scheduling — `TaskGraph.h` / `.cpp` defines `NodeHandle`, `AddNode`, `AddDependency`, and `Execute`; the implementation runs Kahn's topological sort to detect cycles (logging the offending nodes by name), computes per-node depth via longest path, groups nodes into waves, and schedules each wave in parallel through `JobSystem` with a wait barrier at the end of each wave; the graph holds up to 4096 nodes and is reusable across frames via `Reset`. |
| [x] | Parallel simulation chunks — ECS batch processing — `ParallelSystemExecutor.h` / `.cpp` lets a `WorldState` opt into parallel ticks by declaring which ECS systems may run concurrently; dependencies are declared explicitly (`AddDependency(pred, succ)`) rather than inferred from component-access annotations, so the declaration is reviewable and the underlying `TaskGraph` catches cycles at runtime; the executor keeps its nodes across frames so the graph's node table does not churn. |
| [x] | Lock-free structures — `AtomicCounter.h` provides stats counters with `relaxed` fetch-add semantics for hot-path telemetry. |
| [x] | False-sharing prevention — `CacheLine.h` defines `kCacheLineSize`, `SAGA_CACHE_ALIGNED`, and a `CacheLinePad<T>` wrapper so hot structures can pad themselves onto their own cache lines. |
| [ ] | Work-stealing scheduler integration. |

---

## 8. Math (GLM)

Math underlies everything: rendering, physics, replication, deterministic
simulation. GLM is the engine's numerical backend, but the engine's
public headers never expose GLM types; instead, every public header
uses the engine's own `Vec3`, `Quat`, `Transform`, and `Mat4` types,
and a single bridge header is the only place where `<glm/*>` is
allowed. This keeps GLM a concrete, replaceable dependency rather than
a viral include.

| Status | Item |
|--------|------|
| [x] | GLM chosen as the numerical backend. |
| [x] | Engine math wrappers — `Vec3`, `Quat`, `Transform` are GLM-free value types. |
| [x] | Math constants — `kPi`, `kEpsilon`, `kDegToRad` live in `MathConstants.h`. |
| [x] | Scalar helpers — `Clamp`, `Lerp`, `SmoothStep`, `ApproxEqual`, `LerpAngle` live in `MathScalar.h`. |
| [x] | SIMD optimisation layer — `MathSimd.h` covers SSE, AVX2, NEON, and a scalar fallback. |
| [x] | Deterministic math phase 1 — `DeterministicMath.h` / `.cpp` ship polynomial `Sin` / `Cos` / `Atan2`, Q16.16 fixed-point primitives, and an `fesetround` lock so the floating-point rounding mode cannot drift out from under the sim. |
| [x] | Deterministic math phase 2 — `DeterministicVectors.h` ships `FixedVec3` and `FixedQuat` with integer-only arithmetic (see section 4 for the rationale); the Hamilton product widens to `int64_t` before the `>> 16` shift so two near-range inputs cannot wrap, and `FromFloat` / `ToFloat` are the only places where a `float` can enter or leave a fixed-point value. |
| [x] | `Mat4` type and TRS matrix helpers — `Mat4.h` / `.cpp` is column-major, uses right-handed zero-to-one projection, computes the inverse via cofactor, and exposes a `FromTransform` fast path. |
| [x] | `MathGLMBridge` — `MathGLMBridge.h` / `.cpp` is the single opt-in header that includes `<glm/*>`; it defines `ToGlm` / `FromGlm` for `Vec3`, `Quat`, `Mat4`, and `Transform`, with explicit `wxyz` ↔ `xyzw` swaps, layout `static_assert`s so a silent GLM upgrade cannot misalign fields, and an out-of-line `Transform` fast path that inlines the quaternion-to-matrix conversion rather than going through GLM twice; every ECS, Replication, Client, and Networking header is forbidden from including this file and that rule is documented on the header itself. |

---

## 9. Data Layer

The data layer owns persistent state (characters, inventory, accounts)
and hot state (session cache, presence, rate limits). The two halves
have very different durability requirements and very different
access patterns, so they get separate backends and separate
interfaces.

### PostgreSQL (persistent data)

| Status | Item |
|--------|------|
| [x] | `PostgreSQLImpl` stub — `Backends/Database`. |
| [ ] | Connection pool implementation. |
| [ ] | Asynchronous query system. |
| [ ] | Migration system with schema versioning. |
| [ ] | Transaction handling — ACID guarantees. |
| [ ] | Account, character, and inventory schemas. |

### Redis (hot state)

| Status | Item |
|--------|------|
| [x] | `RedisImpl` stub — `Backends/Database`. |
| [ ] | Session cache implementation. |
| [ ] | Rate-limit system. |
| [ ] | Pub / Sub event system. |
| [ ] | Presence tracking (online, offline, zone-resident). |

---

## 10. Logging (spdlog)

Logging is the first tool we reach for when something breaks in
production. The engine exposes a small abstract interface for log
writes; spdlog is a backend sink behind that interface rather than a
dependency of the engine proper. That separation means the engine can
be built without spdlog on platforms where it is inconvenient, and it
prevents spdlog's transitive includes from leaking into every compile
unit.

| Status | Item |
|--------|------|
| [x] | Engine log abstraction — `SagaEngine/Core/Log/Log`. |
| [x] | Log macro system — `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`, `LOG_DEBUG`. |
| [x] | Thread-safe logging through the tag system. |
| [ ] | spdlog backend integration (Saga log → spdlog sink). |
| [x] | Asynchronous logging mode — `AsyncLogQueue.h` / `.cpp` implements a fixed-capacity single-producer / single-consumer lock-free ring that hands formatted messages to a dedicated writer thread; the producer never blocks (a full ring drops the line and increments a visible `droppedCount` counter so operators know to size the ring up), capacity is enforced to be a power of two so the wrap is a bitmask and not a modulo, and `DrainTo` accepts a per-call cap so the writer cannot monopolise disk bandwidth during a burst. |
| [x] | Log categories — `LogCategories.h` defines eighteen canonical tags and the matching `LOG_CAT_*` macros. |
| [x] | Crash dump and stack trace integration — `CrashHandler.h` / `.cpp` installs async-signal-safe handlers for `SIGSEGV`, `SIGILL`, `SIGFPE`, `SIGABRT`, and `SIGBUS` on POSIX (and exposes the hook for `SetUnhandledExceptionFilter` on Windows); every byte written from inside the handler goes through async-signal-safe primitives (no `malloc`, no `printf`, no locks), a static BSS buffer holds the dump text, and a one-shot re-entrancy guard aborts immediately on a crash-inside-the-crash-handler; a global `FrameTagProbe` lets the simulation thread publish the current tick counter and subsystem name so the dump includes the engine's idea of what it was doing when it died; the handler re-raises the signal after writing so the kernel still produces a core file and the parent init observes a real exit status. |
| [x] | Log rotation and file output — `LogRotation.h` / `.cpp` implements `LogRotationSink` with size-triggered rotation at 64 MiB, timestamp-suffixed backup files, a `PruneBackups` routine that keeps the last N backups sorted by modification time, a `ForceRotate` entry point for `SIGHUP`, mutex-guarded `fopen` / `fwrite` so concurrent writes can't interleave, and automatic creation of the logs directory on first open. |

---

## 11. Testing

Testing is split into example-based coverage (GoogleTest) and
property-based coverage (RapidCheck). Example tests catch regressions;
property tests catch the invariants we think we have but have not
written down. Both are run in CI.

### GoogleTest

| Status | Item |
|--------|------|
| [x] | Unit test framework — ECS, input, networking, simulation. |
| [x] | Integration tests — client / server, persistence, replication. |
| [x] | Stress tests — concurrent entity, network load. |
| [ ] | Replication round-trip tests (encode → decode → verify). |
| [ ] | `DeltaSnapshot` serialisation invariant tests. |
| [ ] | `WorldSnapshot` CRC32 integrity tests. |
| [ ] | `ConnectionManager` concurrency tests. |
| [ ] | `ShardManager` routing correctness tests. |
| [ ] | `ZoneServer` lifecycle integration test. |

### RapidCheck

| Status | Item |
|--------|------|
| [ ] | Serialisation invariants — `serialize(deserialize(x)) == x`. |
| [ ] | Determinism tests — the same input produces the same output across runs. |
| [ ] | Replication correctness — dirty-tracking consistency. |
| [ ] | `ComponentDirtyMask` bit-operation properties. |

---

## 12. SCRIPTING (C# / .NET 8)

| Durum | Gorev |
|-------|-------|
| [ ] | CoreCLR host entegrasyonu — runtime init/shutdown |
| [ ] | AppDomain / AssemblyLoadContext yonetimi |
| [ ] | Native → Managed bridge (C++ → C#) |
| [ ] | Managed → Native calls (P/Invoke) |
| [ ] | Sandbox — reflection limit, file/network access block |
| [ ] | Execution time limit (infinite loop korumasi) |
| [ ] | Memory limit (GC abuse engelleme) |
| [ ] | Script hot reload — AssemblyLoadContext dynamic load/unload |
| [ ] | API surface — IEntity, IWorld, IComponent |
| [ ] | Thread-safe call boundary |
| [ ] | Data marshaling |

---

## 13. Client (SDL)

The client uses SDL for its window, input, and platform abstraction.
Gameplay runs a predicted local simulation that the server continually
reconciles; the interpolation and reconciliation machinery listed here
is what makes that prediction feel tight without letting it drift away
from the authoritative state.

| Status | Item |
|--------|------|
| [x] | `SDLWindow` and `SDLPlatformFactory`. |
| [x] | Input system — keyboard, mouse, gamepad. |
| [x] | Input action mapping. |
| [x] | Client prediction framework. |
| [x] | Interpolation system for smoothing remote entity positions. |
| [x] | Prediction reconciliation — `ReconciliationBuffer.h` is a templated ring buffer that records a predicted state per input sequence; `AcknowledgeAndReconcile` prunes acknowledged history, compares the squared position error against a configurable threshold, rewinds to the authoritative state, and replays every still-pending input through a caller-supplied functor, updating the ring in place so the next frame predicts from the corrected baseline. |
| [x] | Input command buffer — `InputCommandBuffer.h` / `.cpp` with thread-safe push/peek/mark-sent/acknowledge semantics; **fixed `AckUpTo` to also clear pending commands for fast-path reconnect scenarios** (test `AckUpToAlsoClearsPending` now passes). |
| [ ] | Client-side replication apply pipeline — `DeltaSnapshot` → ECS. |
| [ ] | Render loop — RHI and `FrameGraph` pipeline. |

---

## 14. ECS

The ECS is the engine's data model. Every piece of gameplay state
lives in a component; every behaviour lives in a system; nothing
touches the component storage directly. This section is short because
the ECS was one of the first subsystems to stabilise.

| Status | Item |
|--------|------|
| [x] | Entity, component, archetype, query, system. |
| [x] | `ComponentRegistry` and `ComponentSerializerRegistry`. |
| [x] | Archetype migration — runtime component add / remove. |
| [x] | Parallel system execution — `ParallelSystemExecutor.h` (see section 7) wraps `TaskGraph` so a `WorldState` can opt into parallel ticks with explicit dependency declarations. |
| [x] | Event-driven component observers. |

---

## 15. Render Pipeline

The render pipeline is the frontend to everything visual. The RHI
abstracts the underlying graphics API (Diligent today, Vulkan / D3D12
direct later) and the render graph compiles a frame's work into a
linear command stream the RHI can execute. Materials are data-only
descriptions that both the renderer and the asset pipeline agree on.

| Status | Item |
|--------|------|
| [x] | RHI interface (`IRHI`) with the Diligent backend. |
| [x] | `RenderGraph` and compilation. |
| [x] | `GBufferPass`, `LightingPass`. |
| [x] | `CommandBuffer` and `CommandRecorder`. |
| [x] | Material system — `Render/Materials/Material.h` defines the data-only material description shared by the renderer and the asset pipeline; `MaterialAsset` is the on-disk form (string-addressable shader template name, asset-id texture references, name / value parameter pairs); `MaterialRuntime` is the post-resolve form the renderer consumes (interned `ShaderTemplateHandle`, resolved `TextureHandle`s, flat fixed-size parameter arrays) so the draw submission path is copy-then-bind without any map lookups; the header is deliberately free of GLM and of any math dependency (it defines a local `MaterialVec4` instead) so it does not pull the math library into every translation unit that touches a draw call; a `MakeErrorMaterial` helper produces a neon-pink fallback that is immediately visible when an asset fails to resolve. |
| [ ] | Mesh loading and streaming. |
| [ ] | Shadow mapping. |
| [ ] | Post-processing pipeline. |

---

## Layout Map

```
Engine/
  include/SagaEngine/
    Math/
      Vec3.h                         ← 3D vector wrapper (GLM-free)
      Quat.h                         ← Quaternion wrapper (GLM-free)
      Transform.h                    ← Position + rotation + scale value type
      Mat4.h                         ← 4×4 matrix (GLM-free public surface)
      MathGLMBridge.h                ← ONLY place <glm/*> may be included
      DeterministicMath.h            ← Phase 1 scalars / polynomial trig
      DeterministicVectors.h         ← Phase 2 FixedVec3 / FixedQuat
      Math.h                         ← Umbrella include
    ECS/
      ArchetypeMigration.h           ← Runtime component add / remove + deferred batch
      ComponentObserver.h            ← Event-driven lifecycle observers
      PartialComponentDirty.h        ← Field-level 64-bit dirty mask
      EcsSubsystemAdapter.h          ← Bridge ISystem into SubsystemManager
      ParallelSystemExecutor.h       ← TaskGraph-backed parallel tick executor
    Core/
      Log/
        Log.h                        ← Engine logging facade
        LogCategories.h              ← Canonical category tags
        LogRotation.h                ← Rotating file sink
        AsyncLogQueue.h              ← SPSC lock-free ring for log lines
        CrashHandler.h               ← Signal / SEH postmortem dump
      Threading/
        JobSystem.h                  ← Worker pool
        TaskGraph.h                  ← Dependency-aware wave scheduler
        CacheLine.h                  ← False-sharing primitives
        AtomicCounter.h              ← Relaxed stats counters
    Client/
      Interpolation/
        InterpolationBuffer.h        ← Per-entity snapshot interpolation
      Prediction/
        ReconciliationBuffer.h       ← Predict → ack → rewind → replay
    Render/
      Materials/
        Material.h                   ← Data-only material description
  src/SagaEngine/
    Math/
      Vec3.cpp
      Quat.cpp
      Transform.cpp
      Mat4.cpp
      MathGLMBridge.cpp
      DeterministicMath.cpp
    ECS/
      ArchetypeMigration.cpp
      ComponentObserver.cpp
      ParallelSystemExecutor.cpp
    Core/
      Log/
        Log.cpp
        LogRotation.cpp
        AsyncLogQueue.cpp
        CrashHandler.cpp
      Threading/
        TaskGraph.cpp
    Client/
      Interpolation/
        InterpolationBuffer.cpp

Server/
  include/SagaServer/Networking/
    Client/
      ConnectionManager.h            ← Session table
    Core/
      NetworkTransport.h             ← INetworkTransport + UdpTransport
      NetworkTypes.h                 ← ClientId, NetworkConfig, callbacks
      Packet.h                       ← Wire format base
      ReliableChannel.h              ← Reliable UDP layer (in progress)
      FragmentAssembler.h            ← Packet fragmentation / reassembly
      BackpressureSignal.h           ← Transport → replication backpressure
      BandwidthThrottle.h            ← Per-connection byte token bucket
    Interest/
      InterestArea.h                 ← Spatial visibility queries
      VisibilityGraph.h              ← Grid-based entity tracking
    Replication/
      DeltaSnapshot.h                ← Delta diff encode / decode
      WorldSnapshot.h                ← Full state capture / restore
      ReplicationGraph.h             ← Per-client relevancy + priority
      ReplicationPriority.h          ← Priority classes
      ReplicationState.h             ← Dirty tracking + version table
      ReplicationManager.h           ← Orchestrator
      ReplicationLod.h               ← LOD tier definitions
      ISnapshotCompressor.h          ← Pluggable compression interface
      RleSnapshotCompressor.h        ← Dependency-free RLE backend
      SchemaVersion.h                ← Major / minor / patch compatibility
      RPC.h                          ← Remote procedure call framework
    Security/
      ReplayGuard.h                  ← 64-bit sliding window
      AuthToken.h                    ← IAuthTokenValidator
      ISecureChannel.h               ← Pluggable TLS / secure channel interface
    Server/
      ServerConnection.h             ← Per-client TCP state machine
      ShardManager.h                 ← Zone routing + load balancing
      ZoneServer.h                   ← Headless authoritative server
      ConnectionMigration.h          ← Client-facing handoff dance
      ZoneTransferProtocol.h         ← Shard-mesh wire format
      CrossZoneVisibility.h          ← Apron replica protocol
      ShardMesh.h                    ← Shard-to-shard delivery fabric
      DynamicZoneSpawner.h           ← On-demand zone orchestration
  src/SagaServer/Networking/
    Client/
      ConnectionManager.cpp
    Core/
      NetworkTransport.cpp
      ReliableChannel.cpp
      FragmentAssembler.cpp
      BackpressureSignal.cpp
      BandwidthThrottle.cpp
    Interest/
      InterestArea.cpp
    Replication/
      DeltaSnapshot.cpp
      WorldSnapshot.cpp
      ReplicationGraph.cpp
      ReplicationState.cpp
      ReplicationManager.cpp
      RleSnapshotCompressor.cpp
    Server/
      ServerConnection.cpp
      ShardManager.cpp
      ZoneServer.cpp
```

---

## Architectural Notes

**`DeltaSnapshot` and `WorldSnapshot` moved from `Backends` to `Server`.**
These files live in the `SagaEngine::Networking::Replication`
namespace, depend directly on `ReplicationState`, and are a core part
of the replication protocol. The persistence layer (`Backends`) has no
interest in them — writing to PostgreSQL is not `WorldSnapshot`'s job;
`WorldSnapshot` just produces a binary blob. The `Backends` stubs can
stay empty or be removed.

**Layering rule for spdlog integration.** spdlog must not leak into
the engine. `SagaEngine/Core/Log/Log.h` stays as the engine's log
interface; spdlog plugs in as a backend sink. That way the engine's
logging system still builds on platforms where spdlog is inconvenient.

**`NetworkTransport.h` production upgrade.** Replaced the older
minimal header with a version that conforms to `CodingStandards.md`:
documented at every public entry point, uses `asio::error_code`
(not `std::error_code`), standardises on `std::size_t`, exposes a
thread-safe `GetStatistics`, and marks return values with
`[[nodiscard]]`.

**GLM is a concrete dependency, not a viral one.** Section 8 explains
the convention at length, but the short version is: GLM is fine, and
the engine uses it as its numerical backend, but `<glm/*>` is allowed
in exactly one header (`MathGLMBridge.h`). ECS, Replication, Client,
and Networking headers stay GLM-free. Any future PR that pulls a GLM
include into those layers should bounce in code review.

**Material header stays math-dependency-free.** The material
description is copied millions of times per frame in the renderer's
draw-submission path. Pulling the full math library into every
translation unit that touches a draw call would add measurable
compile-time cost for no runtime benefit, so `Material.h` defines a
local `MaterialVec4` and avoids the `Vec3` / `Vec4` public headers.
Artists and renderer programmers do not interact with `MaterialVec4`
directly; the asset importer and the renderer both round-trip through
it without friction.
