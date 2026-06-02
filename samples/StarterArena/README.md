# StarterArena Runtime Smoke Sample

`StarterArena` is a future sample definition for project metadata validation.
It now has a narrow `SagaRuntime` smoke seam for Phase 10.

This is not an interactive game. The runtime smoke command consumes the
`.sagaproj` file, runs a deterministic built-in local loop in headless mode,
writes a smoke report, and exits. It does not use renderer, client networking,
server authority, C# scripts, Visual Blocks, editor workflow, package output, or
distribution output.

Runtime smoke command:

```sh
build/RelWithDebInfo-0.0.9/bin/SagaRuntime --headless --project samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke --smoke-report-out /tmp/starter_arena_runtime_smoke.json --smoke-frames 30 --fixed-dt 0.016
```

The tracked directories exist only because the current project schema validates
the diagnostics and generated report paths:

- `Diagnostics`
- `Build/Reports`

Expected focused check:

```sh
Tools/SagaProjectKit/sagaproject validate --project samples/StarterArena/StarterArena.sagaproj --out /tmp/starter_arena_validate.json
```

Phase 10 acceptance notes are tracked in `ACCEPTANCE.md`. Known limitations are
tracked in `KNOWN_LIMITATIONS.md`.
