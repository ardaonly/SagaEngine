# Saga Authoring Personas

> Last updated: 2026-05-15
> Status: Proposed product/UX architecture document
> Target: A profile-driven authoring model that lets beginner, intermediate, advanced, C# gameplay programmer, network/server developer, tool developer, and team/collaboration users work in the same Saga project architecture without forking the engine or corrupting ownership boundaries.
> Scope: User personas, authoring profiles, editor surface visibility, block palette depth, scripting exposure, graph complexity, diagnostics verbosity, authority visibility, asset workflow exposure, build/publish visibility, collaboration UX, onboarding, and migration between skill levels.

---

## 0. Document Status

This document defines Saga's authoring personas and profile-driven UX model.

It is not a marketing segmentation document.

It exists because Saga's product goal is unusually broad:

```txt
Scratch-like beginner accessibility
GameMaker-like event authoring
Unreal-like graph control
C# scripting for gameplay programmers
MMO-first server-authoritative runtime
Advanced diagnostics and tooling for serious projects
```

Trying to show all of that to every user at the same time is not power.

It is UI negligence with a toolbar.

---

## 1. Purpose

The purpose of this document is to define how different users interact with the same Saga project, runtime, asset pipeline, graph system, scripting layer, and build/publish workflow through different authoring profiles.

Correct model:

```txt
Same project format
Same runtime/server model
Same SDE/Forge/Prism/artifact pipeline
Same authority rules
Different editor surfaces and abstraction depth
```

Incorrect model:

```txt
Beginner mode is a different engine.
Advanced mode is a different project type.
C# mode bypasses visual graph/authority rules.
Network developer mode ignores beginner workflows.
```

The profile system should hide complexity when useful, reveal it when needed, and never lie about the underlying architecture.

---

## 2. Companion Documents

| Document | Purpose |
|---|---|
| `SAGA_PRODUCT_ROADMAP.md` | Product shell, project lifecycle, mode orchestration, profile selection |
| `SAGA_GAMEPLAY_GRAPH_ROADMAP.md` | Gameplay graph, blocks, palettes, high/low-level graph authoring |
| `AUTHORING_AUTHORITY_MODEL.md` | Client/server authority, prediction, replication, persistence rules |
| `SAGA_SCRIPTING_ROADMAP.md` | C# scripting, block-callable APIs, generated C# preview, hot reload |
| `ASSET_PIPELINE_ROADMAP.md` | Import/cook/manifest/runtime asset pipeline |
| `EDITOR_ROADMAP.md` | Editor shell, panels, docking, inspector, graph UI, diagnostics |
| `COLLABORATION_ROADMAP.md` | Team sessions, presence, permissions, locks, conflict UX |
| `DIAGNOSTICS_ROADMAP.md` | Runtime/server/tool diagnostics, reports, health/lifetime visibility |
| `FORGE_ROADMAP.md` | Build, validate, cook, package workflow |
| `PRISM_ROADMAP.md` | Code intelligence, stale artifact checks, dependency reports |
| `DependencyGraph.md` | Ownership and dependency boundaries |

---

## 3. Core Principle

Saga must support different authoring depths without splitting the product architecture.

Required principle:

```txt
Profiles change what the user sees.
Profiles do not change who owns truth.
```

That means:

- beginner users can use high-level blocks,
- intermediate users can use event graphs and templates,
- advanced users can expand blocks and inspect generated artifacts,
- C# users can write script code with explicit metadata,
- network/server users can inspect authority, replication, and prediction behavior,
- tool developers can extend editor/build workflows,
- all users still share the same underlying project/artifact/runtime rules.

A beginner profile may hide replication internals.

It must not pretend replication does not exist.

---

## 4. Authoring Profiles

Required profiles:

```txt
Beginner
Intermediate
Advanced
CSharpGameplay
NetworkDeveloper
ToolDeveloper
TeamLead
DiagnosticEngineer
```

Profiles may be selected at:

- product level,
- project default level,
- workspace/user preference level,
- temporary session level,
- collaboration permission level.

Profiles should be composable where practical.

Example:

```txt
Advanced + NetworkDeveloper
CSharpGameplay + TeamLead
Beginner + CollaborationLimited
```

---

## 5. Persona: Beginner Creator

### 5.1 Target User

A beginner creator wants to make simple gameplay without learning engine architecture first.

Closest mental models:

```txt
Scratch
Roblox Studio beginner mode
simple visual event tools
no-code/low-code game templates
```

This user should not see packet flow, server authority internals, CMake, raw generated code, or low-level runtime blocks by default.

---

### 5.2 Visible Surfaces

Beginner profile should show:

- project dashboard,
- simplified editor layout,
- scene/level view,
- content browser in simplified mode,
- high-level block graph,
- templates,
- play/preview button,
- simple Problems panel,
- guided asset import,
- safe publish readiness summary.

Beginner profile should hide by default:

- raw replication debugger,
- packet timeline,
- generated code panels,
- low-level ECS/entity inspector,
- C# editor,
- server package layout,
- build graph internals,
- advanced Forge/Prism reports,
- raw SDE artifact manifests.

---

### 5.3 Block Palette

Beginner block palette should expose intent blocks such as:

```txt
When Player Joins
Show Message
Give Starter Item
Start Quest
Complete Quest
Spawn Enemy
Open Door
Teleport Player
Play Sound
Show Dialogue
```

Beginner blocks must be safe wrappers over validated graph/server flows.

Example:

```txt
Give Starter Item
    internally routes through server-authoritative inventory mutation
```

Beginner blocks must not expose raw server mutation without safe scaffolding.

---

### 5.4 Diagnostics

Beginner diagnostics should be:

- short,
- actionable,
- resource-linked,
- not overloaded with implementation jargon,
- capable of expanding into technical detail.

Example beginner diagnostic:

```txt
This item reward must run on the server.
Saga can create a safe server request flow for you.
```

Expandable detail:

```txt
Inventory.GiveItem requires ServerOnly authority and TransactionalWrite persistence.
Current graph is ClientOnly.
```

---

### 5.5 Non-Goals

Beginner profile must not:

- allow insecure shortcuts,
- hide publish-blocking errors,
- fork the runtime model,
- pretend client-side actions are authoritative,
- generate unmaintainable hidden code with no inspection path.

Beginner mode is allowed to simplify.

It is not allowed to lie.

---

## 6. Persona: Intermediate Event Author

### 6.1 Target User

An intermediate author understands events, variables, objects/entities, and simple conditions.

Closest mental models:

```txt
GameMaker
Construct
Roblox intermediate scripting
Unreal Blueprint beginner/intermediate
```

This user can handle more graph structure but should not be forced into low-level network details.

---

### 6.2 Visible Surfaces

Intermediate profile should show:

- event graph editor,
- block palette with gameplay logic blocks,
- variables panel,
- asset browser,
- inspector,
- Problems panel,
- simple generated C# preview toggle,
- simple server/client explanation when needed,
- build/preview status.

Intermediate profile may hide:

- raw packet debugging,
- low-level replication budget tools,
- server lifecycle internals,
- raw artifact manifests,
- advanced Prism dependency graphs.

---

### 6.3 Block Palette

Intermediate blocks may include:

```txt
Branch / If
Compare
Set Variable
Get Component
Set Component where safe
Has Item
Give Item through safe server flow
Apply Damage through server flow
Start Quest
Complete Objective
Spawn Entity through server flow
Send Server Request
On Server Response
```

The key difference from beginner mode:

```txt
Beginner sees intent.
Intermediate sees event logic.
```

---

### 6.4 Diagnostics

Intermediate diagnostics should show:

- affected graph,
- affected node,
- why it failed,
- authority summary,
- suggested safe block/flow.

Example:

```txt
ApplyDamage cannot run directly in this ClientOnly graph.
Use SendServerRequest → ServerValidated Damage Handler.
```

---

## 7. Persona: Advanced Graph Author

### 7.1 Target User

An advanced graph author wants Unreal-like control over gameplay flow, expanded blocks, graph macros, debugging, generated artifacts, and validation.

Closest mental models:

```txt
Unreal Blueprint advanced user
technical designer
gameplay engineer using visual scripting
systems designer
```

---

### 7.2 Visible Surfaces

Advanced profile should show:

- full gameplay graph editor,
- high-level and low-level block palettes,
- macro/subgraph tools,
- generated C# preview,
- graph artifact status,
- authority/replication/persistence metadata,
- detailed Problems panel,
- graph diagnostics panel,
- asset dependency view,
- build step summary,
- runtime preview diagnostics.

---

### 7.3 Block Palette

Advanced profile may expose:

```txt
Authority Check
Server Event
Client Event
Replicated Property Write
Transaction Scope
Persistence Write
Prediction Boundary
Reconciliation Hook
Diagnostic Emit
Entity/Component Access
Artifact Reference
```

Advanced blocks are powerful and must be explicit about risk.

A block that mutates replicated/persistent state must visibly declare it.

---

### 7.4 Expected Capabilities

Advanced user should be able to:

- expand high-level blocks,
- collapse graph regions into macros,
- inspect generated C# preview,
- inspect graph IR/artifact status,
- inspect authority validation result,
- trace graph diagnostics to source locations,
- run Forge validation from product/editor UI,
- see whether graph artifacts are stale.

---

## 8. Persona: C# Gameplay Programmer

### 8.1 Target User

A C# gameplay programmer wants to write gameplay logic in code while still integrating with graph/block authoring and Saga's build/authority pipeline.

Closest mental models:

```txt
Unity C# developer
Godot C# developer
Unreal C++/Blueprint hybrid developer
server gameplay programmer
```

---

### 8.2 Visible Surfaces

CSharpGameplay profile should show:

- script editor integration,
- C# project/source browser,
- block-callable binding inspector,
- generated code preview,
- script compile diagnostics,
- script artifact status,
- authority metadata inspector,
- hot reload controls where allowed,
- Prism symbol navigation where available.

---

### 8.3 Core Rules

C# gameplay code must follow these rules:

```txt
C# can expose methods as blocks only through explicit metadata.
Side-effecting C# APIs must declare authority metadata.
Generated C# is read-only preview unless explicitly converted.
Arbitrary C# is not guaranteed to convert cleanly back into visual blocks.
Server authority remains mandatory.
```

---

### 8.4 Expected Capabilities

C# gameplay programmer should be able to:

- write `[BlockCallable]` methods,
- declare `[ServerOnly]`, `[SharedPure]`, `[PredictionUnsafe]`, etc.,
- compile scripts through Forge/editor workflow,
- see script diagnostics in Problems panel,
- expose safe C# APIs to graph users,
- inspect where generated code came from,
- detect stale generated/binding artifacts through Prism.

---

### 8.5 Diagnostics

Example diagnostic:

```txt
Method Inventory.GiveItem writes persistent player state but is missing authority metadata.
Add [ServerOnly], [WritesPersistentState], and persistence policy attributes.
```

This profile should not dumb down scripting errors.

This user needs precise technical feedback.

---

## 9. Persona: Network/Server Developer

### 9.1 Target User

A network/server developer cares about server authority, session lifecycle, replication, prediction, reconciliation, persistence, diagnostics, security, and package correctness.

Closest mental models:

```txt
MMO backend developer
multiplayer gameplay engineer
network programmer
server runtime engineer
```

---

### 9.2 Visible Surfaces

NetworkDeveloper profile should show:

- authority metadata overlays,
- server/client graph split,
- replication diagnostics,
- prediction/reconciliation diagnostics,
- packet/session diagnostics where available,
- server preview controls,
- local dev server lifecycle,
- persistence diagnostics,
- server package artifact status,
- runtime health/diagnostic reports.

---

### 9.3 Exposed Tools

This profile may expose:

```txt
Replication Debugger
Packet Timeline
Connection State Viewer
Server Session Inspector
Authority Validation Panel
Prediction/Reconciliation Viewer
Persistence Transaction Viewer
Runtime Diagnostics Reports
```

These tools should not be visible to beginner users by default.

Not because beginners are inferior, but because raw packet state is not a warm welcome.

---

### 9.4 Expected Capabilities

Network/server developer should be able to:

- verify that client requests are not trusted as truth,
- inspect graph/script authority metadata,
- inspect server-only artifact placement,
- validate replication budget warnings,
- inspect prediction correction behavior,
- diagnose stale session/entity/resource state,
- run server preview builds,
- read diagnostics reports.

---

## 10. Persona: Tool Developer

### 10.1 Target User

A tool developer extends editor workflows, build steps, validation passes, asset pipeline behavior, project automation, or internal productivity tools.

Closest mental models:

```txt
technical artist tools engineer
tooling programmer
build engineer
editor extension developer
```

---

### 10.2 Visible Surfaces

ToolDeveloper profile should show:

- editor tooling scripts,
- custom panel/plugin hooks,
- build tool integration status,
- Forge step diagnostics,
- SDE output/diagnostic reports,
- Prism boundary reports,
- asset pipeline import/cook diagnostics,
- editor extension diagnostics.

