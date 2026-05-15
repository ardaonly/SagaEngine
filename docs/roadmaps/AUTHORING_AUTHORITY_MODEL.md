# Saga Authoring Authority Model

> Last updated: 2026-05-15
> Status: Proposed architecture specification
> Target: A production-grade authority model for Saga gameplay authoring across visual graphs, blocks, SDE definitions, C# scripting, runtime preview, client prediction, server validation, replication, persistence, diagnostics, collaboration, and publish gates.
> Scope: Authority contexts, graph/block/script authority metadata, validation rules, authoring diagnostics, prediction safety, replication effects, persistence effects, security boundaries, runtime/server consumption, build/publish gates, and editor UX expectations.

---

## 0. Document Status

This document defines the authority model used by Saga authoring systems.

It is not a generic networking overview.

It exists because Saga is an MMO-first engine, and an MMO-first engine cannot let authoring tools pretend that every block can run anywhere.

A single-player visual scripting system can get away with sloppy authority semantics for a while.

A multiplayer/server-authoritative engine cannot.

If a user can accidentally place `GiveItem` inside a client-only graph and ship it, the editor is not beginner-friendly.

It is breach-friendly.

---

## 1. Purpose

The purpose of the Saga Authoring Authority Model is to make every gameplay authoring surface aware of where logic is allowed to run, what side effects it may perform, and whether those effects are safe for multiplayer execution.

This model applies to:

- visual gameplay graphs,
- high-level intent blocks,
- low-level runtime/network blocks,
- SDE graph/source definitions,
- C# block-callable APIs,
- generated C# previews,
- runtime preview execution,
- server authoritative execution,
- client prediction,
- replication,
- persistence,
- diagnostics,
- build and publish gates.

Correct model:

```txt
Authoring Surface
    ↓ declares intent
Authority Metadata
    ↓ validated by SDE / editor / Forge
Compiled Artifact
    ↓ consumed by runtime/server according to authority
Server remains authoritative
```

Incorrect model:

```txt
A graph runs wherever the user dragged it because the editor allowed it.
```

That is not flexibility.

That is a vulnerability with a UI.

---

## 2. Companion Documents

| Document | Purpose |
|---|---|
| `SAGA_GAMEPLAY_GRAPH_ROADMAP.md` | Gameplay graph, blocks, palettes, SDE/C#/runtime integration |
| `ClientReplicationFormalSpec.md` | Client replication correctness, prediction, reconciliation, server authority |
| `ENGINE_ROADMAP.md` | Runtime/server ownership, authoritative simulation, replication, security |
| `EDITOR_ROADMAP.md` | Editor graph UX, Problems panel, generated code preview, authoring surfaces |
| `SDE_ROADMAP.md` | Deterministic compiler, semantic validation, canonical IR, artifact emission |
| `SHARED_ROADMAP.md` | Neutral contracts for graph, diagnostics, artifacts, scripting, authority metadata |
| `FORGE_ROADMAP.md` | Build orchestration, validation, SDE compile, script compile, publish gates |
| `PRISM_ROADMAP.md` | Stale generated code/artifact detection and boundary validation |
| `COLLABORATION_ROADMAP.md` | Claims, locks, conflicts, collaboration diagnostics |
| `DIAGNOSTICS_ROADMAP.md` | Runtime diagnostics, structured reports, health and failure visibility |
| `DependencyGraph.md` | Dependency and ownership boundaries |

---

## 3. Core Principle

Saga uses a server-authoritative model for multiplayer gameplay.

Required principle:

```txt
Clients may request.
Clients may predict.
Clients may render.
Clients may interpolate.
Servers decide authoritative truth.
```

Forbidden principle:

```txt
The client ran the graph, so the world state is true.
```

This distinction must be visible at authoring time, not discovered after someone cheats inventory state in a live session.

---

## 4. Ownership Model

### 4.1 Saga Product Ownership

- [ ] `Saga` owns product-level authority mode selection and publish gating.

  Done means:

  - project settings can declare multiplayer/server-authoritative mode,
  - product workflows can show authority validation failures,
  - preview/run/publish operations respect authority diagnostics,
  - invalid authority usage blocks publish where severity requires it.

