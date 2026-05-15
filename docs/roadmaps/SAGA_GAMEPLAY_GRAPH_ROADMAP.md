# Saga Gameplay Graph Roadmap



---

## 0. Roadmap Convention



This roadmap exists because “visual scripting” is too small a phrase for the system Saga needs.

Saga needs an authority-aware gameplay authoring graph, not just colorful rectangles connected by optimistic spaghetti.

---

## 1. Document Purpose

This document defines the roadmap for Saga Gameplay Graph.

Saga Gameplay Graph is the shared gameplay authoring model that connects:



The target is not merely a node editor.

The target is a production-grade MMO-first gameplay authoring system where users can express game logic safely at different abstraction levels without accidentally giving the client authority over the world.

Correct model:



Incorrect model:

```txt
Editor panel owns graph truth because drawing nodes was convenient.
```

That path ends with a pretty editor and a runtime that quietly hates everyone.

---

## 2. Companion Documents



---

## 3. Product Definition

- [ ] Define Saga Gameplay Graph as the primary gameplay authoring abstraction.

  Done means:



- [ ] Keep gameplay graph separate from editor UI implementation.

  Done means:

  - editor graph panels display and edit graph resources,
  - graph contracts do not live under editor-private headers,
  - runtime/server do not include editor graph UI headers,
  - graph source and graph artifacts survive editor implementation changes.



  Done means:



---

## 4. Ownership Model

### 4.1 Saga Product Ownership

- [ ] `Saga` owns product-level gameplay authoring workflows.

  Done means `Saga` owns:

  - project open/create entry points,
  - mode selection,
  - authoring profile selection,
  - preview/build/publish entry points,
  - product-level graph validation result routing,
  - product-level failure state when graph artifacts are invalid.

- [ ] `Saga` does not own graph compiler internals.

  Done means Saga calls services/tools and consumes results instead of becoming a compiler.

---

### 4.2 SagaEditor Ownership

- [ ] `SagaEditor` owns graph authoring UI.

  Done means the editor owns:

  - graph canvas UI,
  - block palette UI,
  - node placement and routing UI,
  - graph inspector UI,
  - graph diagnostics display,
  - generated C# preview display,
  - graph command integration,
  - undo/redo behavior for graph edits,
  - editor-local graph adapters.

- [ ] `SagaEditor` does not own final graph truth.

  Done means the editor does not own:



Correct model:



Incorrect model:

```txt
The node editor is the compiler because it can serialize JSON.
```

A node editor that thinks serialization is compilation is already in technical debt.

---

### 4.3 SagaShared Ownership

- [ ] `SagaShared` owns neutral graph and artifact contracts only.

  Done means `SagaShared` may contain:

  - graph ids,
  - graph resource descriptors,
  - graph artifact references,
  - graph diagnostic payloads,
  - block definition descriptors,
  - block palette descriptors,
  - authority metadata descriptors,
  - replication effect descriptors,
  - persistence effect descriptors,
  - script binding descriptors,
  - source location descriptors.

- [ ] `SagaShared` does not own implementation.

  Forbidden in `SagaShared`:



Shared contracts are language.

They are not a landfill for systems nobody wanted to own.

---













---

### 4.5 Runtime Ownership

- [ ] Runtime consumes compiled graph artifacts for client-side execution where allowed.

  Done means runtime may execute or bind graph artifacts for:

  - client-only UI logic,
  - local visual effects,
  - client prediction logic where explicitly allowed,
  - runtime preview simulation,
  - safe non-authoritative gameplay presentation.

- [ ] Runtime does not accept graph logic as authority unless server policy allows it.

  Done means client graph execution cannot create authoritative world state.

---

### 4.6 Server Ownership

- [ ] Server consumes compiled graph artifacts for authoritative gameplay where approved.

  Done means server can execute or bind graph artifacts for:

  - authoritative gameplay events,
  - quest rewards,
  - inventory mutations,
  - ability execution,
  - combat rules,
  - zone rules,
  - persistence-affecting state transitions,
  - server-side validation hooks.

