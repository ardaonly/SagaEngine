---
title: Replication, networking, and server authority
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Replication, networking, and server authority

This reference records the durable client replication correctness model, transport separation, chaos policy, and server-authority boundaries. The 0.0.10 formal specification remains valuable as a correctness checklist, but 0.0.11 code and tests decide which portions are implemented. The existence of these contracts is not a production networking or dedicated-server claim.

## Owner separation

`Networking` owns transport-neutral packet movement, channels, fragmentation/reassembly, bandwidth/rate/backpressure policy, packet registration, normalization, and controlled network-chaos injection. `Replication` owns snapshot/delta decoding, ordering, baseline state, interest, apply, prediction/reconciliation support, interpolation, memory/rate guards, telemetry, and client gameplay command encoding. `ServerAuthority` owns authoritative movement/gameplay decisions, zones, actor ownership, cross-zone visibility, shard relationships, and server-specific connection composition.

`World`, `Simulation`, and ECS own live state according to their contracts. Replication may mutate that state only through the declared apply bridge and authority table. Networking never gains gameplay authority because it delivered a packet.

EditorCollaboration is separate. It may use operations, presence, locks, or transport-like names, but editor document collaboration is not runtime replication.

## High-level client pipeline

The durable receive direction is:

```text
transport bytes
  -> packet envelope and size validation
  -> packet registry/demultiplex
  -> baseline or delta decode
  -> session/sequence/state-machine validation
  -> bounded staging
  -> atomic replication apply bridge
  -> interpolation/presentation views
```

No unvalidated bytes mutate ECS or world state. Each stage returns a classified result. A decode success only means the bytes match the wire contract; it does not mean sequence, baseline, authority, or schema rules accept the snapshot.

## Packet envelope

A packet envelope identifies protocol/schema version, packet category, session/connection identity, sequence or tick information, payload length, and integrity data where the contract defines it. Limits are checked before allocation and decode. Arithmetic uses overflow-safe bounds.

Unknown packet categories, unsupported versions, truncated headers, inconsistent lengths, oversized entity/component counts, duplicate identifiers, invalid offsets, and payload references outside the buffer are rejected. The decoder does not trust sender-provided counts to reserve unbounded memory.

Diagnostic context includes category, bounded size, session/sequence/tick, and stable reason. It does not log arbitrary payload bytes or secrets by default.

### Packet categories

Useful protocol categories remain semantically separate:

- handshake/version/capability negotiation;
- baseline/full world snapshot;
- delta snapshot/patch;
- input acknowledgement and correction;
- client gameplay command and command result;
- interest/create/destroy or dormancy transition;
- reliable control, heartbeat, resync request, and disconnect;
- diagnostics/test-only chaos metadata only where explicitly enabled.

Each category defines reliability/order expectations and maximum payload/counts. A category cannot be reinterpreted from arbitrary payload content after the envelope has been accepted.

### Decode discipline

Decoders use cursor/remaining-length checks for every field and nested record. Count multiplied by element size is overflow-checked before reserve/read. Variable blobs are copied or referenced only within the owned packet lifetime. Strings have encoding/length policy. Floating-point values used for movement are finite and later validated against gameplay bounds.

Decode produces neutral records without pointers into a receive buffer that will be reused. If zero-copy views are used, the packet storage lifetime is retained explicitly through apply.

## Replication state machine

The conceptual client states are idle, connecting, handshaking, waiting for a baseline, synchronized, resynchronizing, disconnected, and failed. Current enum names may differ, but the transition rules remain useful:

- no snapshot apply before session establishment and accepted protocol/schema;
- a client cannot become synchronized without a valid baseline;
- an accepted disconnect stops further mutation for that session;
- resynchronization rejects dependent deltas until a new baseline arrives;
- a terminal failure remains visible and cannot be cleared by an unrelated late packet;
- a new session resets sequence windows, baselines, prediction history, and interpolation buffers.

Unexpected packets are rejected or ignored with an explicit reason according to state. They do not opportunistically force the machine forward.

