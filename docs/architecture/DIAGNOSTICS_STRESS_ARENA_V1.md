# Diagnostics Stress Arena v1

Phase 4 adds `SagaStressArena`, the first safe stress/soak diagnostics artifact
proof for Saga diagnostics.

## Scope

- `SagaStressArenaLib` owns bounded scenario configuration, stress tiers, report
  paths, and the direct local ZoneServer scenario runner.
- `SagaStressArena` is the CLI entry point for running bounded diagnostics
  scenarios.
- The supported v1 scenario is `direct_zone_diagnostics_smoke`.
- Phase 6 adds `direct_zone_packet_chaos_smoke` as a bounded direct/local
  NetworkChaos policy smoke scenario on the same harness seam.
- The runner attaches `DiagnosticSystem` to `ZoneServer`, registers
  deterministic controlled actors, feeds deterministic movement input, ticks
  movement authority, records bounded tick-duration diagnostics, and writes
  report artifacts.

## Stress Tiers

The tiers are explicit and capped:

- `smoke`: default tier, safe for developer laptops and focused tests.
- `mini_soak`: larger but still bounded, intended for manual local runs.
- `heavy`: documented/manual opt-in only in v1 and not executed by focused tests,
  default CTest, or default CLI behavior.

Every tier has actor, tick, and duration caps. No scenario runs unbounded.

## Report Artifacts

The direct ZoneServer harness writes:

```txt
direct_zone_stress_operational_snapshot.json
direct_zone_stress_reliability_report.json
direct_zone_stress_lifetime_leaks.json
```

The reports include stable fields for artifact proof, including
`server.tick.count`, `server.tick.ms`, `world.entities.active`,
`server.movement.accepted_inputs`, `server.movement.rejected_inputs`,
`server.packets.rejected`, scenario/tier metadata, health rule information,
recent log events, and lifetime leak summary content.

For `direct_zone_packet_chaos_smoke`, the same report artifacts also include
`net.chaos.*` metrics such as frames seen, delivered, dropped, duplicated,
deferred, released, queue depth, and queue-full drops when those decisions occur.

Tests compare stable fields rather than raw JSON bytes because generation
sequence, thread id, absolute paths, and platform-specific fields may vary.

## Dependency Direction

```txt
SagaStressArenaLib may depend on SagaServerLib and SagaDiagnostics.
SagaDiagnostics must not depend on Tools or SagaStressArenaLib.
SagaServerLib must not depend on Tools or SagaStressArenaLib.
```

This keeps tool-level diagnostics proof outside Engine diagnostics and Server
runtime ownership.

## Bot Client Boundary

Phase 4 does not add a real network BotClient. The current implementation uses
the existing local ZoneServer harness APIs and does not open sockets or claim
real transport stress.

Real transport BotClient work is deferred until the stable client/server
transport path is ready and can be tested without machine-killing defaults.

## Non-Claims

Phase 4 non-claims remain: no full network stress, no real bot swarm,
no MMO-scale load claim, no 100/500/1000 client claim, no load/performance
readiness, no production readiness, no default heavy stress, no unbounded stress
loops, no StateValidation, no FaultBoundary, no OS crash handler, no stack
traces, no remote telemetry, no SDE-driven diagnostics config, and no full raw
CTest health claim. Phase 6 adds deterministic local/direct NetworkChaos policy
evidence only; it is not real transport chaos or SagaChaosLab.
