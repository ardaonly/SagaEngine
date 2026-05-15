# Saga Collaboration Roadmap

> Last updated: 2026-05-14  
> Status: Active product roadmap  
> Scope: Product-level collaboration, session lifecycle, presence, permissions, claims, locks, change streams, conflict handling, reconnect/recovery, and editor-facing collaboration UX.

---

## 0. Roadmap Convention

- `[x]` — Shipped. The note after the item names the files that represent the work and highlights any decisions worth preserving.
- `[ ]` — Open. Either unstarted or partially explored; the item describes the finished state rather than interim scaffolding.
- Shipped items must name the files, modules, or integration points that represent the completed work.
- Open items must describe the finished production state, not temporary scaffolding.
- Collaboration contracts must not be finalized under editor-private headers.
- Neutral contracts belong in `SagaShared`.
- Collaboration implementation belongs in `SagaCollaboration`.
- Editor collaboration UI and adapters may live under `Editor/include/SagaEditor/Collaboration/...` and `Editor/src/SagaEditor/Collaboration/...`.

---

## 1. Document Purpose

This document defines Saga’s collaboration roadmap.

Collaboration is not just “host and join”.

The target is a production-grade workflow where multiple developers can safely build, edit, review, test, and publish game projects together.

This roadmap owns product collaboration behavior.

It does not make the editor the owner of collaboration truth.

```txt
Saga owns product lifecycle.
SagaCollaboration owns collaboration implementation.
SagaShared owns neutral contracts.
SagaEditor owns collaboration UI and editor-facing adapters.
```

Anything else is how a codebase becomes a historical warning label.

---

## 2. Product Vision

- [ ] Provide a product-level collaboration workflow where users can create or open a project, start a collaboration session, invite others, edit together, review changes, test together, and publish safely.

- [ ] Support Quick Collaboration for temporary local/dev sessions with minimal setup and fast host/join behavior.

- [ ] Support Team Collaboration for persistent projects, team membership, permissions, review workflows, audit history, and safe publishing.

- [ ] Keep collaboration as a Saga product capability instead of hiding it inside editor-private systems.

- [ ] Make collaboration visible and understandable through editor UX without making the editor the owner of session truth.

---

## 3. Ownership Model

### 3.1 Saga Product Ownership

- [ ] `Saga` owns product-level collaboration entry points.

  Done means:

  - project dashboard exposes collaboration actions,
  - project open/create flow can route into collaborative workflows,
  - session selection is product-level,
  - mode switching remains product-owned,
  - global collaboration errors are surfaced through Saga product UX.

- [ ] `Saga` owns project/session lifecycle orchestration at product level.

  Done means:

  - starting a session is initiated from product workflow,
  - joining a session is initiated from product workflow,
  - leaving a session returns user to a safe product state,
  - editor/runtime/server modules are mounted or detached through Saga-owned orchestration.

---

### 3.2 SagaShared Ownership

- [ ] Move neutral collaboration contracts into `SagaShared`.

  Done means `SagaShared` owns only small neutral contracts such as:

  - session descriptors,
  - workspace descriptors,
  - room codes,
  - participant ids,
  - participant display names,
  - presence snapshots,
  - permission grants,
  - edit operation envelopes,
  - artifact references,
  - stable resource ids,
  - diagnostic payloads.

- [ ] Prevent `SagaShared` from becoming implementation storage.

  Done means `SagaShared` does not contain:

  - Qt widgets,
  - editor panels,
  - runtime internals,
  - server internals,
  - transport implementations,
  - persistence engines,
  - conflict engines,
  - session orchestration,
  - backend clients.

A shared folder is not a trash can with a nicer name.

---

### 3.3 SagaCollaboration Ownership

- [ ] Create `SagaCollaboration` as the owner of collaboration services and implementation.

  Done means `SagaCollaboration` owns:

  - quick/dev session lifecycle,
  - production team session lifecycle,
  - host/join state,
  - presence,
  - permissions,
  - claims,
  - locks,
  - change streams,
  - conflict detection,
  - reconnect/recovery,
  - backend availability state,
  - service APIs,
  - transport adapters,
  - persistence integration,
  - editor/runtime/server bridges.

