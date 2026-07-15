---
title: Diagnostics and reliability
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Diagnostics and reliability

Diagnostics are part of runtime ownership, not a log-file afterthought. The Diagnostics module provides structured reports, fault and crash context, server metrics, memory tracking, and resource snapshots that other systems can consume without redefining their own report formats.

## Crash and fault context

Crash context should capture stable identifiers and bounded state that can be collected safely during a failure. A crash report must distinguish observed facts from unavailable fields. Failure handling must not depend on complex recovery work succeeding after the process is already compromised.

Fault boundaries normalize expected failure paths into diagnostic records. They are not permission to suppress errors or continue after invariants required for correctness have failed.

## Operational and reliability reports

Operational reports describe the current diagnostic snapshot. Reliability reports join relevant fault, memory, resource, and service information for analysis. A report should remain deterministic enough for tests and automation, while making missing inputs explicit.

## Memory and resource snapshots

The current Diagnostics module exposes memory and resource trackers, deterministic snapshots, active-resource summaries, and report integration. Unit evidence covers byte/peak accounting, overflow and over-removal rejection, deterministic ordering, resource registration/release, invalid and double release handling, and inclusion in operational and reliability reports.

These facilities are foundations for diagnosis. They are not a general leak detector for every allocation or a production observability service by themselves. Code must register the resources a tracker is expected to report.

## Six-month rule

The durable contract is structured, deterministic, owner-provided diagnostic data. Individual sample report paths and one-off audit snapshots belong in evidence storage or Git history, not in the wiki.

See [Diagnostics, reliability, and observability](../reference/diagnostics-reliability-and-observability.md) for fault/crash boundaries, health, leak/resource snapshots, server-oriented reports, and evidence limits.