- [ ] Server remains the authority boundary.

  Done means graph logic cannot bypass server validation, permission policy, persistence rules, or anti-cheat checks.

---

### 4.7 Forge Ownership

- [ ] Forge owns graph-related build orchestration.

  Done means Forge can:



- [ ] Forge does not own graph compiler internals.

---

### 4.8 Prism Ownership

- [ ] Prism owns graph/code/artifact relationship analysis.

  Done means Prism can report:

  - generated C# source origin,
  - stale generated code,
  - graph artifact source mapping,
  - block binding references,
  - invalid dependency direction,
  - missing generated artifacts,
  - outdated artifact hashes.

- [ ] Prism does not compile graphs.

Prism observes.

It does not become the mayor of the compiler pipeline.

---

## 5. Dependency Rules

### 5.1 Allowed Dependencies

Allowed dependency directions:















---

### 5.2 Forbidden Dependencies

Forbidden dependency directions:



Forbidden design shortcuts:



---

## 6. Core Concepts

### 6.1 Graph Resource

- [ ] Define graph resource identity.

  Done means each graph resource has:

  - graph id,
  - project id,
  - resource id,
  - display path,
  - source file path,
  - graph kind,
  - schema version,
  - source hash,
  - compiled artifact reference,
  - validation state.

Graph resource identity must be stable enough for diagnostics, collaboration, build artifacts, and generated code mapping.

---

### 6.2 Graph Kind

- [ ] Define graph kinds.

  Done means graph kinds include at least:

  - gameplay event graph,
  - entity behavior graph,
  - ability graph,
  - quest graph,
  - inventory rule graph,
  - combat rule graph,
  - zone rule graph,
  - UI graph,
  - editor workflow graph,
  - macro/subgraph,
  - block expansion graph.

Each graph kind must declare its allowed authority contexts and runtime destinations.

---

### 6.3 Graph Document

- [ ] Define graph document model.

  Done means a graph document can represent:

  - nodes,
  - pins,
  - edges,
  - variables,
  - graph metadata,
  - source ranges,
  - comments,
  - groups,
  - subgraphs,
  - macros,
  - versioning metadata,
  - diagnostics.

The graph document is a source-level authoring representation.

It is not automatically the runtime representation.

---

### 6.4 Graph Node

- [ ] Define graph node model.

  Done means a graph node has:

  - node id,
  - block definition id,
  - display name,
  - input pins,
  - output pins,
  - execution pins where applicable,
  - literal/default values,
  - authority context,
  - source location,
  - editor position metadata,
  - validation state.

Node position is editor state.

Node semantics are graph/compiler state.

Do not confuse them unless the goal is to manufacture bugs recreationally.

---

### 6.5 Graph Pin

- [ ] Define graph pin model.

  Done means a pin has:

  - pin id,
  - name,
  - direction,
  - type,
  - optional/required state,
  - default value support,
  - connection cardinality,
  - authority restrictions where relevant,
  - diagnostics location.

---

### 6.6 Graph Edge

- [ ] Define graph edge model.

  Done means an edge has:

  - edge id,
  - source node id,
  - source pin id,
  - target node id,
  - target pin id,
  - edge kind,
  - validation state.

Required validation:

- pin types must be compatible,
- execution flow edges must connect execution pins,
- data flow edges must not create invalid cycles,
- authority context must not be weakened by connection,
- cross-graph references must be explicit.

---

### 6.7 Block Definition

- [ ] Define block definition contract.

  Done means every block definition includes:

  - stable block id,
  - display name,
  - category,
  - description,
  - input pin descriptors,
  - output pin descriptors,
  - execution behavior descriptor,
  - authority rule,
  - replication effect,
  - persistence effect,
  - prediction safety,
  - diagnostics behavior,
  - C# binding reference where applicable,
  - expansion graph reference where applicable,
  - visibility profile.

A block is not just a rectangle.

A block is a contract.

---

### 6.8 Block Registry

- [ ] Add block registry model.

  Done means the system can discover and validate blocks from:



