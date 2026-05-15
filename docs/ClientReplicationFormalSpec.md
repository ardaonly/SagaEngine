# Client-Side Replication Pipeline — Formal Specification

> Last updated: 2026-05-14  
> Status: Formal runtime specification  
> Recommended location: `docs/specs/replication/ClientReplicationFormalSpec.md`  
> Related roadmap: `ENGINE_ROADMAP.md`  
> Related systems: Packet demux, replication state machine, snapshot apply pipeline, reconciliation buffer, interpolation manager, prediction, diagnostics, runtime networking.  
> Scope: Client-side replication apply order, determinism, state transitions, thread ownership, memory boundaries, validation rules, and failure behavior.

---

## 0. Document Status

This document is a formal runtime specification.

It is not a roadmap.

It defines the correctness contract for the client-side replication pipeline.

Roadmap progress belongs in:

```txt
ENGINE_ROADMAP.md
```

This document should describe invariants the implementation must preserve.

If the implementation changes, this specification must be updated deliberately.

Not accidentally.

Not “the code works now, trust me”.

That sentence has personally escorted thousands of bugs into production.

---

## 1. Purpose

The client-side replication pipeline receives authoritative server state, validates it, orders it, applies it to client runtime state, reconciles predicted local state, and feeds interpolation buffers for remote visual smoothing.

The pipeline must preserve:

- deterministic apply order,
- safe packet validation,
- bounded memory usage,
- thread-safe handoff,
- stale packet rejection,
- controlled resynchronization,
- client prediction correction,
- remote entity interpolation,
- actionable diagnostics,
- strict separation between runtime replication and editor collaboration policy.

The client must never blindly trust network input.

The server is authoritative.

The client is a visual, predictive, interactive consumer of server truth.

---

## 2. Non-Roadmap Rule

This specification must not track feature progress using checklist items.

Correct use:

```txt
Defines required behavior.
Defines invalid behavior.
Defines ordering rules.
Defines failure behavior.
Defines invariants.
Defines state transitions.
```

Incorrect use:

```txt
Tracks whether feature X is done.
Tracks task ownership.
Tracks implementation milestones.
Tracks sprint planning.
```

Use `ENGINE_ROADMAP.md` for progress.

Use this file for correctness.

---

## 3. Ownership

This specification belongs to the runtime/client replication layer.

It may define:

- packet validation rules,
- replication state transitions,
- snapshot ordering,
- snapshot apply order,
- baseline/delta handling,
- prediction reconciliation behavior,
- interpolation buffer rules,
- threading boundaries,
- memory limits,
- diagnostics requirements.

It does not own:

- server authority implementation,
- editor collaboration policy,
- product session lifecycle,
- SDE compiler internals,
- editor UI,
- asset import/cook workflows,
- transport backend implementation,
- collaboration backend implementation.

---

## 4. Related Runtime Components

The current client-side replication chain is expected to involve these components:

```txt
PacketDemux
  receives decoded network packets and routes them by type

ReplicationStateMachine
  tracks replication connection/apply state

SnapshotApplyPipeline
  validates and applies authoritative snapshots

ReplicationApplyBridge
  maps replicated data into runtime ECS/world state

ReconciliationBuffer
  stores predicted local inputs and applies server corrections

InterpolationManager
  stores remote state samples for visual interpolation
```

This spec defines the contract between these components.

The exact class names may evolve.

The required behavior must not disappear just because someone renamed a file and felt productive.

---

## 5. High-Level Pipeline

The replication pipeline follows this order:

```txt
Network receive
    ↓
Packet framing validation
    ↓
Packet decode
    ↓
PacketDemux
    ↓
Replication packet validation
    ↓
ReplicationStateMachine transition check
    ↓
Snapshot ordering / stale rejection
    ↓
Baseline or delta validation
    ↓
SnapshotApplyPipeline
    ↓
ReplicationApplyBridge
    ↓
Authoritative runtime state update
    ↓
ReconciliationBuffer correction
    ↓
InterpolationManager sample update
    ↓
Diagnostics / metrics emission
```

No stage may skip validation and directly mutate runtime state.

No stage may apply stale authoritative state.

No stage may mutate editor state.

No stage may assume packet ordering is guaranteed by the transport.

---

## 6. Authority Model

The server is authoritative for multiplayer simulation.

