# Client-Side Replication Pipeline -- Formal Specification

## 1. Apply Order Contract

### 1.1 Entity Iteration Order

All entities within a delta snapshot are applied in **ascending order of EntityId**. This is enforced by the `PatchJournal::Sort()` method which uses the composite key `(entityId, generation, componentTypeId)`.

**Invariant:** Two clients processing the same delta stream in any network order will produce identical `WorldState` bytes after apply.

### 1.2 Component Apply Order

Within a single entity, components are applied in **ascending order of ComponentTypeId**. This ensures that the internal state of an entity is built deterministically regardless of the order components appear in the wire format.

**Invariant:** `Apply(EntityId=X, TypeId=1)` always executes before `Apply(EntityId=X, TypeId=2)`.

### 1.3 Parallel Apply Isolation

In `SchedulerMode::Parallel`, entity groups are partitioned so that no two threads write to the same `ComponentBlock` slot simultaneously. Each entity's component writes are isolated to its own slot in the parallel arrays.

**Invariant:** No data race occurs between parallel apply operations. False sharing is prevented by the `CacheLinePad` wrapper on hot structures.

## 2. Determinism Invariants

### 2.1 Float Canonicalisation

All floating-point values deserialized from wire format pass through `CanonicaliseFloat32()` which:
- Replaces NaN with zero
- Flushes denormals to zero
- Clamps infinity to the largest finite float

**Invariant:** `CanonicaliseFloat32(x) == CanonicaliseFloat32(y)` for all `x, y` that differ only in NaN payload, denormal bit pattern, or infinity sign.

### 2.2 Hash Determinism

The canonical world hash (`ComputeCanonicalWorldHash`) is computed as follows:
1. Entities sorted by EntityId ascending
2. Components sorted by ComponentTypeId ascending
3. Only `ServerOwned` and `ClientPredicted` components included
4. Raw byte pattern used (IEEE 754 little-endian, no conversion)
5. FNV-1a 64-bit with offset basis `14695981039346656037` and prime `1099511628211`

**Invariant:** Two `WorldState` instances with identical ServerOwned and ClientPredicted component bytes produce the same hash.

### 2.3 State Machine Determinism

The `ReplicationStateMachine` is a deterministic finite automaton. Given the same sequence of inputs (packet accept/drop decisions, sequence recordings, tick calls), it always produces the same sequence of state transitions.

**Invariant:** `State(t+1) = Transition(State(t), Input(t))` where Transition is a pure function.

## 3. State Machine Legal Transitions

```
Boot ────────────────────────── FullSnapshot  ──► AwaitingFullSnapshot
AwaitingFullSnapshot ───────── FullSnapshot    ──► SyncingBaseline
SyncingBaseline ────────────── FullApplied     ──► Synced
Synced ────────────────────── DeltaValid      ──► Synced
Synced ────────────────────── DeltaInvalid    ──► Desynced
Synced ────────────────────── GapExceeded     ──► Desynced
Desynced ──────────────────── GraceExpired    ──► ResyncRequested
ResyncRequested ───────────── FullSnapshot    ──► AwaitingFullSnapshot
Any ───────────────────────── Reset           ──► Boot
```

### Illegal Transitions (must never occur)

- `Synced → Boot` (must go through Reset)
- `Desynced → Synced` (must go through ResyncRequested → AwaitingFullSnapshot → SyncingBaseline → Synced)
- `Boot → Synced` (must receive full snapshot first)

## 4. Thread Safety Contract

### 4.1 Network Thread

The network thread may only call:
- `PacketDemux::Enqueue()` -- lock-free SPSC, wait-free
- `RateLimitGuard::AcceptPacket()` -- atomic token bucket, lock-free

No other replication subsystem methods may be called from the network thread.

### 4.2 Game Thread

The game thread owns:
- `PacketDemux::ProcessQueue()` -- drains SPSC queue
- `ReplicationStateMachine::RecordSequence()`, `UpdateRtt()`, `Tick()`
- `SnapshotApplyPipeline::SubmitFull()`, `SubmitDelta()`, `Tick()`
- `PatchJournal::Apply()` -- two-phase validate and commit