### Transition table

| Current | Event | Result |
| --- | --- | --- |
| Idle | Connect requested | Connecting with new session generation |
| Connecting | Transport established | Handshaking |
| Handshaking | Compatible handshake accepted | Waiting for baseline |
| Handshaking | Version/schema rejected | Failed or Disconnected with reason |
| Waiting for baseline | Valid baseline atomically applied | Synchronized |
| Synchronized | Valid next delta/snapshot | Remain synchronized and advance authority |
| Synchronized | Missing/mismatched base or catastrophic divergence | Resynchronizing and request baseline |
| Resynchronizing | Valid fresh baseline | Synchronized with old histories reset |
| Any active state | Valid disconnect/transport close | Disconnected and stop mutation |
| Any active state | Internal invariant/security bound failure | Failed or policy disconnect |

Connection retries construct a new generation. They do not reuse old pending packet queues or acknowledgement history.

## Sequence and ordering

Authoritative apply is monotonic within a session. A sequence window detects stale packets, duplicates, and values outside the accepted reorder window. Wraparound uses a defined modular comparison rather than ordinary unsigned greater-than.

Reordering at the transport level does not justify out-of-order world mutation. Packets can be buffered within a bounded window or rejected according to category. Reliable control messages and snapshot streams can have different ordering needs, but the apply layer sees a deterministic accepted order.

Session identity is part of ordering. A numerically newer sequence from an old session is still stale.

## Baseline and delta rules

A baseline is a complete enough authoritative state to establish a known apply base. It declares its session, tick/sequence, schema, entities/components, and integrity/size bounds. Acceptance replaces or initializes the client's baseline in one committed operation.

A delta identifies the exact baseline or predecessor it depends on. It is accepted only if that base is present and compatible. Missing base, mismatched session/schema, stale predecessor, or invalid patch causes rejection/resynchronization rather than best-effort application.

Delta decode and delta apply are separate. The decoder can produce a neutral bounded patch; the apply stage validates entity/component registration, authority, operation type, and atomicity against current state.

The active baseline advances only after successful apply. A failed delta never changes the baseline marker while leaving half of its components mutated.

Baseline retention is bounded. If delta `D` references baseline `B`, `B` is retained until `D` is no longer admissible or a newer protocol rule replaces it. A delta chain is capped; the client requests a full baseline before dependency depth or memory exceeds policy.

Delta operations identify create/update/remove and component field/byte changes according to the schema. The decoder rejects duplicate operations that would make result order ambiguous unless the format explicitly orders them. Removing and then updating the same entity in one patch is invalid without a defined recreate semantic.

## Entity and component rules

Replicated entity records distinguish create, update, and remove semantics where supported. Create rejects identity collision unless the protocol explicitly treats the record as idempotent. Update rejects a missing entity unless an allowed recovery path converts it. Remove is ordered and cannot delete a locally authoritative object without the authority contract.

Component records carry registered type/schema identity and bounded payload. Unknown required components reject the snapshot or trigger compatibility policy. Optional unknown components can be skipped only when the wire format makes skipping safe and the compatibility contract allows it.

Component bytes are decoded into validated values before mutation. A type mismatch, size mismatch, invalid enum, non-finite/out-of-range value, or schema migration failure remains a rejection.

## Atomic application

Snapshot apply is atomic from the live world's perspective. The pipeline first validates and plans operations. It can apply into a staging world/transaction or record reversible mutations. Only after every required operation succeeds does it publish the new authoritative state and baseline marker.

If an operation fails, the previous committed world remains valid. Diagnostics preserve the failing entity/component/operation without dumping unbounded data. Partial success is not reported as synchronized.

The apply bridge is the only replication path permitted to mutate live replicated ECS/world state. Packet handlers and worker decoders do not retain mutable world references.

An apply plan can record preconditions and operations in deterministic entity/component order. Before commit it verifies every target registration, authority, expected existence/version, and memory bound. During commit, either the world transaction guarantees all-or-nothing or the bridge keeps undo data sufficient to restore the exact prior committed state.

