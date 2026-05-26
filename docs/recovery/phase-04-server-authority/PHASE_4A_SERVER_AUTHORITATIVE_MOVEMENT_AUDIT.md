# Phase 4A Server Authoritative Movement Audit

> Last updated: 2026-05-25
> Status: Phase 4A report-only audit
> Decision: current movement authority is mixed and not yet proven

This audit opens Phase 4 with evidence gathering only. It records the current
server authority, client command, replication, prediction, and session ownership
model before any server-authoritative movement implementation.

No behavior is implemented by this checkpoint. Full test health remains
unverified.

## 1. Executive Summary

Current server authority maturity is mixed:

- Server has authority-shaped primitives: `ZoneServer` owns a fixed tick shell,
  `ConnectionManager` owns sessions, `InputCommandQueue` can buffer per-client
  input, `MovementValidator` can reject/clamp movement proposals,
  `GameplayCommandDispatcher` can decode and gate typed gameplay commands, and
  `ReplicationManager` can send dirty state.
- Production movement authority is not proven. `ZoneServer::DrainInputPackets`
  drains inbound packets but discards the contents, `ZoneServer::StepSimulation`
  is a placeholder, `OnMoveRequest` logs and returns success, and no production
  path mutates server-owned movement state from validated input.
- The current client movement/input path is client-driven at creation and send
  time. The client queues `InputCommand` values and sends them through
  `ClientNetworkSession`; server acceptance/rejection is not wired into the
  production zone loop.
- Replication/prediction infrastructure exists, but replication tests can pass
  without proving authoritative movement because they exercise codecs, snapshot
  apply, dirty state, and harness flows rather than a validated fixed-tick
  server mutation path.

Main blockers before authoritative movement proof:

- Inbound `PacketType::InputCommand` is not decoded and routed by `ZoneServer`.
- Engine input validation seams exist but are not connected to the production
  server tick loop.
- Server-owned movement mutation is missing from the fixed tick.
- Entity ownership/identity validation is not tied to connection/session state.
- Accepted state changes are not the sole source of replication output.
- `ClientHost` and test snapshot paths have a large blast radius and should not
  be refactored as part of the first proof.

## 2. Current Ownership Inventory

Server gameplay command dispatch:

- `SagaServer::Gameplay::GameplayCommandDispatcher` decodes typed gameplay
  command blobs under RPC name `gcmd`, applies trait-based auth/rate gates, and
  calls registered handlers.
- `RegisterGameplayHandlerStubs` registers shipped handlers, including
  `OnMoveRequest`.
- `OnMoveRequest` is symbolic for authority: it logs decoded intent and returns
  success without validation, state mutation, or replication output.
- `GameplayCommandDispatcher::Install` currently assumes `clientAuth = true`
  inside the bridge callback, so production auth state is not actually piped
  through that install path.

Server simulation/input command handling:

- `InputCommandQueue` and `InputCommandRouter` own per-client buffering,
  sequence sorting, duplicate rejection, tick-window checks, and drain APIs.
- `MovementValidator` owns speed, teleport, acceleration, tolerance, and
  violation evaluation for tracked entities.
- Engine `ServerInputProcessor` and `InputPacketHandler` can deserialize,
  validate, sanitize, ack, and expose clean `InputCommand` values.
- These pieces are not connected into `ZoneServer::DrainInputPackets` or
  `ZoneServer::StepSimulation`.

Server session/connection lifecycle:

- `ZoneServer` owns accept, fixed tick loop, world state, connection manager,
  interest manager, and replication manager.
- `ConnectionManager` owns the client session table and inbound packet queue.
- `ServerConnection` owns one TCP connection state machine, receive framing,
  heartbeat, handshake, and send queue.
- Client identity currently comes from allocated `ClientId`; controlled entity
  ownership is not wired into the movement path.

Replication manager/graph/snapshot state:

- `ReplicationManager` owns client replication state, registered entities,
  dirty component queues, RPC registry, and snapshot send output.
- `ReplicationManager::CheckEntityAuthority` returns true when no authority
  manager is set, so authority checks can be effectively disabled.
- `ReplicationGraph` owns interest/relevancy scoring, client positions, and
  per-client entity ordering, but it does not establish movement authority.
- `ZoneServer::FlushReplication` gathers dirty entities and sends updates, but
  no authoritative movement mutation currently marks accepted movement dirty.

Client command send path:

- `ClientHost::TickInput` polls keyboard state, creates
  `SagaEngine::Input::InputCommand`, and queues it on `ClientNetworkSession`.
- `ClientNetworkSession::TickSend` serializes pending commands as
  `PacketType::InputCommand` and sends through a reliable channel over the
  client transport.
- `GameplayCommandClient` can encode high-level gameplay commands such as
  `MoveRequest`, but `ClientHost` movement currently uses `InputCommand`, not
  `MoveRequest`.

Client prediction/reconciliation:

- `ClientNetworkSession` owns client `WorldState`, `ReconciliationBuffer`,
  interpolation, input buffer, snapshot demux, snapshot apply pipeline, and
  replication apply bridge.
- Reconciliation replay integrates unacked `InputCommand` movement locally and
  treats server snapshot state as authoritative when applied.
- This is acceptable client-side prediction plumbing, but it must not be
  mistaken for server authority.

Engine shared primitives:

- Engine owns neutral input command structures, serializers, input inbox,
  server input processor, packet handler, gameplay command codecs/traits, and
  client replication/prediction helpers.
- These contracts are useful seams, but several are public Engine surfaces and
  should remain neutral rather than becoming server policy.

Runtime involvement:

- Runtime startup/lifecycle foundations are unrelated to server movement
  authority.
- No Phase 4A implementation should move server movement concerns into Runtime.

## 3. Client-To-Server Command Flow Map

Current movement/input flow:

1. `ClientHost::TickInput` creates an `InputCommand` from keyboard state.
2. `ClientNetworkSession::QueueInput` stores it in `InputCommandBuffer`.
3. `ClientNetworkSession::TickSend` sends each pending command as
   `PacketType::InputCommand`.
4. `ConnectionManager::OnConnectionPacket` stores inbound packet bytes in its
   tick-thread inbound queue.
5. `ZoneServer::DrainInputPackets` drains inbound packets but currently ignores
   `clientId`, `data`, and `size`.
6. `ZoneServer::StepSimulation` does not apply input or mutate world state.
7. `ZoneServer::FlushReplication` gathers and sends dirty state, but accepted
   movement does not currently create the dirty state.
8. `ClientNetworkSession::TickReceive` receives packets, handles `InputAck`, or
   enqueues snapshot payloads into the demux/pipeline path.
9. Client snapshot apply and reconciliation update local client state.

Parallel high-level gameplay command flow:

1. `GameplayCommandClient` encodes typed gameplay command payloads such as
   `MoveRequest`.
2. `GameplayCommandDispatcher` can decode `gcmd` payloads and call handlers.
3. `OnMoveRequest` currently logs and returns success.
4. This path is not the per-tick WASD movement input path and does not mutate
   server-owned state today.

Available but unwired Engine input path:

1. `InputPacketHandler` can deserialize `PacketType::InputCommand`.
2. `ServerInputProcessor` can validate/sanitize commands and emit ack entries.
3. No current `ZoneServer` integration uses that processor to produce fixed
   tick movement mutations.

## 4. Authority Boundary Map

Server-owned truth:

- Intended truth should be server `WorldState`, controlled entity ownership,
  fixed tick movement mutation, accepted command history, and dirty replication
  output.
- Current repo evidence has the container and fixed tick shell, but not the
  production movement mutation.

Client-owned prediction/render state:

- Client may own input capture, local prediction, interpolation, debug render,
  and transient unacked command buffers.
- Client prediction must remain temporary and corrected by server snapshots.

Shared neutral contracts:

- `InputCommand`, input serializers, input inbox/processor interfaces,
  gameplay command codecs/traits, packet types, snapshot contracts, and client
  replication helpers are neutral contracts.

Client-too-authoritative risks:

- Test snapshot flows mutate based on client input without proving session
  ownership or production fixed tick validation.
- Client replay logic includes movement integration comments matching test
  server behavior; that is useful for prediction but unsafe as authority proof.
- Any future slice must prevent client position or speed hints from becoming
  server truth without server validation.

Stubbed or symbolic server behavior:

- `OnMoveRequest` returns success for all decoded movement intents.
- `ZoneServer::StepSimulation` is placeholder logic.
- `ReplicationManager::CheckEntityAuthority` allows all modifications when no
  authority manager is set.
- `DrainInputPackets` does not route inbound commands.

## 5. Replication/Prediction Boundary Map

Safe to preserve:

- Client-side buffering of unacked input for reconciliation.
- Snapshot demux, ordering, and apply pipeline as client-side infrastructure.
- Server-owned component authority metadata for snapshot application.
- Replication dirty queue and interest filtering as output mechanisms.

Unsafe for MMO authority until proven:

- Treating passing replication tests as proof that server movement is
  authoritative.
- Sending snapshots that were not produced from validated fixed-tick server
  mutation.
- Applying prediction to entities without verified local control.
- Letting `MoveRequest::speedHint` or client-proposed positions directly
  mutate server state.
- Leaving authority checks default-open in movement/RPC paths.

Required Phase 4B boundary:

- Prediction may make the client responsive, but only server-accepted fixed
  tick state may produce authoritative replication.

## 6. Existing Test Inventory

CTest label inventory:

- `server`: `UnitTests`, `IntegrationTests`; broad/coarse.
- `networking`: `UnitTests`, `IntegrationTests`; broad/coarse.
- `replication`: `UnitTests`, `IntegrationTests`, `ReplicationTests`;
  includes slow/long-running replication target.
- `integration`: `SagaPipelineTests`, `IntegrationTests`, `ReplicationTests`;
  includes unrelated tooling and slow replication coverage.

Relevant focused or useful tests:

- `Tests/Unit/Gameplay/CommandCodecTests.cpp`: focused command codec coverage;
  useful for command wire compatibility.
- `Tests/Unit/Gameplay/GameplayCommandDispatcherTests.cpp`: focused dispatcher
  coverage; useful for typed decode, auth/rate gates, and stub registration.
- `Tests/Unit/Networking/ReplicationTests.cpp`: useful replication manager
  unit coverage, including authority validation behavior, but not movement
  authority proof.
- `Tests/Unit/Networking/ConnectionManagerTests.cpp`: useful session table and
  lifecycle coverage, not movement proof.
- `Tests/Unit/Networking/ReliableChannelTests.cpp`: useful transport reliability
  coverage, not movement proof.
- `Tests/Unit/Networking/InterestTests.cpp`: useful interest/relevancy coverage,
  not movement proof.

Broad, risky, or harness-heavy tests:

- `Tests/Integration/ClientServerTests.cpp`: broad; includes server
  authoritative naming but uses integration harness behavior.
- `Tests/Integration/ClientServerRoundTripTest.cpp`: useful end-to-end shape,
  but test harness movement is not the production `ZoneServer` path.
- `Tests/Integration/ReplicationIntegrationTests.cpp`: useful replication and
  reconciliation integration coverage, not sufficient authority proof.
- `Tests/Integration/ReplicationTests.cpp`: useful snapshot pipeline coverage.
- `Tests/Replication/*`: replication fuzz/chaos/soak/performance coverage;
  opt-in slow/long-running and not movement authority proof.

Missing focused tests for Phase 4B:

- no focused production-style test that routes a client input command through
  server validation, fixed-tick mutation, dirty replication, and client apply
- no focused rejection test for unauthorized movement by client/entity id
- no focused rejection/clamp test proving invalid movement cannot produce
  replication output

## 7. Minimum Authoritative Gameplay Loop

The smallest meaningful Phase 4 implementation proof should:

- accept a movement/input command from a client identity
- map that client identity to one controlled entity
- validate command version, sequence, tick window, input magnitude, and entity
  ownership
- evaluate movement limits with `MovementValidator` or an equivalent
  server-owned movement gate
- mutate server-owned state only during the fixed tick
- reject invalid, stale, duplicate, or unauthorized movement
- emit an input ack/result only for processed commands
- mark accepted authoritative movement dirty for replication
- send replication output only for accepted state changes
- let the client receive/apply authoritative state and reconcile prediction

## 8. Candidate Implementation Slices

Candidate 1: Wire server input into fixed tick.

- Goal: connect inbound `PacketType::InputCommand` from `ZoneServer` to an
  input processor/router and consume it during `ExecuteTick`.
- Likely touched: `ZoneServer`, Engine input networking seams, focused
  server/network tests.
- Tests needed: packet decode to queue, tick drain ordering, invalid packet
  rejection, ack generation.
- Risk: `ZoneServer` and packet transport formats have high blast radius; client
  uses UDP transport while current `ZoneServer` accept path is TCP-shaped.
- Order: after a server movement core is proven, unless production transport
  wiring is chosen as the first risk to retire.