The client may predict local controlled state.

The client may interpolate remote replicated state.

The client may render speculative visuals.

The client must not create authoritative truth.

Required authority rules:

```txt
Server snapshot state overrides client predicted state where authority applies.
Client input is intent, not truth.
Client prediction is temporary.
Server correction is authoritative.
Remote entity interpolation is visual-only.
```

Forbidden behavior:

```txt
Client accepts its predicted state as authority without server confirmation.
Client applies remote snapshots directly from unvalidated payload.
Client allows local systems to overwrite authoritative replicated state after apply.
Client treats editor collaboration operations as gameplay replication.
```

---

## 7. Packet Categories

The client replication pipeline may consume these packet categories:

```txt
HandshakeAccepted
HandshakeRejected
ServerSnapshot
ServerSnapshotDelta
InputAck
ServerCorrection
ReplicationBaseline
ReplicationResyncRequest
ReplicationResyncBegin
ReplicationResyncEnd
DisconnectNotice
ServerTimeSync
DiagnosticsPacket
```

Each packet category must have:

- packet type,
- protocol version,
- payload size,
- connection/session context,
- decode result,
- validation result,
- diagnostic failure path.

Unknown packet types must be rejected safely.

Malformed packets must not reach replication apply.

---

## 8. Packet Envelope Requirements

Every replication-relevant packet must be wrapped in a validated packet envelope.

Required envelope fields:

```txt
ProtocolVersion
PacketType
SequenceNumber
ConnectionId
SessionId
PayloadSize
PayloadBytes
ValidationField
```

Optional fields:

```txt
CompressionMode
EncryptionMode
ChannelId
AckRange
SendTimestamp
ServerTick
```

Validation requirements:

- protocol version must be supported,
- packet type must be known,
- payload size must match actual bytes,
- payload size must not exceed configured limits,
- session id must match active session where required,
- packet sequence must be valid for the channel,
- validation field must pass where enabled.

Invalid envelope behavior:

```txt
Reject packet.
Do not decode payload.
Do not mutate replication state.
Emit diagnostic.
```

---

## 9. Decode Rules

Packet decode must be deterministic and bounded.

Decode rules:

- decode must not allocate unbounded memory from packet-provided sizes,
- decode must validate all length fields,
- decode must validate enum ranges,
- decode must validate entity/component ids,
- decode must validate string sizes where strings exist,
- decode must reject unknown required fields,
- decode must preserve unknown optional fields only if explicitly supported.

Decode failure behavior:

```txt
Reject packet.
Record diagnostic.
Increment invalid packet counter.
Do not transition replication state unless the failure policy requires disconnect.
```

Forbidden decode behavior:

```txt
Partial decode mutates runtime state.
Decoder trusts payload size blindly.
Decoder allocates memory directly from untrusted length.
Decoder silently clamps invalid ids and continues.
```

Silently “fixing” invalid network input is not robustness.

It is bug laundering.

---

## 10. Replication State Machine

The client replication pipeline must maintain an explicit state machine.

Required states:

```txt
Idle
Connecting
Handshaking
WaitingForBaseline
Synchronized
Resynchronizing
Disconnected
Failed
```

Optional states:

```txt
Reconnecting
Draining
ShuttingDown
```

---

## 11. State Definitions

### 11.1 Idle

The client is not connected to a replication session.

Allowed actions:

- begin connection,
- clear replication buffers,
- reset diagnostics counters.

Forbidden actions:

- apply snapshots,
- accept input acknowledgements,
- accept deltas,
- mutate replicated runtime state from network packets.

---

### 11.2 Connecting

The client is establishing transport-level connectivity.

Allowed actions:

- send connection request,
- receive transport-level connection result,
- timeout connection attempt.

Forbidden actions:

- apply snapshots,
- accept baselines,
- process gameplay replication payloads.

---

### 11.3 Handshaking

The client and server are negotiating protocol/session compatibility.

Allowed actions:

- send handshake request,
- receive handshake accepted,
- receive handshake rejected,
- validate protocol version,
- validate session descriptor.

Forbidden actions:

- apply snapshots before handshake acceptance,
- enter synchronized state without baseline,
- accept input acknowledgement before controlled entity/session is established.

---

### 11.4 WaitingForBaseline

