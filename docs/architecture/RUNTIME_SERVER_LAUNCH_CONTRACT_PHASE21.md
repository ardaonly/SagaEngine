# Runtime/Server Launch Contract Phase 21

> Block C launch contract evidence for Phase 21.

Phase 21 introduces a narrow local preview launch contract. The only supported
profile in this slice is the bounded server-side headless profile for
MultiplayerSandbox.

## Supported Profile

`samples/MultiplayerSandbox/launch_profiles.json` contains:

```text
local-server-headless
```

Required fields:

- `id`
- `role`
- `mode`
- `executable`
- `reportPath`
- `diagnosticsPath`
- `arguments`

Supported role:

```text
server
```

Unsupported roles fail deterministically in SagaLaunchLab.

## Tool Arguments

`MultiplayerSandboxHeadless` accepts:

```text
--project <path>
--report-out <path>
--diagnostics-out <dir>
--ticks <n>
--fixed-dt <seconds>
```

`SagaLaunchLab` accepts:

```text
sagalaunch server --project <path> --launch-profile <id> --out <report> --timeout-sec <n> --bin-dir <dir>
sagalaunch accept --project <path> --launch-profile <id> --out <report> --timeout-sec <n> --bin-dir <dir>
```

## Failure Behavior

SagaLaunchLab writes a failed report for:

- missing project path;
- missing launch profile;
- unsupported profile role;
- missing executable;
- process timeout;
- non-zero process exit;
- missing expected headless report.

The tool uses strict timeout caps and at most one server process for the
supported path.

## Future Runtime Client Contract

A future runtime client profile needs a bounded ClientHost preview mode, a
preview duration or clean auto-exit mechanism, a report path, and a runtime UI
status surface. That work is intentionally not part of Phase 20-25.