- [ ] `Saga` does not implement low-level authority validation internals.

  Done means Saga consumes validation results from SDE, Forge, runtime/server services, and diagnostics instead of becoming a compiler or gameplay validator.

---

### 4.2 SagaEditor Ownership

- [ ] `SagaEditor` displays authority information in authoring UI.

  Done means the editor can show:

  - graph authority context,
  - block authority requirements,
  - pin-level authority restrictions where needed,
  - replication effects,
  - persistence effects,
  - prediction safety,
  - validation diagnostics,
  - suggested safe alternatives.

- [ ] `SagaEditor` does not decide final server truth.

  Done means the editor cannot silently override authority errors just because the user wants the graph to run.

The editor can help the user understand the rules.

It cannot repeal physics, networking, or security.

---

### 4.3 SagaShared Ownership

- [ ] `SagaShared` owns neutral authority metadata contracts.

  Done means `SagaShared` may contain:

  - `AuthorityContext`,
  - `AuthorityRequirement`,
  - `ReplicationEffect`,
  - `PersistenceEffect`,
  - `PredictionSafety`,
  - `SecurityBoundary`,
  - `ExecutionDomain`,
  - `AuthoringDiagnosticPayload`,
  - `AuthorityValidationResult`.

- [ ] `SagaShared` does not own validation implementation.

  Forbidden in `SagaShared`:

  - graph compiler passes,
  - runtime/server authority implementation,
  - editor diagnostics panels,
  - scripting host internals,
  - network transport logic,
  - persistence services,
  - anti-cheat implementation.

---

### 4.4 SDE Ownership

- [ ] SDE performs deterministic authority validation for graph/source definitions.

  Done means SDE can reject or warn about:

  - client graphs using server-only blocks,
  - predicted graphs using prediction-unsafe blocks,
  - persistent writes without authority,
  - replicated mutations from invalid execution domains,
  - editor-only logic compiled into runtime artifacts,
  - missing authority annotations,
  - incompatible authority metadata between graph and block.

- [ ] SDE emits machine-readable authority diagnostics.

  Done means diagnostics include:

  - diagnostic code,
  - severity,
  - graph id,
  - node id,
  - pin id where applicable,
  - source range,
  - required authority,
  - actual authority,
  - suggested correction where safe.

---

### 4.5 Runtime Ownership

- [ ] Runtime executes client-authorized graph artifacts only within allowed domains.

  Done means runtime can execute:

  - client-only UI graphs,
  - visual-only graphs,
  - prediction-safe graphs,
  - local preview graphs,
  - non-authoritative presentation logic.

- [ ] Runtime rejects or quarantines invalid authority artifacts.

  Done means runtime does not silently execute graph artifacts that require server authority.

---

### 4.6 Server Ownership

- [ ] Server owns authoritative gameplay graph execution.

  Done means server can execute approved graph artifacts for:

  - inventory mutation,
  - quest reward logic,
  - combat resolution,
  - ability effects,
  - entity spawn/despawn,
  - persistent player/world state changes,
  - authoritative replication source updates.

- [ ] Server validates client requests before invoking authoritative graph behavior.

  Done means client requests are treated as intent, not as proof.

Example:

```txt
Client requests: CompleteQuest(QuestId)
Server validates: player state, quest requirements, inventory, permissions, anti-cheat checks
Server executes: authoritative quest reward graph
```

---

### 4.7 Forge Ownership

- [ ] Forge enforces authority build gates.

  Done means Forge can:

  - invoke SDE validation,
  - collect authority diagnostics,
  - fail builds based on severity/profile,
  - prevent packaging invalid graph/script artifacts,
  - produce authority validation reports.

---

### 4.8 Prism Ownership

- [ ] Prism detects stale or inconsistent authority-related generated outputs.

  Done means Prism can report:

  - generated C# does not match graph authority metadata,
  - block binding metadata changed but generated artifacts are stale,
  - server/client artifact placement mismatch,
  - forbidden dependency direction involving authority contracts,
  - missing generated source maps for authority diagnostics.

---

## 5. Authority Contexts

Authority context describes where a graph, block, script binding, or artifact is allowed to execute.

Required contexts:

```txt
ClientOnly
ServerOnly
ClientPredicted
ServerValidated
Replicated
VisualOnly
EditorOnly
BuildOnly
ToolOnly
SharedPure
```

