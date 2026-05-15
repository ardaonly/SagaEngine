# SagaCollaboration — Collaboration Roadmap

> Last updated: 2026-05-15
> Status: Active roadmap
> Target: A production-grade collaboration layer for Saga projects that supports local/team/cloud collaboration workflows, presence, permissions, claims, locks, conflicts, change streams, resource ownership, publish-blocking collaboration state, and editor/product/runtime integration without making the editor or product shell own collaboration truth.
> Scope: Collaboration sessions, room codes, team projects, participants, presence, permissions, resource claims, hard locks, conflict descriptors, graph/script/asset/SDE/package/build resource categories, collaboration diagnostics, editor UX integration, Saga product integration, build/publish gates, offline/degraded modes, and backend/service boundaries.

---

## 0. Roadmap Convention

* `[x]` — Shipped. The note after the item names the files, modules, tests, or integration points that represent completed work and highlights decisions worth preserving.
* `[ ]` — Open. The item describes the finished production state, not temporary scaffolding.
* Shipped collaboration work must include implementation files, contract files, tests, UI integration points, and observable behavior where practical.
* Open collaboration work must describe observable product/editor behavior, not vague multiplayer/product ambition.
* SagaCollaboration owns collaboration implementation.
* SagaShared may own neutral collaboration contracts.
* Saga owns product-level collaboration entry points.
* SagaEditor owns collaboration authoring UI.
* Forge may consume collaboration state for build/publish gates.
* Runtime/server do not own editor collaboration truth.
* Collaboration profiles/permissions must not be confused with authoring profiles.

Collaboration is not a chat window next to an editor.

Collaboration is resource ownership, conflict management, permissions, and publish safety under multiple humans changing the same project.

---

## 1. Document Purpose

This document defines Saga's collaboration roadmap.

Saga collaboration exists to let users work on the same Saga project safely without corrupting source assets, SDE schemas, gameplay graphs, scripts, build profiles, package manifests, or publish state.

It covers:

* collaboration sessions,
* quick rooms,
* team projects,
* participants,
* presence,
* roles,
* permissions,
* resource claims,
* hard locks,
* conflict descriptors,
* change streams,
* disconnected/offline behavior,
* editor collaboration UX,
* product collaboration UX,
* build/publish collaboration gates,
* diagnostics,
* backend/service boundaries.

Correct model:

```txt
SagaCollaboration
    owns collaboration session state and conflict/permission/claim/lock behavior

SagaShared
    owns neutral contracts

Saga
    owns product entry points and mode orchestration

SagaEditor
    owns collaboration UI display and resource-level interaction

Forge
    consumes collaboration state for build/publish gates
```

Incorrect model:

```txt
Editor panel stores who owns the session because that was easier than writing a real service.
```

That is how collaboration turns into a UI-shaped race condition.

---

## 2. Companion Documents

| Document                            | Purpose                                                                          |
| ----------------------------------- | -------------------------------------------------------------------------------- |
| `SAGA_PRODUCT_ROADMAP.md`           | Product shell, collaboration entry points, project lifecycle, mode orchestration |
| `EDITOR_ROADMAP.md`                 | Editor collaboration toolbar, participants panel, claims/locks/conflict UI       |
| `SHARED_ROADMAP.md`                 | Neutral collaboration contracts and resource references                          |
| `BUILD_PUBLISH_PIPELINE_ROADMAP.md` | Collaboration state as build/publish gate                                        |
| `FORGE_ROADMAP.md`                  | Forge validates collaboration state before strict build/publish profiles         |
| `SAGA_GAMEPLAY_GRAPH_ROADMAP.md`    | Graph resources, node/edge conflict potential, graph claims/locks                |
| `AUTHORING_AUTHORITY_MODEL.md`      | Authority-sensitive resources and server/publish safety                          |
| `SAGA_SCRIPTING_ROADMAP.md`         | Script source/binding/generated-code collaboration resources                     |
| `ASSET_PIPELINE_ROADMAP.md`         | Asset metadata/import settings/cooked artifact collaboration resources           |
| `SDE_ROADMAP.md`                    | SDE schemas/source artifacts and schema migration locks                          |
| `PRISM_ROADMAP.md`                  | Artifact/resource relationship reports consumed by collaboration diagnostics     |
| `DIAGNOSTICS_ROADMAP.md`            | Structured diagnostics/reporting model                                           |
| `DependencyGraph.md`                | Dependency ownership rules                                                       |

---

## 3. Core Ownership Rule

* [x] Define collaboration implementation outside SagaEditor.

  Represented by:

  ```txt
  COLLABORATION_ROADMAP.md
  SHARED_ROADMAP.md
  EDITOR_ROADMAP.md
  DependencyGraph.md
  ```

  Preserved decision:

  ```txt
  SagaEditor displays collaboration state.
  SagaCollaboration owns collaboration state.
  SagaShared owns neutral contracts.
  ```