---

### 10.3 Core Rules

Tooling extensions must not silently own product/runtime truth.

Rules:

```txt
Editor tooling scripts are EditorOnly unless explicitly packaged otherwise.
Build tooling scripts are BuildOnly unless explicitly packaged otherwise.
Tool extensions must respect DependencyGraph boundaries.
Tool-generated artifacts must be tracked through manifests.
Tool diagnostics must be structured.
```

---

## 11. Persona: Team Lead / Producer

### 11.1 Target User

A team lead or producer cares less about low-level authoring and more about project health, collaboration state, publish readiness, conflicts, permissions, diagnostics summaries, and build status.

Closest mental models:

```txt
technical producer
small studio lead
project owner
collaboration admin
```

---

### 11.2 Visible Surfaces

TeamLead profile should show:

- project dashboard,
- build/publish status,
- collaboration session summary,
- active conflicts,
- locked resources,
- blocked publish reasons,
- diagnostics summary,
- team permissions summary,
- recent activity/change stream.

---

### 11.3 Expected Capabilities

Team lead should be able to:

- see whether project can publish,
- see why publish is blocked,
- see who is editing what,
- see active locks/conflicts,
- start build/publish checks,
- review high-level diagnostics,
- route technical diagnostics to specialists.

This profile is not about reducing technical truth.

It is about summarizing it without turning the dashboard into a compiler textbook.

---

## 12. Persona: Diagnostic Engineer

### 12.1 Target User

A diagnostic engineer investigates runtime/server/tool failures, stress results, memory/resource pressure, replication issues, crashes, and build/publish failures.

Closest mental models:

```txt
engine reliability engineer
server diagnostics engineer
QA automation engineer
performance engineer
```

---

### 12.2 Visible Surfaces

DiagnosticEngineer profile should show:

- full diagnostics router,
- runtime health snapshots,
- server diagnostic reports,
- lifetime leak reports,
- resource tracking reports,
- asset streaming diagnostics,
- replication diagnostics,
- Forge build reports,
- SDE diagnostics,
- Prism stale/boundary reports,
- crash/shutdown reports where available.

---

### 12.3 Expected Capabilities

Diagnostic engineer should be able to:

- trace a publish blocker to source system,
- inspect structured diagnostic payloads,
- compare expected vs actual artifact hashes,
- inspect runtime/server health snapshots,
- identify long-running server degradation,
- export diagnostic reports,
- connect diagnostics to graph/script/asset/source locations.

---

## 13. Profile Visibility Matrix

| Surface | Beginner | Intermediate | Advanced | C# Gameplay | Network Dev | Tool Dev | Team Lead | Diagnostic Eng |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Project Dashboard | Full | Full | Full | Full | Full | Full | Full | Full |
| Scene/Viewport | Simple | Full | Full | Full | Full | Optional | Summary | Optional |
| Content Browser | Simple | Full | Full | Full | Full | Full | Summary | Full |
| High-Level Blocks | Full | Full | Full | Optional | Optional | Optional | Hidden | Optional |
| Gameplay Logic Blocks | Limited | Full | Full | Optional | Full | Optional | Hidden | Optional |
| Low-Level Runtime Blocks | Hidden | Hidden/Limited | Full | Limited | Full | Optional | Hidden | Optional |
| C# Script Editor | Hidden | Optional | Optional | Full | Full | Full | Hidden | Optional |
| Generated C# Preview | Hidden | Optional | Full | Full | Full | Optional | Hidden | Optional |
| Authority Metadata | Hidden summary | Simple | Full | Full | Full | Optional | Summary | Full |
| Replication Debugger | Hidden | Hidden | Optional | Optional | Full | Optional | Hidden | Full |
| Packet Timeline | Hidden | Hidden | Hidden/Optional | Optional | Full | Optional | Hidden | Full |
| Forge Build Details | Summary | Summary | Full | Full | Full | Full | Summary | Full |
| Prism Reports | Hidden | Hidden | Optional | Full | Full | Full | Summary | Full |
| Diagnostics Reports | Simple | Medium | Full | Full | Full | Full | Summary | Full |
| Collaboration Controls | Simple | Medium | Full | Medium | Full | Medium | Full | Full |

This matrix is a default.

Project settings, permissions, and user preferences may refine it.

---