---

### 5.1 ClientOnly

Client-only logic runs on the client and must not mutate authoritative world state.

Allowed examples:

```txt
update local HUD
play local sound
show dialogue UI
highlight interactable object
local camera shake
client-only tutorial hint
```

Forbidden examples:

```txt
give item
remove item
set authoritative health
complete quest
spawn authoritative entity
write persistent player data
write replicated property as authority
```

---

### 5.2 ServerOnly

Server-only logic runs on the authoritative server.

Allowed examples:

```txt
give item
remove item
apply authoritative damage
complete quest
spawn entity
save player progress
write replicated state
validate ability activation
```

Forbidden examples:

```txt
client-only UI drawing
local camera animation
editor panel mutation
raw viewport selection
```

---

### 5.3 ClientPredicted

Client-predicted logic runs temporarily on the client for responsiveness, but server correction remains authoritative.

Allowed examples:

```txt
predict local movement
predict local ability windup
play local predicted animation
start temporary visual effect
buffer input command
```

Forbidden examples:

```txt
award permanent item
commit quest completion
save persistent state
spawn authoritative entity
modify economy state
accept predicted result without server confirmation
```

---

### 5.4 ServerValidated

Server-validated logic begins from a client request but must be checked by the server before authoritative effects occur.

Allowed flow:

```txt
Client request
    ↓
Server validates request
    ↓
Server executes approved authoritative graph
    ↓
Server replicates result
```

Examples:

```txt
use item
activate ability
complete quest objective
interact with NPC
enter zone portal
trade request
```

---

### 5.5 Replicated

Replicated logic affects state that is transmitted from server to clients.

Rules:

- replicated state mutation requires server authority unless explicitly marked prediction-safe,
- client replicas are presentation copies,
- replicated state must be versioned and validated,
- replication effects must be declared.

---

### 5.6 VisualOnly

Visual-only logic affects presentation but not authoritative gameplay state.

Allowed examples:

```txt
particles
screen effects
sound triggers
animation blend hints
camera shake
local UI hints
```

Visual-only logic must not be used as gameplay truth.

---

### 5.7 EditorOnly

Editor-only logic runs inside authoring tools.

Allowed examples:

```txt
editor workflow automation
custom panel behavior
selection helpers
graph layout helpers
asset authoring helpers
```

Forbidden:

```txt
runtime artifact execution
server gameplay mutation
shipping game logic
```

---

### 5.8 BuildOnly

Build-only logic runs during validation, compilation, cooking, packaging, or publishing.

Allowed examples:

```txt
asset cook rule
package validation
artifact generation
schema migration
static analysis pass
```

Forbidden:

```txt
runtime gameplay mutation
server session mutation
client UI behavior during gameplay
```

---

### 5.9 ToolOnly

Tool-only logic belongs to standalone tools such as SDE, Forge, Prism, or SagaTools.

Tool-only code must not become runtime gameplay behavior by accident.

---

### 5.10 SharedPure

SharedPure logic is deterministic, side-effect-free, and safe to evaluate across domains.

Allowed examples:

```txt
math expression
pure condition
constant lookup
schema-safe enum comparison
string formatting for diagnostics
```

Rules:

- no network send,
- no persistence write,
- no runtime object mutation,
- no global state mutation,
- no time/random dependence unless explicitly modeled.

---

## 6. Execution Domains

Execution domain describes where an artifact is physically intended to run.

Required domains:

```txt
ClientRuntime
ServerRuntime
EditorProcess
BuildProcess
ToolProcess
TestProcess
```

A graph may declare both an authority context and an execution domain.

Example:

```txt
authority: ServerOnly
executionDomain: ServerRuntime
```

Example:

```txt
authority: VisualOnly
executionDomain: ClientRuntime
```

Invalid example:

```txt
authority: ServerOnly
executionDomain: ClientRuntime
```

Unless this is a generated client request stub, this should fail validation.

---

## 7. Side Effect Model

Every graph/block/script binding must declare side effects.

Required side effect categories:

```txt
Pure
ReadLocalState
WriteLocalState
ReadAuthoritativeState
WriteAuthoritativeState
ReadReplicatedState
WriteReplicatedState
SendClientEvent
SendServerRequest
SendReliableNetworkEvent
SendUnreliableNetworkEvent
ReadPersistentState
WritePersistentState
StartTransaction
CommitTransaction
AllocateRuntimeResource
ScheduleJob
ScheduleTimer
EmitDiagnostic
EditorMutation
BuildArtifactWrite
```

Rules:

- pure logic can be widely reused,
- authoritative writes require server authority,
- persistent writes require approved persistence context,
- network sends require explicit channel/effect metadata,
- editor mutations cannot appear in runtime artifacts,
- build artifact writes cannot appear in gameplay runtime artifacts.

Hidden side effects are forbidden.

If a block mutates server state but advertises itself like a harmless math node, that block is defective.

---

## 8. Block Authority Metadata

Every block definition must declare authority requirements.

Required block metadata:

```txt
blockId
displayName
category
requiredAuthority
allowedExecutionDomains
sideEffects
replicationEffect
persistenceEffect
predictionSafety
securityBoundary
failurePolicy
diagnostics
```

Example:

```txt
block Inventory.GiveItem {
    requiredAuthority: ServerOnly
    allowedExecutionDomains: [ServerRuntime]
    sideEffects: [WriteAuthoritativeState, WritePersistentState, WriteReplicatedState]
    replicationEffect: ReplicateInventory
    persistenceEffect: TransactionalWrite
    predictionSafety: PredictionUnsafe
    securityBoundary: TrustedServerOnly
}
```

Example:

```txt
block UI.ShowMessage {
    requiredAuthority: ClientOnly
    allowedExecutionDomains: [ClientRuntime]
    sideEffects: [WriteLocalState]
    replicationEffect: None
    persistenceEffect: None
    predictionSafety: VisualOnly
    securityBoundary: ClientPresentation
}
```

Example:

```txt
block Math.ClampFloat {
    requiredAuthority: SharedPure
    allowedExecutionDomains: [ClientRuntime, ServerRuntime, EditorProcess, BuildProcess, ToolProcess, TestProcess]
    sideEffects: [Pure]
    replicationEffect: None
    persistenceEffect: None
    predictionSafety: PredictionSafe
    securityBoundary: Pure
}
```

---

## 9. Graph Authority Metadata

Every graph must declare authority metadata.

Required graph metadata:

```txt
graphId
graphKind
authorityContext
executionDomain
allowedEntryPoints
allowedSideEffects
replicationPolicy
persistencePolicy
predictionPolicy
validationProfile
artifactDestination
```

Example:

```txt
graph QuestReward.OnQuestCompleted {
    authorityContext: ServerOnly
    executionDomain: ServerRuntime
    allowedSideEffects: [ReadAuthoritativeState, WriteAuthoritativeState, WritePersistentState, WriteReplicatedState, EmitDiagnostic]
    persistencePolicy: TransactionRequired
    replicationPolicy: ServerSourceOnly
    artifactDestination: ServerPackage
}
```

Example:

```txt
graph HUD.UpdateQuestTracker {
    authorityContext: ClientOnly
    executionDomain: ClientRuntime
    allowedSideEffects: [ReadReplicatedState, WriteLocalState]
    persistencePolicy: None
    replicationPolicy: ReadOnlyReplica
    artifactDestination: ClientPackage
}
```

---

## 10. C# Authority Metadata

C# block-callable APIs must declare authority metadata explicitly.

Required concept:

```csharp
[BlockCategory("Inventory")]
[BlockName("Give Item")]
[ServerOnly]
[WritesPersistentState]
[Replicates("Inventory")]
[PredictionUnsafe]
public static void GiveItem(PlayerRef player, ItemId item, int count)
{
    // implementation
}
```

Rules:

- C# methods exposed as blocks must provide authority metadata,
- missing metadata is an error unless default policy explicitly allows pure functions,
- server-only C# APIs cannot be placed in client-only graphs,
- client-only C# APIs cannot mutate authoritative state,
- freeform C# is not assumed safe,
- generated C# must preserve authority comments/metadata for diagnostics.

Incorrect model:

```txt
Any public static C# method can become a block.
```

That is a convenient way to make the engine unreviewable.

---

## 11. Replication Effects

Replication effects describe how graph/block execution affects networked state.