Events visible to gameplay/render observers are published after commit. Observers cannot see entity creation followed by rollback because a later component failed. Telemetry for a rejected apply is emitted without publishing state-change events.

## Authority table

Authority distinguishes server-authoritative state, permitted client prediction, local-only presentation state, and any explicitly delegated ownership. The client cannot overwrite server-authoritative components merely because a local script requests it.

Authority transfer, where implemented, is an explicit ordered record tied to a session/tick and validated by ServerAuthority. A zone handoff or actor ownership change does not occur by setting a generic networking flag.

ServerAuthority validates gameplay commands against actor/session ownership, rate policy, world state, and command-specific rules. A syntactically valid command can still be denied.

## Prediction and reconciliation

Prediction is temporary client state derived from local inputs. It never becomes authoritative solely because it rendered for several frames. Inputs carry stable sequence/tick identity and are retained in a bounded history until acknowledged or discarded.

When an authoritative snapshot arrives, reconciliation compares the confirmed state with the predicted state at the corresponding point. If within policy tolerance it can continue smoothly; otherwise it restores authoritative state and replays still-valid unacknowledged inputs through deterministic simulation where supported.

Prediction history, rollback snapshots, and patch journals have explicit memory limits. If required history is missing, the client uses a declared recovery/resync path rather than replaying from guessed data.

Not every component is predicted. The component/command registration contract identifies eligible state. Interpolation-only remote objects do not enter the local prediction loop.

Input acknowledgement removes only history older/equal to the authoritative ack under the protocol's modular sequence rule. An ack from the wrong session or ahead of sent input is rejected. Replay uses the same deterministic simulation functions and fixed tick inputs; side effects such as audio, particles, persistence, or network sends are suppressed/deduplicated during replay.

Correction thresholds are component/gameplay policy. A visual smoothing step can hide a correction for presentation but cannot retain a divergent authoritative value.

## Interpolation

Interpolation is presentation. It buffers accepted authoritative samples and produces a visual value for render time between samples. It does not mutate authoritative simulation truth, acknowledge input, or repair an invalid baseline.

Buffers are ordered, bounded, and reset on session/resync/teleport conditions. Extrapolation, if present, has a strict horizon; exceeding it freezes, snaps, or follows configured fallback rather than extending indefinitely.

Render consumes interpolated snapshots through a copied boundary. Gameplay and authority queries use committed simulation state unless a contract explicitly says otherwise.

## Tick and time

Network receive time, sender tick, local simulation tick, render interpolation time, and wall clock are distinct. RTT estimation uses a monotonic clock and bounded sampling. Clock offset estimates do not rewrite authoritative tick ordering.

Packets too far in the future or past are classified. The client can buffer within configured bounds or request resynchronization. It never allocates an unbounded queue waiting for a missing tick.

## Thread ownership

Transport and decode can run away from the simulation thread, but world mutation occurs on the declared owner thread/transaction. Decoded snapshots cross through bounded queues. The queue owns its payload until the apply stage consumes or rejects it.

Disconnect/shutdown stops admission, prevents callbacks into destroyed owners, cancels or drains queued work, and joins owned threads. A late transport callback checks session generation before publishing.

Queues use explicit capacity and ownership:

- receive owns packet bytes until decode completes;
- decode owns neutral records until queued or rejected;
- apply queue owns records until simulation consumes them;
- interpolation/render snapshots are immutable copies or retained generations;
- telemetry observes copies/counters and never retains mutable world objects.

Backpressure propagates toward transport. It does not block the simulation thread indefinitely waiting for a decode worker while holding world locks.

## Memory and rate limits

Every externally influenced collection has a bound: packet size, fragment count and bytes, entity/component count, pending reorder entries, baselines, rollback snapshots, patch journal, interpolation samples, command queue, and per-peer rate buckets.

