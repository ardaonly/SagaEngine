# StarterArena Known Limitations

- StarterArena has a bounded headless runtime smoke loop and a separate visible
  frame mode.
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
- The `--starter-arena-playable` path also bypasses `ClientHost` and networking,
  but initializes the existing platform window and render backend.
- Visible mode uses scene test input for deterministic motion. It does not yet
  read keyboard, mouse, or gamepad actions.
- Visible mode renders app-local procedural arena geometry. It does not prove
  cooked asset loading or a reusable scene renderer.
- Script metadata, invocation, and lifecycle flags remain headless smoke modes;
  visible mode rejects them until world mutation is integrated explicitly.
- The smoke loop reads spawn, bounds, camera metadata, input, and expected
  results from the declared scene resource.
- `Scripts/GameRules.cs` is compile/analyze evidence, one controlled runtime
  pure-method invocation smoke, one focused C# lifecycle proof, and optional
  runtime smoke lifecycle evidence. Runtime invocation and lifecycle evidence
  can be requested together, but they remain separate report sections.
- StarterArena runtime smoke can load the compiled C# assembly only when
  `--invoke-starter-arena-script` or
  `--run-starter-arena-script-lifecycle` is passed with valid script metadata.
- The only invoked method is `GameRules.AddPickupScore(10, 5)`, and the smoke
  expects result `15`.
- StarterArena has only the focused `GameRules` C# lifecycle proof and runtime
  smoke lifecycle evidence; arbitrary script methods remain unsupported.
- The controlled invocation and lifecycle smokes require a .NET host
  environment where `hostfxr` is discoverable.
- Generated StarterArena script artifacts, smoke reports, and visible reports
  are expected to live under temporary output roots, not under
  `samples/StarterArena`.
- Restart behavior is reported as deferred.
- No Visual Blocks, editor workflow, external client/server networking, broad
  runtime C# gameplay loop binding, package install, package output, or
  distribution output exists for this sample.
