# SagaEngine — Module Dependency Graph

> Son guncelleme: 2026-04-09
> Amac: Modüller arasindaki include yonu ve runtime bagimliliklarini tek sayfada tutmak. Yeni bir modul eklerken bu dokumani guncellemek zorunludur.

---

## 1. High-level layers

```
                ┌─────────────────────────┐
                │        Apps             │   (game clients, tools, editor)
                └────────────┬────────────┘
                             │ uses
        ┌────────────────────┴────────────────────┐
        │                                         │
┌───────┴────────┐                       ┌────────┴────────┐
│    Server      │                       │     Engine      │
│  (SagaServer)  │  ────── uses ──────►  │   (SagaEngine)  │
└───────┬────────┘                       └────────┬────────┘
        │                                         │
        │                                         │
        │           ┌─────────────────┐           │
        └─── uses ─►│    Backends     │◄─ uses ───┘
                    │ (DB, Redis, FS) │
                    └─────────────────┘
```

**Hard rule:** `Engine/` MUST NOT include from `Server/` or `Apps/`. Include direction is strictly one-way: Apps → Server → Engine → Backends.

---

## 2. Engine — internal module graph

| Module             | Depends on                                          | Notes |
|--------------------|-----------------------------------------------------|-------|
| Core/Log           | (none)                                              | Foundation — no other engine headers. |
| Core/Memory        | Core/Log                                            | RAII + arena/pool allocators. |
| Core/Threading     | Core/Log, Core/Memory                               | JobSystem, SpinLock, AtomicCounter, CacheLine. |
| Core/Time          | (none)                                              | Tick clock, monotonic time source. |
| Core/Events        | Core/Memory                                         | Event bus, type-erased. |
| Core/Profiling     | Core/Time, Core/Log                                 | Frame timings, span markers. |
| Core/Subsystem     | Core/Log, Core/Memory                               | Subsystem manager / lifecycle. |
| Math               | (none — GLM-free public API)                        | Vec3, Quat, Transform, Mat4, MathConstants, MathScalar, MathSimd, DeterministicMath. |
| Platform           | Core/Log                                            | Window, input abstraction, file system. |
| Input              | Platform, Core/Events                               | Action mapping on top of platform input. |
| ECS                | Core/Memory, Core/Events, Math                      | Entity/Component/Archetype, observers. |
| Simulation         | ECS, Math, Core/Time, Core/Threading                | Tick driver, authority, deterministic guards. |
| Physics            | ECS, Math                                           | Currently stub, no third-party engine. |
| Client             | ECS, Input, Platform, Math                          | Prediction, interpolation buffer. |
| RHI                | Core/Log, Math                                      | Render hardware interface (Diligent backend). |
| Render             | RHI, ECS, Math, Resources                           | Render graph, GBuffer, lighting. |
| Resources          | Core/Memory, Core/Log                               | Asset loading, streaming. |
| Audio              | Core/Log, Math                                      | Audio mixer, currently minimal. |
| Scripting          | Core/Log                                            | C# host bridge (planned). |
| World              | ECS, Math                                           | Scene graph, spatial queries. |

### Forbidden edges (compile-time invariants)

- `Engine/Math/*` cannot include anything from `Engine/Core/*` except indirectly through `<cmath>` / `<cstdint>`.
  - Reason: Math is the foundation used by Core/Events payloads; a cycle breaks bootstrap.
- `Engine/ECS/*` cannot include `Engine/Render/*` or `Engine/RHI/*`.
  - Reason: Headless server link must not drag GPU headers.
- `Engine/Core/Log` cannot include `Engine/Core/Threading`.
  - Reason: Logging is used from inside the threading primitives' error paths.

---

## 3. Server — internal module graph

| Module                            | Depends on                                              | Notes |
|-----------------------------------|---------------------------------------------------------|-------|
| Networking/Core                   | Engine/Core/Log, Engine/Core/Threading                  | Transport, Packet, RateLimiter, PacketBandwidthTracker. |
| Networking/Client                 | Networking/Core                                         | ConnectionManager, session table. |
| Networking/Security               | Networking/Core, Engine/Core/Log                        | ReplayGuard, AuthToken interface. |
| Networking/Interest               | Engine/ECS, Engine/Math                                 | InterestArea, VisibilityGraph. |
| Networking/Replication            | Engine/ECS, Networking/Core, Networking/Interest        | DeltaSnapshot, WorldSnapshot, ReplicationGraph, ReplicationLod, ReplicationPriority, ReplicationManager, RPC. |
| Server/ZoneServer                 | Engine/Simulation, Networking/* (all)                   | 5-phase tick pipeline. |
| Server/ShardManager               | Server/ZoneServer                                       | Routing, load balancing, health. |
| Server/ServerConnection           | Networking/Core, Networking/Security                    | Per-client TCP state machine. |

### Forbidden edges

- `Networking/Core` cannot include anything from `Networking/Replication`.
  - Reason: Replication is a consumer of transport; a reverse dependency creates a cycle and blurs the protocol/payload boundary.
- `Networking/Security` cannot include `Networking/Replication`.
  - Reason: Auth must be evaluable before any replication code is loaded.
- Nothing in `Server/` may include `Apps/`.

---

## 4. Backends

```
Backends/
  Database/
    PostgreSQLImpl   ← uses libpq (external)
    RedisImpl        ← uses hiredis (external)
```

- Backends are leaf modules. They include `Engine/Core/Log` and nothing else from Engine.
- Server uses Backends through abstract interfaces defined in `Server/` headers — Backends never include `Server/`.
- WorldSnapshot and DeltaSnapshot **do not** depend on Backends. They produce byte blobs; persistence is a separate concern that writes those blobs.

---

## 5. Third-party dependency map

| Library           | Used by (modules)                                  | Boundary |
|-------------------|----------------------------------------------------|----------|
| Boost.Asio        | Networking/Core (UdpTransport, ServerConnection)   | Confined to .cpp — headers only forward-declare. |
| OpenSSL           | Networking/Security (planned TLS backend)          | Confined to one backend TU. |
| GLM               | Engine/Math (bridge .cpp only)                     | Never in public headers. |
| spdlog            | Engine/Core/Log (planned backend sink)             | Plugged in as a sink, engine API unchanged. |
| Diligent Engine   | Engine/RHI                                         | Wrapped by IRHI. |
| FlatBuffers       | Networking/Replication (planned)                   | .fbs-generated headers only in .cpp. |
| oneTBB            | Engine/Core/Threading                              | Wrapped by JobSystem. |
| GoogleTest        | Tests/                                             | Test TUs only. |
| RapidCheck        | Tests/                                             | Test TUs only. |

The boundary rule is: **every third-party dependency must be reachable through at most one engine header, and that header must not transitively include the library's own headers.** This keeps the engine compile time sane and makes library swaps (e.g. GLM → custom SIMD path) a one-file change.

---

## 6. How to add a new module

1. Decide the layer (Engine / Server / Backends / Apps).
2. Add an entry to the table above with its dependencies.
3. If the new module introduces a forbidden edge, stop and redesign — do not suppress the rule.
4. Update `ROADMAP.md` with a checkbox for the new module.
5. Run `grep -r "#include \"Server/" Engine/` and friends to confirm no new upward edges sneaked in.

---

## 7. Validation checklist for reviewers

- [ ] Does the change preserve the Apps → Server → Engine → Backends direction?
- [ ] Does the change avoid new forbidden edges listed in §2 and §3?
- [ ] Does the public header of the changed module avoid leaking third-party types?
- [ ] If a new library is added, is it confined to a single TU?
- [ ] Is the dependency table in this document updated?
