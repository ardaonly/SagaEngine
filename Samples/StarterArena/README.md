# StarterArena Runtime Sample

`StarterArena` is a future sample definition for project metadata validation.
It has a narrow `SagaRuntime` headless smoke path and an explicit visible-frame
path.

This is not a complete game. The headless runtime smoke command consumes the
`.sagaproj` file and the declared scene resource at
`Scenes/arena.scene.json`, runs a deterministic scene-backed local loop in
headless mode, writes a smoke report, and exits. The sample also has one C#
script for SagaScript compile/analyze evidence. When script manifests are
provided, the runtime smoke can record script metadata only, invoke exactly one
known pure method with an explicit opt-in flag, or record focused `GameRules`
C# lifecycle evidence with a separate explicit opt-in flag. The invocation and
lifecycle flags can be used together, and the report records them independently.
This is not arbitrary script execution, broad interactive gameplay, Visual
Blocks, editor workflow, package install, package output, or distribution
output. The visible command consumes the same project and scene metadata,
advances the same deterministic simulation kernel, and renders an arena,
boundary markers, and a player marker through the existing render backend.
Visible mode accepts scene-authored deterministic input, a validated synthetic
JSON input script, or an explicit keyboard source. All three feed the same
fixed-step app-local simulation and visible player transform. Keyboard mode uses
the existing `InputManager`, SDL input backend, keyboard device, and action map;
it does not execute C#. Server authority evidence is tracked separately through
a bounded socket-free headless smoke.

The `Saga` Product Launcher exposes only the fixed 30-frame headless smoke,
fixed 30-frame visible synthetic-input action, and existing first-playable
check for a selected `starter-arena` project. Their reports are written under
launcher application-data/evidence roots, not this source project. Manual
keyboard capture and repository-only server-headless evidence remain CLI or
operator workflows, not primary launcher actions.

Runtime smoke command:

```sh
<build-dir>/bin/SagaRuntime --headless --project Samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke --smoke-report-out /tmp/starter_arena_runtime_smoke.json --smoke-frames 30 --fixed-dt 0.016
```

Visible frame command:

```sh
<build-dir>/bin/SagaRuntime --project Samples/StarterArena/StarterArena.sagaproj --starter-arena-playable --playable-frames 30 --playable-report-out /tmp/starter_arena_playable.json --fixed-dt 0.016
```

Deterministic synthetic input command:

```sh
<build-dir>/bin/SagaRuntime --project Samples/StarterArena/StarterArena.sagaproj --starter-arena-playable --playable-frames 30 --playable-input-source synthetic --playable-input-script Samples/StarterArena/Input/playable.synthetic-input.json --playable-report-out /tmp/starter_arena_playable_synthetic.json --fixed-dt 0.016
```

Manual keyboard command:

```sh
<build-dir>/bin/SagaRuntime --project Samples/StarterArena/StarterArena.sagaproj --starter-arena-playable --playable-input-source keyboard --playable-report-out /tmp/starter_arena_playable_keyboard.json --fixed-dt 0.016
```

Keyboard bindings are `W`, `A`, `S`, and `D`; `Escape` requests close. An idle
bounded keyboard run succeeds with `input.status: NoInputObserved`. Real keyboard
evidence remains pending until a manual report records source `keyboard`, status
`Passed`, `realDeviceObserved: true`, at least one input frame, and a changed
final position. Automated tests do not inject or claim a real key press.

Omit `--playable-frames` to keep the visible window open until it is closed.
The bounded form writes project, scene, input source and action counts,
simulation, backend, presented-frame,
draw-submission, player-marker, viewport, teardown, diagnostic, and non-claim
evidence. `--starter-arena-playable` rejects `--headless`; the headless contract
continues to use `--starter-arena-smoke`.

The tracked directories exist only because the current project schema validates
the diagnostics and generated report paths:

- `Diagnostics`
- `Build/Reports`

The tracked scene resource exists only for the bounded smoke and visible paths:

- `Scenes/arena.scene.json`

The tracked synthetic input resource is deterministic test source, not generated
output:

- `Input/playable.synthetic-input.json`

The tracked script source exists only for SagaScript compile/analyze evidence
and the focused C# lifecycle proof:

- `Scripts/GameRules.cs`

Expected focused check:

```sh
nix-shell --run "Tools/SagaProjectKit/sagaproject validate --project Samples/StarterArena/StarterArena.sagaproj --out /tmp/starter_arena_validate.json"
```

Focused SagaScript compile smoke:

```sh
nix-shell --run "dotnet build Engine/Managed/SagaScript.RuntimeBridge/SagaScript.RuntimeBridge.csproj -c Release"
nix-shell --run "SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY=Engine/Managed/SagaScript.RuntimeBridge/bin/Release/net10.0/SagaScript.RuntimeBridge.dll Tools/SagaScript/sagascript compile --source Samples/StarterArena/Scripts --out /tmp/starter_arena_sagascript/Manifests --artifacts-out /tmp/starter_arena_sagascript/Artifacts/Scripts --project-root Samples/StarterArena --assembly-name StarterArenaScripts --diagnostics /tmp/starter_arena_sagascript/sagascript_diagnostics.json --json"
```

Generated manifests, assemblies, diagnostics, runtime smoke reports, and
visible-frame reports should stay under temporary output roots such as
`/tmp/starter_arena_sagascript`; they must not be written into
`Samples/StarterArena`.

Focused runtime script metadata smoke:

```sh
<build-dir>/bin/SagaRuntime --headless --project Samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke --script-manifest /tmp/starter_arena_sagascript/Manifests/script_bindings.json --script-artifacts /tmp/starter_arena_sagascript/Manifests/script_artifacts.json --smoke-report-out /tmp/starter_arena_script_binding_smoke.json --smoke-frames 30 --fixed-dt 0.016
```

This records `scriptBinding.status: Passed` and
`scriptBinding.execution: MetadataOnly`. It does not load or invoke the C#
assembly.

Focused runtime script invocation smoke:

```sh
nix-shell --run "<build-dir>/bin/SagaRuntime --headless --project Samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke --script-manifest /tmp/starter_arena_sagascript/Manifests/script_bindings.json --script-artifacts /tmp/starter_arena_sagascript/Manifests/script_artifacts.json --invoke-starter-arena-script --smoke-report-out /tmp/starter_arena_script_invocation_smoke.json --smoke-frames 30 --fixed-dt 0.016"
```

This records controlled pure-method evidence under `scriptInvocation`:
`scriptInvocation.execution: Invoked`, method `AddPickupScore`, arguments
`[10, 5]`, and result `15`.

Focused runtime script lifecycle smoke:

```sh
nix-shell --run "<build-dir>/bin/SagaRuntime --headless --project Samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke --script-manifest /tmp/starter_arena_sagascript/Manifests/script_bindings.json --script-artifacts /tmp/starter_arena_sagascript/Manifests/script_artifacts.json --run-starter-arena-script-lifecycle --smoke-report-out /tmp/starter_arena_script_lifecycle_smoke.json --smoke-frames 30 --fixed-dt 0.016"
```

This records focused lifecycle evidence under `scriptLifecycle`:
`scriptLifecycle.execution: Invoked`, the `GameRules` script id/type, and the
`OnCreate`, `OnStart`, `OnUpdate`, and `OnDestroy` callbacks.

Focused combined invocation and lifecycle smoke:

```sh
nix-shell --run "<build-dir>/bin/SagaRuntime --headless --project Samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke --script-manifest /tmp/starter_arena_sagascript/Manifests/script_bindings.json --script-artifacts /tmp/starter_arena_sagascript/Manifests/script_artifacts.json --invoke-starter-arena-script --run-starter-arena-script-lifecycle --smoke-report-out /tmp/starter_arena_script_combined_smoke.json --smoke-frames 30 --fixed-dt 0.016"
```

This records `scriptBinding.execution: MetadataOnly` while reporting
`scriptInvocation.execution: Invoked` and `scriptLifecycle.execution: Invoked`
independently.

Focused gameplay-spine smoke:

```sh
nix-shell --run "<build-dir>/bin/SagaRuntime --headless --project Samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke --script-manifest /tmp/starter_arena_sagascript/Manifests/script_bindings.json --script-artifacts /tmp/starter_arena_sagascript/Manifests/script_artifacts.json --run-starter-arena-script-lifecycle --run-starter-arena-gameplay --smoke-report-out /tmp/starter_arena_gameplay_smoke.json --smoke-frames 30 --fixed-dt 0.016"
```

The opt-in gameplay proof keeps one managed `GameRules` instance alive across
the deterministic loop. Input-derived player position reaches the scene pickup,
and the capability-gated app-local state port records collection, score `10`,
and player state `powered` with ordered before/after mutations.

The same two gameplay flags may be combined with bounded visible synthetic
mode and valid script metadata. The visible report proves that the pickup was
submitted before collection, omitted afterward, and the powered player
material was submitted. Default visible mode still does not execute C#.

The invocation and lifecycle smokes require a .NET host environment; use the dev
shell unless `hostfxr` is already discoverable in the local environment.

## Product Shell first-playable diagnostics

The Saga Product Shell can compile the sample scripts into a generated output
root, run five mandatory runtime evidence profiles, then run the mandatory
in-process `starter-arena-visual-blocks-descriptor` profile and write one
consolidated summary:

```sh
<build-dir>/bin/Saga --first-playable-check --project Samples/StarterArena/StarterArena.sagaproj --runtime-executable <build-dir>/bin/SagaRuntime --sagascript-tool Tools/SagaScript/sagascript --runtime-bridge-assembly <build-dir>/Managed/SagaScript.RuntimeBridge/SagaScript.RuntimeBridge.dll --first-playable-output /tmp/saga-first-playable
```

The command covers headless smoke, controlled invocation and lifecycle,
headless gameplay, visible synthetic input, visible gameplay reflection, and
read-only generation of a 12-block descriptor from C# and generated evidence.
`Scripts/GameRules.cs` is the only authored source of truth. The Product Shell
cross-checks the original and workspace C# hashes, callable metadata and
artifacts, lifecycle callbacks, and typed gameplay mutations before it creates
the in-memory catalog. There is no checked-in block JSON.

The consolidated summary exposes `capabilities.visualBlocksDescriptor`, while
the detailed generated proof is written to
`profiles/starter-arena-visual-blocks-descriptor/report.json`. That profile
records `processLaunched: false` and `csharpExecuted: false`; the five runtime
reports remain authoritative for actual C# lifecycle and gameplay execution.
Runtime JSON reports remain the evidence source. Compiler output, process
captures, profile reports, and `first_playable_summary.json` remain under the
selected generated directory and never under this sample source tree.

The generated descriptor makes exactly these non-claims: no Visual Blocks
canvas, graph execution, generated C# from blocks, editor graph editing,
production block library,
package install/distribution, networking/multiplayer, or production readiness.
It is read-only evidence outside the sample and contains no graph edges,
layout, instances, execution order, or generated code. Real keyboard input
remains `PendingManualEvidence` until a human-run keyboard report records real
device input and movement.

The release-candidate gate validates this existing sample project and writes
`first_playable_gate.json`, `source_manifest.json`, and
`evidence_manifest.json` beside the consolidated summary. A missing manual
keyboard report produces `AcceptedWithManualEvidencePending`; a supplied
report is imported only after its real-device, input-frame, and movement
evidence validates. Keyboard-provider behavior remains external test evidence
from `StarterArenaPlayableTests`.

The automated RC gate and the human operator capture are separate Product
Shell workflows. The automated command above may accept the proof spine while
leaving real keyboard evidence pending. The operator command opens the same
visible Runtime keyboard path, asks the operator to move with `W`, `A`, `S`, or
`D`, strictly validates the resulting real-device report, imports it into the
external evidence package, and then runs the authoritative RC gate:

```sh
<build-dir>/bin/Saga --first-playable-human-capture --project Samples/StarterArena/StarterArena.sagaproj --runtime-executable <build-dir>/bin/SagaRuntime --sagascript-tool Tools/SagaScript/sagascript --runtime-bridge-assembly <build-dir>/Managed/SagaScript.RuntimeBridge/SagaScript.RuntimeBridge.dll --first-playable-output /tmp/saga-first-playable-human
```

By default the visible capture is bounded to 600 frames and 30 seconds.
`Escape` may end it early after movement has been observed. A valid capture is
copied byte-for-byte to `manual/real_keyboard_report.json`, and its SHA-256 is
recorded in the evidence manifest. A previously captured report can instead be
validated without launching a second capture by adding
`--first-playable-keyboard-report <path>`. Valid human evidence can upgrade the
gate to `Accepted`; idle, synthetic, malformed, or non-moving evidence is
rejected. This focused workflow is not a full game, full editor, production
input UX, or a platform-wide keyboard/device compatibility claim. It does not
create projects, execute Visual Blocks graphs, or package/distribute a game.

Canonical release-candidate non-claims:

- No project creation workflow claim
- No full editor claim
- No Visual Blocks canvas claim
- No Visual Blocks runtime graph claim
- No generated C# from blocks claim
- No multiplayer claim
- No package install or distribution claim
- No production readiness claim

Focused server-authoritative smoke:

```sh
<build-dir>/bin/MultiplayerSandboxHeadless --project Samples/StarterArena/StarterArena.sagaproj --starter-arena-server-smoke --report-out /tmp/starter_arena_server_smoke.json --diagnostics-out /tmp/starter_arena_server_diagnostics --ticks 1 --fixed-dt 1.0
```

This is repository-only development/sample evidence. The executable is not
included in the default Linux package and packaged users must not expect this
command to exist. It is not a Product Shell launch action and is not emitted by
Product Workflow Smoke schema 2 or the Editor schema-2 inspection report.

This server smoke is local and deterministic. It proves server-owned state,
one accepted input, one rejected invalid input, and one authoritative snapshot
report. It is not full multiplayer gameplay and does not start an external
client.

Acceptance notes are tracked in `ACCEPTANCE.md`. Known limitations are tracked
in `KNOWN_LIMITATIONS.md`.
