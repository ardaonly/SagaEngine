# SagaDiagnostics Roadmap

> Last updated: 2026-05-15
> Status: Active engineering roadmap
> Scope: Runtime diagnostics, structured logging, health metrics, lifetime tracking, memory/resource visibility, crash context, diagnostic reports, stress/soak observability, network chaos reporting, state validation reporting, and production reliability verification.

---

## 0. Roadmap Convention

* `[x]` — Shipped. The note after the item names the concrete files, tests, modules, or integration points that represent completed work.
* `[ ]` — Open. The item describes the finished production state, not temporary scaffolding.
* Shipped diagnostics work must include implementation, tests, and verification evidence.
* Open diagnostics work must describe observable runtime behavior, not vague architecture intent.
* Runtime diagnostics must be proven through automated tests and reproducible reports.
* Manual console inspection is not accepted as proof that diagnostics work.

Testing standards:

* **GoogleTest** is the deterministic correctness test framework.
* **RapidCheck** is the property-based invariant test framework.
* Both are part of the long-term diagnostics verification model.
* GoogleTest verifies exact expected behavior.
* RapidCheck verifies state invariants across large input spaces and operation sequences.
* Neither framework replaces the other.

---

## 1. Document Purpose

This document defines the roadmap for `SagaDiagnostics`.

`SagaDiagnostics` is the runtime reliability and lifetime diagnostics layer for SagaEngine.

Its purpose is to make long-running MMO-style server processes observable, debuggable, testable, and diagnosable before runtime degradation becomes a production incident.

`SagaDiagnostics` must expose enough information to understand failures such as:

* sessions that are not cleaned up,
* disconnected players that remain in world registries,
* replication observers that outlive sessions,
* entities that survive after zone unload,
* packet queues that grow without bound,
* database jobs that accumulate,
* timers that survive their owner,
* memory growth under stable load,
* resource handles that leak without obvious memory leaks,
* tick time degradation,
* crash reports without useful runtime context.

The target is not a pretty logging layer.

The target is a production diagnostics system that can explain what the server was doing, what degraded, what survived too long, and what must be investigated.

---

## 2. Product Vision

* [ ] Provide a runtime diagnostics system that becomes a standard dependency of long-running engine and server processes.

* [ ] Make server behavior observable through structured logs, health metrics, lifetime records, reports, and crash context.

* [ ] Detect logical lifetime leaks, not only raw memory leaks.

* [ ] Preserve useful context before crashes, shutdowns, stress failures, and long-running degradation.

* [ ] Produce machine-readable diagnostic reports for automated testing, stress analysis, and future tooling.

* [ ] Support production-grade reliability verification through deterministic tests, property-based invariant tests, sanitizers, stress tests, and build matrix coverage.

* [ ] Keep diagnostics explicit, testable, and controlled. A hidden global diagnostic system is not a serious ownership model.

---

## 3. Ownership Model

### 3.0 Module Layout

* [x] Place `SagaDiagnostics` under the engine-wide Public/Private layout.

  Represented by:

  ```txt
  Engine/Public/SagaEngine/Diagnostics/
  Engine/Private/SagaEngine/Diagnostics/
  ```

  Preserved decision:

  ```txt
  Root Source/SagaEngine is not an engine module location.
  Engine/include and Engine/src are no longer accepted for engine code.
  Do not create Engine/<Module>/Public or Engine/<Module>/Private directories.
  ```

* [x] Keep diagnostics dependencies narrow.

  Represented by:

  ```txt
  SagaDiagnostics -> SagaCoreLog
  SagaEngine -> SagaCoreLog
  ```

  Preserved decision:

  ```txt
  SagaDiagnostics must not link against the SagaEngine aggregate target.
  Engine module boundaries are preserved through SagaEngine/<Module> paths and CMake targets, not per-module Public/Private folders.
  ```

### 3.1 SagaDiagnostics Ownership

* [ ] `SagaDiagnostics` owns runtime diagnostics behavior.

  Done means `SagaDiagnostics` owns:

  * structured logging,
  * log sinks,
  * recent log retention,
  * health metric storage,
  * health snapshots,
  * lifetime tracking,
  * lifetime leak inspection,
  * memory visibility,
  * resource visibility,
  * crash context collection,
  * diagnostic report creation,
  * stress and soak report data.

