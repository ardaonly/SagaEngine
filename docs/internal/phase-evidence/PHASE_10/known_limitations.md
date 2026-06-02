# Phase 10 Known Limitations

- This phase is not verified.
- StarterArena has a bounded headless runtime smoke loop, not interactive
  gameplay.
- `Scenes/arena.scene.json` is loaded only by the app-local smoke seam.
- The scene resource is not a reusable engine scene format.
- No renderer, client networking, server authority, C# gameplay scripts, Visual
  Blocks, editor workflow, package output, or distribution output is involved.
- Restart behavior remains deferred.
- `SagaLaunchLab` does not currently expose a runtime/client launch command.
- No public product claim should be derived from this file.