## 14. Profile-Driven Block Palette

- [ ] Define profile-based block palette filtering.

  Done means every block definition can declare visibility rules such as:

```txt
visibleIn: [Beginner, Intermediate, Advanced]
hiddenIn: [Beginner]
requiresProfile: NetworkDeveloper
requiresPermission: ManageServerAuthority
```

- [ ] Define block danger levels.

  Required levels:

```txt
Safe
Guided
Advanced
Dangerous
Internal
Deprecated
```

- [ ] Prevent unsafe beginner exposure.

  Done means beginner palettes do not show dangerous/internal blocks unless wrapped by safe high-level flows.

Example:

```txt
Beginner sees: Give Quest Reward
Advanced sees: StartTransaction → GiveItem → GiveXP → CommitTransaction → ReplicateInventory
NetworkDeveloper sees: authority, persistence, replication, diagnostics effects
```

---

## 15. Profile-Driven Diagnostics

Diagnostics must support layered detail.

Required diagnostic detail levels:

```txt
Simple
Standard
Technical
RawPayload
```

Example diagnostic across profiles:

Beginner:

```txt
This reward must run on the server. Saga can create a safe server flow.
```

Intermediate:

```txt
GiveItem cannot run in ClientOnly graph. Use SendServerRequest → ServerValidated Reward Handler.
```

Advanced:

```txt
Inventory.GiveItem requires ServerOnly authority and TransactionalWrite persistence. Current graph authority is ClientOnly.
```

NetworkDeveloper / DiagnosticEngineer:

```txt
Authority.BlockRequiresServer
Graph: HUD.OnQuestCompleteButton
Node: Inventory.GiveItem
RequiredAuthority: ServerOnly
ActualAuthority: ClientOnly
SideEffects: WriteAuthoritativeState, WritePersistentState, WriteReplicatedState
SuggestedFlow: ServerValidated request graph
```

Same error.

Different presentation depth.

Not different truth.

---

## 16. Profile-Driven Editor Layouts

- [ ] Define default layouts per profile.

  Required layouts:

```txt
BeginnerLayout
IntermediateLayout
AdvancedGraphLayout
CSharpGameplayLayout
NetworkDebugLayout
ToolDeveloperLayout
TeamDashboardLayout
DiagnosticsLayout
```

- [ ] Layouts must be SDE/configurable where approved.

  Done means profile layouts can be described through editor profile definitions without hardcoding every workflow into C++.

- [ ] Layout migration must be safe.

  Done means old profile layouts do not crash after panels are renamed or removed.

---

## 17. Skill Progression and Mode Switching

- [ ] Support safe profile switching.

  Done means users can move from Beginner → Intermediate → Advanced without project migration.

- [ ] Preserve advanced state when returning to beginner profile.

  Done means advanced graph/script data is not destroyed just because the visible profile hides it.

- [ ] Provide reveal mechanisms.

  Examples:

```txt
Show technical details
Expand high-level block
View generated C#
Open authority details
Switch to advanced mode for this graph
```

- [ ] Warn before irreversible conversions.

  Example:

```txt
Converting this graph to freeform C# may prevent clean conversion back to blocks.
```

---

## 18. Permissions and Collaboration

Profiles are not the same as permissions.

Required distinction:

```txt
Profile = what UX depth the user prefers/sees.
Permission = what the user is allowed to do.
```

- [ ] Integrate profiles with collaboration permissions.

  Done means a beginner profile user may still be blocked from actions they do not have permission to perform, and an advanced profile user cannot bypass team permissions.

- [ ] Support role/profile combinations.

Examples:

```txt
Beginner + AssetContributor
Advanced + GameplayMaintainer
CSharpGameplay + ServerScriptAuthor
NetworkDeveloper + ServerMaintainer
TeamLead + PublishManager
```

- [ ] Keep publish/authority permissions explicit.

  Done means profile selection alone does not grant publish rights, lock override rights, or server authority modification rights.

---

## 19. Onboarding

- [ ] Add profile-aware onboarding.

  Done means first-run/project onboarding can ask:

  - what user wants to make,
  - whether they prefer blocks or code,
  - whether they understand multiplayer/server concepts,
  - whether they want beginner or advanced UI,
  - whether project is solo or team-based.

- [ ] Avoid fake beginner promises.

  Forbidden onboarding language:

```txt
No networking knowledge needed ever.
Everything is automatic.
Make an MMO in minutes.
```