* [ ] `SagaDiagnostics` exposes stable runtime services.

  Done means server, runtime, networking, persistence, and test tools can report diagnostic data through stable APIs.

* [ ] `SagaDiagnostics` owns diagnostic state storage.

  Done means consuming systems emit events, metrics, and lifetime records, but do not own diagnostic storage internals.

---

### 3.2 Server Ownership

* [ ] Server systems report lifecycle events to diagnostics.

  Done means the server emits diagnostics for:

  * startup,
  * shutdown request,
  * shutdown complete,
  * session connected,
  * session disconnected,
  * entity created,
  * entity destroyed,
  * slow tick,
  * abnormal queue growth,
  * lifetime leak detected,
  * reconnect failure,
  * persistence backlog,
  * replication observer mismatch.

* [ ] Server systems remain responsible for domain truth.

  Done means diagnostics records server state but does not become the owner of gameplay, session, persistence, replication, or world authority.

Diagnostics observes.

It does not secretly become the engine.

---

### 3.3 Tooling Ownership

* [ ] Stress and soak tools consume diagnostics output.

  Done means load tests can produce structured reports from the same diagnostic model used by runtime systems.

* [ ] Future diagnostics viewers consume reports.

  Done means viewer tools inspect reports and snapshots; they do not redefine diagnostic storage or runtime behavior.

* [ ] Crash dump tools consume crash reports.

  Done means crash investigation tools operate on diagnostics output instead of inventing separate crash metadata formats.

---

## 4. Core Diagnostic Concepts

### 4.1 Diagnostic System

* [ ] Provide a central `DiagnosticSystem`.

  Done means the system coordinates:

  * diagnostic configuration,
  * logger,
  * health monitor,
  * lifetime tracker,
  * memory tracker,
  * resource tracker,
  * crash context,
  * report writer.

* [ ] Support explicit ownership.

  Done means the diagnostic system can be created, configured, passed, flushed, inspected, and destroyed predictably.

* [ ] Support deterministic teardown.

  Done means shutdown can flush logs, finalize reports, inspect lifetime leaks, and release sinks without relying on undefined static object order.

A diagnostic system that cannot shut down cleanly is already lying about reliability.

---

### 4.2 Diagnostic Configuration

* [ ] Define stable diagnostic configuration.

  Done means configuration can control:

  * enabled log levels,
  * enabled sinks,
  * report directory,
  * retained log event count,
  * health snapshot cadence,
  * lifetime tracking mode,
  * crash report mode,
  * memory tracking mode,
  * resource tracking mode,
  * stress report mode.

* [ ] Provide safe defaults.

  Done means diagnostics can run without optional configuration files or external systems.

* [ ] Support runtime-readable configuration state.

  Done means tests and reports can inspect which diagnostic features were enabled during execution.

---

### 4.3 Structured Logging

* [ ] Add structured log events.

  Done means every log event contains:

  * timestamp,
  * level,
  * system tag,
  * message,
  * context key-value data.

* [ ] Support standard log levels.

  Done means the logger supports:

  * Trace,
  * Debug,
  * Info,
  * Warn,
  * Error,
  * Fatal.

* [ ] Support required sinks.

  Done means diagnostics supports:

  * console sink,
  * file sink,
  * retained in-memory recent event sink.

* [ ] Preserve recent logs for reports.

  Done means crash reports, stress reports, and shutdown reports can include the last relevant events.

* [ ] Keep log context structured.

  Done means log context can represent keys such as:

  * session id,
  * player id,
  * entity id,
  * zone id,
  * tick,
  * connection id,
  * packet type,
  * subsystem,
  * operation id.

A log line without context is not diagnostics. It is decoration.

---

### 4.4 Health Metrics

* [ ] Add runtime health metrics.

  Done means the health monitor supports:

  * gauges,
  * counters,
  * timings.

* [ ] Track required server metrics.

  Done means diagnostics can represent:

  * server uptime,
  * tick duration,
  * active sessions,
  * active entities,
  * loaded zones,
  * process memory estimate.

* [ ] Track extended runtime metrics.

  Done means diagnostics can represent:

  * packet queue size,
  * packets in per second,
  * packets out per second,
  * pending database jobs,
  * pending worker jobs,
  * loaded resources,
  * replication observers,
  * replication snapshot bytes.