- [ ] Expose stable collaboration service APIs.

  Done means product, editor, runtime, and server consumers use public collaboration services instead of implementation internals.

---

### 3.4 Editor Ownership

- [ ] Keep `SagaEditor` responsible only for collaboration UI and editor-facing adapters.

  Done means the editor may own:

  - collaboration toolbar,
  - participants panel,
  - presence indicators,
  - locked-resource badges,
  - conflict resolution UI,
  - collaboration diagnostics panel,
  - editor command integration,
  - editor-local collaboration adapters.

- [ ] Prevent editor-private collaboration contracts from becoming product truth.

  Done means the editor does not own:

  - session truth,
  - final collaboration contracts,
  - backend collaboration state,
  - global product lifecycle,
  - team membership source of truth,
  - transport implementation,
  - persistence implementation.

Correct model:

```txt
SagaEditor displays and operates collaboration UX.
SagaCollaboration owns collaboration behavior.
Saga owns the product lifecycle around it.
```

Incorrect model:

```txt
Editor/include/SagaEditor/Collaboration owns everything because it was convenient.
```

Convenience is how future bugs get a passport.

---

## 4. Layer Architecture

- [ ] Establish the target collaboration layer split.

  Target architecture:

```txt
Saga
  product shell, project lifecycle, collaboration entry points

SagaEditor
  collaboration UI, editor adapters, conflict display

SagaRuntime
  runtime preview consumers, simulation/session integration

SagaServer
  authority/session consumers, production server integration

SagaShared
  neutral contracts and identifiers

SagaCollaboration
  service APIs, session lifecycle, permissions, presence, conflicts

SagaCollaborationCore
  optional lower-level implementation internals

Backends
  persistence, auth, relay, storage, telemetry
```

- [ ] Keep `SagaCollaborationCore` optional until the collaboration implementation becomes large enough to justify a lower-level split.

---

## 5. Dependency Rules

### 5.1 Allowed Dependencies

- [ ] Allow `Saga` to consume `SagaCollaboration` APIs and `SagaShared` contracts.

```txt
Saga → SagaCollaboration API
Saga → SagaShared
```

- [ ] Allow `SagaEditor` to consume `SagaCollaboration` APIs and `SagaShared` contracts.

```txt
SagaEditor → SagaCollaboration API
SagaEditor → SagaShared
```

- [ ] Allow runtime and server modules to consume stable collaboration APIs where needed.

```txt
Runtime → SagaCollaboration API
Runtime → SagaShared

Server → SagaCollaboration API
Server → SagaShared
```

- [ ] Allow `SagaCollaboration` to depend on `SagaShared` and approved backend interfaces.

```txt
SagaCollaboration → SagaShared
SagaCollaboration → approved backend interfaces
```

---

### 5.2 Forbidden Dependencies

- [ ] Prevent runtime and server modules from including editor-private collaboration headers.

```txt
Runtime → Editor/include/SagaEditor/Collaboration
Server → Editor/include/SagaEditor/Collaboration
```

- [ ] Prevent `SagaShared` from depending upward.

```txt
SagaShared → SagaCollaboration
SagaShared → Editor
SagaShared → Runtime internals
SagaShared → Server internals
```

- [ ] Prevent `SagaCollaboration` from depending on UI ownership.

```txt
SagaCollaboration → Editor UI
SagaCollaboration → product shell widgets
```

---

### 5.3 Deprecated Ownership Location

- [ ] Freeze final-contract additions under the deprecated editor collaboration path.

Deprecated path:

```txt
Editor/include/SagaEditor/Collaboration
```

Migration rule:

```txt
Existing code may remain temporarily.
New final collaboration contracts must not be added there.
```

---

## 6. Core Collaboration Concepts

### 6.1 Project

- [ ] Define collaborative project identity.

  Done means each collaborative project has:

  - project id,
  - display name,
  - owner/team id,
  - local path or remote workspace reference,
  - collaboration mode,
  - active session state,
  - member list,
  - permission policy,
  - artifact registry,
  - current publish state.

---

### 6.2 Workspace

