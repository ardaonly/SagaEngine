# StarterArena Runtime Smoke Sample

`StarterArena` is a future sample definition for project metadata validation.
It now has a narrow `SagaRuntime` smoke seam for Phase 10.

This is not an interactive game. The runtime smoke command consumes the
`.sagaproj` file and the declared scene resource at
`Scenes/arena.scene.json`, runs a deterministic scene-backed local loop in
headless mode, writes a smoke report, and exits. The sample also has one C#
script for SagaScript compile/analyze evidence. When script manifests are
provided, the runtime smoke consumes script metadata and reports
`execution: NotExecuted`. The runtime smoke does not load or execute that
script, and the sample does not use renderer, client networking, server
authority, Visual Blocks, editor workflow, package output, or distribution
output.

Runtime smoke command:

```sh
build/RelWithDebInfo-0.0.9/bin/SagaRuntime --headless --project samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke --smoke-report-out /tmp/starter_arena_runtime_smoke.json --smoke-frames 30 --fixed-dt 0.016
```

The tracked directories exist only because the current project schema validates
the diagnostics and generated report paths:

- `Diagnostics`
- `Build/Reports`

The tracked scene resource exists only for the bounded runtime smoke seam:

- `Scenes/arena.scene.json`

The tracked script source exists only for SagaScript compile/analyze evidence:

- `Scripts/GameRules.cs`

Expected focused check:

```sh
nix-shell --run "Tools/SagaProjectKit/sagaproject validate --project samples/StarterArena/StarterArena.sagaproj --out /tmp/starter_arena_validate.json"
```

Focused SagaScript compile smoke:

```sh
nix-shell --run "SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY=Engine/Managed/SagaScript.RuntimeBridge/obj/Release/net10.0/SagaScript.RuntimeBridge.dll Tools/SagaScript/sagascript compile --source samples/StarterArena/Scripts --out /tmp/starter_arena_sagascript/Manifests --artifacts-out /tmp/starter_arena_sagascript/Artifacts/Scripts --project-root samples/StarterArena --assembly-name StarterArenaScripts --diagnostics /tmp/starter_arena_sagascript/sagascript_diagnostics.json --json"
```

Focused runtime script metadata smoke:

```sh
build/RelWithDebInfo-0.0.9/bin/SagaRuntime --headless --project samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke --script-manifest /tmp/starter_arena_sagascript/Manifests/script_bindings.json --script-artifacts /tmp/starter_arena_sagascript/Manifests/script_artifacts.json --smoke-report-out /tmp/starter_arena_script_binding_smoke.json --smoke-frames 30 --fixed-dt 0.016
```

Phase 10 acceptance notes are tracked in `ACCEPTANCE.md`. Known limitations are
tracked in `KNOWN_LIMITATIONS.md`.