Memory pressure is observable through `ReplicationMemoryTracker`, size guards, telemetry, and diagnostics. Exceeding a bound produces drop, disconnect, backpressure, or resync according to severity. It never silently disables validation.

Rate limiting operates at transport, packet category, command, and actor/session layers where appropriate. A token bucket or window uses a monotonic clock and deterministic test clock injection. Rate policy is not authentication.

### Bound violations

Small isolated violations can drop a packet with telemetry. Repeated malformed/oversized/rate violations can disconnect according to policy. A condition suggesting local invariant corruption stops the replication owner rather than blaming the peer. The response is deterministic and does not allocate a detailed diagnostic proportional to hostile input.

Fragment assembly has per-message and per-peer byte/count/deadline bounds. Duplicate fragments do not increase completed byte accounting. Overlap, inconsistent total, invalid index, or timeout releases the assembly.

## Interest management

Interest decides which authoritative entities/components should be replicated to a client. It can consider zone, distance, ownership, visibility policy, and bandwidth budget. Adaptive policy remains server-controlled and deterministic enough to diagnose.

Entering interest requires sufficient state, usually a baseline/create record. Leaving interest removes or dormants the client representation according to protocol. An interest transition cannot be represented as a delta against a base the client never received.

Replication interest is not render visibility. A replicated entity can be offscreen; a local render helper may have no network identity.

## Server authority and zones

ServerAuthority owns zone identifiers, player/actor ownership, movement validation, command dispatch, cross-zone visibility, shard peer contracts, and the private `ConnectionManager`/`ServerConnection` composition. Generic Networking does not own these authority types.

Movement validation checks actor ownership, finite/bounded values, allowed displacement/speed/time, world constraints exposed by the appropriate query, and replay/order state. A verdict states accepted, corrected, denied, rate-limited, or invalid as supported. It does not trust a client transform because transport authenticated a connection.

Cross-zone apron/visibility data describes bounded neighbor replication. A shard mesh transports server-to-server messages through its interface and peer state, but the presence of that interface does not prove deployed multi-shard operation, secure inter-server identity, or seamless production handoff.

### Authority transfer

A transfer identifies entity/actor, source zone, destination zone, transfer generation/tick, serialized validated state, and acknowledgement/commit state. The source remains authoritative until the protocol's commit point; the destination does not expose the actor as authoritative early. Timeout/peer failure resolves to one owner or an explicit failed/quarantined state, never two silent owners.

Cross-zone apron replicas are read/visibility copies. They cannot accept authoritative gameplay commands unless ownership transfers. Updates are ordered and removed when apron interest ends.

## Gameplay commands

The client encodes typed commands with a registered command/schema id, actor/session context, input sequence, target/value fields, and bounded payload. The server normalizes and dispatches only registered commands. Command-specific validators run before simulation mutation.

Unknown commands, wrong actor ownership, invalid target, failed precondition, excessive rate, malformed payload, or stale sequence return structured status. A response/ack is not proof that persistence or a later world outcome succeeded unless the protocol defines that result.

## Network chaos

Chaos injection is a controlled developer evidence layer around transport behavior. A profile can define seeded loss, duplication, delay, jitter, reordering, bandwidth pressure, disconnect/reconnect, or bounded corruption where safely modeled.

The same seed and ordered inputs must produce the same scheduled fault decisions. Chaos never runs implicitly in a production profile. Its report records profile, seed, counts, effective latency/order decisions, bounds, and resulting client state/diagnostics.

Fault injection does not bypass packet validation. Corrupted or duplicated data traverses the normal defensive path. A chaos test proves behavior under the modeled faults, not Internet-scale resilience or security.

Chaos scheduling uses a deterministic stream keyed by seed plus packet/event order. Delay/reorder queues are bounded by count, bytes, and maximum age. Disconnect/reconnect increments session generation. Reports distinguish injected drop from receiver validation drop so failures can be explained.

## Diagnostics and telemetry