Required replication effect kinds:

```txt
None
ReadReplicatedState
WriteReplicatedProperty
SendClientEvent
SendServerRequest
SendReliableEvent
SendUnreliableEvent
BroadcastEvent
OwnerOnlyReplication
RelevanceFilteredReplication
SnapshotSourceMutation
PredictionCorrection
```

Validation rules:

- `WriteReplicatedProperty` requires server authority unless explicitly prediction-safe,
- `SnapshotSourceMutation` requires server authority,
- `SendServerRequest` is allowed from client domains but must route to validation,
- `SendClientEvent` requires server or approved local presentation context,
- reliable event spam must produce diagnostics,
- high-frequency replicated writes must produce warnings/errors based on profile.

Example diagnostic:

```txt
Graph Combat.OnTick writes replicated property Health every tick.
This may exceed replication budget. Use change-based replication or lower frequency.
```

---

## 12. Persistence Effects

Persistence effects describe whether graph/block execution affects saved state.

Required persistence effect kinds:

```txt
None
ReadOnly
SessionState
PlayerState
WorldState
TransactionalWrite
DeferredWrite
BuildArtifactWrite
```

Validation rules:

- persistent writes require server authority or build/tool authority,
- player/world persistent writes require schema-compatible payloads,
- transactional writes require transaction scope,
- high-frequency persistent writes require batching/deferred policy,
- client-only graphs cannot write persistent state,
- predicted graphs cannot commit persistent state.

Example diagnostic:

```txt
Block Inventory.GiveItem writes persistent player state but graph PlayerHUD.OnButtonClicked is ClientOnly.
Route this through a ServerValidated request graph.
```

---

## 13. Prediction Safety

Prediction safety describes whether logic can run speculatively on the client.

Required states:

```txt
PredictionSafe
PredictionUnsafe
VisualOnly
ServerCorrectionRequired
DeterministicPure
```

Rules:

- predicted logic cannot commit authoritative state,
- predicted logic cannot write persistent state,
- predicted logic cannot allocate unbounded runtime resources,
- predicted logic must be correctable or visual-only,
- deterministic pure logic may be reused in prediction and server validation,
- predicted movement/abilities must connect to server correction where applicable.

Allowed predicted examples:

```txt
local movement prediction
local animation prediction
local ability windup visualization
input buffering
client-side cooldown display
```

Forbidden predicted examples:

```txt
quest reward commit
inventory mutation
currency award
trade commit
world spawn authority
```

---

## 14. Security Boundaries

Security boundary describes trust assumptions.

Required security boundary kinds:

```txt
Pure
ClientPresentation
ClientRequest
ServerValidation
TrustedServerOnly
EditorTooling
BuildTooling
TestOnly
```

Rules:

- `ClientRequest` must never directly mutate authoritative state,
- `ServerValidation` must validate identity, permission, state, rate limits, and domain rules,
- `TrustedServerOnly` cannot be called from client artifacts,
- `EditorTooling` must not ship as runtime gameplay,
- `TestOnly` must not be included in shipping artifacts.

Example:

```txt
UseItemButton.OnClick
    securityBoundary: ClientRequest
    allowed effect: SendServerRequest

Server.UseItemRequestHandler
    securityBoundary: ServerValidation
    allowed effect: validate + execute server graph
```

---

## 15. Authority Validation Rules

### 15.1 Graph-Level Validation

- [ ] Validate graph authority context.

  Done means a graph fails validation if:

  - authority context is missing,
  - execution domain is incompatible,
  - artifact destination is incompatible,
  - graph kind is not allowed for declared authority,
  - graph entry point conflicts with authority policy.

---

### 15.2 Node-Level Validation

- [ ] Validate every node against graph authority.

  Done means a node fails validation if:

  - block requires stronger authority than graph provides,
  - block side effects exceed graph policy,
  - block execution domain is incompatible,
  - block is editor-only/build-only in runtime graph,
  - block is deprecated or missing migration.

---

### 15.3 Edge-Level Validation

- [ ] Validate authority weakening through edges.

  Done means an edge fails validation if:

  - client request output connects directly to server-only mutation,
  - visual-only data is used as authoritative state,
  - replicated read-only data is connected to mutation pin without validation,
  - unsafe prediction path reaches persistence or authoritative mutation.