- [ ] Define workspace state for local and collaborative editing.

  Done means each workspace can describe:

  - workspace id,
  - project id,
  - local root path,
  - current branch or snapshot id,
  - active user id,
  - open editor mode,
  - sync state,
  - dirty resources,
  - pending operations.

---

### 6.3 Session

- [ ] Define active collaboration session state.

  Done means each session can describe:

  - session id,
  - project id,
  - session mode,
  - host authority,
  - participants,
  - permissions,
  - presence state,
  - claimed resources,
  - active locks,
  - operation stream position,
  - diagnostics state.

---

### 6.4 Participant

- [ ] Define participant identity and connection state.

  Done means each participant can describe:

  - participant id,
  - account id when available,
  - display name,
  - connection state,
  - role,
  - permissions,
  - current activity,
  - active selection,
  - viewed resource,
  - last heartbeat timestamp.

---

### 6.5 Presence

- [ ] Define presence snapshots.

  Done means presence can describe:

  - online/offline state,
  - current mode,
  - open asset,
  - selected entity,
  - active viewport location,
  - cursor location,
  - current tool,
  - editing state,
  - idle state.

Presence must be useful, not creepy.

The goal is coordination, not building a surveillance dashboard because software people lost moral imagination again.

---

### 6.6 Permissions

- [ ] Define collaboration permission domains.

  Done means permissions can cover:

  - project access,
  - asset edit,
  - scene edit,
  - script edit,
  - data graph edit,
  - build/run,
  - publish,
  - invite members,
  - change roles,
  - manage locks,
  - resolve conflicts.

Implicit trust is not a security model. It is a future incident report.

---

### 6.7 Claims

- [ ] Define soft resource claims.

  Done means a user or tool can claim a resource to signal active work without hard-blocking other users.

Examples:

```txt
Arda is editing this scene.
Maya is working on this asset.
Tool process is generating this artifact.
```

---

### 6.8 Locks

- [ ] Define hard resource locks.

  Done means locks are:

  - visible,
  - scoped,
  - revocable by authorized users,
  - time-bounded where possible,
  - recoverable after disconnect.

Examples:

```txt
scene locked for structural edit
asset locked during import/cook
publish locked during validation
graph locked during schema migration
```

---

### 6.9 Change Stream

- [ ] Define ordered collaboration change stream.

  Done means the stream can record:

  - resource opened,
  - resource edited,
  - entity modified,
  - asset imported,
  - script changed,
  - graph compiled,
  - scene saved,
  - lock acquired,
  - lock released,
  - conflict detected,
  - conflict resolved.

---

### 6.10 Conflict

- [ ] Define conflict detection and conflict records.

  Done means conflicts include:

  - affected resources,
  - involved actors,
  - local operation summary,
  - remote operation summary,
  - conflict reason,
  - available resolution options,
  - audit record.

A conflict dialog that just says “failed” is not UX.

It is a shrug with pixels.

---

## 7. Quick Collaboration Roadmap

### 7.1 Local Session Skeleton

- [ ] Provide a local host/join collaboration session for development testing.

  Done means:

  - user can start a local session,
  - another client can join,
  - session descriptor exists,
  - participants are visible,
  - basic presence updates work,
  - disconnect is handled cleanly,
  - session shuts down without corrupting project state.

Expected outputs:

```txt
SagaShared session descriptors
SagaCollaboration quick session service
Saga product entry point
SagaEditor participants UI
```

---

### 7.2 Room Code Join Flow

- [ ] Add room-code based joining.

  Done means:

  - host generates a room code,
  - joiner enters the room code,
  - invalid codes fail clearly,
  - expired codes fail clearly,
  - participant appears in session,
  - host sees joiner state.

Required contracts:

```txt
RoomCode
SessionEndpointDescriptor
JoinRequest
JoinResult
JoinFailureReason
```

---

### 7.3 Presence and Activity

- [ ] Show useful real-time collaborator presence.

  Done means:

  - participants panel shows active users,
  - editor shows active resource/user indicators,
  - selected asset/scene presence is visible,
  - idle state is visible,
  - disconnected state is visible,
  - stale presence expires safely.

---

### 7.4 Soft Claims