- [ ] Reject unstable block identity.

  Done means block ids are stable across editor sessions, builds, generated artifacts, and collaboration operations.

---

### 6.9 Block Palette

- [ ] Add block palette model.

  Done means block palettes can expose different block sets for:

  - beginner mode,
  - intermediate mode,
  - advanced mode,
  - network/server developer mode,
  - editor tool developer mode,
  - package/plugin-specific workflows.

Block visibility must be profile-driven.

Deleting complexity is not the same thing as hiding it safely.

---

### 6.10 Block Expansion

- [ ] Support high-level block expansion.

  Done means high-level blocks can expand into lower-level graphs where allowed.

Examples:

```txt
Give Quest Reward
    ↓ expands to
Validate Quest State
Start Inventory Transaction
Give Items
Give XP
Commit Transaction
Mark Inventory Dirty
Notify Client
```

- [ ] Support expansion diagnostics.

  Done means the editor and compiler can report when a high-level block expansion is invalid, missing, deprecated, or incompatible with the current schema version.

---

## 7. Block Layers

### 7.1 Intent Blocks

- [ ] Define intent-level blocks.

  Done means beginner-facing blocks can express high-level gameplay intent without exposing runtime internals.

Examples:

```txt
When Player Joins
Give Starter Item
Complete Quest
Give Quest Reward
Spawn Enemy Wave
Show Dialogue
Teleport Player To Zone
```

Intent blocks must expand to validated lower-level graph behavior or bind to approved runtime/server services.

---

### 7.2 Gameplay Logic Blocks

- [ ] Define gameplay logic blocks.

  Done means intermediate users can compose game rules using:

  - event blocks,
  - branch blocks,
  - variables,
  - entity references,
  - component access,
  - ability logic,
  - quest logic,
  - inventory logic,
  - combat logic,
  - zone logic.

Examples:

```txt
On Damage Received
If DamageType == Fire
Apply Damage Multiplier
If Target Has Buff
Reduce Damage
Apply Final Damage
```

---

### 7.3 Runtime/Network Blocks

- [ ] Define low-level runtime/network blocks.

  Done means advanced users can access controlled lower-level behavior such as:

  - entity/component access,
  - replicated property writes,
  - server event dispatch,
  - client event dispatch,
  - prediction buffers,
  - authority checks,
  - transaction scopes,
  - persistence writes,
  - diagnostic event emission.

These blocks must be hidden from beginner palettes by default.

Advanced power is good.

Accidentally giving a beginner a packet-level footgun is not.

---

## 8. Type System

- [ ] Define graph type system.

  Done means graph pins and values can represent:

  - `Bool`,
  - `Int`,
  - `Float`,
  - `String`,
  - `Vector2`,
  - `Vector3`,
  - `Transform`,
  - `EntityRef`,
  - `PlayerRef`,
  - `SessionRef`,
  - `ZoneRef`,
  - `ItemId`,
  - `QuestId`,
  - `AbilityId`,
  - `ResourceId`,
  - `AssetId`,
  - `Replicated<T>`,
  - `ServerEvent<T>`,
  - `ClientEvent<T>`,
  - `Optional<T>`,
  - arrays/lists where supported,
  - maps/dictionaries where explicitly supported.

- [ ] Define type compatibility rules.

  Done means the compiler can reject invalid graph connections deterministically.

- [ ] Define type versioning and migration behavior.

  Done means old graph resources do not silently compile into wrong runtime behavior after type/schema changes.

---

## 9. Execution Model

- [ ] Define graph execution semantics.

  Done means each graph kind specifies:

  - event entry points,
  - execution order rules,
  - data flow rules,
  - allowed side effects,
  - allowed authority context,
  - runtime destination,
  - failure behavior,
  - diagnostics behavior.

- [ ] Define side-effect classification.

  Done means blocks can declare whether they:

  - read state,
  - mutate local state,
  - mutate authoritative state,
  - emit network events,
  - write persistent state,
  - allocate resources,
  - schedule jobs/timers,
  - emit diagnostics.