---

### 15.4 Script Binding Validation

- [ ] Validate C# block bindings.

  Done means validation catches:

  - missing authority attributes,
  - mismatched C# authority vs block definition,
  - server-only C# method exposed to client graph,
  - unsafe persistence writes,
  - unsupported parameter types,
  - stale generated binding metadata.

---

### 15.5 Artifact Destination Validation

- [ ] Validate artifact placement.

  Done means build/package fails if:

  - server-only artifact is staged into client package as executable logic,
  - client-only artifact is staged into server authority package as gameplay truth,
  - editor-only artifact is staged into runtime package,
  - build-only artifact is staged into runtime package,
  - artifact lacks authority manifest.

---

## 16. Request/Validation Flow

Client-to-server gameplay requests must use explicit validation flow.

Correct flow:

```txt
Client graph emits request
    ↓
Request envelope created
    ↓
Server receives request
    ↓
Server validates session/account/player/entity/context/rate limit
    ↓
Server invokes authoritative graph if valid
    ↓
Server mutates state
    ↓
Server replicates result
    ↓
Client applies authoritative update
```

Incorrect flow:

```txt
Client graph calls GiveItem directly
    ↓
Inventory changed
```

That is not networking.

That is trusting the attacker with a form.

---

## 17. Editor UX Requirements

### 17.1 Authority Badges

- [ ] Show authority badges on graphs and blocks.

  Required badges:

```txt
ClientOnly
ServerOnly
Predicted
ServerValidated
Replicated
VisualOnly
EditorOnly
BuildOnly
```

---

### 17.2 Invalid Placement Feedback

- [ ] Block invalid placement in strict modes.

  Done means users cannot place server-only blocks in client-only graphs without an explicit conversion/request flow.

- [ ] Provide explainable diagnostics.

  Example:

```txt
Inventory.GiveItem cannot run in ClientOnly graph HUD.OnButtonClicked.
Use SendServerRequest → ServerValidated Inventory Request instead.
```

---

### 17.3 Safe Alternatives

- [ ] Offer safe replacement flows.

  Examples:

```txt
GiveItem in client graph
    → Create ServerValidated request flow

ApplyDamage in client prediction graph
    → Use PredictHitVisual + ServerValidateHit

SavePlayerData in tick graph
    → Use DeferredPersistenceTransaction
```

---

### 17.4 Profile-Aware Visibility

- [ ] Hide dangerous blocks from beginner profiles.

  Done means beginner palettes do not expose:

  - raw replicated property writes,
  - persistence transaction internals,
  - packet/network primitives,
  - server authority mutation internals,
  - unsafe prediction blocks.

Hiding complexity is acceptable.

Hiding consequences is not.

---

## 18. Build and Publish Gates

### 18.1 Build Gate

- [ ] Forge blocks invalid authority builds.

  Done means build fails when:

  - authority errors exceed profile threshold,
  - server-only artifacts are missing,
  - generated binding metadata is stale,
  - graph artifact authority manifest is missing,
  - client package contains executable server-only logic.

---

### 18.2 Publish Gate

- [ ] Saga product publish blocks invalid authority state.

  Done means publish fails when:

  - authoritative graphs fail validation,
  - persistence effects are unsafe,
  - replication effects are invalid,
  - security boundary is missing,
  - runtime/server package artifact manifests disagree.

---

### 18.3 CI Gate

- [ ] CI can enforce authority validation.

  Done means CI can run:

```txt
forge validate --profile server
forge build --profile shipping
prism stale
prism validate-boundaries
```

and fail on authority violations.

---

## 19. Diagnostics

Required diagnostic families:

```txt
Authority.MissingContext
Authority.InvalidExecutionDomain
Authority.BlockRequiresServer
Authority.BlockRequiresClient
Authority.PredictionUnsafe
Authority.PersistenceRequiresServer
Authority.ReplicationRequiresServer
Authority.VisualUsedAsAuthority
Authority.EditorOnlyInRuntime
Authority.BuildOnlyInRuntime
Authority.InvalidArtifactDestination
Authority.MissingSecurityBoundary
Authority.ScriptBindingMismatch
Authority.StaleGeneratedMetadata
```

Required diagnostic fields:

```txt
diagnosticCode
severity
message
graphId
nodeId
pinId
blockId
scriptBindingId
sourceRange
requiredAuthority
actualAuthority
executionDomain
sideEffects
suggestedFix
```

Example:

```txt
Authority.BlockRequiresServer
Error: Inventory.GiveItem requires ServerOnly authority, but graph HUD.OnButtonClicked is ClientOnly.
Suggested fix: route through a ServerValidated request graph.
```

---

## 20. Testing Strategy

### 20.1 Contract Tests

- [ ] Add authority contract tests.

  Required coverage:

  - authority enum serialization,
  - authority requirement descriptors,
  - replication effect descriptors,
  - persistence effect descriptors,
  - prediction safety descriptors,
  - diagnostic payload serialization.

---

### 20.2 SDE Validation Tests

- [ ] Add SDE authority validation tests.

  Required cases:

  - valid server-only graph,
  - valid client-only graph,
  - valid server-validated request graph,
  - invalid client graph with server-only block,
  - invalid predicted graph with persistence write,
  - invalid editor-only block inside runtime graph,
  - missing authority metadata,
  - invalid artifact destination.

---

### 20.3 Editor Tests

- [ ] Add editor authority UX tests.

  Required cases:

  - badges render for graph/block authority,
  - invalid block placement is rejected or diagnosed,
  - Problems panel receives authority diagnostics,
  - safe alternative action creates request/validation graph scaffold,
  - beginner palette hides unsafe blocks.

---

### 20.4 Runtime/Server Tests

- [ ] Add artifact authority consumption tests.

  Required cases:

  - client runtime rejects server-only artifact execution,
  - server runtime accepts server-only artifact,
  - runtime rejects missing authority manifest,
  - server rejects invalid client request,
  - server executes valid request after validation,
  - replication result is server-sourced.

---

### 20.5 Forge/Prism Tests

- [ ] Add build and stale artifact tests.

  Required cases:

  - Forge fails invalid authority graph build,
  - Forge prevents wrong package placement,
  - Prism detects stale generated authority metadata,
  - Prism detects forbidden dependency direction,
  - CI profile fails on publish-blocking authority error.

---

## 21. MVP Vertical Slice

The first implementation target should be a quest reward authority slice.

Required scenario:

```txt
Client clicks Complete Quest
    ↓
Client sends ServerValidated request
    ↓
Server validates quest state and required items
    ↓
Server executes QuestReward graph
    ↓
Server removes required items
    ↓
Server gives gold and XP
    ↓
Server marks inventory/progression dirty
    ↓
Server persists transaction
    ↓
Server replicates result
    ↓
Client updates UI from replicated state
```

Required authoring behavior:

- client graph cannot place `GiveGold` directly,
- editor suggests `SendServerRequest`,
- server graph allows `GiveGold`, `GiveXP`, `RemoveItem`, and `CommitTransaction`,
- SDE validates graph authority,
- Forge fails if graph is mis-staged,
- runtime/client cannot execute server-only artifact,
- server executes authoritative artifact,
- diagnostics explain any invalid placement.

This slice is small but exposes the entire truth:

```txt
authoring → authority → validation → build → server execution → replication → client display
```

If this path works, the engine has a real MMO authoring foundation.

If this path fails, adding more blocks is just decorating the problem.

---

## 22. Non-Goals

This authority model does not attempt to:

- replace all networking design,
- replace server security policy,
- make arbitrary client logic trusted,
- make C# scripts automatically safe,
- make beginner mode capable of unsafe server mutation,
- allow editor-only workflows into runtime packages,
- turn SDE into runtime authority,
- hide all multiplayer complexity from professional users.

The goal is controlled authoring power.

Not fantasy no-code authority.

---

## 23. Risk Register

### 23.1 Risk: Authority Metadata Becomes Optional

Mitigation:

- missing authority metadata is an error for side-effecting blocks,
- pure functions may have safe defaults,
- CI enforces metadata completeness.

---

### 23.2 Risk: Beginner UX Hides Too Much

Mitigation:

- beginner profile hides unsafe primitives,
- diagnostics explain safe request flows,
- advanced mode exposes deeper controls intentionally.

---

### 23.3 Risk: Runtime Trusts Compiled Artifacts Blindly