- [ ] Add soft claims for collaborative resources.

  Done means:

  - users can claim resources,
  - editor displays claimed resources,
  - claim ownership is visible,
  - claim release works,
  - disconnected participant claims recover safely,
  - claims do not permanently block work.

---

### 7.5 Basic Locking

- [ ] Add hard locks for unsafe concurrent edits.

  Done means:

  - resources can be locked,
  - locks are visible,
  - unauthorized edits are blocked,
  - lock owner is shown,
  - lock release works,
  - disconnect recovery exists,
  - host/admin override exists.

---

### 7.6 Conflict Detection

- [ ] Detect unsafe concurrent modifications.

  Done means:

  - conflicting edits are detected,
  - affected resources are identified,
  - local and remote operation summaries are shown,
  - user receives actionable resolution options,
  - conflict result is recorded.

---

### 7.7 Reconnect and Recovery

- [ ] Survive temporary connection loss.

  Done means:

  - disconnected users can reconnect,
  - participant identity is restored,
  - stale presence is corrected,
  - claims and locks are reconciled,
  - pending operations are resolved or rejected clearly,
  - corrupted state is not accepted.

---

## 8. Team Collaboration Roadmap

### 8.1 Team Project Model

- [ ] Introduce persistent team projects.

  Done means:

  - project has persistent id,
  - project has owner/team id,
  - project has member list,
  - project has role model,
  - project has collaboration settings,
  - project can be opened through Saga product shell.

---

### 8.2 Roles and Permissions

- [ ] Enforce role-based collaboration permissions.

  Done means:

  - roles exist,
  - permissions are explicit,
  - editor actions check permissions,
  - publish/build actions check permissions,
  - unauthorized actions fail safely,
  - permission errors are readable.

Example roles:

```txt
Owner
Admin
Developer
Artist
Designer
Reviewer
Viewer
```

---

### 8.3 Persistent Operation Log

- [ ] Record project collaboration history.

  Done means:

  - operations are persisted,
  - operations are ordered,
  - operations include actor identity,
  - operations include affected resource,
  - operations include timestamp,
  - operation log can be inspected,
  - invalid operations do not publish state.

---

### 8.4 Review Workflow

- [ ] Support reviewable changes before publishing.

  Done means:

  - users can submit change sets,
  - reviewers can inspect affected resources,
  - comments or diagnostics can be attached,
  - changes can be accepted or rejected,
  - accepted changes can flow toward publish/build,
  - rejected changes remain recoverable.

---

### 8.5 Publish Gates

- [ ] Prevent unsafe project publishing.

  Done means publish is blocked when:

  - unresolved conflicts exist,
  - failed validation exists,
  - failed SDE compile exists,
  - failed asset cook exists,
  - failed runtime validation exists,
  - caller lacks publish permission.

---

### 8.6 Cloud/Backend Session Services

- [ ] Move team collaboration from local/dev flow to production service flow.

  Done means:

  - backend-backed session discovery exists,
  - authenticated participants exist,
  - persistent team membership exists,
  - reconnect works across machines,
  - project metadata persists,
  - diagnostics are observable.

---

### 8.7 Audit and History

- [ ] Provide reliable project history and accountability.

  Done means:

  - project-level audit events exist,
  - sensitive operations are recorded,
  - role changes are recorded,
  - publish actions are recorded,
  - lock overrides are recorded,
  - conflict resolutions are recorded,
  - audit records are queryable.

---

## 9. Editor UX Roadmap

### 9.1 Participants Panel

- [ ] Add participants panel.

  Done means the panel shows:

  - participant name,
  - connection state,
  - current activity,
  - role,
  - active resource,
  - stale state warnings.

Expected editor location:

```txt
Editor/include/SagaEditor/Collaboration/ParticipantsPanel.hpp
Editor/src/SagaEditor/Collaboration/ParticipantsPanel.cpp
```

---

### 9.2 Collaboration Toolbar

- [ ] Add collaboration toolbar.

  Done means the toolbar supports:

  - start session,
  - join session,
  - copy invite/room code,
  - leave session,
  - view participants,
  - view conflicts,
  - view locks,
  - view sync state.

Expected editor location:

```txt
Editor/include/SagaEditor/Collaboration/CollaborationToolbar.hpp
Editor/src/SagaEditor/Collaboration/CollaborationToolbar.cpp
```

---

### 9.3 Presence Indicators

- [ ] Add presence indicators across editor surfaces.

  Done means presence appears in:

  - hierarchy,
  - content browser,
  - inspector,
  - scene viewport,
  - graph editors,
  - asset editor tabs.

---

### 9.4 Lock and Claim UI

- [ ] Add lock and claim UI.

  Done means UI shows:

  - claimed resource badge,
  - locked resource badge,
  - owner display,
  - lock reason,
  - release request,
  - host/admin override.

---

### 9.5 Conflict Resolution UI

- [ ] Add conflict resolution panel.

  Done means the panel supports:

  - affected resource list,
  - local change summary,
  - remote change summary,
  - conflict reason,
  - accept local,
  - accept remote,
  - manual resolve,
  - defer,
  - rollback where supported.

Expected editor location:

```txt
Editor/include/SagaEditor/Collaboration/ConflictResolutionPanel.hpp
Editor/src/SagaEditor/Collaboration/ConflictResolutionPanel.cpp
```

---

### 9.6 Collaboration Diagnostics Panel

- [ ] Add collaboration diagnostics panel.

  Done means diagnostics show:

  - session state,
  - backend state,
  - participant connectivity,
  - operation lag,
  - pending operations,
  - rejected operations,
  - lock state,
  - reconnect attempts,
  - sync errors.

Expected editor location:

```txt
Editor/include/SagaEditor/Collaboration/CollaborationDiagnosticsPanel.hpp
Editor/src/SagaEditor/Collaboration/CollaborationDiagnosticsPanel.cpp
```

---

## 10. Runtime and Server Integration

- [ ] Allow runtime preview to consume collaboration session metadata through stable APIs.

  Done means runtime preview can understand session/project context without importing editor-private headers.

- [ ] Allow server systems to consume collaboration contracts through stable APIs.

  Done means server systems can coordinate authority/session state without importing editor UI or editor-private collaboration contracts.

- [ ] Keep runtime multiplayer policy separate from editor collaboration policy.

  Runtime multiplayer optimizes for:

  - low latency,
  - prediction,
  - reconciliation,
  - authority,
  - relevance,
  - bandwidth.

  Editor collaboration optimizes for:

  - correctness,
  - visibility,
  - permissions,
  - edit ownership,
  - conflict handling,
  - safe publishing.

Runtime multiplayer and editor collaboration are related.

They are not the same thing. Naturally, that means someone will try to merge them unless the document yells loudly enough.

---

## 11. SDE Integration

- [ ] Consume SDE outputs in collaboration workflows without making SDE depend on Saga modules.

  Done means collaboration may consume:

  - schema ids,
  - compiled graph references,
  - artifact hashes,
  - diagnostics payloads,
  - validation results.

- [ ] Block unsafe publish when SDE compile fails.

  Done means failed SDE compile results prevent publishing invalid runtime state.

Forbidden dependency direction:

```txt
SDE → Saga
SDE → SagaEngine
SDE → SagaEditor
SDE → SagaServer
SDE → SagaShared
SDE → SagaCollaboration
SDE → Forge
SDE → Prism
SDE → SagaTools
```

Allowed model:

```txt
SDE produces deterministic artifacts.
SagaCollaboration records and coordinates artifact state.
Saga product decides how failed validation affects UX.
```

Forbidden model:

```txt
SDE becomes a hidden engine/editor subsystem.
```

---

## 12. Asset and Resource Collaboration

- [ ] Define collaborative resource metadata.

  Done means each collaborative resource can describe:

  - stable id,
  - display path,
  - content hash,
  - claim state,
  - lock state,
  - dirty state,
  - last modified actor,
  - validation status,
  - conflict status.

Supported resource categories should include:

```txt
scenes
prefabs
materials
textures
meshes
scripts
SDE graphs
packages
project settings
build profiles
runtime configuration
```

---

## 13. Security and Safety Requirements

- [ ] Validate all incoming collaboration operations.

- [ ] Reject unauthorized edits.

