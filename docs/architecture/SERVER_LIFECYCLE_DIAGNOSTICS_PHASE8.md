# Server Lifecycle Diagnostics Phase 8

Phase 8 adds bounded, direct/local server lifecycle diagnostics to the
Engine-owned diagnostics model and optional `ZoneServer` integration.

## Contracts

`ServerLifecycleEvent` captures transition evidence:

- `sequence`: deterministic tracker-local event sequence.
- `eventName`: lifecycle event name.
- `category`: `server`, `session`, `entity`, `tick`, or `lifetime`.
- `severity`: `info` or `warning`.
- `message`: short human-readable transition summary.
- `zoneId`: zone that emitted the event.
- `tick`: supplied server tick when available, otherwise `0`.
- `payload`: sorted key/value fields for deterministic reports.

`ServerLifecycleRecord` captures lifetime state:

- `recordId`: deterministic tracker-local record id.
- `recordKind`: `Server`, `SessionConnection`, or `Entity`.
- `zoneId`, `externalId`, and optional `ownerRecordId`.
- `createdTick` and `destroyedTick`.
- `active`: true until an explicit destroy transition closes the record.
- `status` and `label`: local status/debug fields.

`ServerLifecycleSnapshot` contains ordered `events`, ordered `records`, derived
`leaks`, and a `summary` with event, record, active record, leak, and dropped
counts.

## Events

Current `ZoneServer` event names:

- `ServerStarted`: emitted at the existing Run start notification seam and by
  the direct/local test hook. Payload includes `bound_port`, `record_id`, and
  `zone_name`.
- `ServerStopped`: emitted when the server stop notification seam is reached.
- `ServerShutdownRequested`: emitted when shutdown is first requested.
- `SessionConnected` / `SessionDisconnected`: emitted from connection manager
  lifecycle callbacks or direct/local test hooks. Payload includes `client_id`
  and `record_id`.
- `EntityCreated` / `EntityDestroyed`: emitted after successful controlled
  actor register/unregister. Payload includes `client_id`, `entity_id`, owner
  record id, and record id when available.
- `TickSlow`: emitted from explicit tick reliability diagnostics when the
  supplied duration exceeds `maxTickBudgetUs`. Payload includes `budget_us` and
  `duration_us`.
- `LifetimeLeakDetected`: emitted only by explicit tracker leak-detection APIs
  or explicit transitions such as shutdown request.

## Bounded Storage

`DiagnosticConfig` owns the lifecycle caps:

```txt
maxServerLifecycleEvents = 512
maxServerLifecycleRecords = 512
```

When a cap is reached, the tracker increments `droppedEventCount` or
`droppedRecordCount` and does not grow storage. Dropped entries do not allocate
fallback records.

## Pure Snapshots And Reports

Snapshot and report construction is read-only:

- `ServerLifecycleTracker::Snapshot()` copies current events and records.
- `leaks` are derived from active records during snapshot construction.
- `DiagnosticSystem::BuildOperationalSnapshot()` includes the copied snapshot.
- JSON writing serializes the supplied report only.

These paths do not append events, close records, increment lifecycle sequence
numbers, or change dropped counts. Leak events are transition-time evidence, not
serialization side effects.

## Report Shape

Operational reports include an additive top-level section:

```json
"serverLifecycle": {
  "events": [],
  "records": [],
  "leaks": [],
  "summary": {
    "eventCount": 0,
    "recordCount": 0,
    "activeRecordCount": 0,
    "leakCount": 0,
    "droppedEventCount": 0,
    "droppedRecordCount": 0
  }
}
```

## Integration

`ZoneServer` emits lifecycle diagnostics only when diagnostics is attached. The
direct/local test hooks are diagnostics-only and avoid sockets, clients,
reconnect storms, sleeps, and transport stress.

## Non-Goals

Phase 8 does not claim a production account/session service, production-ready
MMO server, full replication lifecycle correctness, full replication observer
lifecycle, real transport stress, bot swarm, MMO-scale stress, release
readiness, product beta, or raw full CTest pass claim.
