# StarterArena Known Limitations

- StarterArena has only a bounded headless runtime smoke loop.
- StarterArena is not an interactive playable game.
- StarterArena has one bounded socket-free server-authoritative smoke, not full
  multiplayer gameplay.
- `Scenes/arena.scene.json` is a minimal smoke resource, not a broad runtime
  scene format.
- No launch profile is declared for StarterArena.
- `SagaLaunchLab` currently exposes server/headless launch commands, not a
  runtime/client sample launch command.
- The `--starter-arena-smoke` path bypasses `ClientHost`, renderer setup, and
  UDP/client networking.
- The smoke loop reads spawn, bounds, camera metadata, input, and expected
  results from the declared scene resource.
- `Scripts/GameRules.cs` is compile/analyze evidence, one controlled runtime
  pure-method invocation smoke, and one focused C# lifecycle proof.
- StarterArena runtime smoke can load the compiled C# assembly only when
  `--invoke-starter-arena-script` is passed with valid script metadata.
- The only invoked method is `GameRules.AddPickupScore(10, 5)`, and the smoke
  expects result `15`.
- StarterArena has only the focused `GameRules` C# lifecycle proof; arbitrary
  script methods remain unsupported.
- The controlled invocation smoke requires a .NET host environment where
  `hostfxr` is discoverable.
- Restart behavior is reported as deferred.
- No Visual Blocks, editor workflow, external client/server networking, broad
  runtime C# gameplay loop binding, package output, distribution output, or
  fake runtime smoke evidence exists for this sample.