The client is connected but lacks a valid replication baseline.

Allowed actions:

- receive baseline snapshot,
- request baseline resend,
- reject deltas that require missing baseline,
- buffer limited relevant packets only if explicitly allowed.

Forbidden actions:

- apply delta snapshots,
- enter synchronized state without baseline,
- use stale baseline from a previous session.

---

### 11.5 Synchronized

The client has a valid baseline and can apply authoritative updates.

Allowed actions:

- apply valid snapshots,
- apply valid deltas,
- process input acknowledgements,
- perform reconciliation,
- update interpolation buffers,
- detect stale packets,
- transition to resynchronizing on baseline loss or severe invalid state.

Forbidden actions:

- apply snapshots from wrong session,
- apply stale ticks,
- mutate runtime replicated state from invalid payloads,
- ignore baseline mismatch.

---

### 11.6 Resynchronizing

The client is recovering replication consistency.

Allowed actions:

- request full baseline,
- clear or quarantine invalid pending deltas,
- preserve local non-replicated state where safe,
- suspend prediction if required,
- resume synchronized state after valid baseline.

Forbidden actions:

- apply deltas against invalid baseline,
- continue prediction as if synchronized when correction is impossible,
- silently discard authority mismatch without diagnostics.

---

### 11.7 Disconnected

The client has cleanly disconnected.

Allowed actions:

- clear network-owned buffers,
- mark replicated state stale,
- release session-owned resources,
- show disconnect diagnostics.

Forbidden actions:

- apply late snapshots,
- process input acknowledgements,
- keep session locks or network ownership alive.

---

### 11.8 Failed

The client has encountered an unrecoverable replication failure.

Allowed actions:

- emit failure diagnostics,
- transition to disconnected after cleanup,
- attempt reconnect only through explicit policy.

Forbidden actions:

- continue applying snapshots,
- hide failure from diagnostics,
- reuse corrupted baseline.

---

## 12. State Transition Rules

Allowed transitions:

```txt
Idle → Connecting
Connecting → Handshaking
Connecting → Failed
Handshaking → WaitingForBaseline
Handshaking → Failed
WaitingForBaseline → Synchronized
WaitingForBaseline → Resynchronizing
WaitingForBaseline → Failed
Synchronized → Resynchronizing
Synchronized → Disconnected
Synchronized → Failed
Resynchronizing → Synchronized
Resynchronizing → Disconnected
Resynchronizing → Failed
Disconnected → Idle
Failed → Disconnected
Failed → Idle
```

Forbidden transitions:

```txt
Idle → Synchronized
Connecting → Synchronized
Handshaking → Synchronized
WaitingForBaseline → Synchronized without valid baseline
Failed → Synchronized
Disconnected → Synchronized
```

Every state transition must record:

- previous state,
- next state,
- reason,
- server tick if available,
- local time,
- diagnostic code if applicable.

---

## 13. Snapshot Ordering Rules

Snapshots must be applied in authoritative tick order.

Each snapshot must include:

```txt
ServerTick
SnapshotId
BaselineId
SessionId
EntityUpdates
ComponentUpdates
RemovedEntities
AuthorityMetadata
```

Ordering rules:

- snapshot tick must be newer than last applied authoritative tick,
- duplicate snapshot id must be ignored,
- stale snapshot tick must be rejected,
- delta snapshot must reference a known valid baseline,
- baseline snapshot may replace the current baseline during resync,
- snapshot from wrong session must be rejected,
- snapshot from future protocol version must be rejected unless compatibility is explicitly supported.

Stale snapshot behavior:

```txt
Reject snapshot.
Do not mutate runtime state.
Record diagnostic.
Increment stale snapshot counter.
```

Duplicate snapshot behavior:

```txt
Ignore snapshot.
Do not emit high-severity diagnostic unless duplicates exceed threshold.
```

---

## 14. Baseline and Delta Rules

### 14.1 Baseline Snapshot

A baseline snapshot is a full authoritative replication reference.

Baseline rules:

- baseline must be self-contained,
- baseline must include baseline id,
- baseline must include server tick,
- baseline must pass payload validation,
- baseline must reset delta dependency chain,
- baseline must be used to enter or restore synchronized state.

Baseline apply behavior:

```txt
Validate baseline.
Clear incompatible pending deltas.
Apply baseline through SnapshotApplyPipeline.
Update last applied tick.
Update active baseline id.
Feed interpolation/prediction systems as required.
Emit diagnostics.
```

---

### 14.2 Delta Snapshot

A delta snapshot contains changes relative to a baseline or prior acknowledged state.

Delta rules:

- delta must reference known baseline id,
- delta tick must be newer than last applied tick,
- delta must not skip required dependency unless gap recovery is supported,
- delta payload must validate against registered component serializers,
- delta must fail safely on missing baseline.

Missing baseline behavior:

```txt
Reject delta.
Request resync or baseline resend.
Transition to Resynchronizing if required.
```

Forbidden behavior:

```txt
Apply delta against guessed baseline.
Apply delta against stale baseline from previous session.
Patch missing fields using local predicted state unless explicitly defined.
```

Guessing baseline state is not networking.

It is improvisational corruption.

---

## 15. Entity Replication Rules

Each replicated entity update must include:

```txt
EntityId
EntityOperation
AuthorityOwner
ReplicationFlags
ComponentPayloads
```

Allowed entity operations:

```txt
Create
Update
Destroy
AuthorityChanged
Dormant
Awake
```

Rules:

- entity id must be valid,
- create must not overwrite existing entity unless explicitly allowed by resync baseline,
- update must target existing entity or trigger defined missing-entity behavior,
- destroy must be idempotent,
- stale updates for destroyed entities must be rejected,
- authority changes must be ordered.

Missing entity update behavior:

```txt
If in baseline apply:
  create entity if baseline says it exists.

If in delta apply:
  reject update or trigger resync depending on policy.
```

Destroy behavior:

```txt
Mark entity destroyed.
Remove or detach replicated components.
Clear interpolation samples.
Clear prediction ownership if affected.
Reject later stale updates for same authority generation.
```

---

## 16. Component Replication Rules

Each replicated component payload must include:

```txt
ComponentTypeId
ComponentVersion
PayloadSize
PayloadBytes
ReplicationMode
```

Rules:

- component type must be registered,
- serializer/deserializer must exist,
- component version must be supported,
- payload size must be bounded,
- payload must validate before apply,
- apply order must be deterministic,
- invalid component payload must not partially mutate entity state.

Component apply order:

```txt
1. Validate all component payload metadata.
2. Decode component payloads into temporary validated structures.
3. Validate entity/component operation compatibility.
4. Apply entity structural changes.
5. Apply component data changes.
6. Emit component change events.
7. Update diagnostics and metrics.
```

Forbidden behavior:

```txt
Decode component directly into live component storage.
Apply half of a snapshot after later component validation fails.
Allow unknown component type to mutate entity state.
```

If apply is not atomic at whole-snapshot level, it must at least be atomic per entity or have explicit rollback semantics.

Pick one.

Not picking one is just hoping.

---

## 17. Snapshot Apply Atomicity

The pipeline must define its atomicity level.

Recommended production rule:

```txt
A snapshot is validated fully before live state mutation begins.
```

Minimum acceptable rule:

```txt
Each entity update is validated fully before that entity is mutated.
Failed entity update does not corrupt other entities.
Snapshot diagnostic records partial apply status.
```

Preferred behavior:

```txt
Validate entire snapshot.
Prepare apply plan.
Apply plan.
Commit authoritative tick.
```

Failure before commit:

```txt
No last-applied tick update.
No baseline update.
No reconciliation update.
No interpolation sample update for failed data.
```

Failure after partial commit must be explicitly diagnosed.

Silent partial apply is forbidden.

---

## 18. Prediction Rules

Client prediction is allowed only for locally controlled state.

Prediction requires:

```txt
ControlledEntityId
InputCommand
LocalTick
PredictedState
PendingInputBuffer
LastServerAck
```

Rules:

- predicted input must be stored by tick,
- server acknowledgement must identify accepted input range,
- rejected input must be diagnosed,
- correction must rewind/replay where supported,
- correction magnitude must be tracked,
- prediction must not affect remote entity authoritative state.

Prediction apply flow:

```txt
Collect local input
    ↓
Apply local prediction
    ↓
Store input in ReconciliationBuffer
    ↓
Send input command to server
    ↓
Receive authoritative snapshot / input ack
    ↓
Compare predicted state to authority
    ↓
Correct and replay pending inputs
    ↓
Update diagnostics
```

