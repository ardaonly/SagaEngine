# StarterArena Acceptance

StarterArena acceptance is intentionally narrow and evidence-based. The sample
is useful when each supported path exits successfully, writes deterministic
evidence, and keeps unsupported product claims explicit.

Accepted runtime smoke evidence:

- `SagaRuntime --headless --project samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke` exits `0`;
- the smoke report has `status: Passed`;
- the report records `project.projectId: starter-arena`;
- the report records `project.projectPath`;
- the report records `scene.sceneId: starter-arena-local-loop`;
- the report records scene source, player spawn, bounds, and expected result;
- the report records `project.sceneSource: ProjectSceneReference`;
- the report records spawn, final position, fixed frame count, bounds, input
  vector, clamp count, and scene expectation status;
- the report keeps renderer, client/network, arbitrary script execution,
  Visual Blocks, editor workflow, package install, package output, and
  distribution output as non-claims.

Accepted visible-frame evidence:

- `SagaRuntime --project samples/StarterArena/StarterArena.sagaproj --starter-arena-playable --playable-frames 30` exits `0` on a supported display/GPU environment;
- `--starter-arena-playable` is rejected when combined with `--headless`;
- the visible report records the same project id, scene id, spawn, bounds,
  camera, fixed timestep, test input, final position, and clamp count used by
  headless smoke;
- the report records a concrete initialized backend, exactly the requested
  presented-frame count, nonzero draw submissions, and
  `playerDrawSubmitted: true`;
- the report records completed render-resource and backend shutdown;
- GPU evidence contains non-clear arena pixels and distinct ground, player,
  and boundary color regions;
- visible reports and generated resources stay outside `samples/StarterArena`;
- the visible path does not use `ClientHost`, networking, input devices,
  editor code, or C# execution.

Accepted SagaScript compile and metadata evidence:

- `sagaproject validate` accepts the `Scripts` folder reference;
- `sagascript analyze` exits `0` and writes analysis diagnostics;
- `sagascript compile` exits `0` and writes script manifests plus
  `StarterArenaScripts.scripts.dll`;
- generated metadata records `script://starter-arena/game-rules`,
  `StarterArena.Scripts.GameRules`, `AddPickupScore`, and `net10.0`;
- generated manifests, assemblies, diagnostics, and smoke reports stay outside
  `samples/StarterArena`;
- metadata-only runtime smoke records `scriptBinding.status: Passed` and
  `scriptBinding.execution: MetadataOnly`;
- incomplete or missing script metadata inputs fail with clear diagnostics.

Accepted controlled C# invocation evidence:

- the managed runtime bridge builds successfully;
- `SagaRuntime --starter-arena-smoke --invoke-starter-arena-script` exits `0`
  when run in an environment where `hostfxr` is discoverable;
- the smoke report records `scriptInvocation.status: Passed`;
- the smoke report records `scriptInvocation.execution: Invoked`;
- the smoke report records
  `scriptInvocation.scriptId: script://starter-arena/game-rules`;
- the smoke report records `scriptInvocation.method: AddPickupScore`;
- the smoke report records `scriptInvocation.arguments: [10, 5]`;
- the smoke report records `scriptInvocation.result: 15`;
- incomplete or missing script metadata inputs fail before invocation and
  report `scriptInvocation.execution: NotExecuted`.

Accepted C# lifecycle evidence:

- `CSharpGameplayProofTests` compiles the real `Scripts/GameRules.cs` through
  `Tools/SagaScript/sagascript compile` in a temporary project root;
- the generated `script_artifacts.json` is accepted by
  `ScriptLifecycleService` and loaded by `CSharpScriptHost`;
- `StarterArena.Scripts.GameRules` is created from
  `script://starter-arena/game-rules`;
- the test invokes create, start, update, and destroy lifecycle methods;
- deterministic log evidence records the `GameRules` lifecycle callbacks;
- the test does not require graphics, GPU, editor UI, networking, multiplayer,
  package installation, or generated artifacts inside `samples/StarterArena`.

Accepted runtime smoke lifecycle evidence:

- `SagaRuntime --starter-arena-smoke --run-starter-arena-script-lifecycle`
  exits `0` when run with valid script metadata in an environment where
  `hostfxr` is discoverable;
- the smoke report records `scriptLifecycle.status: Passed`;
- the smoke report records
  `scriptLifecycle.scriptId: script://starter-arena/game-rules`;
- the smoke report records
  `scriptLifecycle.typeName: StarterArena.Scripts.GameRules`;
- the smoke report records `scriptLifecycle.execution: Invoked`;
- the smoke report records observed `OnCreate`, `OnStart`, `OnUpdate`, and
  `OnDestroy` callbacks;
- running the same smoke without the lifecycle opt-in reports
  `scriptLifecycle.status: NotRequested`;
- missing or invalid script metadata fails before lifecycle execution.

Accepted combined runtime smoke evidence:

- `SagaRuntime --starter-arena-smoke --invoke-starter-arena-script --run-starter-arena-script-lifecycle`
  exits `0` when run with valid script metadata in an environment where
  `hostfxr` is discoverable;
- the smoke report records `scriptBinding.execution: MetadataOnly`;
- the smoke report records `scriptInvocation.status: Passed` and
  `scriptInvocation.execution: Invoked`;
- the smoke report records `scriptLifecycle.status: Passed` and
  `scriptLifecycle.execution: Invoked`.

Accepted server-authoritative smoke evidence:

- `MultiplayerSandboxHeadless --starter-arena-server-smoke` exits `0`;
- the report records `projectId: starter-arena`;
- the report records `serverAuthority: true`;
- the report records `networkMode: HeadlessSmoke`;
- the report records `inputAcceptedCount: 1`;
- the report records `inputRejectedCount: 1`;
- the report records authoritative initial and final state;
- the report records `snapshotCount: 1`;
- the report records invalid input diagnostics;
- the report keeps full multiplayer, MMO proof, editor workflow, package
  install, package output, and Visual Blocks as non-claims.

This is not acceptance for interactive gameplay, a reusable scene system,
arbitrary C# execution, Visual Blocks, package launch,
package install, editor launch, full multiplayer gameplay, external
client/server networking, or MMO-scale networking.