* [ ] Produce health snapshots.

  Done means snapshots can be inspected in tests, written into reports, and attached to crash context.

* [ ] Detect degradation patterns.

  Done means diagnostics can expose trends such as increasing tick time, growing queue sizes, or object counts that do not return to baseline.

---

### 4.5 Lifetime Tracking

* [ ] Add logical lifetime tracking.

  Done means diagnostics can register runtime objects, mark them destroyed, query alive records, and report records that outlive their expected owner.

* [ ] Track server-relevant object categories.

  Done means lifetime tracking supports:

  * session,
  * connection,
  * player entity,
  * entity,
  * zone,
  * instance,
  * replication observer,
  * job,
  * timer,
  * database transaction,
  * resource.

* [ ] Track owner relationships.

  Done means diagnostics can model ownership relationships such as:

  * session owns player entity,
  * connection owns session,
  * zone owns entities,
  * session owns replication observers,
  * system owns jobs,
  * object owns timers.

* [ ] Report logical leaks.

  Done means diagnostics can report:

  * session disconnected but not destroyed,
  * player entity remained after logout,
  * entity remained after zone unload,
  * replication observer outlived session,
  * job survived owner shutdown,
  * timer survived object destruction.

This is a core production requirement.

Raw memory leak detection is not enough for an MMO server. Logical lifetime leaks are where the expensive bugs live.

---

### 4.6 Memory Visibility

* [ ] Add memory snapshots.

  Done means diagnostics can record memory-related runtime state without replacing sanitizers.

* [ ] Track per-system memory counters.

  Done means systems can report memory pressure using stable diagnostic counters.

* [ ] Support allocation scopes.

  Done means instrumented builds can associate allocation-heavy regions with diagnostic scopes.

* [ ] Preserve sanitizer compatibility.

  Done means AddressSanitizer, LeakSanitizer, and UndefinedBehaviorSanitizer remain part of the reliability model.

* [ ] Avoid expensive production defaults.

  Done means production servers do not capture stack traces for every allocation unless explicitly configured.

A custom allocator is not a diagnostics strategy by itself. It is a liability unless the rest of the visibility system already exists.

---

### 4.7 Resource Visibility

* [ ] Add resource tracking.

  Done means diagnostics can track non-memory resources such as:

  * socket handles,
  * database connections,
  * file handles,
  * thread handles,
  * job handles,
  * timers,
  * asset handles,
  * GPU resources where relevant.

* [ ] Associate resources with owners.

  Done means resource leaks can be connected to systems, sessions, zones, jobs, or runtime objects.

* [ ] Report resource leaks separately from memory leaks.

  Done means diagnostics can distinguish memory health from resource health.

Memory can look stable while sockets, files, jobs, or timers leak. Pretending otherwise is amateur hour.

---

### 4.8 Crash Context

* [ ] Add crash context collection.

  Done means crash reports can include:

  * signal or exception information,
  * thread id,
  * stack trace where supported,
  * build configuration,
  * platform information,
  * commit hash where available,
  * last health snapshot,
  * last retained log events,
  * active session count,
  * active entity count,
  * last tick duration.

* [ ] Write crash reports deterministically.

  Done means crash reports use a predictable report directory and stable naming convention.

* [ ] Keep crash reporting safe.

  Done means crash handling avoids unsafe allocations or complex behavior in restricted crash contexts where the platform requires it.

---

### 4.9 Diagnostic Reports

* [ ] Add machine-readable diagnostic reports.

  Done means reports can be generated for:

  * shutdown,
  * lifetime leaks,
  * health snapshots,
  * stress runs,
  * soak runs,
  * crash events,
  * network chaos scenarios,
  * validation failures.

* [ ] Define stable report sections.

  Done means reports can contain:

  * runtime metadata,
  * configuration summary,
  * health snapshot,
  * recent logs,
  * lifetime records,
  * memory snapshot,
  * resource snapshot,
  * warnings,
  * failure summary.

* [ ] Support JSON as a primary machine-readable format.

  Done means tests and tools can parse diagnostics output without fragile text scraping.

---

## 5. Implementation Roadmap

## Phase 1 — Diagnostics Foundation

