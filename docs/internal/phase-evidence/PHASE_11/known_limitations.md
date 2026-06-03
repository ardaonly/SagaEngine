# Phase 11 Known Limitations

- This phase is not verified.
- Phase 11C invokes only `GameRules.AddPickupScore(10, 5)`.
- No arbitrary C# script invocation is implemented.
- No C# lifecycle method execution is implemented for StarterArena.
- No runtime C# gameplay loop binding is implemented.
- The invocation smoke requires a .NET host environment where `hostfxr` is
  discoverable; the passing command uses `nix-shell`.
- No Visual Blocks, editor workflow, server-authoritative multiplayer, package
  output, or distribution output is implemented by this phase.
- SagaScript `analyze` reports one non-blocking warning because the sample does
  not declare `[SagaBehavior]` metadata.
- No public product claim should be derived from this file.
