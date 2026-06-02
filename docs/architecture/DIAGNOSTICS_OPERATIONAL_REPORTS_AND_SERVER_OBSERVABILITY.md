# Diagnostics Operational Reports And Server Observability

Phase 2 adds the first operational diagnostics vertical slice on top of the
existing Engine-shaped `SagaDiagnostics` target.

## Scope

- `DiagnosticReport` is an Engine-owned local report model with
  `schemaVersion`, `reportKind`, deterministic `generationSequence`, summary
  counts, health metrics, alive lifetime leak records, recent log events, and
  sorted string metadata.
- `DiagnosticReportWriter` writes deterministic JSON for `health_snapshot`,
  `lifetime_leaks`, and `operational_snapshot` reports.
- `DiagnosticSystem` builds health, lifetime leak, and operational snapshots and
  can write an operational report to a local path.
- `ZoneServer` accepts an optional non-owning `SagaDiagnostics` pointer and
  emits local health metrics only when diagnostics is attached.

## Dependency Direction

```txt
Core must not depend on Diagnostics.
SagaDiagnostics may depend on Core/CoreLog and privately on nlohmann_json.
Server may privately depend on SagaDiagnostics.
SagaDiagnostics must not depend on Server, Runtime, Editor, Product, Tools, or SDE.
```

This keeps the report writer local to Engine diagnostics and keeps server
observability as an optional Server-owned integration point.

## ZoneServer Metrics

The first server observability points are intentionally narrow:

- `server.tick.count` increments from movement authority ticks.
- `world.entities.active` is updated after controlled actor register/unregister.
- `server.movement.accepted_inputs` increments for queued movement input.
- `server.movement.rejected_inputs` increments for malformed or rejected
  movement input.
- `server.packets.rejected` increments for raw frame normalization rejection.

Attaching diagnostics also emits a structured `ZoneServer` log event. These
events and metrics do not mutate simulation state, and behavior remains
unchanged when diagnostics is absent.

## Test Coverage

Focused coverage lives in:

```txt
Tests/Unit/Diagnostics/DiagnosticReportTests.cpp
Tests/Unit/Server/ZoneServerDiagnosticsTests.cpp
```

These tests cover deterministic report output, `operational_snapshot` writing,
`lifetime_leaks` filtering, recent logs, optional `ZoneServer` metrics, rejected
packets, `server.tick.count`, diagnostics attach logging, and movement mutation
timing.

## Non-Claims

Phase 2 non-claims: no crash reports, no MemoryTracker/ResourceTracker, no
StressArena, no NetworkChaos, no StateValidation, no FaultBoundary, no SDE
diagnostics config, no remote telemetry, no production readiness, and no full
raw CTest health claim.