Preferred:

```txt
Start with safe high-level blocks. Saga will show server/network details when your game needs them.
```

Honesty scales better than hype.

---

## 20. Build and Publish Visibility by Profile

### 20.1 Beginner

Beginner sees:

```txt
Ready to Preview
Needs Fixes
Ready to Publish
Publish Blocked
```

With short explanations and guided fixes.

---

### 20.2 Intermediate

Intermediate sees:

```txt
Graph errors
Asset errors
Script errors if scripts enabled
Build profile summary
Preview artifact status
```

---

### 20.3 Advanced / C# / Network / Tool / Diagnostic

Advanced technical profiles may see:

```txt
Forge build steps
SDE compile status
script compile status
asset cook status
artifact manifests
hash mismatch reports
Prism stale reports
package placement errors
server/client artifact split
```

---

## 21. Profile Configuration Model

- [ ] Define authoring profile descriptor.

  Done means each profile can describe:

  - profile id,
  - display name,
  - description,
  - default layout,
  - visible panels,
  - hidden panels,
  - block palettes,
  - diagnostics level,
  - scripting visibility,
  - authority visibility,
  - build detail visibility,
  - collaboration detail visibility,
  - allowed dangerous actions prompt policy.

Example concept:

```txt
authoringProfile Beginner {
    layout: BeginnerLayout
    blockPalettes: [BeginnerGameplay]
    diagnosticsLevel: Simple
    showGeneratedCode: false
    showLowLevelBlocks: false
    showReplicationTools: false
    dangerousActionPolicy: GuidedOnly
}
```

- [ ] Store project default profile.

  Done means project manifest can define default authoring profile.

- [ ] Store user override profile.

  Done means user can override profile locally without mutating project defaults unless explicitly intended.

---

## 22. Testing Strategy

### 22.1 Profile Contract Tests

- [ ] Add profile descriptor tests.

  Required coverage:

  - profile id serialization,
  - visible/hidden panel rules,
  - block palette selection,
  - diagnostics level selection,
  - user override behavior,
  - project default behavior.

---

### 22.2 Block Palette Tests

- [ ] Add profile block palette tests.

  Required coverage:

  - beginner hides dangerous blocks,
  - intermediate exposes gameplay logic blocks,
  - advanced exposes low-level blocks,
  - network profile exposes authority/replication blocks,
  - deprecated/internal blocks are gated.

---

### 22.3 Diagnostics Presentation Tests

- [ ] Add diagnostic layering tests.

  Required coverage:

  - same diagnostic has simple/technical/raw presentations,
  - beginner detail can expand to technical detail,
  - publish-blocking severity is not hidden,
  - diagnostic navigation remains accurate.

---

### 22.4 Layout Tests

- [ ] Add profile layout tests.

  Required coverage:

  - default profile layout loads,
  - missing panel handled safely,
  - renamed panel migrates or is skipped with diagnostic,
  - switching profile preserves hidden advanced state.

---

### 22.5 Permission/Profile Tests

- [ ] Add permission/profile interaction tests.

  Required coverage:

  - advanced profile does not bypass permissions,
  - team lead can see publish status without editing authority graphs,
  - beginner user cannot perform forbidden publish action,
  - collaboration role overrides unsafe actions where needed.

---

## 23. MVP Vertical Slice

The first persona slice should demonstrate one project opened by three profiles.

Required scenario:

```txt
Same MMO Starter project
    ↓
Beginner user edits quest reward through high-level block
Intermediate user inspects event graph
NetworkDeveloper user inspects authority/replication details
```

Required behavior:

1. Project opens in Saga.
2. Beginner profile shows simplified layout and `Give Quest Reward` block.
3. Beginner cannot directly place raw `WriteReplicatedProperty` block.
4. Intermediate profile can expand quest reward into event graph.
5. Advanced/NetworkDeveloper profile can inspect `ServerOnly`, `TransactionalWrite`, and `ReplicateInventory` metadata.
6. Same underlying graph/artifact is used.
7. Diagnostics present different detail levels but same truth.
8. Switching profiles does not corrupt project state.
9. Publish check uses full authority/artifact validation regardless of profile.

This slice proves the profile system is real and not just a theme selector.

---

## 24. Non-Goals

Authoring personas must not become:

- separate engines,
- separate project formats,
- separate runtime semantics,
- excuses to hide publish-blocking truth,
- permission systems by another name,
- marketing labels with no technical effect,
- a way to bypass authority validation,
- a reason to make beginner output unmaintainable.

Profiles are UX and workflow lenses over one architecture.

They are not alternate realities.

---

## 25. Risk Register

### 25.1 Risk: Beginner Mode Becomes Toy Mode

Mitigation:

- use same project/artifact/runtime architecture,
- high-level blocks expand to real graphs,
- diagnostics can reveal technical detail,
- publish validation remains strict.

---

### 25.2 Risk: Advanced Mode Overwhelms Everyone

Mitigation:

- profile-based visibility,
- safe defaults,
- progressive disclosure,
- specialized layouts.

---

### 25.3 Risk: Profiles Are Confused With Permissions

Mitigation:

- document distinction,
- test permission/profile interaction,
- enforce collaboration permissions independently.

---

### 25.4 Risk: C# Users Bypass Graph/Authority Model

Mitigation:

- require metadata for block-callable APIs,
- validate script artifacts through Forge/SDE/authority model,
- show binding diagnostics.

---

### 25.5 Risk: Profile Switching Destroys Hidden State

Mitigation:

- hidden does not mean deleted,
- profile switching is view-level by default,
- irreversible conversions require explicit confirmation.

---

## 26. Suggested File Targets

Expected shared contracts:

```txt
Shared/include/SagaShared/Authoring/AuthoringProfileId.hpp
Shared/include/SagaShared/Authoring/AuthoringProfileDescriptor.hpp
Shared/include/SagaShared/Authoring/AuthoringPersona.hpp
Shared/include/SagaShared/Authoring/DiagnosticsDetailLevel.hpp
Shared/include/SagaShared/Authoring/PanelVisibilityRule.hpp
Shared/include/SagaShared/Authoring/BlockPaletteVisibilityRule.hpp
```

Expected product files:

```txt
Saga/include/Saga/Authoring/AuthoringProfileService.h
Saga/include/Saga/Authoring/AuthoringProfileSelection.h
Saga/include/Saga/Authoring/UserProfilePreferences.h
Saga/src/Authoring/AuthoringProfileService.cpp
Saga/src/Authoring/AuthoringProfileSelection.cpp
```

Expected editor files:

```txt
Editor/include/SagaEditor/Authoring/ProfileLayoutResolver.h
Editor/include/SagaEditor/Authoring/ProfilePanelVisibility.h
Editor/include/SagaEditor/Authoring/ProfileBlockPaletteFilter.h
Editor/include/SagaEditor/Authoring/ProfileDiagnosticPresenter.h
Editor/src/SagaEditor/Authoring/ProfileLayoutResolver.cpp
Editor/src/SagaEditor/Authoring/ProfilePanelVisibility.cpp
Editor/src/SagaEditor/Authoring/ProfileBlockPaletteFilter.cpp
Editor/src/SagaEditor/Authoring/ProfileDiagnosticPresenter.cpp
```

Expected profile definitions:

```txt
<Project>/.sde/editor/profiles/beginner.sde
<Project>/.sde/editor/profiles/intermediate.sde
<Project>/.sde/editor/profiles/advanced.sde
<Project>/.sde/editor/profiles/csharp_gameplay.sde
<Project>/.sde/editor/profiles/network_developer.sde
```

or engine defaults:

```txt
Engine/Defaults/AuthoringProfiles/Beginner.sde
Engine/Defaults/AuthoringProfiles/Intermediate.sde
Engine/Defaults/AuthoringProfiles/Advanced.sde
Engine/Defaults/AuthoringProfiles/CSharpGameplay.sde
Engine/Defaults/AuthoringProfiles/NetworkDeveloper.sde
```

---

## 27. Decision Summary

Preserve these decisions:

```txt
Profiles change visibility and workflow depth, not engine truth.
Beginner mode uses the same project/artifact/runtime architecture.
High-level blocks must expand or bind to validated lower-level behavior.
Diagnostics may be simplified but publish-blocking truth cannot be hidden.
C# users remain inside authority/build/artifact validation.
Network/server developers get authority/replication/prediction detail.
Team leads get project health and publish readiness, not raw implementation overload.
Profiles are not permissions.
Profile switching must not corrupt hidden advanced state.
```

The goal is to make Saga approachable without making it dishonest.

A serious engine can be friendly.

It cannot be fake-simple at the cost of correctness.