* [ ] Create a dedicated `SagaCollaboration` implementation module/service.

  Done means collaboration implementation owns:

  * sessions,
  * participants,
  * presence,
  * room codes,
  * roles,
  * permissions,
  * claims,
  * locks,
  * conflicts,
  * change streams,
  * reconnect/recovery behavior,
  * collaboration diagnostics,
  * backend adapters.

Expected files:

```txt
Collaboration/include/SagaCollaboration/Session/CollaborationSession.h
Collaboration/include/SagaCollaboration/Session/SessionService.h
Collaboration/include/SagaCollaboration/Presence/PresenceService.h
Collaboration/include/SagaCollaboration/Permissions/PermissionService.h
Collaboration/include/SagaCollaboration/Resources/ResourceClaimService.h
Collaboration/include/SagaCollaboration/Resources/ResourceLockService.h
Collaboration/include/SagaCollaboration/Conflicts/ConflictService.h
Collaboration/src/SagaCollaboration/Session/SessionService.cpp
```

* [ ] Keep final collaboration contracts out of editor-private headers.

  Deprecated location:

  ```txt
  Editor/include/SagaEditor/Collaboration
  ```

  Correct contract location:

  ```txt
  Shared/include/SagaShared/Collaboration
  ```

  Correct implementation location:

  ```txt
  Collaboration/include/SagaCollaboration
  Collaboration/src/SagaCollaboration
  ```

---

## 4. Dependency Rules

### 4.1 Allowed Dependencies

Allowed dependency directions:

```txt
Saga → SagaCollaboration public service API
Saga → SagaShared collaboration contracts
SagaEditor → SagaCollaboration public service API
SagaEditor → SagaShared collaboration contracts
Forge → SagaShared collaboration contracts / collaboration status report
Runtime → SagaShared contracts where explicitly needed
Server → SagaShared contracts where explicitly needed
SagaCollaboration → SagaShared contracts
SagaCollaboration → backend/transport adapters through explicit boundaries
```

---

### 4.2 Forbidden Dependencies

Forbidden dependency directions:

```txt
SagaShared → SagaCollaboration implementation
SagaCollaboration → SagaEditor UI
SagaCollaboration → Saga product shell internals
SagaCollaboration → Runtime private internals
SagaCollaboration → Server private internals
SagaCollaboration → SDE compiler internals
SagaCollaboration → Forge planner internals
SagaCollaboration → Prism internals
SagaCollaboration → asset cooker internals
SagaCollaboration → scripting compiler internals
```

Forbidden shortcuts:

```txt
Editor panel owns session truth.
Product shell stores locks directly.
Forge mutates collaboration state during publish check.
Runtime/server rely on editor collaboration state for authority.
Authoring profile grants collaboration permission.
Conflict resolution is done by overwriting latest file silently.
```

---

## 5. Collaboration Modes

Required collaboration modes:

```txt
SoloLocal
QuickRoom
TeamProject
ReviewOnly
OfflineDraft
ReadOnlySnapshot
```

---

### 5.1 SoloLocal

* [ ] Define solo local mode.

  Done means:

  * no remote session is required,
  * collaboration services can still expose local resource dirty/claim-like state,
  * publish/build gates do not require network collaboration checks,
  * local diagnostics still use collaboration resource descriptors where useful.

---

### 5.2 QuickRoom

* [ ] Define quick collaboration room.

  Done means users can:

  * create a room,
  * share a room code,
  * join by room code,
  * see participants,
  * claim/lock resources,
  * receive conflict diagnostics,
  * leave safely.

QuickRoom is for lightweight collaboration, not full production source-control replacement.

---

### 5.3 TeamProject

* [ ] Define team project collaboration.

  Done means:

  * project members have roles,
  * permissions are persistent,
  * resource claims/locks persist across sessions where configured,
  * conflict records survive reconnects,
  * build/publish gates can check team collaboration state.

---

### 5.4 ReviewOnly

* [ ] Define review-only mode.

  Done means reviewers can:

  * inspect project state,
  * comment where supported,
  * view diagnostics,
  * view publish readiness,
  * not mutate resources unless permission changes.

---

### 5.5 OfflineDraft

* [ ] Define offline draft mode.

  Done means users can:

  * work locally while disconnected,
  * record local changes,
  * see degraded collaboration status,
  * reconnect and produce conflict records where needed,
  * avoid falsely claiming publish-ready team state.

---

### 5.6 ReadOnlySnapshot

* [ ] Define read-only snapshot mode.

  Done means users can open an immutable project snapshot for inspection, diagnostics, review, or archival reproduction.

---

## 6. Session Model

* [ ] Add collaboration session descriptor.

  Done means a session describes:

  * session id,
  * project id,
  * workspace id,
  * mode,
  * host/owner,
  * participants,
  * backend adapter,
  * permissions model,
  * resource state summary,
  * conflict summary,
  * connection state.

Expected contracts:

```txt
Shared/include/SagaShared/Collaboration/SessionId.hpp
Shared/include/SagaShared/Collaboration/SessionDescriptor.hpp
Shared/include/SagaShared/Collaboration/RoomCode.hpp
```