Useful metrics include packet/byte rates by bounded category, drops by stable reason, decode failures, stale/duplicate/out-of-window counts, baseline/delta acceptance, resyncs, apply duration, entity/component operations, interpolation depth, prediction corrections, rollback/replay counts, memory usage, rate-limit decisions, RTT distribution, disconnect reason, zone/authority transitions, and chaos actions.

Cardinality is bounded. Entity identifiers can appear in sampled diagnostics but should not become unbounded metric labels. Telemetry collection does not hold locks across I/O or change replication decisions.

### Resynchronization diagnostics

A resync record includes trigger (missing baseline, gap, hash divergence, schema/migration failure, memory/history loss, or explicit server request), last committed session/tick/sequence, rejected packet identity, requested recovery, attempts, and terminal result. Repeated resync is rate-limited and can escalate to disconnect rather than loop forever.

Granular world hashes can localize divergence by subsystem/component. They are diagnostics and must use canonical deterministic serialization. A hash mismatch does not reveal which peer is correct; authority policy decides.

## Schema and compatibility

Handshake establishes protocol and replication schema compatibility. Component registration binds stable ids to decoder/apply contracts. Version migration is explicit and bounded; an unknown required version does not fall back to byte-copy into the current component.

Backward/forward compatibility claims name the exact versions and packet/component categories tested. Old clients are not assumed compatible indefinitely. Removing a field/type requires protocol transition or coordinated version break, not silent reinterpretation.

## Security robustness boundary

Defensive decode, bounds, rate limits, session isolation, and authority validation reduce damage from malformed/untrusted input. They are necessary security properties but do not constitute authentication, encryption, anti-cheat, secret management, DDoS protection, secure deployment, or a formal security review.

Never trust client timestamps, transforms, actor ids, permission flags, or command results as authority. Never log raw tokens/credentials/payloads by default. Cryptographic integrity or transport security is claimed only when the actual configured protocol and key/identity lifecycle are implemented and tested.

## Disconnect and recovery

Disconnect records a stable reason and whether retry is allowed. It stops new sends/applies, invalidates session generation, clears/bounds queues and prediction/interpolation state, releases transport resources, and publishes a terminal client state. Gameplay/world code receives a deliberate connection-lost transition rather than continuing with stale network authority indefinitely.

Retry begins from handshake/baseline. It does not apply queued old-session deltas after reconnect. User-visible recovery can preserve local UI/project state while resetting replicated runtime state.

## Formal invariants

The durable invariants are:

1. Unvalidated network data never mutates runtime state.
2. Accepted authoritative apply is monotonic within one session.
3. A delta is applied only against its declared compatible base.
4. State from one session cannot affect another.
5. Prediction is temporary and subordinate to authority.
6. Interpolation is presentation and does not become simulation truth.
7. Externally influenced memory and queues are bounded.
8. World mutation occurs through the declared owner thread/transaction.
9. Failure and resynchronization are diagnostically visible.
10. Runtime replication and editor collaboration remain separate owners.

## Evidence

Unit tests can prove wire bounds, sequence windows, state transitions, baseline/delta application, atomic rollback, authority tables, rate/memory guards, deterministic chaos, interpolation buffers, prediction reconciliation, and telemetry. Integration tests can connect client/server runtime owners with deterministic transport fixtures. Stress and soak tests are separately labeled and are not assumed safe local acceptance.

A passing loopback fixture does not prove production Internet networking, secure authentication, denial-of-service resistance, deployed shards, persistence, or a complete dedicated server. `SagaServer` currently has no implemented entry point, so server-library evidence must be described as such.

## Change checklist

- Keep transport, replication, authority, world, and editor collaboration separate.
- Validate length/count/version/session before allocation or mutation.
- Define state and sequence behavior for every packet category.
- Preserve baseline dependency and atomic apply.
- Bound every network-controlled queue and history.
- Keep prediction temporary and interpolation visual.
- route world mutation through the apply/authority owner;
- make chaos deterministic, opt-in, and reportable;
- match claims to focused evidence and repeat explicit deployment/security non-claims.