* [ ] Add the core diagnostics foundation.

  Done means the implementation includes:

  * `DiagnosticSystem`,
  * `DiagnosticConfig`,
  * `Core::Log::Logger`,
  * `Core::Log::Level`,
  * `Core::Log::LogEvent`,
  * `Core::Log::LogSink`,
  * `Core::Log::ConsoleLogSink`,
  * `Core::Log::FileLogSink`,
  * retained recent log storage,
  * `HealthMonitor`,
  * `HealthMetric`,
  * `HealthSnapshot`,
  * `LifetimeTracker`,
  * `LifetimeHandle`,
  * `LifetimeRecord`.

* [ ] Provide deterministic lifecycle behavior.

  Done means diagnostics can be initialized, used, flushed, inspected, and shut down predictably.

* [ ] Provide complete foundation tests.

  Done means the foundation is covered by GoogleTest and RapidCheck according to the testing model in this roadmap.

---

## Phase 2 — Logging System

* [ ] Complete structured logging behavior.

  Done means logs preserve timestamp, level, tag, message, and context data.

* [ ] Complete sink behavior.

  Done means console, file, and retained event sinks are covered by tests.

* [ ] Add filtering behavior.

  Done means disabled log levels are filtered consistently.

* [ ] Add retained event access.

  Done means recent logs can be queried for reports and crash context.

* [ ] Add logging invariants.

  Done means RapidCheck verifies properties such as:

  * filtering never emits disabled levels,
  * retained log storage never exceeds configured capacity,
  * event order is preserved,
  * context keys survive event storage.

---

## Phase 3 — Health Metrics

* [ ] Complete gauge, counter, and timing behavior.

  Done means metrics can be set, incremented, recorded, snapshotted, and queried.

* [ ] Add required runtime metric names.

  Done means standard metrics exist for server uptime, tick duration, sessions, entities, zones, and process memory estimate.

* [ ] Add snapshot consistency guarantees.

  Done means snapshots are stable views and are not corrupted by later metric updates.

* [ ] Add health invariants.

  Done means RapidCheck verifies properties such as:

  * counters never decrease except through explicit reset,
  * snapshots preserve values captured at snapshot time,
  * timing records remain valid under repeated writes,
  * metric lookup does not mutate metric state.

---

## Phase 4 — Lifetime Tracking

* [ ] Complete lifetime record registration.

  Done means objects can be registered with type, owner system, object id, and creation tick.

* [ ] Complete destruction marking.

  Done means valid objects can be marked destroyed exactly once.

* [ ] Complete alive record inspection.

  Done means diagnostics can report records that are still alive.

* [ ] Complete owner relationship support.

  Done means lifetime records can describe parent-child ownership relationships.

* [ ] Add lifetime leak reporting.

  Done means diagnostics can produce reports for objects that remain alive after expected shutdown points.

* [ ] Add lifetime invariants.

  Done means RapidCheck verifies properties such as:

  * invalid handles cannot destroy valid records,
  * destroyed records do not appear in alive-only queries,
  * repeated destruction is safe and detectable,
  * arbitrary register/destroy sequences keep internal state consistent,
  * ownership graph operations cannot create impossible ownership states.

---

## Phase 5 — Server Lifecycle Integration

* [ ] Integrate diagnostics into server lifecycle.

  Done means diagnostics records:

  * server started,
  * server shutdown requested,
  * server shutdown complete,
  * tick timing,
  * slow tick warnings,
  * active session count,
  * active entity count.

* [ ] Integrate diagnostics into session lifecycle.

  Done means diagnostics records:

  * session connected,
  * session disconnected,
  * session destroyed,
  * reconnect attempt,
  * reconnect failure,
  * session lifetime leak.

* [ ] Integrate diagnostics into entity lifecycle.

  Done means diagnostics records:

  * entity created,
  * entity destroyed,
  * player entity attached,
  * player entity detached,
  * entity leak after zone unload.

* [ ] Add server integration tests.

  Done means test server scenarios can prove lifecycle events, metrics, and lifetime records are emitted correctly.

---

## Phase 6 — Diagnostic Report System

* [ ] Add report model.

  Done means reports have stable fields and sections.

* [ ] Add report writer.

  Done means reports can be written to deterministic paths.