Expected implementation:

```txt
Collaboration/include/SagaCollaboration/Session/CollaborationSession.h
Collaboration/include/SagaCollaboration/Session/SessionService.h
Collaboration/src/SagaCollaboration/Session/SessionService.cpp
```

* [ ] Add session lifecycle.

  Done means sessions support:

  * create,
  * join,
  * leave,
  * reconnect,
  * pause/degrade,
  * close,
  * recover.

* [ ] Add session diagnostics.

  Done means failures explain:

  * room not found,
  * permission denied,
  * version mismatch,
  * project mismatch,
  * network/backend unavailable,
  * conflict requires resolution,
  * stale workspace state.

---

## 7. Participants and Presence

* [ ] Add participant model.

  Done means participants have:

  * participant id,
  * display name,
  * connection state,
  * role summary,
  * current activity,
  * active resource focus,
  * last seen timestamp.

Expected contracts:

```txt
Shared/include/SagaShared/Collaboration/ParticipantId.hpp
Shared/include/SagaShared/Collaboration/ParticipantDescriptor.hpp
Shared/include/SagaShared/Collaboration/PresenceSnapshot.hpp
```

* [ ] Add presence service.

  Done means presence updates can show:

  * who is online,
  * who is editing what,
  * who is reviewing,
  * who holds a claim/lock,
  * who is disconnected/idle.

Expected implementation:

```txt
Collaboration/include/SagaCollaboration/Presence/PresenceService.h
Collaboration/src/SagaCollaboration/Presence/PresenceService.cpp
```

* [ ] Keep presence non-authoritative for resource permissions.

  Done means presence display does not grant edit rights or publish rights by itself.

---

## 8. Permissions Model

Profiles are not permissions.

Required distinction:

```txt
Authoring profile = what UI depth the user sees.
Collaboration permission = what the user is allowed to do.
```

* [ ] Add permission model.

  Required permission domains:

```txt
ProjectRead
ProjectWrite
AssetEdit
SceneEdit
GraphEdit
ScriptEdit
SDEEdit
SchemaMigration
BuildRun
PackageStage
PublishCheck
PublishRelease
LockOverride
ConflictResolve
PermissionManage
ServerAuthorityEdit
```

Expected contracts:

```txt
Shared/include/SagaShared/Collaboration/PermissionGrant.hpp
Shared/include/SagaShared/Collaboration/PermissionDomain.hpp
Shared/include/SagaShared/Collaboration/RoleDescriptor.hpp
```

Expected implementation:

```txt
Collaboration/include/SagaCollaboration/Permissions/PermissionService.h
Collaboration/src/SagaCollaboration/Permissions/PermissionService.cpp
```

* [ ] Add role model.

  Suggested roles:

```txt
Owner
Admin
TeamLead
GameplayDesigner
Programmer
NetworkDeveloper
Artist
TechnicalArtist
Reviewer
Guest
```

* [ ] Enforce permissions independently from authoring profiles.

  Done means:

  * advanced profile does not grant publish permission,
  * beginner profile user may still have asset edit permission,
  * network developer profile does not automatically grant server authority edit permission,
  * team lead profile does not automatically grant lock override unless role allows it.

---

## 9. Resource Model

Collaboration must operate on project resources, not vague files alone.

Required resource categories:

```txt
ProjectManifest
WorkspaceState
Scene
Prefab
Entity
Component
AssetSource
AssetMetadata
ImportSettings
CookSettings
CookedArtifact
Material
Mesh
Texture
Audio
SDEPackage
SDESchema
SDEData
GameplayGraph
GraphNode
GraphEdge
GraphMacro
CSharpScript
ScriptBindingManifest
GeneratedCode
BuildProfile
PackageProfile
PackageManifest
PublishReport
DiagnosticsReport
EditorProfile
AuthoringProfile
```

* [ ] Add resource reference contract.

  Done means collaboration resources can be identified independently from editor UI.

Expected contracts:

```txt
Shared/include/SagaShared/Resources/ResourceId.hpp
Shared/include/SagaShared/Resources/ResourceKind.hpp
Shared/include/SagaShared/Resources/ResourceRef.hpp
Shared/include/SagaShared/Collaboration/CollaborativeResourceDescriptor.hpp
```

* [ ] Add resource hierarchy.

  Done means resource relationships can represent:

```txt
Project → Scene → Entity → Component
Project → AssetMetadata → SourceAsset / CookedArtifact
Project → SDEPackage → SDESchema / SDEData
Project → GameplayGraph → GraphNode / GraphEdge
Project → CSharpScript → ScriptBindingManifest / GeneratedCode
Project → BuildProfile → PackageManifest
```

* [ ] Add resource diagnostics.

  Done means collaboration diagnostics identify exact affected resource, not just “someone changed something”.

---

## 10. Claims

A claim means “I am currently working on this resource.”

Claims are collaborative signals, not absolute locks.

* [ ] Add resource claim model.

  Done means claims include:

  * resource ref,
  * participant id,
  * claim type,
  * claim timestamp,
  * activity summary,
  * expiration/heartbeat where applicable.

