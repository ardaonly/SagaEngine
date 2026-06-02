# Real Transport Stress Smoke Phase 9 Readiness

Phase 9 evaluated whether a bounded localhost real transport smoke can be added
without duplicating protocol behavior in a raw test-only TCP client.

## Findings

The real TCP server seam exists:

- `ZoneServer::Initialize()` can bind `127.0.0.1` on an ephemeral port.
- `ZoneServer::Run()` starts the accept loop, IO threads, and tick loop.
- `ConnectionManager::AcceptSocket()` wraps accepted TCP sockets in
  `ServerConnection` and emits client lifecycle callbacks.
- Phase 8 lifecycle diagnostics can observe server, session, shutdown, and leak
  transitions when diagnostics is attached.

The reusable client/test seam is not ready:

- `Apps/Client/ClientNetworkSession` is defined inside `ClientHost.cpp`, is not
  exposed as a stable test helper, and uses the UDP `INetworkTransport` path.
- Existing integration tests use simulated loopback queues or reliable-channel
  loopback, not localhost TCP against `ZoneServer`.
- `Apps/Server/TestSnapshotServer` is UDP and Engine `Packet` based, not the
  `ZoneServer` TCP path.
- No reusable test client, bot client, client connection helper, or
  localhost-TCP test adapter was found for the current server path.

The wire utilities are incomplete for this smoke:

- Engine `Packet` serialization exists and is reusable for Engine-packet flows.
- `InputCommandSerializer` exists and is reusable for input payload bytes.
- `ServerPacketNormalizer` validates the 8-byte inbound server frame, but there
  is no public frame builder/writer API paired with it.
- Several tests and tools define local `MakeFrame()` helpers, which is evidence
  that frame construction is still ad hoc.
- `ServerConnection` sends an outbound handshake challenge as an Engine
  `Packet`, while inbound handshake and game traffic are read as the 8-byte
  `ServerPacketNormalizer` frame. A raw smoke client would need to mirror this
  mismatch by hand.

## Decision

Phase 9 defers the real transport smoke with evidence.

A raw mini-client is rejected for this phase because it would duplicate
handshake and frame-writing details inside a test, creating fragile evidence and
a risk of falsely proving only the test's protocol mirror.

The accepted proof remains the existing direct/local harness:

- `NetworkChaosLayerTests`
- `SagaChaosLabTests`
- `SagaStressArenaTests`
- `ZoneServerDiagnosticsTests`
- `ServerLifecycleDiagnosticsTests`

## Future Prerequisite

Before adding a bounded localhost transport smoke, add one official reusable
seam that owns protocol writing:

- a test client or client transport adapter for `ZoneServer` TCP, or
- shared handshake/frame writer utilities paired with `ServerConnection` and
  `ServerPacketNormalizer`.

That future seam should own handshake response construction, server-frame
writing, input payload wrapping, timeouts, and disconnect behavior. Phase 9 does
not add that seam.

## Non-Claims

This deferral does not prove real transport stress, production network
readiness, replication correctness, bot swarm behavior, reconnect behavior,
MMO-scale stress, release readiness, product beta readiness, or raw full CTest
health.