- [ ] Verify participant identity where available.

- [ ] Prevent stale operations from overwriting newer state.

- [ ] Prevent failed validation from publishing.

- [ ] Make destructive actions explicit.

- [ ] Record sensitive operations.

Security by “everyone is nice” is not security.

It is a children’s story with sockets.

---

## 14. Diagnostics Requirements

- [ ] Add actionable diagnostics for collaboration failures.

  Done means diagnostics exist for:

  - session create failure,
  - session join failure,
  - participant disconnect,
  - permission denial,
  - lock denial,
  - claim conflict,
  - operation rejection,
  - conflict detection,
  - conflict resolution failure,
  - backend unavailable,
  - reconnect failure,
  - publish blocked.

- [ ] Add structured diagnostic payloads.

  Done means diagnostics include:

  - error code,
  - readable message,
  - affected resource,
  - actor if available,
  - suggested next action,
  - recoverability flag.

---

## 15. Persistence Strategy

- [ ] Start Quick Collaboration with in-memory state where acceptable.

- [ ] Add persistent state for Team Collaboration.

  Done means persistence covers:

  - projects,
  - teams,
  - members,
  - roles,
  - sessions,
  - operation logs,
  - locks,
  - claims,
  - review records,
  - publish records,
  - audit events.

- [ ] Keep persistence behind collaboration services.

  Done means editor panels do not open database connections.

Apparently that has to be said out loud.

---

## 16. Transport Strategy

- [ ] Support local in-process transport for early collaboration testing.

- [ ] Support local network transport for quick sessions.

- [ ] Support relay-backed quick sessions.

- [ ] Support authenticated cloud sessions.

- [ ] Support production team collaboration service transport.

Transport must remain below `SagaCollaboration`.

Editor code should not care whether a participant joined through localhost, LAN, relay, or backend service.

---

## 17. Conflict Strategy

- [ ] Phase 1: Detect conflicting edits and stop unsafe overwrites.

- [ ] Phase 2: Explain affected resources and involved actors.

- [ ] Phase 3: Provide safe resolution actions.

- [ ] Phase 4: Support structured merges for resources that can be safely merged.

- [ ] Phase 5: Route complex conflicts through review workflow.

Not every conflict should be auto-merged.

Automatic merging without understanding resource semantics is just corruption with confidence.

---

## 18. Publishing Strategy

- [ ] Block publishing when collaboration state is unsafe.

  Done means publish is blocked when:

  - unresolved conflicts exist,
  - required locks are active,
  - SDE compile failed,
  - asset cook failed,
  - validation failed,
  - unauthorized user tries to publish,
  - operation stream is inconsistent,
  - project state is stale.

- [ ] Allow publishing only when project state is coherent.

  Done means publish may proceed only when:

  - required validation passes,
  - caller has permission,
  - collaboration state is synchronized,
  - publish gate returns success.

---

## 19. Recommended File/Module Layout

- [ ] Add neutral collaboration contracts under `SagaShared`.

```txt
Shared/include/SagaShared/Collaboration/
  ParticipantId.hpp
  SessionId.hpp
  SessionDescriptor.hpp
  WorkspaceDescriptor.hpp
  RoomCode.hpp
  PresenceSnapshot.hpp
  PermissionGrant.hpp
  ResourceClaim.hpp
  ResourceLock.hpp
  CollaborationDiagnostic.hpp
```

- [ ] Add collaboration service APIs under `SagaCollaboration`.

```txt
Collaboration/include/SagaCollaboration/
  CollaborationService.hpp
  SessionService.hpp
  PresenceService.hpp
  PermissionService.hpp
  ClaimService.hpp
  LockService.hpp
  ChangeStreamService.hpp
  ConflictService.hpp
  CollaborationBackend.hpp
```

- [ ] Add collaboration service implementation under `SagaCollaboration`.

```txt
Collaboration/src/SagaCollaboration/
  SessionService.cpp
  PresenceService.cpp
  PermissionService.cpp
  ClaimService.cpp
  LockService.cpp
  ChangeStreamService.cpp
  ConflictService.cpp
```

- [ ] Keep editor collaboration UI and adapters under `SagaEditor`.