Expected contracts:

```txt
Shared/include/SagaShared/Collaboration/ResourceClaim.hpp
Shared/include/SagaShared/Collaboration/ClaimType.hpp
```

Expected implementation:

```txt
Collaboration/include/SagaCollaboration/Resources/ResourceClaimService.h
Collaboration/src/SagaCollaboration/Resources/ResourceClaimService.cpp
```

* [ ] Add claim types.

  Required types:

```txt
Viewing
Editing
Reviewing
Testing
Building
ResolvingConflict
```

* [ ] Surface claims in editor.

  Done means editor shows claims in:

  * hierarchy,
  * content browser,
  * graph editor,
  * script editor,
  * SDE source view,
  * asset inspector,
  * build/publish panel.

* [ ] Claims should not block by default.

  Done means claims warn users and improve coordination, but hard locks enforce exclusivity where required.

---

## 11. Hard Locks

A lock means “this resource cannot be concurrently mutated without explicit override.”

* [ ] Add hard lock model.

  Done means locks include:

  * resource ref,
  * participant id/service id,
  * lock type,
  * reason,
  * timestamp,
  * expiration policy,
  * override policy,
  * diagnostics.

Expected contracts:

```txt
Shared/include/SagaShared/Collaboration/ResourceLock.hpp
Shared/include/SagaShared/Collaboration/LockType.hpp
```

Expected implementation:

```txt
Collaboration/include/SagaCollaboration/Resources/ResourceLockService.h
Collaboration/src/SagaCollaboration/Resources/ResourceLockService.cpp
```

* [ ] Add lock-worthy operations.

  Required hard lock cases:

```txt
SDE schema migration
asset id migration
bulk reimport
package manifest rewrite
build profile migration
script binding manifest regeneration
generated code detach/convert
graph macro extraction affecting shared nodes
scene/prefab structural migration
publish release
```

* [ ] Enforce locks in editor/product/build workflows.

  Done means locked resources:

  * cannot be edited without permission/override,
  * block relevant build/publish profiles where configured,
  * show lock owner/reason,
  * produce actionable diagnostics.

* [ ] Support lock override with permission.

  Done means only users with `LockOverride` permission can override locks, and override is recorded.

---

## 12. Conflict Model

* [ ] Add conflict descriptor.

  Done means conflicts include:

  * conflict id,
  * resource refs,
  * participants involved,
  * conflict kind,
  * base version/hash,
  * local version/hash,
  * remote version/hash,
  * severity,
  * resolution options,
  * publish/build blocking flag.

Expected contracts:

```txt
Shared/include/SagaShared/Collaboration/ConflictId.hpp
Shared/include/SagaShared/Collaboration/ConflictDescriptor.hpp
Shared/include/SagaShared/Collaboration/ConflictKind.hpp
Shared/include/SagaShared/Collaboration/ConflictResolution.hpp
```

Expected implementation:

```txt
Collaboration/include/SagaCollaboration/Conflicts/ConflictService.h
Collaboration/src/SagaCollaboration/Conflicts/ConflictService.cpp
```

* [ ] Add conflict kinds.

  Required kinds:

```txt
TextFileConflict
SceneStructuralConflict
PrefabOverrideConflict
GraphNodeConflict
GraphEdgeConflict
GraphMacroConflict
ScriptSourceConflict
GeneratedCodeConflict
AssetSourceConflict
AssetMetadataConflict
ImportSettingsConflict
CookSettingsConflict
SDEDataConflict
SDESchemaConflict
BuildProfileConflict
PackageManifestConflict
PublishStateConflict
```

* [ ] Add conflict severity.

  Required severities:

```txt
Info
Warning
Blocking
PublishBlocking
Destructive
```

* [ ] Add conflict resolution workflow.

  Done means users can:

  * inspect conflict,
  * compare versions where supported,
  * choose local/remote/manual merge where safe,
  * resolve graph conflicts visually where supported,
  * resolve asset metadata conflicts through inspector,
  * resolve script/SDE text conflicts through editor/source tools,
  * mark resolved with audit record.

---

## 13. Graph Collaboration

* [ ] Add graph resource collaboration model.

  Done means gameplay graphs expose collaborative resources for:

  * graph file,
  * graph metadata,
  * nodes,
  * pins,
  * edges,
  * macros/subgraphs,
  * generated code refs,
  * authority metadata.

* [ ] Support graph node/edge claims.

  Done means users can claim graph regions or nodes while editing.

* [ ] Support graph conflict descriptors.

  Done means conflicts can identify:

  * same node edited by multiple users,
  * edge deleted while another user edits connected node,
  * macro extraction conflict,
  * authority metadata conflict,
  * generated code stale/detach conflict.

* [ ] Add publish blockers for unresolved graph conflicts.

  Done means unresolved graph conflicts can block publish/package profiles.

---

## 14. Script Collaboration

* [ ] Add script collaboration resources.

  Required resources:

```txt
CSharpScript
ScriptProjectManifest
ScriptBindingManifest
GeneratedCode
GeneratedCodeOrigin
```

* [ ] Support script claims/locks.

  Done means editing scripts can claim files, while binding regeneration or generated-code detach can require hard locks.

* [ ] Support script conflict descriptors.

  Done means conflicts can identify:

  * source file text conflict,
  * binding manifest conflict,
  * generated code manually edited conflict,
  * graph-to-generated-code origin conflict,
  * authority metadata mismatch.

* [ ] Connect script conflicts to build/publish gates.

  Done means unresolved script conflicts and stale binding conflicts can block strict build/publish profiles.

---

## 15. Asset Collaboration

* [ ] Add asset collaboration resources.

  Required resources:

```txt
AssetSource
AssetMetadata
ImportSettings
CookSettings
CookedArtifact
AssetManifest
Material
Mesh
Texture
Audio
```

* [ ] Support asset claims/locks.

  Done means:

  * normal asset edits use claims,
  * bulk reimport uses hard locks,
  * asset id migration uses hard locks,
  * cook profile migration uses hard locks,
  * destructive source conversion uses hard locks.

* [ ] Support asset conflict descriptors.

  Done means conflicts can identify:

  * source asset conflict,
  * import settings conflict,
  * cook settings conflict,
  * metadata conflict,
  * cooked artifact stale conflict,
  * package asset inclusion conflict.

* [ ] Connect asset collaboration to asset pipeline.

  Done means asset claims/locks/conflicts are visible in:

  * content browser,
  * asset inspector,
  * import/reimport workflow,
  * build/publish panel.

---

## 16. SDE and Schema Collaboration

* [ ] Add SDE resource collaboration model.

  Required resources:

```txt
SDEPackage
SDESchema
SDEData
SDEGraphSource
SDEValidationRule
SDEGeneratedArtifact
```

* [ ] Require hard locks for schema migrations.

  Done means schema migrations lock affected packages/resources and can block publish until resolved.

* [ ] Support SDE conflict descriptors.

  Done means conflicts can identify:

  * schema changed while data edited,
  * enum value changed while referenced,
  * graph schema changed while graph edited,
  * artifact format changed while stale generated output exists,
  * validation rule conflict.

* [ ] Keep SDE compiler standalone.

  Done means collaboration may track SDE resources, but SDE itself does not depend on collaboration implementation.

---

## 17. Build, Package, and Publish Collaboration

* [ ] Add build/publish collaboration resources.

  Required resources:

```txt
BuildProfile
PackageProfile
PackageManifest
BuildReport
DiagnosticsReport
PublishReport
PublishRelease
```

* [ ] Add publish gate integration.

  Done means publish readiness can be blocked by:

  * unresolved blocking conflicts,
  * hard locks on publish-relevant resources,
  * insufficient publish permission,
  * dirty resources not included in package,
  * disconnected lock owner where policy requires resolution,
  * pending schema/asset/package migration.

* [ ] Add Forge collaboration validation step.

  Done means Forge can run a read-only validation step that consumes collaboration status and emits build/publish diagnostics.

* [ ] Prevent Forge from mutating collaboration state during publish check.

  Done means Forge reports blockers; it does not silently resolve locks/conflicts.

---

## 18. Change Stream

* [ ] Add collaboration change envelope.

  Done means changes can describe:

  * change id,
  * participant id,
  * resource ref,
  * operation kind,
  * timestamp,
  * base version/hash,
  * new version/hash,
  * payload ref,
  * diagnostic refs.

Expected contracts:

```txt
Shared/include/SagaShared/Collaboration/CollaborationChangeEnvelope.hpp
Shared/include/SagaShared/Collaboration/ChangeId.hpp
Shared/include/SagaShared/Collaboration/ChangeKind.hpp
```

* [ ] Add change stream service.

  Done means collaboration implementation can:

  * publish changes,
  * subscribe to changes,
  * replay recent changes,
  * detect missing changes,
  * produce diagnostics.

Expected implementation:

```txt
Collaboration/include/SagaCollaboration/Changes/ChangeStreamService.h
Collaboration/src/SagaCollaboration/Changes/ChangeStreamService.cpp
```

* [ ] Keep change payloads resource-aware.

  Done means changes reference resources and versions, not just raw files.

---

## 19. Versioning and State Hashes

* [ ] Add resource version model.

  Done means collaborative resources can carry:

  * resource version,
  * content hash,
  * metadata hash,
  * generated artifact hash where applicable,
  * base version reference.

Expected contracts:

```txt
Shared/include/SagaShared/Collaboration/ResourceVersion.hpp
Shared/include/SagaShared/Collaboration/ResourceStateHash.hpp
```

* [ ] Use hashes for conflict detection.

  Done means collaboration can detect stale edits when base hash/version no longer matches current resource state.

* [ ] Integrate with Prism/Forge reports where useful.

  Done means stale artifact/resource relationship reports can inform collaboration diagnostics but not become collaboration truth by themselves.

---

## 20. Offline and Reconnect Behavior

