# StarterArena Known Limitations

- StarterArena has only a bounded headless runtime smoke loop.
- StarterArena is not an interactive playable game.
- `Scenes/arena.scene.json` is a minimal smoke resource, not a broad runtime
  scene format.
- No launch profile is declared for StarterArena.
- `SagaLaunchLab` currently exposes server/headless launch commands, not a
  runtime/client sample launch command.
- The `--starter-arena-smoke` path bypasses `ClientHost`, renderer setup, and
  UDP/client networking.
- The smoke loop reads spawn, bounds, camera metadata, input, and expected
  results from the declared scene resource.
- `Scripts/GameRules.cs` is compile/analyze evidence and runtime metadata
  consumption evidence only.
- StarterArena runtime smoke reads script metadata but does not load or execute
  C# assemblies.
- Restart behavior is reported as deferred.
- No Visual Blocks, editor workflow, server-authoritative multiplayer, runtime
  C# gameplay binding, package output, distribution output, or fake runtime
  smoke evidence exists for this sample.
