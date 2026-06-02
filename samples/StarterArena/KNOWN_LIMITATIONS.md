# StarterArena Known Limitations

- StarterArena has only a bounded headless runtime smoke loop.
- StarterArena is not an interactive playable game.
- No StarterArena scene or gameplay resource is tracked yet.
- No launch profile is declared for StarterArena.
- `SagaLaunchLab` currently exposes server/headless launch commands, not a
  runtime/client sample launch command.
- The `--starter-arena-smoke` path bypasses `ClientHost`, renderer setup, and
  UDP/client networking.
- The smoke loop uses built-in deterministic movement and bounds; it does not
  load a scene file.
- Restart behavior is reported as deferred.
- No C# scripts, Visual Blocks, editor workflow, server-authoritative
  multiplayer, package output, distribution output, or fake runtime smoke
  evidence exists for this sample.