Mitigation:

- artifacts include authority manifest,
- runtime/server validate artifact destination,
- Forge package gates enforce placement,
- diagnostics report mismatch.

---

### 23.4 Risk: C# Bindings Bypass Graph Validation

Mitigation:

- C# block bindings require authority attributes,
- script compile emits binding manifest,
- SDE/Forge validate binding metadata,
- Prism detects stale generated metadata.

---

### 23.5 Risk: Prediction Logic Commits Real State

Mitigation:

- prediction-safe metadata required,
- predicted graphs reject persistence and authority writes,
- server correction flow required for gameplay-affecting prediction.

---

## 24. Suggested File Targets

Expected shared contracts:

```txt
Shared/include/SagaShared/Authority/AuthorityContext.hpp
Shared/include/SagaShared/Authority/AuthorityRequirement.hpp
Shared/include/SagaShared/Authority/ExecutionDomain.hpp
Shared/include/SagaShared/Authority/SideEffect.hpp
Shared/include/SagaShared/Authority/ReplicationEffect.hpp
Shared/include/SagaShared/Authority/PersistenceEffect.hpp
Shared/include/SagaShared/Authority/PredictionSafety.hpp
Shared/include/SagaShared/Authority/SecurityBoundary.hpp
Shared/include/SagaShared/Authority/AuthorityValidationResult.hpp
Shared/include/SagaShared/Authority/AuthorityDiagnostic.hpp
```

Expected graph integration contracts:

```txt
Shared/include/SagaShared/Graph/GraphAuthorityMetadata.hpp
Shared/include/SagaShared/Graph/BlockAuthorityMetadata.hpp
Shared/include/SagaShared/Graph/GraphArtifactAuthorityManifest.hpp
Shared/include/SagaShared/Graph/GraphArtifactDestination.hpp
```

Expected SDE validation files:

```txt
Tools/SystemDefinitionEngine/include/SDE/Authority/AuthorityValidator.hpp
Tools/SystemDefinitionEngine/include/SDE/Authority/AuthorityDiagnostic.hpp
Tools/SystemDefinitionEngine/src/Authority/AuthorityValidator.cpp
Tools/SystemDefinitionEngine/tests/AuthorityValidationTests.cpp
```

Expected editor files:

```txt
Editor/include/SagaEditor/Authority/AuthorityBadgePresenter.h
Editor/include/SagaEditor/Authority/AuthorityDiagnosticAdapter.h
Editor/include/SagaEditor/Authority/SafeAuthorityFlowActions.h
Editor/src/SagaEditor/Authority/AuthorityBadgePresenter.cpp
Editor/src/SagaEditor/Authority/AuthorityDiagnosticAdapter.cpp
Editor/src/SagaEditor/Authority/SafeAuthorityFlowActions.cpp
```

Expected Forge files:

```txt
Tools/Forge/include/Forge/Validation/AuthorityValidationStep.hpp
Tools/Forge/src/Validation/AuthorityValidationStep.cpp
Tools/Forge/tests/AuthorityValidationStepTests.cpp
```

Expected Prism files:

```txt
Tools/Prism/include/Prism/Validation/StaleAuthorityMetadataCheck.hpp
Tools/Prism/include/Prism/Validation/ArtifactAuthorityPlacementCheck.hpp
Tools/Prism/src/Validation/StaleAuthorityMetadataCheck.cpp
Tools/Prism/src/Validation/ArtifactAuthorityPlacementCheck.cpp
```

---

## 25. Decision Summary

Preserve these decisions:

```txt
Server authority is the default for MMO gameplay truth.
Client input is intent, not truth.
Client prediction is temporary and correctable.
Visual-only logic is never gameplay authority.
Every side-effecting block must declare authority metadata.
C# block bindings must declare authority metadata.
SDE validates authority deterministically.
Forge enforces authority build/package gates.
Runtime/server validate artifact authority manifests.
Editor shows authority errors before runtime.
Beginner profiles hide dangerous blocks but do not remove the rules.
```

The goal is not to make multiplayer complexity disappear.

The goal is to make invalid multiplayer authoring impossible, obvious, or at least publish-blocking.

That is the difference between an MMO-first engine and a pretty single-player editor with network features taped on later.
