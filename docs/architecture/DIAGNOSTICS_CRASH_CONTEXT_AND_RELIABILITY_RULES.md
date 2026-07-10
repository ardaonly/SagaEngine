# Diagnostics Crash Context And Reliability Rules

This document adds the first crash-context and reliability diagnostics layer on top of
the current contract diagnostics foundation and this document operational report writer.

## Scope

- `CrashContext` captures the requested report kind, reason, optional thread id,
  and sorted context metadata.
- `CrashReport` is an Engine-owned report model for `manual_crash_report`,
  `fatal_error_report`, `reliability_failure_report`, and `slow_tick_report`.
- `CrashReportBuilder` collects the current health snapshot, health rule
  violations, lifetime leak candidates, lifetime leak summaries, and recent log
  events into a deterministic report.
- `DiagnosticReportWriter` writes `CrashReport` JSON through the existing
  deterministic report writer path.
- `DiagnosticSystem` exposes helpers to build and write manual crash-context
  and reliability reports without adding a global singleton.
- `HealthRule`, `HealthRuleResult`, and `HealthSeverity` add the first local
  rule evaluation layer for gauge, counter, and timing metrics.
- Lifetime leak diagnostics classify active lifetime records by type and owner
  system.
- `ZoneServer` remains optionally observable and records `server.tick.ms` plus
  `server.tick.budget_overruns` from real tick duration data.

## Dependency Direction

```txt
Core must not depend on Diagnostics.
SagaDiagnostics may depend on Core/CoreLog and privately on nlohmann_json.
Server may privately depend on SagaDiagnostics.
SagaDiagnostics must not depend on Server, Runtime, Editor, Product, Tools.
```

Crash/reliability report construction stays in Engine diagnostics. Server
integration remains a Server-owned optional injection point, and diagnostics
absence leaves existing server behavior unchanged.

## Crash Handling Policy

This document implements manual crash-context report generation only. It does not
install OS signal handlers, Windows SEH handlers, or other production crash
capture hooks.

This is intentional. The repository did not have a safe established
signal-handler crash reporting pattern for this milestone, and deterministic local
report generation is the safer first layer. Production crash safety, async-signal
safety, stack traces, minidumps, and platform crash-handler integration are
deferred.

## Health Rules

Health rules evaluate against a deterministic `HealthSnapshot`.

Supported severities are:

- `info`
- `warning`
- `error`
- `critical`

Supported rule types are:

- gauge greater than threshold
- gauge less than threshold
- counter greater than threshold
- timing greater than threshold

Missing metrics produce deterministic not-evaluated results. Not-evaluated
results are available from health rule evaluation, but crash/reliability reports
include evaluated failing rules as violations.

Useful rule names documented for current and future emitters include
`server.tick.ms`, `net.sessions.active`, `world.entities.active`, and
`memory.process.mb`. This document does not add OS memory polling.

## Lifetime Leak Diagnostics

Lifetime leak diagnostics are report classifications over active
`LifetimeRecord` entries:

- destroyed records are excluded
- candidates preserve type name, owner system, object id, external id, owner
  object id, and created tick
- summaries include total active count, count by type, and count by owner system

This is not a full ownership graph and is not connected to new entity/session
lifecycle systems.

## ZoneServer Reliability

`ZoneServer` already measures tick duration through its tick execution path.
This document uses that real duration data to emit:

- `server.tick.ms`
- `server.tick.budget_overruns`
- a structured warning log when a tick exceeds `maxTickBudgetUs`

The emissions happen only when diagnostics is attached. They do not mutate
simulation state, do not change movement timing, and do not force diagnostics
ownership into `ZoneServer`.

## Tests

Focused coverage lives in:

```txt
Tests/Unit/Diagnostics/DiagnosticReliabilityTests.cpp
Tests/Unit/Server/ZoneServerDiagnosticsTests.cpp
```

The reliability tests cover `CrashReport`, `CrashContext`, report writing,
HealthRule behavior, HealthSeverity serialization, recent logs, lifetime leak
candidates, lifetime leak summaries, and deterministic failure paths.

The server diagnostics tests cover optional diagnostics behavior, retained
movement mutation timing, this document accepted/rejected input metrics, rejected
packet metrics, tick count metrics, tick duration metrics, and slow tick budget
overrun diagnostics.

## Non-Claims

This document non-claims: no production crash safety, no unsafe OS signal/SEH crash
handlers, no stack trace requirement, no MemoryTracker or ResourceTracker, no
StressArena, no NetworkChaos, no StateValidation, no FaultBoundary, no
external diagnostics config, no remote telemetry no production readiness, and
no full raw CTest health claim.
