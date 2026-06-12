# SagaChaosLab v1

This document adds `SagaChaosLab`, a bounded tool-level chaos profile runner around
the current contract deterministic `NetworkChaosLayer` and the existing direct/local
`SagaStressArena` ZoneServer harness.

## Scope

- `Tools/SagaChaosLab` owns profile loading, validation, execution, and
  `chaos_report.json` writing.
- The only supported v1 scenario is `direct_zone_packet_chaos_smoke`.
- The runner maps a validated profile to `SagaStressArena::StressScenarioConfig`
  plus a profile-supplied `NetworkChaosConfig`.
- Execution remains single-process, local, direct-to-ZoneServer, and
  `direct_no_socket`.
- The built-in default profile is a smoke-tier profile with deterministic seed,
  small actor/tick/duration bounds, and nonzero drop, duplicate, and defer
  probabilities.

## Profile Format

The profile JSON schema version is `1`:

```json
{
  "schemaVersion": 1,
  "profileName": "smoke_packet_chaos",
  "scenario": "direct_zone_packet_chaos_smoke",
  "tier": "smoke",
  "manualOnly": false,
  "seed": 1516904486,
  "actors": 3,
  "ticks": 10,
  "maxDurationSec": 10,
  "reportDirectory": "diagnostics/reports/saga_chaos_lab",
  "chaos": {
    "dropPermille": 250,
    "duplicatePermille": 250,
    "deferPermille": 250,
    "deferTicks": 1,
    "maxDeferredFrames": 16
  }
}
```

Unsupported top-level fields and unsupported `chaos` fields fail validation.
Missing required fields fail with deterministic diagnostics. The loader does not
accept silent typo fallback.

## Validation

- `schemaVersion` must be `1`.
- `scenario` must be `direct_zone_packet_chaos_smoke`.
- `tier` must be `smoke`, `mini_soak`, or `heavy`.
- `actors`, `ticks`, and `maxDurationSec` must be nonzero and within the
  selected `SagaStressArena` tier bounds.
- `dropPermille`, `duplicatePermille`, and `deferPermille` must each be
  `0..1000`, and their sum must be `<= 1000`.
- `deferTicks` and `maxDeferredFrames` must be nonzero and bounded by tier-safe
  limits.
- `reportDirectory` must be non-empty.
- `manualOnly=true` or `heavy` profiles are blocked unless the CLI receives
  `--allow-manual`.

## Report Shape

`SagaChaosLab` writes:

```txt
chaos_report.json
```

The report includes schema version, tool name, profile name, status, message,
profile safety fields, the effective chaos config, paths to the StressArena
operational/reliability/lifetime reports, StressArena status, diagnostics, and
a deterministic summary of `net.chaos.*` metrics extracted from the operational
report.

The StressArena artifacts remain:

```txt
direct_zone_stress_operational_snapshot.json
direct_zone_stress_reliability_report.json
direct_zone_stress_lifetime_leaks.json
```

## Dependency Direction

```txt
SagaServerLib owns NetworkChaosLayer.
SagaStressArenaLib owns the direct/local ZoneServer harness.
SagaChaosLabLib depends on SagaStressArenaLib, SagaServerLib, SagaDiagnostics,
and nlohmann_json.
SagaDiagnostics must not depend on SagaChaosLabLib, SagaStressArenaLib, or
SagaServerLib.
```

## Non-Goals

SagaChaosLab v1 does not add production network chaos, a real internet condition
emulator, OS-level network shaping, a distributed chaos cloud, a bot swarm,
MMO-scale stress, full network resilience, release readiness, product beta
readiness, socket transport chaos, reconnect storms, external services, or raw
full CTest health.
