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
- The `--starter-arena-smoke` path is Runtime-owned and does not initialize
  renderer setup or UDP/client networking.
- The `--starter-arena-playable` path is also Runtime-owned and socket-free,
  but initializes the existing platform window and render backend.
- Visible mode supports scene-authored input, deterministic synthetic JSON
  input, and explicit keyboard input through the existing engine input path.
- The normal automated RC gate leaves real keyboard control pending unless an
  explicit validated report is supplied. The separate Product Shell human
  capture command can collect and import that report; an idle keyboard run
  still reports `NoInputObserved` and cannot pass human evidence validation.
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
- The Product Shell first-playable workflow is a CLI diagnostics and JSON
  summary surface, not an editor panel or project-creation workflow. Its
  visible profiles require an available window and graphics backend.
- The release-candidate gate accepts the automated spine with manual keyboard
  evidence pending. Keyboard-provider behavior is external unit-test evidence;
  it is not re-run or promoted to real-device evidence by the gate.
- The human capture command is a bounded operator console workflow, not a full
  game, full editor, production input UX, rebinding surface, controller path,
  or platform-wide device compatibility proof. It launches the existing
  Runtime keyboard mode and consumes only its JSON evidence report.
- `Scripts/GameRules.cs` is the source of truth. There is no checked-in block
  JSON; Product Shell generates a read-only representation report outside the
  sample after generated metadata and runtime evidence pass.
- The generated catalog has no canvas, graph instances or edges, graph
  execution, layout, generated C# from blocks, editor graph editing, reusable
  production block library, or package/distribution behavior. The generator
  never launches SagaRuntime or executes C#; lifecycle and gameplay runtime
  reports remain the authority for observed behavior.
- Restart behavior is reported as deferred.
- The state port is not arbitrary key/value storage, ECS access, or a broad
  gameplay scripting framework.
- No Visual Blocks canvas or runtime, editor graph workflow, external
  client/server networking, package install, package output, or distribution
  output exists for this sample.
