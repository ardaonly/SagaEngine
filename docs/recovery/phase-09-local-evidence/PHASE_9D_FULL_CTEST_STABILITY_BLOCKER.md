# Phase 9D Full CTest Stability Blocker

> Last updated: 2026-05-26
> Status: Phase 9D environment/test-stability blocker
> Phase 9: Local Evidence Gates

Full CTest must not be treated as passing. It did not complete, and repeated
full CTest attempts caused terminal/session instability around the same part of
the run.

## Current Classification

This is not a full CTest pass.

This is not yet a classified test failure. No deterministic CTest assertion
failure output was captured, and CTest did not return a complete summary.

Current blocker classification:

`Phase 9D environment/test-stability blocker`

Phase 9 remains open. Phase 10 publish gate work is blocked until Phase 9
full-test/evidence status is known.

## List-Only Evidence

Only list-only CTest checks were run for this follow-up:

| Command | Result |
|---|---|
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -N` | 38 registered entries, including `ReplicationTests` and `StressTests`. |
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L stress` | 1 entry: `StressTests`. |
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L slow` | 2 entries: `ReplicationTests`, `StressTests`. |
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L load` | 1 entry: `StressTests`. |
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L benchmark` | 0 entries. |
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L perf` | 0 entries. |

The list-only inspection is not clean for a normal evidence gate because
stress, slow, and load entries are registered in the same raw full CTest
registry.

## CMake Label Evidence

`cmake/modules/SagaTests.cmake` currently registers:

- `CSharpGameplayProofTests` with labels `unit;runtime;scripting;csharp`
- `IntegrationTests` with labels
  `integration;runtime;server;networking;replication;timing-sensitive`
- `ReplicationTests` with labels `replication;integration;slow;long-running`
- `StressTests` with labels `stress;load;slow`

This means raw full CTest includes opt-in heavy entries unless the command
explicitly excludes them.

## Gate Consequence

Normal local evidence gates must exclude stress, slow, load, benchmark, and
performance-style labels by default. In the current registry, that specifically
means normal gates must exclude at least:

- `ReplicationTests`, because it is `slow;long-running`
- `StressTests`, because it is `stress;load;slow`

No safer normal-gate dry run was attempted in this follow-up because the
list-only inspection found registered stress/slow/load entries, so the
inspection was not clean enough to continue.

## CSharp Gameplay Proof Follow-Up

Both previous full CTest attempts reached `CSharpGameplayProofTests` before the
terminal/session ended. Since `CSharpGameplayProofTests` is labeled as a normal
runtime scripting C# test, it should be isolated later if it still appears near
the crash point after heavy labels are excluded.

That future isolation should be a separate controlled stability slice, not a
Phase 9E or Phase 9F closure step.

## Non-Claims

Phase 9D does not claim:

- full CTest pass evidence
- full normal local gate readiness
- CI readiness
- Phase 9 closure
- Phase 10 readiness
- release, stress, load, benchmark, performance, package, or publish readiness

## Stop Decision

Do not proceed to Phase 9E or Phase 9F while full CTest status is unknown and
the terminal/session stability blocker remains unresolved.

Do not run full CTest again until the heavy-label exclusion strategy and the
`CSharpGameplayProofTests` stability question are handled in a controlled
follow-up.

## Verification

Completed:

- list-only CTest registry check
- list-only `stress`, `slow`, `load`, `benchmark`, and `perf` label checks
- CMake/test/doc label inspection with `rg`

Deferred if safe:

- `git diff --check`
- touched-doc trailing whitespace scan
- targeted `rg` for this stability-blocker wording
