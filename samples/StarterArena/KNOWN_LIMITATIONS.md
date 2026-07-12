# StarterArena Known Limitations

- StarterArena has a bounded headless runtime smoke loop and a separate visible
  frame mode.
- StarterArena is not a complete interactive game.
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
- Visible mode supports scene-authored input, deterministic synthetic JSON
  input, and explicit keyboard input through the existing engine input path.
- Real keyboard control remains pending manual evidence. An idle keyboard run
  reports `NoInputObserved` without failing.
- Input bindings are fixed to WASD and Escape for this sample-local slice; there
  is no remapping UI, mouse movement, gamepad mapping, or generic controller
  framework.
- Visible mode renders app-local procedural arena geometry. It does not prove
  cooked asset loading or a reusable scene renderer.
- Script metadata and lifecycle flags remain headless by default. Visible mode
  accepts them only with the explicit `--run-starter-arena-gameplay` proof.
- The smoke loop reads spawn, bounds, camera metadata, input, and expected
  results from the declared scene resource.
- `Scripts/GameRules.cs` is compile/analyze evidence, one controlled runtime
  pure-method invocation smoke, one focused C# lifecycle proof, and optional
  gameplay-spine evidence through four whitelisted typed state operations.
- StarterArena runtime smoke can load the compiled C# assembly only when
  `--invoke-starter-arena-script` or
  `--run-starter-arena-script-lifecycle` is passed with valid script metadata.
- The controlled method remains `GameRules.AddPickupScore(10, 5) == 15`.
  Gameplay mode separately runs lifecycle updates against one scene pickup.
- StarterArena has only the focused `GameRules` C# lifecycle proof and runtime
  smoke lifecycle evidence; arbitrary script methods remain unsupported.
- The controlled invocation and lifecycle smokes require a .NET host
  environment where `hostfxr` is discoverable.
- Generated StarterArena script artifacts, smoke reports, and visible reports
  are expected to live under temporary output roots, not under
  `samples/StarterArena`.
- Restart behavior is reported as deferred.
- The state port is not arbitrary key/value storage, ECS access, or a broad
  gameplay scripting framework.
- No Visual Blocks, editor workflow, external client/server networking, package
  install, package output, or
  distribution output exists for this sample.
