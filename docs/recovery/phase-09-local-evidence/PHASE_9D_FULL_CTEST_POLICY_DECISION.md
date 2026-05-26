# Phase 9D-4 Raw Full CTest Policy Decision

> Last updated: 2026-05-26
> Status: Raw full CTest retry unsafe; normal local gate defined
> Phase 9: Local Evidence Gates

This document records the Phase 9D-4 raw full CTest policy decision after the
Phase 9D-2 normal local gate passed with heavy label exclusion and
`CSharpGameplayProofTests` passed inside that gate.

## Decision

Decision B/C is selected: raw full CTest retry is unsafe for this local Phase 9
evidence path and remains blocked by heavy-test policy.

Do not rerun raw full CTest as Phase 9 pass evidence in this continuation.

The normal local gate is:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -LE "stress|slow|load|long-running|benchmark|perf" --output-on-failure --timeout 120 -j 1
```

This command passed 36/36 in Phase 9D-2.

## Rationale

The previous raw full CTest attempts did not complete and caused
terminal/session instability before CTest returned a complete summary. The
Phase 9D-2 normal gate was stable, but raw full CTest would reintroduce the
heavy entries that the normal gate deliberately excludes.

Raw full CTest includes:

- `ReplicationTests`, labeled `replication;integration;slow;long-running`
- `StressTests`, labeled `stress;load;slow`

Those entries are classified as opt-in heavy and are not part of the normal
local gate.

## Policy

Normal local gate:

- excludes `stress`, `slow`, `load`, `long-running`, `benchmark`, and `perf`
- is immediate local evidence when run serially with `--timeout 120 -j 1`
- may support limited Phase 9 closure when paired with focused evidence and
  explicit non-claims

Opt-in heavy gate:

- includes `StressTests`
- includes `ReplicationTests` because it is `slow;long-running`
- must be requested intentionally
- must not be used as default local or default CI evidence until separately
  stabilized

Raw full CTest:

- remains unresolved
- is not pass evidence
- must not be represented as passing without a complete passing run
- is unsafe as a default local gate on this machine after the documented
  terminal/session instability

## Non-Claims

This raw full CTest policy does not claim:

- full CTest pass evidence
- `ReplicationTests` pass evidence in this continuation
- `StressTests` pass evidence in this continuation
- stress, load, slow, long-running, benchmark, perf, package, publish, release,
  or broad CI readiness

## Verification

Evidence used:

- Phase 9D-2 normal local gate passed 36/36
- `CSharpGameplayProofTests` passed inside Phase 9D-2
- Phase 9D stability blocker documented previous raw full CTest instability
- CTest labels identify `ReplicationTests` and `StressTests` as heavy opt-in
  entries

Follow-up verification for this docs slice:

- `git diff --check`
- touched-doc trailing whitespace scan
- targeted phrase scan for `raw full CTest policy`, `unsafe`,
  `opt-in heavy`, and `normal local gate`