Candidate 2: Authoritative movement core test harness.

- Goal: create a narrow server-side movement authority path around validated
  input, controlled entity identity, `MovementValidator`, and server-owned state
  mutation without changing production transport.
- Likely touched: `Server/Simulation` and focused unit tests.
- Tests needed: valid movement mutates state, unauthorized entity is rejected,
  stale/duplicate/out-of-window input is rejected, teleport/speed violation does
  not produce accepted state, accepted mutation can be marked for replication.
- Risk: it can become another symbolic island if not followed by `ZoneServer`
  wiring.
- Order: first, because it proves the authority contract before networking
  blast radius.

Candidate 3: Replace `OnMoveRequest` stub with rejection-first intent handling.

- Goal: stop high-level `MoveRequest` from unconditionally succeeding; reject
  unsupported teleport/path/follow intents until real systems exist.
- Likely touched: gameplay handlers and dispatcher tests.
- Tests needed: invalid/unsupported movement intent returns failure, allowed
  cancel/stop semantics are explicit if retained, dispatcher preserves typed
  decode behavior.
- Risk: `MoveRequest` is high-level intent, not per-tick movement input, so this
  does not prove authoritative WASD movement.
- Order: after the core input movement proof or as a narrow cleanup if handler
  stubs become the immediate risk.

## 9. Recommended First Implementation Slice

Recommended Phase 4B slice: Candidate 2, Authoritative movement core test
harness.

Reason:

- It is narrow, deterministic, and server-authoritative.
- It proves the key contract before production transport wiring.
- It can be tested with focused unit tests instead of broad integration tests.
- It avoids `ClientHost` refactor, broad networking rewrite, and full MMO scope.

Expected Phase 4B outcome:

- A valid client-owned movement input mutates server-owned state on a simulated
  fixed tick.
- Invalid, stale, too-fast, teleport, or unauthorized movement is rejected or
  clamped without becoming client truth.
- Accepted movement creates an explicit signal that replication can consume.

## 10. Verification Strategy

Safe verification for Phase 4A documentation:

- `git status --short`
- `git diff --check`
- `git diff --name-only`
- `git diff --stat`
- targeted `rg` inventory for authority, command, movement, prediction,
  replication, and session terms
- `ctest --test-dir build/RelWithDebInfo-0.0.9-sde -N -L server`
- `ctest --test-dir build/RelWithDebInfo-0.0.9-sde -N -L networking`
- `ctest --test-dir build/RelWithDebInfo-0.0.9-sde -N -L replication`
- `ctest --test-dir build/RelWithDebInfo-0.0.9-sde -N -L integration`

Safe verification for the recommended Phase 4B implementation:

- `git diff --check`
- targeted `rg` proving no broad Runtime/App ownership drift
- focused build of server/unit targets if available
- focused server/simulation unit tests for accepted/rejected movement
- focused networking or replication tests only if the slice touches those paths
- CTest inventory commands before claiming suite coverage

Do not run full CTest unless separately justified. Full test health remains
unverified unless the relevant full suite set is actually run and passes.

## 11. Risks And Non-Goals

Risks:

- Broad networking rewrite risk: wiring production `ZoneServer` input too early
  can expand into transport/session redesign.
- Client prediction can be mistaken for server truth if client replay behavior
  is treated as authority proof.
- Replication tests can pass while movement authority remains missing.
- `ClientHost` has high blast radius because it owns input, session, prediction,
  reconciliation, UI, render debug, and snapshot apply composition.
- `MoveRequest` is high-level movement intent and should not be confused with
  per-tick input movement.
- Full MMO gameplay scope creep can hide the narrow authority proof.
- Default-open authority checks can mask missing identity validation.

Non-goals for Phase 4A:

- no implementation
- no broad server/network rewrite
- no full MMO gameplay implementation
- no mass server/client file movement
- no `ClientHost` refactor
- no Runtime ownership continuation
- no full test health claim

## 12. Decision Gate

Phase 4A audit is sufficient to move to a narrow Phase 4B implementation slice.

Recommended Phase 4B implementation slice:

- Authoritative movement core test harness.

Unresolved risks:

- production `ZoneServer` input routing remains unwired
- client UDP send path and server TCP accept/session path need later alignment
- `ClientHost` remains a broad composition boundary
- replication output is not yet tied exclusively to accepted movement mutation
- entity ownership and identity mapping need a concrete server-side contract
- full test health remains unverified
