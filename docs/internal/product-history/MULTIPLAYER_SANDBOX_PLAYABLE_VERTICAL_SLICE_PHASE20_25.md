# MultiplayerSandbox Playable Vertical Slice Phase 20-25

> Block C evidence for Target 1 / Technical Preview Technical Preview product work.

Phase 20-25 establishes the first reproducible local preview path for
MultiplayerSandbox. The accepted path is server-only and headless. Runtime
client preview stages are deferred with evidence until a bounded ClientHost
preview/report seam exists.

## Implemented Path

The supported path is:

```text
SagaProjectKit validate
SagaProjectKit resolve
SagaLaunchLab accept
SagaLaunchLab server
MultiplayerSandboxHeadless
```

`MultiplayerSandboxHeadless` is a bounded server-side proof that uses
`AuthoritativeMovementCore` directly. It creates one controlled entity, queues
one deterministic movement input, verifies that state is unchanged before the
authoritative tick, ticks once, and writes `headless_server_report.json`.

## Reports

Generated reports are:

```text
project_validation_report.json
project_resolution.json
launch_preview_report.json
acceptance_report.json
headless_server_report.json
```

The headless report records:

- project id;
- tick count;
- entity count;
- queued and accepted input counts;
- dirty entity ids;
- initial position;
- before-tick position;
- final position;
- diagnostics.

## Deferred Runtime Client Preview

The desired product shape remains a local server plus runtime client host. It
is deferred because the current client host is still app-bound, lacks a bounded
preview duration/report contract, and `Application::Run()` creates a window
before the current headless flag can remove rendering work.

Runtime UI abstractions exist and should be used by a future preview client for
status surfaces such as connection state, local entity id, diagnostics summary,
input state, and preview state. This phase does not add that UI surface.

## Non-Claims

This slice does not prove production networking, production MMO behavior,
finished game loops, internet sessions, match flow, package assembly, release
publication readiness, editor involvement, or a product ship gate.
