# Phase 11 Evidence

## Status

Implemented-Unverified

## Phase Scope

C# Gameplay Script v1

Phase 11A added a narrow StarterArena C# script compile smoke. Phase 11B added
runtime smoke consumption of the emitted SagaScript binding and artifact
metadata. Phase 11C adds one controlled runtime invocation of a known pure
method: `StarterArena.Scripts.GameRules.AddPickupScore(10, 5)`.

Accepted boundary:

- one StarterArena script source only;
- compile/analyze evidence;
- runtime metadata consumption;
- one controlled pure-method invocation returning `15`;
- no arbitrary script invocation, C# lifecycle execution, Visual Blocks, editor
  workflow, server-authoritative multiplayer, package output, or distribution
  output.

## Changed Files

- `Apps/Runtime/main.cpp`
- `Engine/Managed/SagaScript.RuntimeBridge/NativeBridge.cs`
- `Engine/Public/SagaEngine/Scripting/CSharpScriptHost.hpp`
- `Engine/Public/SagaEngine/Scripting/LowLevel/ISagaScriptHost.hpp`
- `Engine/Private/SagaEngine/Scripting/CSharpScriptHost.cpp`
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
- `nix-shell --run "dotnet build Engine/Managed/SagaScript.RuntimeBridge/SagaScript.RuntimeBridge.csproj -c Release"`
- `nix-shell --run "SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY=Engine/Managed/SagaScript.RuntimeBridge/bin/Release/net10.0/SagaScript.RuntimeBridge.dll Tools/SagaScript/sagascript compile --source samples/StarterArena/Scripts --out /tmp/starter_arena_sagascript/Manifests --artifacts-out /tmp/starter_arena_sagascript/Artifacts/Scripts --project-root samples/StarterArena --assembly-name StarterArenaScripts --diagnostics /tmp/starter_arena_sagascript/sagascript_diagnostics.json --json"`
- `Tools/Forge/bin/forge nix build --target SagaRuntime --build build/RelWithDebInfo-0.0.9 --jobs=1`
- `nix-shell --run "build/RelWithDebInfo-0.0.9/bin/SagaRuntime --headless --project samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke --script-manifest /tmp/starter_arena_sagascript/Manifests/script_bindings.json --script-artifacts /tmp/starter_arena_sagascript/Manifests/script_artifacts.json --invoke-starter-arena-script --smoke-report-out /tmp/starter_arena_script_invocation_smoke.json --smoke-frames 30 --fixed-dt 0.016"`
- `nix-shell --run "build/RelWithDebInfo-0.0.9/bin/SagaRuntime --headless --project samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke --script-manifest /tmp/starter_arena_sagascript/Manifests/script_bindings.json --invoke-starter-arena-script --smoke-report-out /tmp/starter_arena_script_invocation_incomplete_smoke.json --smoke-frames 1 --fixed-dt 0.016"`
- `nix-shell --run "build/RelWithDebInfo-0.0.9/bin/SagaRuntime --headless --project samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke --script-manifest /tmp/starter_arena_sagascript/Manifests/missing_script_bindings.json --script-artifacts /tmp/starter_arena_sagascript/Manifests/script_artifacts.json --invoke-starter-arena-script --smoke-report-out /tmp/starter_arena_script_invocation_missing_smoke.json --smoke-frames 1 --fixed-dt 0.016"`
- `git diff --check`
- `scripts/scan-claims README.md docs samples Tools`
- `scripts/verify-quick`
- `scripts/verify-local --allow-dirty`
- `scripts/verify-phase 11`

## Command Results

The managed runtime bridge build passed. `sagascript compile` exited `0`,
wrote script manifests, copied the updated runtime bridge, and emitted
`StarterArenaScripts.scripts.dll`,
`StarterArenaScripts.scripts.pdb`, and
`StarterArenaScripts.scripts.runtimeconfig.json`.

The raw runtime invocation command outside the dev shell failed with
`Script.Host.HostFxrMissing`; this is recorded as an environment requirement,
not an implementation result. The same `SagaRuntime` invocation under
`nix-shell` exited `0` and wrote
`/tmp/starter_arena_script_invocation_smoke.json` with:

- `status: Passed`;
- `scriptBinding.status: Passed`;
- `scriptBinding.execution: Invoked`;
- `scriptInvocation.status: Passed`;
- `scriptInvocation.method: AddPickupScore`;
- `scriptInvocation.arguments: [10, 5]`;
- `scriptInvocation.result: 15`;
- empty diagnostics.

Negative smoke checks for incomplete and missing script metadata failed
deterministically before invocation and wrote failed reports with
`scriptBinding.execution: NotExecuted`.

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
- [x] Only `AddPickupScore(10, 5)` is invoked.
- [x] Unsupported behavior is not hidden.

## Known Limitations

See `known_limitations.md`.

## Verification Decision

Implemented-Unverified

## Decision Reason

The focused StarterArena SagaScript compile smoke passes locally, SagaRuntime
consumes emitted script metadata, and the dev-shell runtime smoke invokes one
known pure C# method with the expected result. Arbitrary script invocation,
C# lifecycle execution, Visual Blocks, editor workflow, server authority,
package output, distribution output, and maintainer verification remain absent.