* [ ] Add disconnected state model.

  Done means collaboration distinguishes:

  * online,
  * degraded,
  * reconnecting,
  * offline draft,
  * read-only fallback,
  * failed.

* [ ] Add offline draft support.

  Done means users can continue local edits where allowed and later produce conflict records on reconnect.

* [ ] Add reconnect reconciliation.

  Done means reconnect compares:

  * local base versions,
  * current remote versions,
  * local changes,
  * locks/claims,
  * conflicts.

* [ ] Prevent false publish-ready state while disconnected.

  Done means team/publish workflows cannot pretend collaboration state is clean if current state cannot be verified.

---

## 21. Backend and Transport Boundaries

* [ ] Add backend adapter interface.

  Done means collaboration implementation can use different backends without editor/product code depending on backend details.

Expected files:

```txt
Collaboration/include/SagaCollaboration/Backend/ICollaborationBackend.h
Collaboration/include/SagaCollaboration/Backend/BackendConnectionState.h
Collaboration/src/SagaCollaboration/Backend/LocalCollaborationBackend.cpp
```

* [ ] Support local backend first.

  Done means local/single-machine collaboration state can be tested without cloud dependency.

* [ ] Support future network/cloud backend through adapter.

  Done means cloud/team backend can be added without rewriting editor/product UI ownership.

* [ ] Keep backend credentials/secrets out of project source.

  Done means credential/session tokens are stored through appropriate user/local secure settings, not committed project files.

---

## 22. Editor UX Integration

* [ ] Add collaboration toolbar.

  Done means editor shows:

  * session mode,
  * connection state,
  * participant count,
  * active conflicts,
  * lock/claim warnings,
  * leave/reconnect actions.

Expected files:

```txt
Editor/include/SagaEditor/Collaboration/CollaborationToolbar.h
Editor/src/SagaEditor/Collaboration/CollaborationToolbar.cpp
```

* [ ] Add participants panel.

  Done means editor shows:

  * participants,
  * roles,
  * permissions summary,
  * current activity,
  * connection state.

* [ ] Add resource status decorators.

  Done means editor surfaces show claims/locks/conflicts in:

  * hierarchy,
  * viewport selection,
  * content browser,
  * asset inspector,
  * graph editor,
  * script editor,
  * SDE/source editor,
  * build/publish panel.

* [ ] Add conflict resolution UI.

  Done means editor can open conflict-specific resolution surfaces depending on resource kind.

* [ ] Keep editor UX as consumer.

  Done means editor calls SagaCollaboration services and displays state; it does not own session truth.

---

## 23. Saga Product Integration

* [ ] Add product-level collaboration entry points.

  Done means Saga can:

  * start solo/local session,
  * create quick room,
  * join by room code,
  * open team project,
  * show collaboration dashboard summary,
  * route into editor mode with collaboration context.

Expected files:

```txt
Saga/include/Saga/Collaboration/CollaborationWorkflowService.h
Saga/include/Saga/Collaboration/CollaborationStatusPresenter.h
Saga/src/Collaboration/CollaborationWorkflowService.cpp
```

* [ ] Show project-level collaboration health.

  Done means Saga dashboard can show:

  * connected/disconnected state,
  * unresolved conflicts,
  * locks affecting build/publish,
  * publish permission status,
  * team/project availability.

* [ ] Keep product shell as orchestrator.

  Done means Saga does not own lock/conflict/session implementation.

---

## 24. Diagnostics

* [ ] Add collaboration diagnostics.

  Required diagnostic families:

```txt
Collaboration.Session
Collaboration.Permission
Collaboration.Presence
Collaboration.Claim
Collaboration.Lock
Collaboration.Conflict
Collaboration.ChangeStream
Collaboration.Backend
Collaboration.PublishGate
Collaboration.Offline
```

Expected contracts:

```txt
Shared/include/SagaShared/Collaboration/CollaborationDiagnostic.hpp
```

Expected implementation:

```txt
Collaboration/include/SagaCollaboration/Diagnostics/CollaborationDiagnosticService.h
Collaboration/src/SagaCollaboration/Diagnostics/CollaborationDiagnosticService.cpp
```

* [ ] Add resource-linked diagnostics.

  Done means diagnostics reference exact affected resources.

* [ ] Integrate with Problems panel and build/publish reports.

  Done means collaboration diagnostics can appear in:

  * editor Problems panel,
  * Saga collaboration dashboard,
  * Forge publish report,
  * diagnostics report panel.

---

## 25. Security and Safety

* [ ] Treat collaborators as permission-scoped actors.

  Done means every mutating action checks permissions, not just UI visibility.

* [ ] Add audit records for sensitive actions.

  Sensitive actions:

```txt
lock override
conflict resolution
publish release
schema migration
asset id migration
permission change
server authority graph edit
package manifest rewrite
```

* [ ] Avoid silent destructive conflict resolution.

  Done means destructive overwrites require explicit action and permission.

* [ ] Validate room codes and session joins.

  Done means invalid/expired room codes fail safely.

