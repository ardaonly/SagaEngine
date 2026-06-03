# StarterArena Acceptance

Phase 10 acceptance is implemented-unverified through a bounded local
`SagaRuntime` smoke command:

```sh
build/RelWithDebInfo-0.0.9/bin/SagaRuntime --headless --project samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke --smoke-report-out /tmp/starter_arena_runtime_smoke.json --smoke-frames 30 --fixed-dt 0.016
```

Accepted evidence for this phase:

- the runtime command exits `0`;
- the smoke report has `status: Passed`;
- the report records `projectId: starter-arena`;
- the report records `scene.sceneId: starter-arena-local-loop`;
- the report records `project.sceneSource: ProjectSceneReference`;
- the report records spawn, final position, fixed frame count, bounds, input
  vector, clamp count, and scene expectation status;
- the report records non-claims for renderer, client/network, server authority,
  arbitrary C# script invocation, C# lifecycle execution, Visual Blocks,
  editor workflow, package output, and distribution output.

This is not acceptance for interactive gameplay, a reusable scene system,
package launch, editor launch, or server-authoritative multiplayer.

Phase 11A acceptance is implemented-unverified through SagaScript compile and
analyze evidence only. Accepted evidence for Phase 11A:

- `sagaproject validate` accepts the `Scripts` folder reference;
- `sagascript analyze` exits `0` and writes analysis diagnostics;
- `sagascript compile` exits `0` and writes script manifests plus
  `StarterArenaScripts.scripts.dll`;
- docs keep runtime script execution, C# gameplay binding, Visual Blocks,
  server multiplayer, editor workflow, and package output as unsupported.

Phase 11B acceptance is implemented-unverified through runtime script metadata
consumption only. Accepted evidence for Phase 11B:

- `sagascript compile` emits `script_bindings.json` and `script_artifacts.json`;
- `SagaRuntime --starter-arena-smoke` accepts both script metadata paths;
- the smoke report records `scriptBinding.status: Passed`;
- the smoke report records `script://starter-arena/game-rules`,
  `StarterArena.Scripts.GameRules`, `AddPickupScore`, and `net10.0`;
- the smoke report records `execution: NotExecuted`;
- incomplete or missing script metadata inputs fail with clear diagnostics.

Phase 11C acceptance is implemented-unverified through one controlled runtime
C# method invocation. Accepted evidence for Phase 11C:

- the managed runtime bridge builds successfully;
- `sagascript compile` emits `StarterArenaScripts.scripts.dll` plus manifests;
- `SagaRuntime --starter-arena-smoke --invoke-starter-arena-script` exits `0`
  when run in an environment where `hostfxr` is discoverable;
- the smoke report records `scriptBinding.execution: Invoked`;
- the smoke report records `scriptInvocation.method: AddPickupScore`;
- the smoke report records `scriptInvocation.arguments: [10, 5]`;
- the smoke report records `scriptInvocation.result: 15`;
- incomplete or missing script metadata inputs fail before invocation and
  report `scriptBinding.execution: NotExecuted`.

This is not acceptance for arbitrary script invocation, C# lifecycle execution,
interactive gameplay, Visual Blocks, editor workflow, package launch, or
server-authoritative multiplayer.