Forbidden behavior:

```txt
Prediction overwrites newer server state.
Prediction applies to entities without local control.
Prediction buffer grows without bound.
Correction is ignored because it looks small.
```

Small errors become large errors.

That is physics, networking, and most project management.

---

## 19. Reconciliation Rules

Reconciliation aligns predicted local state with server authority.

Required reconciliation inputs:

```txt
AuthoritativeTick
AuthoritativeState
LastAcknowledgedInput
PendingInputsAfterAck
PredictionPolicy
```

Rules:

- acknowledged inputs are removed from pending buffer,
- unacknowledged inputs remain pending,
- authoritative correction is applied first,
- pending inputs are replayed after correction where supported,
- severe divergence may force snap correction or resync,
- reconciliation must emit diagnostics.

Correction categories:

```txt
None
Minor
Moderate
Severe
Unrecoverable
```

Recommended behavior:

```txt
None:
  no correction

Minor:
  smooth correction

Moderate:
  rewind/replay and smooth visual result

Severe:
  snap or controlled hard correction

Unrecoverable:
  request resync
```

---

## 20. Input Acknowledgement Rules

`InputAck` packets must identify which client inputs were accepted by the server.

Required fields:

```txt
ClientInputSequence
ServerTickProcessed
AcceptedRange
RejectedRange
ControlledEntityId
AckReason
```

Rules:

- ack must belong to active session,
- ack must refer to known input sequence range,
- duplicate ack may be ignored,
- stale ack must be rejected,
- rejected input must be diagnosed,
- ack must not be processed before handshake/baseline sync.

Input ack behavior:

```txt
Validate ack.
Remove accepted inputs from pending buffer.
Record rejected inputs.
Trigger reconciliation if authoritative state differs.
Emit diagnostics.
```

---

## 21. Interpolation Rules

Interpolation applies to remote visual state.

It must not mutate authoritative simulation state.

Interpolation requires:

```txt
EntityId
ServerTick
SampleTime
TransformState
OptionalComponentVisualState
```

Rules:

- interpolation samples are stored per entity,
- samples are ordered by server tick,
- duplicate samples are ignored,
- stale samples are rejected or ignored,
- missing samples are handled gracefully,
- teleport or authority discontinuity clears invalid samples,
- destroyed entity clears interpolation buffer.

Interpolation output:

```txt
VisualTransform
VisualState
InterpolationAlpha
SampleHealth
```

Forbidden behavior:

```txt
Interpolation writes into authoritative ECS state.
Interpolation resurrects destroyed entities.
Interpolation hides severe replication desync without diagnostics.
```

Interpolation is visual smoothing.

It is not a legal appeal against server authority.

---

## 22. Time and Tick Rules

The client must distinguish local time, render time, client simulation tick, and server tick.

Required time concepts:

```txt
LocalMonotonicTime
RenderFrameTime
ClientPredictionTick
ServerTick
EstimatedServerTime
InterpolationDelay
```

Rules:

- server tick is authoritative for replicated state order,
- local monotonic time is used for local scheduling/diagnostics,
- render frame time must not reorder authoritative updates,
- interpolation delay must be explicit,
- prediction tick must map to input sequence/tick.

Forbidden behavior:

```txt
Use wall-clock time to order server snapshots.
Apply snapshots based on arrival time instead of server tick.
Mix render frame delta with authoritative simulation step.
```

---

## 23. Thread Ownership Rules

The pipeline must define thread ownership explicitly.

Recommended thread model:

```txt
Network Thread
  receives bytes
  performs packet envelope validation
  queues decoded or raw packet work safely

Replication Thread or Runtime Update Thread
  performs packet decode/validation
  updates replication state machine
  prepares snapshot apply plan

Simulation/Main Runtime Thread
  commits authoritative ECS/world state mutation

Render Thread
  consumes interpolation output or render-safe snapshots
```

Allowed cross-thread communication:

```txt
bounded queues
immutable packet buffers
validated apply plans
double-buffered visual state
atomic diagnostic counters
```

Forbidden cross-thread behavior:

```txt
network thread directly mutates ECS state
render thread mutates authoritative replication state
decoder writes into live component storage from arbitrary worker thread
unbounded packet queue grows forever
```

