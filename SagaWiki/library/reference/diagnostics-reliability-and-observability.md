---
title: Diagnostics, reliability, and observability
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Diagnostics, reliability, and observability

Diagnostics is a runtime owner, not a collection of arbitrary log strings. Its contracts let modules report faults, crash context, health, memory/resource state, and operational snapshots without transferring control of those modules to a global diagnostics manager.

## Dependency direction

Runtime owners emit neutral diagnostic values or expose read-only snapshot interfaces. Diagnostics formats, aggregates, stores, or forwards bounded reports. Diagnostics may depend on low-level Core values; domain modules should not depend on a high-level server dashboard or editor widget just to report status.

Render, Resources, Replication, World, ServerAuthority, Persistence, Scripting, and programs remain responsible for their own recovery decisions. A diagnostic observer does not evict resources, resynchronize a client, or restart a zone unless an explicit control-plane contract exists.

## Stable diagnostic shape

A useful diagnostic includes:

- stable identifier/category;
- severity;
- owning subsystem;
- monotonic and/or wall-clock timestamp according to use;
- operation/session correlation where bounded;
- human-readable message;
- typed context fields with bounded names and values;
- a disposition such as continued, degraded, retried, stopped, or crashed.

Stable identifiers support tests and automation. Messages remain for humans and can change without breaking classifiers. Arbitrary exception text, file contents, secret values, addresses, and unbounded per-entity data are not metric labels.

## Fault boundaries

A fault boundary surrounds an operation whose failure can be classified and contained: project open, manifest load, script callback, worker task, frame capture, network decode, package preflight, or zone tick. It records entry context, catches only failures it can translate safely, restores invariants, and returns or escalates a structured result.

Catching every exception at a global loop and continuing is not reliability. If state may be partially mutated, the owner stops or rolls back that operation. Fatal invariant violation, corrupted authority state, or failed native lifecycle can terminate the relevant owner/process after crash context is captured.

Fault boundaries do not swallow programmer errors to make a test green. They distinguish expected operational failure from internal invariant failure.

## Crash context

Crash handling tries to preserve a bounded immutable snapshot of context available before or during fatal termination. Useful fields include build/version identity, program, project/profile identity when safe, active subsystem/operation, current frame/tick/session, thread identity, last stable diagnostic ids, and platform fault information.

Signal/exception context has severe restrictions. A low-level handler must not allocate freely, take ordinary locks, call complex logging code, or traverse mutable engine containers. Rich report assembly can occur through a safer out-of-process or postmortem path where implemented. The contract should state which portion is async-signal-safe rather than imply every report field is guaranteed after memory corruption.

Crash reports are generated evidence. They are not source truth and should not be committed by default. Sensitive paths, environment values, tokens, user data, and network payloads require redaction policy.

## Health model

Health describes current owner state using bounded values such as healthy, degraded, unavailable, stopping, or failed. It includes reason and freshness. A stale health sample is not silently presented as current.

Examples:

- graphics can be unavailable because the workflow intentionally requested headless mode, or failed because a required backend did not initialize;
- replication can be resynchronizing rather than healthy or terminal;
- resources can be over soft budget while still serving active borrows;
- a server-authority zone can be draining and rejecting new work;
- persistence can be unavailable while a workflow that requires it must stop.

Aggregation preserves severity and ownership. A process-level “healthy” status cannot hide a required failed child merely because optional children are healthy.

## Reliability lifecycle

An owner follows initialize, ready/running, quiesce, shutdown, and terminal failure states as appropriate. Work admission closes before dependencies disappear. Queued and in-flight work is drained, cancelled, or handed off under explicit policy. Repeated shutdown is safe.

Watchdog or heartbeat values observe progress but do not assume a slow operation is dead without a deadline contract. Timeouts use monotonic time. Retry has a bound, backoff, and failure class; it does not loop forever on invalid input.

## Lifetime leak diagnostics