* [ ] Add JSON output.

  Done means reports can be parsed by tests and tools.

* [ ] Add report tests.

  Done means GoogleTest verifies report structure and expected fields.

* [ ] Add report invariants.

  Done means RapidCheck verifies properties such as:

  * reports remain parseable across generated diagnostic data,
  * required sections are never omitted,
  * retained event order survives report generation,
  * alive lifetime records remain represented correctly.

---

## Phase 7 — Crash Context

* [ ] Add crash context model.

  Done means crash context can represent platform, build, thread, signal/exception, health, logs, lifetime state, and runtime counters.

* [ ] Add crash report output.

  Done means crash reports are written in deterministic format and location.

* [ ] Add platform-specific stack trace support where available.

  Done means stack traces are included on supported platforms without making unsupported platforms fail the diagnostics system.

* [ ] Add crash context tests.

  Done means crash report formatting and safe context capture are tested without requiring destructive crash behavior in the normal test suite.

---

## Phase 8 — Memory Diagnostics

* [ ] Add memory snapshot model.

  Done means diagnostics can represent current memory estimates and per-system memory counters.

* [ ] Add allocation scope instrumentation.

  Done means debug and instrumented builds can annotate allocation-heavy code paths.

* [ ] Add snapshot diff support.

  Done means diagnostics can compare memory snapshots across time or stress phases.

* [ ] Add sanitizer verification.

  Done means diagnostics builds are validated with supported sanitizers.

* [ ] Add memory invariants.

  Done means RapidCheck verifies properties such as:

  * snapshot diffs are mathematically consistent,
  * counters do not underflow,
  * scope nesting remains balanced under generated operation sequences.

---

## Phase 9 — Resource Diagnostics

* [ ] Add resource tracking model.

  Done means diagnostics can represent sockets, database connections, file handles, timers, jobs, asset handles, and other non-memory resources.

* [ ] Add owner association.

  Done means leaked resources can be tied back to owner systems or lifetime records.

* [ ] Add resource leak reports.

  Done means diagnostics can report resources that survived teardown or owner destruction.

* [ ] Add resource invariants.

  Done means RapidCheck verifies properties such as:

  * acquire/release sequences remain balanced,
  * invalid releases do not corrupt valid resources,
  * resource ownership remains consistent,
  * leaked resources are discoverable.

---

## Phase 10 — Stress and Soak Observability

* [ ] Add stress report support.

  Done means diagnostics can report:

  * scenario name,
  * duration,
  * client count,
  * connection count,
  * disconnect count,
  * reconnect count,
  * packet counts,
  * tick timings,
  * health snapshots,
  * lifetime leaks,
  * memory/resource snapshots.

* [ ] Add soak report support.

  Done means diagnostics can capture long-running trends such as:

  * increasing tick time,
  * increasing memory estimate,
  * increasing live object count,
  * increasing queue size,
  * growing resource count.

* [ ] Add stress verification tests.

  Done means automated tests can run controlled scenarios and assert diagnostic output.

---

## Phase 11 — Network Chaos Reporting

* [ ] Consume network chaos metrics.

  Done means diagnostics can receive data for:

  * packet loss,
  * latency,
  * jitter,
  * reconnect storms,
  * duplicate packets,
  * out-of-order packets where relevant.

* [ ] Add chaos scenario reporting.

  Done means reports can describe chaos configuration, observed behavior, and recovery outcome.

* [ ] Add chaos diagnostics tests.

  Done means controlled chaos scenarios produce expected metrics and warnings.

---

## Phase 12 — State Validation Reporting

* [ ] Consume state validation results.

  Done means diagnostics can report:

  * client-server desync,
  * invalid entity state,
  * replication mismatch,
  * unexpected ownership changes,
  * invalid authority transition.

* [ ] Include validation data in reports.

  Done means stress, soak, and shutdown reports can include validation failures.

* [ ] Add validation diagnostics tests.

  Done means simulated validation failures produce deterministic diagnostics output.

---

## Phase 13 — Fault Boundary Reporting

* [ ] Add fault diagnostic events.

  Done means diagnostics can report:

  * subsystem name,
  * failure type,
  * affected sessions,
  * affected zones,
  * recovery action,
  * process continuation status.

* [ ] Add fault reports.

  Done means subsystem failures can be analyzed after recovery or shutdown.

