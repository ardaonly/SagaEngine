# Network Chaos Policy Foundation

This document adds the first deterministic `NetworkChaosLayer` foundation for direct
local packet policy tests and bounded diagnostics evidence.

## Scope

- `NetworkChaosConfig` defines deterministic policy knobs for drop,
  duplicate-once, defer, seed, and deferred queue capacity.
- `NetworkChaosDecision` records the stable decision kind, input sequence,
  release tick, and queue depth for each submitted direct frame.
- `NetworkChaosLayer` owns an explicit seeded PRNG and an owned deferred frame
  queue.
- The layer is intended for direct/local harnesses and focused tests. It does
  not manipulate sockets or operating system networking.

Disabled policy is transparent pass-through: the input frame is returned as one
delivered frame and no deferred queue state is created.

## Determinism

The policy uses a local SplitMix64 sequence seeded from `NetworkChaosConfig`.
The same config, seed, frame order, and explicit tick values produce the same
decision sequence. Defer release is driven only by explicit ticks through
`ReleaseDueFrames`; it does not use sleeps or wall-clock time.

Decision precedence is stable:

```txt
drop -> duplicate-once -> defer -> deliver
```

The deferred queue is bounded by `maxDeferredFrames`. When the queue is full,
the layer records a queue-full drop and does not grow memory usage.

## Diagnostics Metrics

When a `DiagnosticSystem` is attached, the layer records:

```txt
net.chaos.frames_seen
net.chaos.frames_delivered
net.chaos.frames_dropped
net.chaos.frames_duplicated
net.chaos.frames_deferred
net.chaos.frames_released
net.chaos.queue_depth
net.chaos.queue_full_drops
```

Missing diagnostics is a no-op and does not change decisions or frame output.
Existing diagnostics report writers include these metrics through the normal
health metric snapshot path.

## SagaStressArena Integration

This document adds `direct_zone_packet_chaos_smoke` as a safe extension of the
existing direct ZoneServer harness. It runs in-process, uses `direct_no_socket`,
feeds deterministic frames through `NetworkChaosLayer`, and writes the existing
operational, reliability, and lifetime report artifacts.

## Dependency Direction

```txt
SagaServerLib owns NetworkChaosLayer.
SagaServerLib may optionally record through SagaDiagnostics.
SagaDiagnostics must not depend on Server, Tools, or SagaStressArenaLib.
SagaStressArenaLib may depend on SagaServerLib and SagaDiagnostics.
```

## Non-Claims

This document does not add real socket-level packet manipulation, OS network shaping,
a real network BotClient, reconnect storm testing, SagaChaosLab, MMO-scale
network stress, production network resilience, unbounded queues, sleep-based
latency tests, or raw full CTest health.