Threading bugs are the kind that make everyone religious.

Avoid them with ownership.

---

## 24. Memory Rules

Replication memory usage must be bounded.

Required bounded structures:

```txt
packet receive queue
decoded packet queue
pending snapshot queue
baseline cache
delta cache
prediction input buffer
interpolation sample buffer
diagnostic ring buffer
```

Each bounded structure must define:

- max count,
- max bytes where relevant,
- overflow behavior,
- diagnostic counter,
- recovery behavior.

Overflow behavior examples:

```txt
Drop oldest stale packet.
Drop newest packet and request resync.
Disconnect on abusive input.
Clear interpolation samples for entity.
Force baseline request.
```

Forbidden:

```txt
Unbounded packet buffering.
Unbounded prediction history.
Unbounded interpolation samples.
Unbounded diagnostics growth.
```

Memory leaks do not become less embarrassing because they happen in multiplayer code.

---

## 25. Security Rules

Client replication must treat all server packets as untrusted until validated.

Even if the server is authoritative, the packet path can still be malformed, outdated, corrupted, or incompatible.

Required validation:

- packet envelope validation,
- payload size validation,
- protocol version validation,
- session id validation,
- entity id validation,
- component type validation,
- serializer version validation,
- tick ordering validation,
- baseline reference validation.

Client must fail safely.

Server authority does not mean client decoder gets to be reckless.

---

## 26. Diagnostics Requirements

Every major replication failure must produce diagnostics.

Required diagnostic categories:

```txt
PacketValidation
PacketDecode
ProtocolVersion
SessionMismatch
ReplicationState
SnapshotOrdering
BaselineMissing
DeltaInvalid
EntityApply
ComponentApply
Prediction
Reconciliation
Interpolation
Threading
MemoryBudget
Resync
Disconnect
```

Minimum diagnostic fields:

```txt
DiagnosticCode
Severity
Message
SessionId
ServerTick
SnapshotId
EntityId optional
ComponentTypeId optional
PacketType optional
Recoverability
```

Severity levels:

```txt
Trace
Info
Warning
Error
Fatal
```

Recoverability levels:

```txt
Recoverable
RequiresResync
RequiresReconnect
Unrecoverable
```

Diagnostics must be visible enough for runtime debugging.

A replication failure that only appears as “something feels off” is not a diagnostic.

It is folklore.

---

## 27. Resynchronization Rules

The client must enter resynchronization when it cannot safely continue from current replication state.

Resync triggers:

- missing baseline,
- invalid delta dependency,
- severe prediction divergence,
- repeated invalid snapshot sequence,
- component schema mismatch,
- authority generation mismatch,
- packet loss beyond configured tolerance,
- explicit server resync request.

Resync behavior:

```txt
Enter Resynchronizing state.
Stop applying deltas.
Request baseline or wait for baseline.
Clear incompatible pending deltas.
Quarantine or clear interpolation buffers as needed.
Suspend prediction if required.
Apply valid baseline.
Return to Synchronized.
Emit diagnostics.
```

Forbidden behavior:

```txt
Continue applying deltas after baseline invalidation.
Hide resync from diagnostics.
Reuse old session baseline.
```

---

## 28. Disconnect Rules

Disconnect may be clean or failure-driven.

Clean disconnect behavior:

```txt
Stop accepting replication packets.
Clear session-owned packet queues.
Clear prediction buffer.
Clear interpolation samples or mark stale.
Release replicated session state where appropriate.
Emit disconnect reason.
```

Failure disconnect behavior:

```txt
Record fatal diagnostic.
Stop apply pipeline.
Reject late packets.
Clear unsafe state.
Return to product/runtime-safe state.
```

Disconnect reasons:

```txt
UserRequested
ServerShutdown
Timeout
ProtocolMismatch
InvalidPacket
RateLimited
AuthenticationFailed
SessionExpired
ResyncFailed
InternalError
```

---

## 29. Runtime State Mutation Rules

Only the approved apply stage may mutate authoritative replicated runtime state.

Allowed mutation point:

```txt
SnapshotApplyPipeline → ReplicationApplyBridge → Runtime replicated state
```

Forbidden mutation points:

```txt
Packet decoder directly mutates ECS.
PacketDemux directly mutates components.
InterpolationManager mutates authoritative state.
ReconciliationBuffer mutates remote entity authority.
Render thread mutates replicated runtime state.
Editor panel mutates replicated runtime state.
```

All mutations must be:

- ordered,
- validated,
- diagnostic-capable,
- compatible with rollback/resync policy.

---

## 30. Component Registration Requirement

Replication apply requires component registration.

A component is replication-eligible only if it has:

```txt
ComponentTypeId
Serializer
Deserializer
Version
ValidationPolicy
ApplyPolicy
ReplicationMode
```

Missing component registration behavior:

```txt
Reject component payload.
Record diagnostic.
Reject entity update or snapshot depending on atomicity policy.
Request resync if required.
```

Forbidden behavior:

```txt
Create unknown component from raw payload.
Store unknown bytes in live ECS component.
Ignore missing serializer and mark snapshot applied.
```

If component registration is missing, replicated ECS apply is not production-safe.

That is not pessimism.

That is arithmetic.

---

## 31. Formal Invariants

The implementation must preserve these invariants.

### Invariant 1 — No Unvalidated Mutation

```txt
No network payload may mutate runtime replicated state before validation succeeds.
```

### Invariant 2 — Monotonic Authoritative Apply

```txt
LastAppliedServerTick must never move backward within the same session.
```

### Invariant 3 — Baseline Validity

```txt
A delta snapshot may only apply against a known valid baseline.
```

### Invariant 4 — Session Isolation

```txt
Packets from one session must not affect another session.
```

### Invariant 5 — Prediction Is Temporary

```txt
Predicted client state must yield to authoritative server state.
```

### Invariant 6 — Interpolation Is Visual

```txt
Interpolation must not mutate authoritative runtime state.
```

### Invariant 7 — Bounded Memory

```txt
Replication queues and buffers must have configured bounds.
```

### Invariant 8 — Thread Ownership

```txt
Only the approved runtime/apply thread may commit replicated state mutation.
```

### Invariant 9 — Diagnostic Visibility

```txt
Rejected packets, failed apply, resync, and disconnect must emit diagnostics.
```

### Invariant 10 — Editor Collaboration Separation

```txt
Runtime gameplay replication must not depend on editor-private collaboration policy or headers.
```

---

## 32. Runtime Replication vs Editor Collaboration

Runtime/game replication and editor collaboration may share some primitives.

They must not share policy blindly.

Runtime replication optimizes for:

- low latency,
- prediction,
- reconciliation,
- relevance,
- bandwidth,
- server authority.

Editor collaboration optimizes for:

- correctness,
- visibility,
- permissions,
- edit ownership,
- conflict handling,
- safe publishing.

Allowed shared primitives:

```txt
stable ids
ordered event envelopes
canonical hashes
diagnostic payloads
participant/session descriptors where neutral
bounded memory policy patterns
```

Forbidden coupling:

```txt
Runtime replication includes Editor/include/SagaEditor/Collaboration
Editor collaboration reuses gameplay prediction as edit conflict resolution
Server authority policy is treated as editor permission policy
```

Similar problems are not identical problems.

This should be obvious, which is exactly why it must be written down.

---

## 33. SDE Boundary

SDE remains a standalone deterministic data compiler.

Client replication may consume runtime artifacts produced by build/cook/SDE pipelines.

Allowed examples:

```txt
component schema id
runtime artifact reference
compiled graph artifact id
serialization version
diagnostic payload
```

Forbidden dependency direction:

```txt
Client replication → SDE compiler internals
SDE → SagaEngine runtime headers
SDE → SagaEditor headers
SDE → SagaServer headers
SDE → SagaShared headers
SDE → SagaCollaboration headers
```

Replication consumes runtime-ready schema/artifact references.

It does not host the compiler.

---

## 34. Testing Requirements

The implementation must be tested against this specification.

Required unit tests:

```txt
packet envelope validation
packet decode failure
unknown packet type rejection
protocol mismatch rejection
state transition validation
stale snapshot rejection
duplicate snapshot handling
missing baseline delta rejection
component registration failure
component payload validation failure
entity destroy idempotency
input acknowledgement handling
prediction buffer bounds
interpolation sample ordering
resync trigger behavior
disconnect cleanup
```

Required integration tests:

