# StarterArena Acceptance

Phase 10 acceptance is implemented-unverified through a bounded local
`SagaRuntime` smoke command:

```sh
build/RelWithDebInfo-0.0.9/bin/SagaRuntime --headless --project samples/StarterArena/StarterArena.sagaproj --starter-arena-smoke --smoke-report-out /tmp/starter_arena_runtime_smoke.json --smoke-frames 30 --fixed-dt 0.016
```

Accepted evidence for this phase:

- the runtime command exits `0`;
- the smoke report has `status: Passed`;
- the report records `projectId: starter-arena`;
- the report records spawn, final position, fixed frame count, bounds, input
  vector, and clamp count;
- the report records non-claims for renderer, client/network, server authority,
  C# scripts, Visual Blocks, editor workflow, package output, and distribution
  output.

This is not acceptance for interactive gameplay, a scene system, package
launch, editor launch, or server-authoritative multiplayer.