All methods are thread-affine to the game thread. No internal mutexes are used because single-threaded ownership eliminates the need.

### 4.3 Telemetry Thread

Telemetry counters are `std::atomic` with `memory_order_relaxed`. The `Snapshot()` method provides a consistent read by loading all atomics in a single pass.

## 5. Memory Budget Contract

### 5.1 Patch Journal

- Maximum patches: 4096 per journal
- Maximum byte budget: 64 KiB per journal
- Pre-allocated storage: no heap allocation during commit phase

### 5.2 Jitter Buffer

- Maximum entries: `config.jitterBufferSlots` (default 16)
- Maximum age: `config.maxBufferedAgeTicks` (default 60)
- Eviction policy: oldest entry removed on overflow

### 5.3 SPSC Queue

- Capacity: 128 packets (power of two for bitmask wrap)
- Overflow policy: drop oldest (back-pressure on network layer)
- Packet size limit: 256 KiB

## 6. Performance Budget

| Metric | Budget | Measurement |
|--------|--------|-------------|
| Max apply time per tick | 2000 μs | `TestMaxApplyTimePerTick` |
| Worst-case entity burst (10000 entities) | 10000 μs | `TestWorstCaseEntityBurst` |
| Per-component deserialize | 1 μs | `TestComponentDeserializeBudget` |
| Cold cache cost | 5000 μs | `TestColdCacheCost` |
| Jitter buffer memory | 16 * avg_delta_bytes | Bounded by config |
| SPSC queue memory | 128 * 256 KiB max | 32 MiB worst case |

## 7. Security Contract

### 7.1 Bounds Checks

Every wire decode performs these checks in order:
1. Header size >= minimum
2. entityCount <= kMaxDeltaEntities (65536)
3. payloadBytes <= kMaxDeltaPayloadBytes (256 KiB)
4. componentCount <= kMaxComponentsPerEntity (128)
5. dataLen <= remaining payload bytes
6. typeId <= maxTypeId (forward-compatible skip)
7. Total decoded bytes == declared payloadBytes

### 7.2 Rate Limiting

- Token bucket: 1000 tokens/sec, 2000 capacity
- Decode failure quarantine: 16 consecutive failures = 120 tick cool-down
- Sequence gap anomaly: gap > 256 triggers alert

### 7.3 Replay Protection

- Sequence window: 64 entries, sliding bitmask
- Old packet detection: `baselineTick < lastAppliedTick` → DroppedOld
- Duplicate detection: `baselineTick == lastAppliedTick` → DroppedDuplicate

## 8. Recovery Contract

### 8.1 Escalation Ladder

1. **Soft Resync**: Request full snapshot retry (up to 3 times)
2. **Hard Reconnect**: Tear down and re-establish network connection (up to 5 times)
3. **Graceful Disconnect**: Notify user and return to main menu

### 8.2 State Continuity

- After soft resync success: all counters reset, state → Idle
- After hard reconnect success: predicted state flushed, interpolation baseline reset
- After graceful disconnect: session cache invalidated, local-only components wiped

## 9. Schema Version Contract

### 9.1 Version Format

`major.minor.patch` packed into uint32: 10 bits major, 10 bits minor, 12 bits patch.

### 9.2 Negotiation Rules

| Condition | Result | Action |
|-----------|--------|--------|
| major mismatch | Incompatible | Disconnect client |
| server minor > client minor | ForwardCompatible | Skip unknown component types |
| client minor > server minor | BackwardCompatible | Ignore extra capabilities |
| patch mismatch | Compatible | Use server's patch level |

### 9.3 Deprecation Timeline

When a component type is deprecated:
1. Server continues sending it for N versions (grace period)
2. Server marks it as deprecated in schema
3. Client ignores deprecated components after grace period
4. Server stops sending after grace period