* [ ] Add fault reporting tests.

  Done means simulated subsystem failures produce expected diagnostic records.

---

## 6. Testing Strategy

### 6.1 Permanent Test Architecture

* [ ] Use GoogleTest for deterministic unit and integration tests.

  Done means exact behavior is tested with clear assertions.

* [ ] Use RapidCheck for property-based invariant tests.

  Done means generated operation sequences are used to test internal consistency.

* [ ] Keep both test layers active in CI.

  Done means deterministic tests and invariant tests are both part of the production verification path.

Correct model:

```txt
GoogleTest proves specific expected behavior.
RapidCheck attacks state invariants with generated operation sequences.
Sanitizers catch memory and undefined-behavior classes of defects.
Stress/soak tests validate runtime behavior under load.
```

Incorrect model:

```txt
Use one test framework and pretend reliability is solved.
```

---

### 6.2 GoogleTest Coverage

* [ ] Add `SagaDiagnosticsSmokeTests`.

  Done means diagnostics can be constructed, used, flushed, and destroyed.

* [ ] Add `SagaDiagnosticsLoggerTests`.

  Done means log levels, log context, sinks, filtering, and retained events are verified.

* [ ] Add `SagaDiagnosticsHealthTests`.

  Done means gauges, counters, timings, and snapshots are verified.

* [ ] Add `SagaDiagnosticsLifetimeTests`.

  Done means registration, destruction, alive queries, invalid handles, and ownership links are verified.

* [ ] Add `SagaDiagnosticsReportTests`.

  Done means generated reports contain required sections and parseable output.

* [ ] Add `SagaDiagnosticsCrashContextTests`.

  Done means crash context formatting and safe capture behavior are verified.

* [ ] Add `SagaDiagnosticsMemoryTests`.

  Done means memory snapshots, counters, scopes, and snapshot diffs are verified.

* [ ] Add `SagaDiagnosticsResourceTests`.

  Done means resource acquire/release behavior and leak reporting are verified.

* [ ] Add `SagaDiagnosticsServerIntegrationTests`.

  Done means server lifecycle events produce expected diagnostics.

---

### 6.3 RapidCheck Coverage

* [ ] Add `SagaDiagnosticsLoggerPropertyTests`.

  Done means generated log sequences preserve ordering, filtering, and retention invariants.

* [ ] Add `SagaDiagnosticsHealthPropertyTests`.

  Done means generated metric operations preserve snapshot and counter invariants.

* [ ] Add `SagaDiagnosticsLifetimePropertyTests`.

  Done means generated register/destroy/owner operations preserve lifetime tracker invariants.

* [ ] Add `SagaDiagnosticsReportPropertyTests`.

  Done means generated diagnostic states still produce valid reports.

* [ ] Add `SagaDiagnosticsMemoryPropertyTests`.

  Done means generated memory counter operations preserve arithmetic consistency.

* [ ] Add `SagaDiagnosticsResourcePropertyTests`.

  Done means generated acquire/release sequences preserve resource tracker consistency.

RapidCheck belongs here because diagnostics is state-heavy.

If a random sequence of valid operations corrupts internal state, the diagnostics system is not production-grade. It is just organized optimism.

---

### 6.4 Sanitizer Coverage

* [ ] Run diagnostics tests with AddressSanitizer where supported.

* [ ] Run diagnostics tests with LeakSanitizer where supported.

* [ ] Run diagnostics tests with UndefinedBehaviorSanitizer where supported.

* [ ] Preserve clean sanitizer output for diagnostics tests.

Sanitizers do not replace diagnostics.

Diagnostics does not replace sanitizers.

Both are required because they catch different failure classes.

---

### 6.5 Stress and Soak Verification

* [ ] Add controlled stress scenarios.

  Done means repeatable stress scenarios produce deterministic diagnostic reports.

* [ ] Add long-running soak scenarios.

  Done means diagnostics can expose degradation across time.

* [ ] Define pass/fail criteria.

  Done means a stress or soak run fails when:

  * active session count does not return to baseline,
  * active entity count does not return to baseline,
  * resource count does not return to baseline,
  * tick time exceeds threshold,
  * packet queue grows beyond threshold,
  * required report fields are missing,
  * diagnostic output is not parseable.

