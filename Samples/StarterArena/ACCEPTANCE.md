# StarterArena Acceptance

StarterArena acceptance is intentionally narrow and evidence-based. The sample
is useful when each supported path exits successfully, writes deterministic
evidence, and keeps unsupported product claims explicit.

The Product Launcher may enter only the bounded smoke, synthetic-input visible,
and first-playable paths below after selecting this schema-0 `.sagaproj`.
Opening the project and opening SagaEditor remain separate operations. Generic
Runtime and all server/world/cloud rows stay disabled; repository-only
server-headless evidence is not launcher acceptance.

Accepted runtime smoke evidence:

- `SagaRuntime --headless --project Samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke` exits `0`;
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

- `SagaRuntime --project Samples/StarterArena/StarterArena.sagaproj --starter-arena-playable --playable-frames 30` exits `0` on a supported display/GPU environment;
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
- visible reports and generated resources stay outside `Samples/StarterArena`;
- the visible path uses only Runtime-owned app-local composition and does not
  use networking, editor code, or C# execution.

Accepted input-flow evidence:

- the default visible command keeps scene-authored input and its existing final
  position;
- `--playable-input-source synthetic` requires a valid
  `--playable-input-script` and produces the same result across bounded runs;
- the tracked 30-tick synthetic script records 15 `MoveRight` ticks, 15
  `MoveUp` ticks, `framesWithInput: 30`, and final position `(0.48, 0.48)` at
  fixed dt `0.016`;
- opposing actions cancel, diagonal movement is normalized, and movement remains
  clamped to scene bounds;
- fake-backend tests exercise keyboard events through `InputManager`,
  `InputActionMap`, and `GameplayInputFrame` without injecting OS events;
- the visible report preserves existing fields and adds input status, source,
  action counts, mapping, synthetic script path, input-frame count, and
  real-device observation;
- invalid sources and malformed synthetic scripts fail before rendering with
  explicit diagnostics;
- synthetic GPU evidence updates the player marker from the authoritative
  simulation position while preserving nonzero draw and pixel evidence;
- keyboard mode without a mapped key succeeds with
  `input.status: NoInputObserved` and `realDeviceObserved: false`.

Accepted human keyboard evidence capture:

- `Saga --first-playable-human-capture` launches the existing visible Runtime
  keyboard mode, or imports a report supplied explicitly by the operator;
- the operator moves the marker with a mapped key and may press Escape to end
  the bounded capture early;
- the resulting report records `input.source: keyboard`,
  `input.status: Passed`, `input.realDeviceObserved: true`, and
  `input.framesWithInput` greater than zero;
- the report records finite initial/final positions and measurable movement;
- the imported report hash exactly matches the validated captured or supplied
  report;
- valid human evidence changes the authoritative RC gate to `Accepted`;
- absent evidence remains `PendingManualEvidence` only for the normal automated
  RC command, while invalid capture evidence rejects the human-capture command.

Accepted SagaScript compile and metadata evidence:

- `sagaproject validate` accepts the `Scripts` folder reference;
- `sagascript analyze` exits `0` and writes analysis diagnostics;
- `sagascript compile` exits `0` and writes script manifests plus
  `StarterArenaScripts.scripts.dll`;
- generated metadata records `script://starter-arena/game-rules`,
  `StarterArena.Scripts.GameRules`, `AddPickupScore`, and `net10.0`;
- generated manifests, assemblies, diagnostics, and smoke reports stay outside
  `Samples/StarterArena`;
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
  package installation, or generated artifacts inside `Samples/StarterArena`.

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

Accepted gameplay-spine evidence:

- gameplay requires both `--run-starter-arena-gameplay` and
  `--run-starter-arena-script-lifecycle` plus valid metadata/artifacts;
- scene or synthetic input drives the existing simulation into the declared
  `starter.pickup.0` zone;
- managed `GameRules.OnUpdate` uses only the explicitly granted typed state
  port to collect the pickup, add score `10`, and set `powered` state;
- reports record the input-derived position, ordered typed mutations,
  lifecycle event/tick, initial/final state, and stable diagnostics;
- visible synthetic proof records the pickup draw before collection, its
  absence on the last frame, powered-player submission, and
  `gameplayStateReflected: true`;
- default smoke, metadata-only, lifecycle-only, and default visible modes do
  not claim or perform gameplay mutation.

Accepted Product Shell diagnostics evidence:

- `Saga --first-playable-check` owns subprocess execution and report parsing
  without including StarterArena runtime-private headers;
- all generated script artifacts, runtime reports, stdout/stderr captures, and
  the consolidated summary are written outside the sample source tree;
- smoke, lifecycle/invocation, gameplay, visible synthetic, visible gameplay,
  and the in-process Visual Blocks descriptor profile must all pass for the
  consolidated status to pass;
- runtime reports remain authoritative and malformed or contradictory reports
  fail with stable Product Shell diagnostics;
- the release-candidate gate writes a gate report, source manifest, and
  evidence manifest under a fresh generated output root;
- automated evidence may be accepted with real keyboard evidence explicitly
  pending; only a supplied and validated real-device report changes it to
  passed;
- the workflow validates the existing `.sagaproj`; there is no project
  creation workflow in this acceptance batch;
- `Scripts/GameRules.cs` is the authored source of truth; the strict 12-block
  representation catalog is generated in-process only after generated
  binding/artifact manifests and lifecycle/gameplay runtime evidence validate;
- the descriptor report stays under the selected generated output root and
  records `processLaunched: false` and `csharpExecuted: false`;
- descriptor generation does not launch a process, execute C#, edit the sample,
  require checked-in block JSON, or depend on Editor VisualScripting types;
- real keyboard evidence is reported as `PendingManualEvidence` and is not
  fabricated as passed;
- the human-capture operator workflow writes its report and raw process captures
  under the external evidence root, reuses the strict keyboard validator, and
  does not substitute synthetic or fake-keyboard test evidence;
- the operator workflow is focused human input proof, not production input UX,
  broad interactive gameplay, full editor behavior, project creation, or a
  device/platform compatibility matrix;
- the descriptor makes no canvas, graph execution, generated C# from blocks,
  editor graph editing, production block library, package install/distribution,
  networking/multiplayer, or production-readiness claim.

Accepted server-authoritative smoke evidence:

- `MultiplayerSandboxHeadless --starter-arena-server-smoke` exits `0`;
- this command is repository-only development evidence; the default Linux
  package intentionally does not include `MultiplayerSandboxHeadless`;
- it is not a Product Shell launch action, is absent from Product Workflow
  Smoke schema 2, and is not an Editor schema-2 action;
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

This is not acceptance for broad gameplay scripting, arbitrary world/ECS
access, a reusable scene system, arbitrary C# execution, Visual Blocks, package launch,
package install, editor launch, full multiplayer gameplay, external
client/server networking, or MMO-scale networking.