Registries and managers can expose shutdown snapshots of objects that remain registered or referenced: graphics handles, residency entries, script contexts, connections, locks, transactions, or jobs. A leak report names the owner, object kind, stable/debug identity, expected lifetime, and observed count/bytes where available.

The report is evidence, not an automatic destructor. Diagnostics must not free an object it does not own. Some live objects can be deliberate at a process boundary; the owner defines expected zero/nonzero conditions.

Approximate logical bytes are labeled approximate. Native allocator or GPU totals require native instrumentation. Adding registry sizes does not produce a truthful total-process memory value.

## Memory and resource snapshots

A snapshot is a point-in-time copied view. Useful categories include:

- allocator/domain logical bytes and peak where measured;
- resource residency bytes, budget, entries, hits/misses, and evictions;
- render CPU, staging, and GPU-accounted values separately;
- replication queue/history bytes and configured limits;
- world active/pending object counts;
- script assembly/context counts and reload generations;
- persistence pool/queue counts without credentials or query data.

Snapshot collection must be bounded in time and memory. It should avoid holding central locks during formatting or disk I/O. If a complete consistent snapshot cannot be taken cheaply, the report says values are independently sampled rather than pretending atomicity.

## Operational reports

An operational report combines current health, selected counters, recent stable failure summaries, and configuration identity useful for running a program. It can be rendered as text or structured data, but the schema remains versioned and deterministic enough for tests.

Server-oriented reporting can include zone tick duration/overrun, active connections, command/rate rejection, replication throughput/resync, cross-zone peer state, persistence availability, and world/resource pressure. These fields are only present where the corresponding current owner exposes evidence.

Operational reporting is not a production observability platform. It does not claim remote collection, durable retention, alert routing, dashboards, SLOs, tracing infrastructure, or secure administrative access.

## Logging and cardinality

Logs explain individual events. Metrics summarize bounded dimensions. Traces correlate operations where implemented. Do not place project paths, entity ids, asset keys, query strings, actor ids, or exception messages into unbounded metric labels. Use counters plus sampled diagnostics or bounded top-N reports.

Rate-limit repeated identical diagnostics while retaining first occurrence and aggregate count. Suppression itself is observable so an incident does not look quiet merely because messages were dropped.

## ServerAuthority reliability

An authority owner must fail closed for commands it cannot validate. If a zone is unhealthy or draining, it stops accepting new actors/commands according to policy before shutdown. It preserves session/ownership invariants while connections close or transfer.

Cross-zone peer failure, persistence unavailability, replication backpressure, or tick overrun are distinct reasons. A generic “server error” is insufficient for operation and tests. Recovery must not double-apply commands or transfer authority to two owners.

The current repository has no `SagaServer` program entry point; these are library correctness boundaries, not deployed service claims.

## Tests

Focused diagnostics tests assert stable identifiers, severity, context bounds, deterministic serialization, redaction, suppression, and failure disposition. Reliability tests exercise partial initialization, repeated shutdown, worker failure, cancellation, timeout/retry bounds, late callback prevention, and leak snapshots.

Resource/memory tests verify accounting invariants and label approximate versus exact values. Server/library tests verify health transitions and fail-closed behavior with deterministic clocks. A crash-handler test can prove its bounded fixture path; it cannot prove safe recovery from every form of process corruption.

## Non-claims

Current diagnostics do not establish a hosted monitoring product, production incident response service, remote control plane, complete crash uploader, guaranteed crash recovery, exact whole-process memory accounting, or production server SLO. Reports should repeat the evidence level where ambiguity is likely.

## Change checklist

- Emit stable identifiers plus human messages.
- Keep recovery decisions with the owning module.
- Preserve async-signal and sensitive-data limits in crash paths.
- Make health freshness and degraded/unavailable distinctions explicit.
- Keep snapshots copied, bounded, and non-owning.
- Separate logs, metrics, and traces by cardinality needs.
- Test partial startup, shutdown, retry, cancellation, and late completion.
- Do not turn operational report vocabulary into a production service claim.