---

## 7. Build and Verification Matrix

* [ ] Build diagnostics in Debug.

* [ ] Build diagnostics in Release.

* [ ] Build diagnostics in RelWithDebInfo.

* [ ] Run GoogleTest diagnostics suites in supported build modes.

* [ ] Run RapidCheck diagnostics property suites in supported build modes.

* [ ] Run sanitizer diagnostics configurations where supported.

* [ ] Verify supported compilers.

  Target compilers:

  * MSVC,
  * Clang,
  * GCC where relevant.

* [ ] Do not accept Debug-only correctness.

Release builds have different optimization behavior.

A diagnostics system that only works in Debug is not a diagnostics system. It is a classroom exercise.

---

## 8. Non-Goals

This roadmap does not turn diagnostics into:

* gameplay ownership,
* editor UI ownership,
* full custom allocator ownership,
* cloud telemetry ownership,
* deployment orchestration,
* distributed tracing platform,
* plugin marketplace analytics,
* editor-only debugging UI,
* replacement for sanitizers,
* replacement for stress tests,
* replacement for crash analysis tools.

These systems may consume diagnostics.

They do not define diagnostics.

---

## 9. Production Definition of Done

`SagaDiagnostics` is production-ready only when:

* [ ] Structured logging is implemented and tested.

* [ ] Health metrics are implemented and tested.

* [ ] Health snapshots are generated and reportable.

* [ ] Lifetime tracking detects logical ownership leaks.

* [ ] Lifetime ownership relationships are represented.

* [ ] Server lifecycle emits diagnostic events.

* [ ] Session lifecycle emits diagnostic events.

* [ ] Entity lifecycle emits diagnostic events.

* [ ] Diagnostic reports are machine-readable.

* [ ] Crash reports include useful runtime context.

* [ ] Memory snapshots exist.

* [ ] Resource tracking exists.

* [ ] Stress reports exist.

* [ ] Soak reports expose degradation trends.

* [ ] Network chaos scenarios produce diagnostic output.

* [ ] State validation failures are represented in diagnostics.

* [ ] Fault boundary events are represented in diagnostics.

* [ ] GoogleTest deterministic tests cover expected behavior.

* [ ] RapidCheck property tests cover invariants.

* [ ] Sanitizer configurations are clean where supported.

* [ ] Debug, Release, and RelWithDebInfo builds are verified.

* [ ] Diagnostics remains explicit, inspectable, and testable.

---

## 10. First Complete Milestone

The first complete milestone is:

```txt
SagaDiagnostics Foundation
```

Required deliverables:

* [ ] `DiagnosticSystem`
* [ ] `DiagnosticConfig`
* [ ] `Core::Log::Logger`
* [ ] `Core::Log::Level`
* [ ] `Core::Log::LogEvent`
* [ ] `Core::Log::LogSink`
* [ ] `Core::Log::ConsoleLogSink`
* [ ] `Core::Log::FileLogSink`
* [ ] retained recent log event storage
* [ ] `HealthMonitor`
* [ ] `HealthMetric`
* [ ] `HealthSnapshot`
* [ ] `LifetimeTracker`
* [ ] `LifetimeHandle`
* [ ] `LifetimeRecord`
* [ ] GoogleTest smoke tests
* [ ] GoogleTest logger tests
* [ ] GoogleTest health tests
* [ ] GoogleTest lifetime tests
* [ ] RapidCheck logger property tests
* [ ] RapidCheck health property tests
* [ ] RapidCheck lifetime property tests
* [ ] sanitizer-compatible test execution

First milestone success means:

```txt
SagaDiagnostics can log structured events, retain recent events, record health metrics, produce health snapshots, track object lifetime, detect alive records, verify deterministic behavior with GoogleTest, and verify core invariants with RapidCheck.
```

That is the minimum serious foundation.

Anything less is just a name pretending to be infrastructure.

---

## 11. Final Architecture Rule

`SagaDiagnostics` owns runtime observability and reliability reporting.

Server, runtime, networking, persistence, and stress tools report into it.

They do not redefine it.

The purpose of `SagaDiagnostics` is not to make the engine look mature.

The purpose is to make SagaEngine capable of explaining failure, degradation, leaks, crashes, and long-running server instability before they become invisible production disasters.
