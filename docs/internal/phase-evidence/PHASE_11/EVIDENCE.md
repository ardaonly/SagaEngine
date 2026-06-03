# Phase 11 Evidence

## Status

Implemented-Unverified

## Phase Scope

C# Gameplay Script v1

Phase 11A added a narrow StarterArena C# script compile smoke. Phase 11B adds
runtime smoke consumption of the emitted SagaScript binding and artifact
metadata. SagaRuntime reads `script_bindings.json` and `script_artifacts.json`
when explicitly passed on the StarterArena smoke command line, records the
GameRules metadata in the smoke report, and keeps execution as `NotExecuted`.

Accepted boundary:

- one StarterArena script source only;
- compile/analyze evidence plus runtime metadata consumption;
- no C# assembly load, method invocation, runtime script execution, or C#
  gameplay binding;
- no Visual Blocks, editor workflow, server-authoritative multiplayer, package
  output, or distribution output.

## Changed Files

- `Apps/Runtime/main.cpp`
- `samples/StarterArena/Scripts/GameRules.cs`
- `samples/StarterArena/StarterArena.sagaproj`
- `samples/StarterArena/README.md`
- `samples/StarterArena/ACCEPTANCE.md`
- `samples/StarterArena/KNOWN_LIMITATIONS.md`
- `docs/internal/PHASE_STATUS_MATRIX.md`
- `docs/internal/phase-evidence/PHASE_11/EVIDENCE.md`
- `docs/internal/phase-evidence/PHASE_11/commands.log`
- `docs/internal/phase-evidence/PHASE_11/changed_files.txt`
- `docs/internal/phase-evidence/PHASE_11/known_limitations.md`
- `docs/internal/phase-evidence/PHASE_11/verification_result.json`

## Verification Commands

- `nix-shell --run "Tools/SagaProjectKit/sagaproject validate --project samples/StarterArena/StarterArena.sagaproj --out /tmp/starter_arena_validate.json"`
- `nix-shell --run "Tools/SagaScript/sagascript analyze --source samples/StarterArena/Scripts --out /tmp/starter_arena_sagascript --json"`
- `nix-shell --run "SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY=Engine/Managed/SagaScript.RuntimeBridge/obj/Release/net10.0/SagaScript.RuntimeBridge.dll Tools/SagaScript/sagascript compile --source samples/StarterArena/Scripts --out /tmp/starter_arena_sagascript/Manifests --artifacts-out /tmp/starter_arena_sagascript/Artifacts/Scripts --project-root samples/StarterArena --assembly-name StarterArenaScripts --diagnostics /tmp/starter_arena_sagascript/sagascript_diagnostics.json --json"`
- `Tools/Forge/bin/forge nix build --target SagaRuntime --build build/RelWithDebInfo-0.0.9 --jobs=1`
- `build/RelWithDebInfo-0.0.9/bin/SagaRuntime --headless --project samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke --script-manifest /tmp/starter_arena_sagascript/Manifests/script_bindings.json --script-artifacts /tmp/starter_arena_sagascript/Manifests/script_artifacts.json --smoke-report-out /tmp/starter_arena_script_binding_smoke.json --smoke-frames 30 --fixed-dt 0.016`
- `build/RelWithDebInfo-0.0.9/bin/SagaRuntime --headless --project samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke --script-manifest /tmp/starter_arena_sagascript/Manifests/script_bindings.json --smoke-report-out /tmp/starter_arena_script_binding_incomplete_smoke.json --smoke-frames 1 --fixed-dt 0.016`
- `build/RelWithDebInfo-0.0.9/bin/SagaRuntime --headless --project samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke --script-manifest /tmp/starter_arena_sagascript/Manifests/missing_script_bindings.json --script-artifacts /tmp/starter_arena_sagascript/Manifests/script_artifacts.json --smoke-report-out /tmp/starter_arena_script_binding_missing_smoke.json --smoke-frames 1 --fixed-dt 0.016`
- `git diff --check`
- `scripts/scan-claims README.md docs samples Tools`
- `scripts/verify-quick`
- `scripts/verify-local --allow-dirty`
- `scripts/verify-phase 11`

## Command Results

`sagaproject validate` passed with no diagnostics. `sagascript analyze` exited
`0` and wrote `/tmp/starter_arena_sagascript/analysis_report.json`; it reported
one non-blocking warning that no `[SagaBehavior]` metadata exists. `sagascript
compile` exited `0`, wrote script manifests, copied the runtime bridge, and
emitted `StarterArenaScripts.scripts.dll`,
`StarterArenaScripts.scripts.pdb`, and
`StarterArenaScripts.scripts.runtimeconfig.json` with no blocking diagnostics.
The metadata-backed StarterArena runtime smoke exited `0` and wrote
`/tmp/starter_arena_script_binding_smoke.json` with
`scriptBinding.status: Passed`, script id `script://starter-arena/game-rules`,
type `StarterArena.Scripts.GameRules`, callable method `AddPickupScore`,
target framework `net10.0`, and `execution: NotExecuted`. Negative smoke checks
for incomplete and missing script metadata failed deterministically and wrote
failed reports.

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
- [x] Runtime execution remains explicitly unsupported and reported as
  `NotExecuted`.
- [x] Unsupported behavior is not hidden.

## Known Limitations

See `known_limitations.md`.

## Verification Decision

Implemented-Unverified

## Decision Reason

The focused StarterArena SagaScript analyze and compile smoke passes locally,
and SagaRuntime consumes the emitted script metadata in the StarterArena smoke
report. Runtime script execution and maintainer verification remain absent.