- [ ] Reject hidden side effects.

  Done means blocks that mutate authoritative state, persistent data, or replicated state must declare it.

Hidden side effects are where visual scripting systems go to become haunted houses.

---

## 10. Authority-Aware Authoring

- [ ] Define authority contexts.

  Required contexts:

```txt
ClientOnly
ServerOnly
ClientPredicted
ServerValidated
Replicated
EditorOnly
BuildOnly
```

- [ ] Validate graph authority.

  Done means validation rejects:

  - server-only blocks in client-only graphs,
  - persistent writes from client graphs,
  - replicated state writes outside approved authority context,
  - prediction-unsafe blocks inside predicted graphs,
  - editor-only blocks inside runtime graphs,
  - build-only blocks inside runtime graphs.

- [ ] Surface authority errors in editor diagnostics.

  Example diagnostic:

```txt
GiveItem requires ServerOnly authority.
Move this logic into a Server Event graph or call an approved request/validation flow.
```

- [ ] Surface authority errors in Forge/CI.

  Done means invalid authority usage can block build/package/publish.

---

## 11. Replication-Aware Authoring

- [ ] Define replication effects.

  Done means graph/block metadata can describe:

  - no replication,
  - replicated property write,
  - unreliable event,
  - reliable event,
  - owner-only replication,
  - area/relevance-filtered replication,
  - broadcast/multicast replication,
  - snapshot-affecting state mutation.

- [ ] Validate replication usage.

  Done means validation can detect:

  - excessive per-tick replicated writes,
  - reliable event spam,
  - missing authority for replicated mutation,
  - invalid owner-only target,
  - replication from wrong session/zone context,
  - use of visual-only interpolation state as authority.

- [ ] Emit replication diagnostics.

  Done means editor and reports can explain why a graph may produce high bandwidth cost or invalid replicated state.

---

## 12. Persistence-Aware Authoring

- [ ] Define persistence effects.

  Required effect kinds:

```txt
None
ReadOnly
SessionState
PlayerState
WorldState
TransactionalWrite
BuildArtifactWrite
```

- [ ] Validate persistence usage.

  Done means validation catches:

  - persistent writes from client-only graphs,
  - persistent writes in high-frequency tick graphs without batching,
  - missing transaction boundaries,
  - writes to resources without permission,
  - schema-incompatible persistence payloads.

- [ ] Provide persistence diagnostics.

  Example diagnostic:

```txt
This graph writes player inventory inside a high-frequency event.
Use a transaction or deferred persistence policy.
```

---

## 13. Prediction-Aware Authoring

- [ ] Define prediction safety metadata.

  Required states:

```txt
PredictionSafe
PredictionUnsafe
VisualOnly
ServerCorrectionRequired
```

- [ ] Validate predicted graphs.

  Done means predicted graphs reject:

  - persistence writes,
  - authoritative inventory mutations,
  - non-deterministic random authority changes,
  - server-only state changes,
  - unsafe resource loads,
  - unbounded side effects.

- [ ] Support server correction binding.

  Done means predicted client behavior can be connected to server correction flow where applicable.

---







  - graph declarations,
  - node declarations,
  - edge declarations,
  - block references,
  - graph metadata,
  - authority annotations,
  - replication annotations,
  - persistence annotations,
  - source locations.

- [ ] Define deterministic graph compile output.

  Done means graph compilation emits:

  - canonical graph IR,
  - graph artifact manifest,
  - source map,
  - diagnostics,
  - dependency manifest,
  - artifact hashes,
  - generated-code manifest where applicable.

- [ ] Prevent partial artifact publication on failed compile.

  Done means failed graph compile never publishes fake valid artifact state.

- [ ] Support incremental graph compilation.

  Done means unchanged graph sources can be skipped safely using content hashes, schema versions, compiler version, dependency manifest, and output artifact hashes.

---

## 15. Canonical Graph IR

- [ ] Define canonical graph IR.

  Done means IR is:



- [ ] Define canonical ordering.

  Done means identical graph semantics produce identical IR ordering and hashes.

- [ ] Separate semantic IR from editor layout metadata.

  Done means node positions, visual groups, colors, and UI comments do not affect runtime semantic hash unless explicitly configured.

---

## 16. C# Bridge

- [ ] Define C# block binding model.

  Done means C# methods/classes can expose block definitions through explicit metadata.

  Example concept:

```csharp
[BlockCategory("Inventory")]
[BlockName("Give Item")]
[ServerOnly]
[Replicates("Inventory")]
public static void GiveItem(PlayerRef player, ItemId item, int count)
{
    // implementation owned by scripting/runtime layer
}
```

- [ ] Support generated C# preview from graph IR.

  Done means editor can show generated C# for graph logic where useful.

- [ ] Define one-way and limited two-way conversion rules.

  Required decision:

```txt
Graph/Blocks → generated C# is supported.
Annotated C# APIs → block definitions are supported.
Arbitrary C# → lossless blocks is not guaranteed.
```

- [ ] Support custom code fallback.

  Done means freeform C# can appear as script artifacts or custom script nodes without pretending it is perfectly reversible into visual blocks.

Pretending arbitrary C# can always become clean blocks is not ambition.

It is lying to future debugging sessions.

---

## 17. Editor UX

### 17.1 Graph Canvas

- [ ] Add production graph canvas.

  Done means users can:

  - create graph resources,
  - place nodes,
  - connect pins,
  - edit literals/default values,
  - inspect node metadata,
  - view diagnostics inline,
  - navigate to source locations,
  - undo/redo graph edits,
  - save graph sources,
  - trigger validation/compile.

---

### 17.2 Block Palette

- [ ] Add profile-aware block palette.

  Done means block visibility responds to:

  - beginner mode,
  - intermediate mode,
  - advanced mode,
  - network/server developer mode,
  - package/plugin selection,
  - project configuration,
  - permissions.

---

### 17.3 Generated Code View

- [ ] Add generated C# preview panel.

  Done means users can inspect generated C# output from graph IR without making generated code the primary source of truth.

- [ ] Mark generated output clearly.

  Done means the editor does not invite users to edit generated code as if it were source unless a deliberate conversion operation occurs.

---

### 17.4 Expand/Collapse High-Level Blocks

- [ ] Add high-level block expansion UX.

  Done means users can inspect or convert high-level blocks into lower-level graph representations where permitted.

- [ ] Add macro/subgraph collapse UX.

  Done means repeated graph logic can be extracted into reusable macros/subgraphs with stable identity and validation.

---

### 17.5 Graph Diagnostics UX

- [ ] Integrate graph diagnostics with Problems panel.

  Done means diagnostics show:

  - source graph,
  - node/pin/edge location,
  - severity,
  - diagnostic code,
  - readable message,
  - authority/replication/persistence context,
  - suggested fix where safe.

---

## 18. Authoring Profiles

- [ ] Define beginner authoring profile.

  Done means beginner users see:

  - intent blocks,
  - safe gameplay templates,
  - limited variables,
  - limited event surface,
  - strong diagnostics,
  - no raw replication internals by default.

- [ ] Define intermediate authoring profile.

  Done means intermediate users see:

  - gameplay logic blocks,
  - variables,
  - event graphs,
  - simple C# preview,
  - controlled expansion of high-level blocks.

- [ ] Define advanced authoring profile.

  Done means advanced users see:

  - low-level graph blocks,
  - C# binding controls,
  - generated code view,
  - replication diagnostics,
  - server/client graph split.

- [ ] Define network/server developer profile.

  Done means server developers see:

  - authority metadata,
  - replication effects,
  - prediction/correction flows,
  - persistence effects,
  - packet/session diagnostics references,
  - server-only graph validation.

---

## 19. Collaboration Behavior

- [ ] Add graph collaboration resource metadata.

  Done means graph resources expose:

  - stable resource id,
  - display path,
  - source hash,
  - compiled artifact hash,
  - claim state,
  - lock state,
  - dirty state,
  - validation state,
  - conflict state.

