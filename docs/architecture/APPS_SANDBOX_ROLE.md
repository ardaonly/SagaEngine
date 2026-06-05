# Apps/Sandbox Role

`Apps/Sandbox` is an internal diagnostic sandbox and engine playground. It is
not a playable sample collection and should not be used as evidence that
SagaEngine has finished game samples.

## Current Role

The current sandbox hosts probe-style scenarios for input, networking,
prediction, audio, and rendering behavior. These scenarios are useful for
development diagnostics, but most of them are closer to manual engine probes
than user-facing demos.

The durable ECS and memory allocator checks that previously lived as manual
sandbox probes now have deterministic unit-test coverage in
`Tests/Unit/Sandbox/SandboxDiagnosticMigrationTests.cpp`.

`RenderPlaygroundScenario` is the default windowed scenario today. That makes
the first-run experience a render/backend playground, not a code-first sample
game.

## Product Boundary

User-facing sample focus remains `samples/StarterArena`. StarterArena is still
bounded sample evidence and is not a finished playable game.

`Apps/Sandbox` scenarios must not be described as production samples, complete
gameplay examples, full editor workflow proof, full Visual Blocks UI proof, or
release readiness evidence.

## Cleanup Direction

The intended cleanup direction is:

- keep only small contributor-facing diagnostics in `Apps/Sandbox`;
- migrate deterministic subsystem probes into automated tests;
- move useful manual diagnostics into an internal lab if they cannot become
  stable tests;
- delete stale playground code only after replacement coverage or explicit
  unused evidence exists.

The sandbox public include boundary is also under review. Scenario, debug HUD,
and panel headers are internal diagnostic surface unless a future audit proves
otherwise.