* [ ] Protect backend credentials.

  Done means tokens/secrets are not stored in project manifests or shared source files.

---

## 26. Build and Publish Gate Behavior

* [ ] Define collaboration gate policy per build profile.

  Example:

```txt
editor-preview:
  warn on conflicts, block on hard locks affecting opened resource

dev-client/dev-server:
  block on unresolved blocking conflicts for packaged resources

shipping-full:
  block on unresolved blocking/publish-blocking conflicts, critical locks, missing publish permission, unverifiable team state
```

* [ ] Expose collaboration blockers in publish report.

  Done means publish report can include:

  * conflict id,
  * resource ref,
  * lock owner,
  * permission failure,
  * suggested action.

* [ ] Keep gate read-only.

  Done means publish check reports collaboration blockers but does not resolve them automatically.

---

## 27. Testing Strategy

### 27.1 Contract Tests

* [ ] Add collaboration contract tests.

  Required coverage:

  * session descriptor serialization,
  * participant descriptor serialization,
  * permission grant serialization,
  * resource claim serialization,
  * resource lock serialization,
  * conflict descriptor serialization,
  * change envelope serialization,
  * diagnostic serialization.

---

### 27.2 Permission Tests

* [ ] Add permission tests.

  Required coverage:

  * role grants permission,
  * role denies permission,
  * authoring profile does not grant permission,
  * publish permission required,
  * lock override permission required,
  * server authority edit permission required.

---

### 27.3 Claim/Lock Tests

* [ ] Add claim/lock tests.

  Required coverage:

  * claim created,
  * claim heartbeat/expiration,
  * hard lock created,
  * lock blocks mutation,
  * lock override requires permission,
  * disconnected lock owner policy.

---

### 27.4 Conflict Tests

* [ ] Add conflict tests.

  Required coverage:

  * text file conflict,
  * graph node conflict,
  * graph edge conflict,
  * asset metadata conflict,
  * import settings conflict,
  * SDE schema conflict,
  * script binding conflict,
  * package manifest conflict,
  * conflict resolution audit record.

---

### 27.5 Editor Integration Tests

* [ ] Add editor collaboration tests.

  Required coverage:

  * toolbar displays connection state,
  * participants panel displays users,
  * content browser shows asset claims/locks,
  * graph editor shows node claims/conflicts,
  * script editor shows script locks,
  * conflict opens correct resolution UI.

---

### 27.6 Build/Publish Gate Tests

* [ ] Add collaboration gate tests.

  Required coverage:

  * editor-preview warns but allows non-blocking conflict,
  * shipping-full blocks unresolved publish conflict,
  * hard lock blocks package where configured,
  * missing publish permission blocks publish,
  * offline unverifiable team state blocks release profile.

---

## 28. MVP Vertical Slice

The first collaboration vertical slice should focus on resource safety, not fancy chat.

Required scenario:

```txt
Two users edit the same MMO Starter project.
User A edits QuestReward gameplay graph.
User B edits the same graph and one texture import setting.
SagaCollaboration tracks claims, detects conflict, blocks publish until resolved.
```

Required behavior:

1. Saga starts QuickRoom.
2. User B joins by room code.
3. Both users see participants and presence.
4. User A claims `QuestReward` graph node.
5. User B sees claim in graph editor.
6. User B edits same node anyway where policy allows, creating conflict.
7. Conflict descriptor references graph id/node id/base version/local/remote hashes.
8. User B edits texture import settings while User A has asset claim.
9. Asset metadata conflict or warning is produced depending on policy.
10. Build/publish panel shows unresolved collaboration conflict.
11. Forge publish-check emits `CollaborationConflict` blocker.
12. Conflict is resolved through editor conflict UI.
13. Publish-check no longer blocked by collaboration state.

This slice proves collaboration is not ornamental.

It protects project state.

---

## 29. Non-Goals

SagaCollaboration must not become:

* editor UI,
* product shell,
* source control replacement in the first version,
* runtime/server authority system,
* SDE compiler,
* Forge build planner,
* Prism analyzer,
* asset importer/cooker,
* C# script compiler,
* chat/messaging app with no resource safety,
* permission system confused with authoring profiles.

Collaboration must make simultaneous work safer.

If it only shows avatars while resources corrupt underneath, it is decoration.

---

## 30. Risk Register

### 30.1 Risk: Editor Owns Collaboration Truth

Mitigation:

* collaboration implementation lives in SagaCollaboration,
* contracts live in SagaShared,
* editor consumes services and displays state,
* dependency boundary tests forbid editor-private final contracts.

---

### 30.2 Risk: Profiles Are Treated as Permissions

Mitigation:

* explicit permission model,
* authoring profile controls UX depth only,
* permission checks happen on mutating actions,
* tests cover profile/permission separation.

---

### 30.3 Risk: Conflicts Are Too Generic

Mitigation:

* resource-specific conflict kinds,
* graph/script/asset/SDE/package-specific descriptors,
* source/resource refs in diagnostics,
* editor resolution UI routes by resource kind.