```txt
Editor/include/SagaEditor/Collaboration/
  CollaborationPanel.hpp
  ParticipantsPanel.hpp
  ConflictResolutionPanel.hpp
  EditorCollaborationAdapter.hpp

Editor/src/SagaEditor/Collaboration/
  CollaborationPanel.cpp
  ParticipantsPanel.cpp
  ConflictResolutionPanel.cpp
  EditorCollaborationAdapter.cpp
```

Important:

```txt
Editor/include/SagaEditor/Collaboration
```

is only for editor UI/adapters.

It is not the location for final product collaboration contracts.

---

## 20. Migration Plan

- [ ] Freeze deprecated editor-private ownership.

  Deprecated path:

```txt
Editor/include/SagaEditor/Collaboration
```

  Done means existing code may remain temporarily, but no new final collaboration contracts are added there.

- [ ] Extract neutral contracts into `SagaShared`.

  Candidates:

  - participant id,
  - session id,
  - room code,
  - session descriptor,
  - presence snapshot,
  - permission grant,
  - resource claim,
  - resource lock,
  - diagnostics.

- [ ] Create `SagaCollaboration` service APIs.

  Initial services:

  - session service,
  - presence service,
  - permission service,
  - claim service,
  - lock service,
  - diagnostics service.

- [ ] Convert editor to a collaboration consumer.

  Done means editor keeps:

  - panels,
  - commands,
  - visual state,
  - user interaction.

  Editor loses:

  - session truth,
  - final contracts,
  - collaboration authority,
  - backend ownership.

- [ ] Add Saga product integration.

  Required flows:

  - create/open project,
  - start quick collaboration,
  - join session,
  - view session state,
  - leave session,
  - handle disconnected session.

- [ ] Add runtime/server consumers where needed.

  Done means runtime and server consume stable collaboration APIs only and do not include editor-private collaboration headers.

- [ ] Add CI boundary enforcement.

  Required checks:

```txt
Runtime/** must not include Editor/include/SagaEditor/Collaboration/**
Server/** must not include Editor/include/SagaEditor/Collaboration/**
SagaShared/** must not include SagaCollaboration/**
SagaShared/** must not include Editor/**
SagaCollaboration/** must not include Editor UI/**
```

---

## 21. Non-Goals

This roadmap does not own:

- gameplay replication policy,
- runtime prediction,
- server packet protocol,
- SDE compiler internals,
- Forge build frontend behavior,
- Prism code intelligence behavior,
- SagaTools dispatch behavior,
- asset importer implementation,
- editor docking system,
- general product shell roadmap.

Related ownership:

| Area | Document |
|---|---|
| Runtime/server multiplayer | `ENGINE_ROADMAP.md` |
| Editor authoring UI | `EDITOR_ROADMAP.md` |
| Shared contracts | `SHARED_ROADMAP.md` |
| Tool ecosystem | `TOOLS_ROADMAP.md` |
| SDE compiler | `SDE_ROADMAP.md` |
| Dependency rules | `DependencyGraph.md` |

---

## 22. Production Definition of Done

- [ ] Users can create or open collaborative projects.

- [ ] Users can start and join sessions.

- [ ] Participants are visible.

- [ ] Permissions are enforced.

- [ ] Claims and locks prevent unsafe overlap.

- [ ] Conflicts are detected and resolvable.

- [ ] Reconnect works safely.

- [ ] Operation history exists.

- [ ] Publish gates prevent invalid releases.

- [ ] Diagnostics are actionable.

- [ ] Editor/runtime/server dependencies respect module boundaries.

- [ ] Collaboration contracts are not trapped inside editor-private headers.

---

## 23. Final Architecture Rule

The collaboration architecture should remain:

```txt
Saga
  owns product lifecycle

SagaEditor
  owns collaboration UI

SagaShared
  owns neutral contracts

SagaCollaboration
  owns collaboration implementation

SagaCollaborationCore
  optionally owns low-level internals

Runtime/Server
  consume stable collaboration services where needed
```

The editor displays collaboration.

The editor does not own collaboration truth.

That distinction is not cosmetic.

It is the difference between a product architecture and a folder full of consequences.