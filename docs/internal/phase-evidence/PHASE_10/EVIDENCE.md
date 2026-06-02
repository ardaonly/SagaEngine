# Phase 10 Evidence

## Status

Implemented-Unverified

## Phase Scope

First Playable Runtime Loop

Phase 10 has a narrow app-local runtime smoke seam. `SagaRuntime` now accepts a
bounded `--starter-arena-smoke` mode that consumes
`samples/StarterArena/StarterArena.sagaproj`, runs a deterministic built-in
local loop in headless mode, writes smoke evidence, and exits without entering
`ClientHost` or UDP/client networking.

Accepted boundary:

- app-local `SagaRuntime` CLI seam only;
- no reusable scene system;
- no renderer/client/network dependency;
- no C# gameplay scripts, Visual Blocks, editor workflow, server authority,
  package output, or distribution output;
- not proof of interactive gameplay.

## Changed Files

- `samples/StarterArena/README.md`
- `samples/StarterArena/ACCEPTANCE.md`
- `samples/StarterArena/KNOWN_LIMITATIONS.md`
- `Apps/Runtime/main.cpp`
- `docs/internal/PHASE_STATUS_MATRIX.md`
- `docs/internal/phase-evidence/PHASE_10/EVIDENCE.md`
- `docs/internal/phase-evidence/PHASE_10/commands.log`
- `docs/internal/phase-evidence/PHASE_10/changed_files.txt`
- `docs/internal/phase-evidence/PHASE_10/known_limitations.md`
- `docs/internal/phase-evidence/PHASE_10/verification_result.json`

## Verification Commands

- `Tools/Forge/bin/forge nix build --target SagaRuntime --build build/RelWithDebInfo-0.0.9 --jobs=1`
- `build/RelWithDebInfo-0.0.9/bin/SagaRuntime --headless --project samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke --smoke-report-out /tmp/starter_arena_runtime_smoke.json --smoke-frames 30 --fixed-dt 0.016`
- `build/RelWithDebInfo-0.0.9/bin/SagaRuntime --headless --starter-arena-smoke --smoke-report-out /tmp/starter_arena_missing_project_smoke.json --smoke-frames 1 --fixed-dt 0.016`
- `build/RelWithDebInfo-0.0.9/bin/SagaRuntime --project samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke --smoke-report-out /tmp/starter_arena_missing_headless_smoke.json --smoke-frames 1 --fixed-dt 0.016`
- `build/RelWithDebInfo-0.0.9/bin/SagaRuntime --headless --project samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke --smoke-report-out /tmp/starter_arena_zero_frames_smoke.json --smoke-frames 0 --fixed-dt 0.016`
- `git diff --check`
- `scripts/scan-claims README.md docs samples Tools`
- `scripts/verify-quick`
- `scripts/verify-local --allow-dirty`
- `scripts/verify-phase 10`

## Command Results

The focused `SagaRuntime` build passed. The real StarterArena smoke command
exited `0` and wrote `/tmp/starter_arena_runtime_smoke.json` with
`status: Passed`, project id `starter-arena`, 30 frames, final position
`(1.0, 0.96)`, bounds `[-1, 1]`, and 15 clamp events. Negative smoke checks
failed deterministically and wrote failed reports. Required local checks passed.
No phase was marked `Verified`.

## Required Files

- `EVIDENCE.md`: present
- `commands.log`: present
- `changed_files.txt`: present
- `known_limitations.md`: present
- `verification_result.json`: present

## Manual Checks

- [x] Public docs do not overclaim.
- [x] Known limitations are documented.
- [x] No placeholder is presented as shipped behavior.
- [x] Runtime behavior is app-local and bounded.
- [x] Unsupported behavior is not hidden.

## Known Limitations

See `known_limitations.md`.

## Verification Decision

Implemented-Unverified

## Decision Reason

The bounded StarterArena runtime smoke seam is implemented and passes locally,
but maintainer verification has not accepted the phase.