---

### 30.4 Risk: Publish Ignores Collaboration State

Mitigation:

* Forge collaboration gate,
* publish blockers for unresolved conflicts/locks/permissions,
* Saga dashboard shows collaboration health,
* shipping profiles enforce strict checks.

---

### 30.5 Risk: Collaboration Scope Becomes Too Big Too Early

Mitigation:

* start with QuickRoom + graph/asset conflict MVP,
* add team/cloud backend later,
* prioritize claims/locks/conflicts over decorative presence features.

---

## 31. Suggested File Targets

Expected shared contracts:

```txt
Shared/include/SagaShared/Collaboration/SessionId.hpp
Shared/include/SagaShared/Collaboration/SessionDescriptor.hpp
Shared/include/SagaShared/Collaboration/RoomCode.hpp
Shared/include/SagaShared/Collaboration/ParticipantId.hpp
Shared/include/SagaShared/Collaboration/ParticipantDescriptor.hpp
Shared/include/SagaShared/Collaboration/PresenceSnapshot.hpp
Shared/include/SagaShared/Collaboration/PermissionGrant.hpp
Shared/include/SagaShared/Collaboration/PermissionDomain.hpp
Shared/include/SagaShared/Collaboration/RoleDescriptor.hpp
Shared/include/SagaShared/Collaboration/ResourceClaim.hpp
Shared/include/SagaShared/Collaboration/ResourceLock.hpp
Shared/include/SagaShared/Collaboration/ConflictDescriptor.hpp
Shared/include/SagaShared/Collaboration/CollaborationChangeEnvelope.hpp
Shared/include/SagaShared/Collaboration/CollaborationDiagnostic.hpp
```

Expected implementation files:

```txt
Collaboration/include/SagaCollaboration/Session/SessionService.h
Collaboration/include/SagaCollaboration/Presence/PresenceService.h
Collaboration/include/SagaCollaboration/Permissions/PermissionService.h
Collaboration/include/SagaCollaboration/Resources/ResourceClaimService.h
Collaboration/include/SagaCollaboration/Resources/ResourceLockService.h
Collaboration/include/SagaCollaboration/Conflicts/ConflictService.h
Collaboration/include/SagaCollaboration/Changes/ChangeStreamService.h
Collaboration/include/SagaCollaboration/Backend/ICollaborationBackend.h
Collaboration/src/SagaCollaboration/Session/SessionService.cpp
Collaboration/src/SagaCollaboration/Presence/PresenceService.cpp
Collaboration/src/SagaCollaboration/Permissions/PermissionService.cpp
Collaboration/src/SagaCollaboration/Resources/ResourceClaimService.cpp
Collaboration/src/SagaCollaboration/Resources/ResourceLockService.cpp
Collaboration/src/SagaCollaboration/Conflicts/ConflictService.cpp
```

Expected editor files:

```txt
Editor/include/SagaEditor/Collaboration/CollaborationToolbar.h
Editor/include/SagaEditor/Collaboration/ParticipantsPanel.h
Editor/include/SagaEditor/Collaboration/ResourceStatusPresenter.h
Editor/include/SagaEditor/Collaboration/ConflictResolutionPanel.h
Editor/src/SagaEditor/Collaboration/CollaborationToolbar.cpp
Editor/src/SagaEditor/Collaboration/ParticipantsPanel.cpp
Editor/src/SagaEditor/Collaboration/ResourceStatusPresenter.cpp
Editor/src/SagaEditor/Collaboration/ConflictResolutionPanel.cpp
```

Expected product files:

```txt
Saga/include/Saga/Collaboration/CollaborationWorkflowService.h
Saga/include/Saga/Collaboration/CollaborationStatusPresenter.h
Saga/src/Collaboration/CollaborationWorkflowService.cpp
Saga/src/Collaboration/CollaborationStatusPresenter.cpp
```

Expected Forge files:

```txt
Tools/Forge/include/Forge/Steps/CollaborationValidateStep.hpp
Tools/Forge/src/Steps/CollaborationValidateStep.cpp
```

---

## 32. Decision Summary

Preserve these decisions:

```txt
SagaCollaboration owns collaboration implementation.
SagaShared owns neutral collaboration contracts.
Saga owns collaboration product entry points.
SagaEditor owns collaboration UI only.
Forge consumes collaboration state for build/publish gates.
Authoring profiles are not permissions.
Claims are soft coordination signals.
Locks are hard mutation guards.
Conflicts must be resource-specific.
Graph, script, asset, SDE, build, package, and publish resources must be collaboration-aware.
Unresolved blocking conflicts and critical locks can block publish.
Offline/unverified team state cannot be treated as publish-clean.
Collaboration must protect project state, not merely show presence.
```

The collaboration layer should answer:

```txt
Who is here?
Who can do what?
Who is editing what?
Which resources are claimed or locked?
Which changes conflict?
Can this project safely build or publish right now?
```

If it cannot answer those questions, it is not collaboration infrastructure.

It is a multiplayer cursor demo with ambition.