```txt
connect → handshake → baseline → synchronized
server snapshot → client apply → interpolation update
client input → server ack → reconciliation
delta with valid baseline applies
delta with missing baseline triggers resync
malformed packet never mutates state
stale snapshot never changes last applied tick
server correction rewinds/replays prediction
disconnect rejects late packets
```

Required stress tests:

```txt
packet loss
packet duplication
packet reordering
snapshot burst
invalid packet spam
large snapshot rejection
prediction buffer overflow
interpolation buffer pressure
resync loop prevention
long-running session memory stability
```

---

## 35. Recommended File Layout

This layout is illustrative, not mandatory.

The important part is ownership and separation.

```txt
Engine/include/SagaEngine/Replication/
  ReplicationState.hpp
  ReplicationStateMachine.hpp
  Snapshot.hpp
  SnapshotId.hpp
  BaselineId.hpp
  SnapshotApplyPipeline.hpp
  ReplicationApplyBridge.hpp
  ReplicationDiagnostic.hpp
  ReplicationLimits.hpp

Engine/src/SagaEngine/Replication/
  ReplicationStateMachine.cpp
  SnapshotApplyPipeline.cpp
  ReplicationApplyBridge.cpp
  ReplicationDiagnostic.cpp

Engine/include/SagaEngine/Network/
  PacketEnvelope.hpp
  PacketType.hpp
  PacketCodec.hpp
  PacketValidationResult.hpp

Engine/src/SagaEngine/Network/
  PacketCodec.cpp
  PacketValidation.cpp

Engine/include/SagaEngine/Prediction/
  InputCommand.hpp
  InputAck.hpp
  ReconciliationBuffer.hpp
  PredictionState.hpp

Engine/src/SagaEngine/Prediction/
  ReconciliationBuffer.cpp
  PredictionState.cpp

Engine/include/SagaEngine/Interpolation/
  InterpolationSample.hpp
  InterpolationBuffer.hpp
  InterpolationManager.hpp

Engine/src/SagaEngine/Interpolation/
  InterpolationBuffer.cpp
  InterpolationManager.cpp
```

---

## 36. Compatibility Rules

Protocol compatibility must be explicit.

Required compatibility checks:

- protocol version,
- packet schema version,
- component serializer version,
- snapshot format version,
- baseline/delta compatibility,
- feature flags where applicable.

Unsupported compatibility behavior:

```txt
Reject packet or session.
Emit diagnostic.
Do not attempt best-effort apply unless explicitly defined.
```

Best-effort network decoding is often just undefined behavior wearing customer-service clothes.

---

## 37. Failure Policy Summary

| Failure | Required Behavior |
|---|---|
| Unknown packet type | Reject packet, emit diagnostic |
| Unsupported protocol version | Reject session or packet, emit diagnostic |
| Payload size mismatch | Reject packet, emit diagnostic |
| Stale snapshot | Reject snapshot, do not mutate state |
| Duplicate snapshot | Ignore safely, count duplicate |
| Missing baseline | Reject delta, request resync |
| Invalid component type | Reject payload/update, emit diagnostic |
| Component decode failure | Reject update/snapshot according to atomicity policy |
| Prediction severe divergence | Correct hard or request resync |
| Interpolation gap | Hold, extrapolate only if policy allows, or snap |
| Buffer overflow | Apply configured overflow behavior and emit diagnostic |
| Thread ownership violation | Treat as fatal in debug, diagnose in release |
| Resync failure | Transition to Failed or Disconnected |

---

## 38. Production Definition of Correctness

The client replication pipeline is correct when:

- every network payload is validated before mutation,
- authoritative ticks apply monotonically,
- stale snapshots are rejected,
- deltas require valid baselines,
- component payloads require registered serializers,
- prediction yields to server correction,
- interpolation remains visual-only,
- buffers are bounded,
- thread ownership is explicit,
- failures are diagnostic-visible,
- resync recovers safely or fails clearly,
- runtime replication remains separate from editor collaboration policy.

---

## 39. Final Rule

Client replication is allowed to be fast.

It is not allowed to be vague.

The pipeline must know:

```txt
what packet arrived,
what state it belongs to,
whether it is valid,
whether it is ordered,
what it mutates,
which thread owns the mutation,
how failure is reported,
and how recovery happens.
```

Anything less is not a replication system.

It is a multiplayer-themed coincidence machine.