- [ ] Support graph-level claims and locks.

  Done means users can claim or lock unsafe graph resources during structural edits.

- [ ] Support node/subgraph-level conflict records where practical.

  Done means conflicts can identify:

  - affected graph,
  - affected nodes,
  - affected edges,
  - involved actors,
  - local operation summary,
  - remote operation summary,
  - available resolution options.

- [ ] Treat schema migration as lock-worthy.

  Done means graph schema migrations can acquire hard locks or publish-blocking states when concurrent editing would be unsafe.

---

## 20. Diagnostics

- [ ] Define graph diagnostic codes.

  Required diagnostic families:

```txt
Graph.Syntax
Graph.Schema
Graph.Type
Graph.Authority
Graph.Replication
Graph.Persistence
Graph.Prediction
Graph.Binding
Graph.Artifact
Graph.Collaboration
Graph.Deprecated
Graph.Performance
```

- [ ] Include source mapping.

  Done means every graph diagnostic points to graph resource, node, pin, edge, source range, or generated artifact where possible.

- [ ] Emit diagnostics for editor, Forge, CI, and reports.

  Done means diagnostics are structured and machine-readable, not only console text.

---

## 21. Build and Publish Gates

- [ ] Add graph validation build gate.

  Done means Forge rejects builds when graph diagnostics exceed profile severity thresholds.

- [ ] Add graph artifact freshness gate.

  Done means stale compiled graph artifacts can be detected and rejected.

- [ ] Add generated code freshness gate.

  Done means generated C# or C++ artifacts are rejected when source hashes do not match expected manifests.

- [ ] Add publish gate.

  Done means Saga product publish workflow blocks when graph compile, authority validation, script binding validation, or artifact staging fails.

A publish button that ships invalid authority graphs is not a feature.

It is a production incident with a nicer icon.

---

## 22. Testing Strategy

- [ ] Add graph contract tests.

  Required coverage:

  - graph ids,
  - node ids,
  - pin compatibility,
  - edge validation,
  - block registry identity,
  - block palette filtering,
  - artifact refs,
  - diagnostic payloads.



  Required coverage:

  - valid graph source,
  - invalid graph syntax,
  - invalid block reference,
  - invalid pin connection,
  - invalid type usage,
  - invalid authority usage,
  - deterministic artifact hash,
  - source map generation.

- [ ] Add authority validation tests.

  Required coverage:

  - client-only graph rejects server mutation,
  - server-only graph accepts authoritative blocks,
  - predicted graph rejects persistence writes,
  - replicated write requires authority,
  - editor-only graph does not compile into runtime artifact.

- [ ] Add generated code tests.

  Required coverage:

  - graph to generated C# output,
  - C# block binding extraction,
  - stale generated-code detection,
  - unsupported freeform C# fallback.

- [ ] Add runtime/server artifact consumption tests.

  Required coverage:

  - runtime loads allowed graph artifact,
  - server loads authoritative graph artifact,
  - invalid artifact version is rejected,
  - missing artifact produces diagnostic,
  - stale artifact is not consumed silently.

---

## 23. MVP Target

- [ ] Build the first end-to-end gameplay graph vertical slice.

  Required scenario:

```txt
Quest reward flow
```

  Required authoring behavior:

```txt
On Quest Completed
    If Player Has Item "WolfFang" x5
        Remove Item "WolfFang" x5
        Give Gold 100
        Give XP 250
        Send Message "Quest completed"
```

  Required implementation evidence:



This scenario is small enough to implement and large enough to expose bad architecture.

That is exactly why it should be the first vertical slice.

---

## 24. Non-Goals

Saga Gameplay Graph must not become:

- a replacement for C++ engine development,
- a replacement for all C# scripting,
- a general-purpose programming language with worse syntax,
- an editor-private JSON format,
- a runtime service that parses source files during gameplay,
- a way for clients to author authoritative server state,
- a dumping ground for every gameplay system,
- a fake no-code product that hides all hard problems until publish time.

