# MultiplayerSandbox

MultiplayerSandbox is a deterministic project, scripting, package, validation,
and headless-integration fixture. It is not a finished or playable multiplayer
sample.

Validate its canonical project manifest with:

```sh
Tools/SagaProjectKit/sagaproject validate \
  --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj \
  --out /tmp/multiplayer_sandbox_validation.json
```

The sample-owned headless source lives under `Source/Headless`. Build and test
registration belongs to the repository CMake test graph. Generated reports are
bounded evidence and must not be treated as project source truth.

Current code does not establish a complete client/server gameplay loop,
production replication, persistent state, networking scale, security, or
deployment readiness.