The graph exists to make gameplay authoring safer and more understandable.

It does not remove the need for real runtime/server architecture.

---

## 25. Risk Register

### 25.1 Risk: Graph System Becomes Too Broad

Mitigation:



---

### 25.2 Risk: Beginner UX Weakens Professional Control

Mitigation:

- use authoring profiles,
- expose advanced blocks behind explicit modes,
- allow high-level block expansion,
- avoid making beginner mode the only source model.

---

### 25.3 Risk: C# Roundtrip Is Overpromised

Mitigation:

- support Graph → generated C#,
- support annotated C# → block definition,
- do not promise arbitrary C# → clean visual graph.

---

### 25.4 Risk: MMO Authority Is Treated As Runtime-Only

Mitigation:

- encode authority metadata in block definitions,
- validate authority at authoring/compile/build/publish time,
- make invalid authority usage visible in editor diagnostics.

---



Mitigation:



---

## 26. Suggested File Targets

Expected shared contracts:

```txt
Shared/include/SagaShared/Graph/GraphId.hpp
Shared/include/SagaShared/Graph/GraphResourceDescriptor.hpp
Shared/include/SagaShared/Graph/GraphKind.hpp
Shared/include/SagaShared/Graph/GraphArtifactRef.hpp
Shared/include/SagaShared/Graph/BlockDefinitionDescriptor.hpp
Shared/include/SagaShared/Graph/BlockPaletteDescriptor.hpp
Shared/include/SagaShared/Graph/AuthorityContext.hpp
Shared/include/SagaShared/Graph/ReplicationEffect.hpp
Shared/include/SagaShared/Graph/PersistenceEffect.hpp
Shared/include/SagaShared/Graph/PredictionSafety.hpp
Shared/include/SagaShared/Graph/GraphDiagnostic.hpp
Shared/include/SagaShared/Graph/ScriptBindingDescriptor.hpp
```

Expected editor surfaces:

```txt
Editor/include/SagaEditor/Graph/GraphEditorPanel.h
Editor/include/SagaEditor/Graph/BlockPalettePanel.h
Editor/include/SagaEditor/Graph/GraphInspectorPanel.h
Editor/include/SagaEditor/Graph/GeneratedCodePreviewPanel.h
Editor/src/SagaEditor/Graph/GraphEditorPanel.cpp
Editor/src/SagaEditor/Graph/BlockPalettePanel.cpp
Editor/src/SagaEditor/Graph/GraphInspectorPanel.cpp
Editor/src/SagaEditor/Graph/GeneratedCodePreviewPanel.cpp
```





Expected Forge integration:

```txt
Tools/Forge/include/Forge/Steps/SdeGraphValidateStep.hpp
Tools/Forge/include/Forge/Steps/SdeGraphCompileStep.hpp
Tools/Forge/include/Forge/Steps/ScriptCompileStep.hpp
Tools/Forge/src/Steps/SdeGraphValidateStep.cpp
Tools/Forge/src/Steps/SdeGraphCompileStep.cpp
Tools/Forge/src/Steps/ScriptCompileStep.cpp
```

Expected Prism integration:

```txt
Tools/Prism/include/Prism/Artifacts/GraphArtifactIndex.hpp
Tools/Prism/include/Prism/Artifacts/GeneratedCodeOrigin.hpp
Tools/Prism/include/Prism/Validation/StaleGraphArtifactCheck.hpp
Tools/Prism/src/Artifacts/GraphArtifactIndex.cpp
Tools/Prism/src/Validation/StaleGraphArtifactCheck.cpp
```

---

## 27. Decision Summary

Preserve these decisions:



The career of SagaEngine depends less on adding more systems and more on making this chain coherent:

```txt
Author → Validate → Compile → Build → Run → Diagnose → Collaborate → Publish
```

If that chain is real, SagaEngine can become a serious MMO-first creation platform.

If that chain is fake, the project becomes a collection of impressive subsystems looking for a product.
