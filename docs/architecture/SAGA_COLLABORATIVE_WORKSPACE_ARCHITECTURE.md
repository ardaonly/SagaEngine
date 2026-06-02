# SagaEngine Collaborative Workspace Architecture

## Document Status

**Status:** Architecture proposal / product-technical design document
**Scope:** Multi-view editor collaboration, presence/cursor awareness, transaction-based project changes, role-scoped editor views, project slicing, enterprise governance, and collaboration dashboard design
**Primary goal:** Define how multiple users can work on the same Saga project through different editor views without forcing everyone into one person's custom editor, while preserving security, auditability, and long-term enterprise readiness.

This document is not a full implementation roadmap. It defines the intended collaboration model, boundaries, product principles, core systems, risks, and staged proof path.

---

# 1. Executive Summary

SagaEngine should not implement collaboration as shared screen control or as everyone viewing the same editor layout. That model fails once editor customization, role-based workflows, and enterprise access restrictions matter.

The correct model is:

> **Every user has a personal editor view. The project is shared. Changes are submitted as semantic transactions. Presence/cursors show live team awareness. Enterprise mode restricts what each user can see and modify through project slices, permissions, audit logs, and approval workflows.**

In short:

```text
Editor is personal.
Project model is shared.
Changes are transactional.
Visibility is permission-scoped.
Presence is live but non-authoritative.
Enterprise is policy + audit + project slicing on top of the same foundation.
```

This means a user in a Scratch-like simple view, another user in an Unreal-like pro view, another in C# source view, and another in an asset review view can all work on the same game without seeing the same UI.

The shared reality is not the editor UI. The shared reality is the **Saga Project Model**.

---

# 2. Core Product Vision

## 2.1 What Saga Collaboration Should Be

Saga collaboration should feel like a live game production room, not a remote desktop session.

The desired user experience:

- Users can see who is online.
- Users can see where teammates are working.
- Users can see cursors/selections/presence in context when permitted.
- Users can use different editor views based on skill, role, task, or preference.
- A beginner can use a Scratch-like view.
- A programmer can use a C# and pro graph view.
- An artist can use an asset-focused view.
- QA can use a test/report-focused view.
- Enterprise users only receive the slices of the project they are allowed to access.
- All real changes become reviewable, auditable semantic transactions.

## 2.2 What Saga Collaboration Should Not Be

Saga collaboration should not be:

- one user hosting a custom editor layout that everyone else must use;
- pure screen sharing;
- uncontrolled real-time mutation of project files;
- a Google Docs-style free-for-all for every scene, behavior, and asset;
- a security model where the entire project is downloaded to every user and merely hidden in the UI;
- an early attempt at full CRDT/OT for every game artifact;
- enterprise governance mixed directly into the low-level editor loop.

---

# 3. Foundational Principle

## 3.1 Shared Model, Personal Views

The most important rule:

> **Different editor views must not synchronize with each other directly. They must synchronize through the canonical Saga Project Model.**

Wrong model:

```text
Scratch View <-> Pro View <-> Code View
```

Correct model:

```text
Scratch View
Pro View
Code View
Asset View
QA View
Team Room
    ↓
Saga Project Model
```

Each view is a projection of the same project model, filtered by user role, task, permissions, and view capabilities.

## 3.2 Editor Customization Must Not Change Project Truth

Users may customize:

- layout;
- panels;
- themes;
- shortcuts;
- visible tools;
- workflow presets;
- inspector templates;
- view modes;
- local graph layout;
- personal dashboards.

Users must not be able to casually mutate:

- canonical asset identities;
- project manifest schema;
- scene graph integrity rules;
- behavior transaction model;
- runtime binding contract;
- package validation rules;
- permission policy;
- audit log;
- publish gate rules.

The short rule:

```text
UI flexible.
Data contract strict.
```

---

# 4. Collaboration Modes

Saga collaboration should evolve through three modes. These are not separate engines. They are different policy levels on the same architecture.

## 4.1 Mode 1 — Friend Co-op Workspace

Target users:

- small friend groups;
- indie prototype teams;
- classroom/lab usage;
- early community collaboration.

Characteristics:

- 2–4 users expected initially;
- everyone can see everyone else’s presence;
- cursor and selection indicators visible;
- simple asset/behavior/entity locks;
- low security assumptions;
- all users mostly trusted;
- local or lightweight session hosting possible;
- project sharing is convenience-first.

This is the closest to the Roblox Studio-style feeling, but with one crucial difference: users still keep their own editor view.

## 4.2 Mode 2 — Team Workspace

Target users:

- small studios;
- serious indie teams;
- mod teams;
- school teams;
- distributed teams.

Characteristics:

- role-aware workspaces;
- task assignments;
- different editor views per role;
- transaction log;
- artifact locks;
- review/diff support;
- basic permissions;
- project-level diagnostics;
- Team Room dashboard.

This mode introduces structured teamwork without the full complexity of enterprise policy.

## 4.3 Mode 3 — Enterprise Controlled Workspace

Target users:

- larger studios;
- external contractors;
- outsourced asset teams;
- publisher-controlled production;
- confidential projects;
- controlled content pipelines.

Characteristics:

- project slices;
- server-side permission enforcement;
- role-based access control;
- source code redaction;
- audit log;
- approval workflow;
- signed extensions;
- publish gates;
- policy engine;
- least-privilege access;
- restricted presence visibility;
- server-side build/preview options.

Enterprise mode is not a different editor. It is the same collaboration foundation with stronger governance.

---

# 5. Core Architecture Overview

## 5.1 High-Level System

```text
User
  ↓
Identity / Role / Task / Permissions
  ↓
Personal Editor View
  ↓
Allowed Actions
  ↓
Semantic Transaction
  ↓
Policy + Validation
  ↓
Saga Project Model
  ↓
Transaction Log + Audit Log + Diagnostics
  ↓
Runtime / Build / Publish Systems
```

## 5.2 Major Services

```text
Saga Collaborative Workspace
    ├── Identity Service
    ├── Session Service
    ├── Presence Service
    ├── View Capability Service
    ├── Permission / Policy Service
    ├── Project Slice Service
    ├── Transaction Service
    ├── Lock Service
    ├── Validation Service
    ├── Audit Log Service
    ├── Review / Approval Service
    ├── Team Room Dashboard
    └── Publish Gate Integration
```

## 5.3 Local vs Server-Based Deployment

Saga should support different deployment strengths:

| Deployment | Intended Use | Security |
|---|---|---|
| Local session | friends / prototype | low |
| LAN/team host | small team | medium-low |
| Workspace server | serious team | medium |
| Enterprise workspace server | controlled production | high |
| Remote/streamed editor | highest security, expensive | very high |

Early development should not start with remote/streamed editor. It should start with local/team workspace architecture and leave enterprise hooks clean.

---

# 6. Personal Editor Views

## 6.1 Supported View Types

Saga should support multiple editor views over the same project model.

Recommended view families:

| View | Target User | Purpose |
|---|---|---|
| Simple Blocks View | beginner/designer | safe high-level behavior editing |
| Pro Graph View | technical designer/programmer | full behavior graph / advanced authoring |
| C# Code View | programmer | source-preserving code editing |
| Scene View | level designer | world/entity editing |
| Asset View | artist/importer | asset review/import/config |
| QA View | tester | test results, bug markers, diagnostics |
| Build/Publish View | build engineer/lead | package/export/release gates |
| Team Room | all/lead | collaboration overview |

## 6.2 View Capability Manifest

Each view must declare what it can display and edit.

Example:

```json
{
  "viewId": "saga.view.simple_blocks",
  "displayName": "Simple Blocks View",
  "capabilities": {
    "displaySimpleEvents": true,
    "editSimpleEvents": true,
    "displayBranches": true,
    "editBranches": true,
    "displayLoops": "limited",
    "displayAdvancedCSharp": "opaque",
    "editAdvancedCSharp": false,
    "displayNativeExtensions": "summary",
    "editReplicationPolicy": false,
    "editPackageManifest": false
  }
}
```

A Pro View may declare:

```json
{
  "viewId": "saga.view.pro_graph",
  "displayName": "Pro Graph View",
  "capabilities": {
    "displaySimpleEvents": true,
    "editSimpleEvents": true,
    "displayBranches": true,
    "editBranches": true,
    "displayAdvancedCSharp": "sourceLink",
    "editAdvancedCSharp": "openCodeView",
    "displayNativeExtensions": "metadata",
    "editReplicationPolicy": "permissionGated",
    "editPackageManifest": "permissionGated"
  }
}
```

## 6.3 View Projection Rules

A view must not lie.

If a behavior cannot be represented simply, Simple View should show:

```text
Advanced Behavior
Edited in Pro View
Open as read-only summary
```

It must not pretend that an advanced graph is still a simple block if important details are hidden.

Rule:

> **A simplified view may omit detail, but it must not misrepresent behavior.**

---

# 7. Presence System

## 7.1 Purpose

Presence answers:

```text
Who is here?
Where are they working?
What are they looking at?
What are they editing?
What is locked?
```

Presence is live collaboration awareness. It is not project history.

## 7.2 Presence Data

A presence record may include:

```json
{
  "userId": "user_arda",
  "displayName": "Arda",
  "sessionId": "session_001",
  "viewId": "saga.view.pro_graph",
  "workspaceMode": "TeamWorkspace",
  "activeArtifact": "Behaviors/CoinPickup",
  "activeScene": "Scenes/Map_A",
  "selectedObject": "Entity/Coin_04",
  "cursor": {
    "space": "SceneView",
    "x": 0.42,
    "y": 0.77,
    "z": null
  },
  "status": "editing",
  "visibleTo": "policyScoped"
}
```

## 7.3 Cursor Visibility

Cursor visibility should be contextual:

- Scene View: show cursor/selection in world or viewport.
- Behavior View: show cursor/selection over nodes.
- Code View: show caret/selection only if allowed.
- Asset View: show active asset/preview focus.
- Team Room: show summary, not raw cursor movement.

## 7.4 Presence Is Not A Transaction

Cursor movement should not enter project history.

```text
cursor move = ephemeral presence state
behavior edit = transaction
asset import = transaction
lock acquisition = session/control event
publish approval = audit event
```

## 7.5 Enterprise Presence Redaction

In enterprise mode, presence itself can leak information.

Example problem:

```text
User without access sees: Mehmet is editing FinalBossSecretEnding.cs
```

That leaks content.

Correct behavior:

```text
Mehmet is editing a restricted script.
```

or:

```text
Mehmet is working in a restricted area.
```

Presence visibility must be filtered through permission policy.

---

# 8. Semantic Transaction System

## 8.1 Purpose

All meaningful project edits should become semantic transactions.

A transaction describes **what changed in project meaning**, not what UI gesture occurred.

Bad synchronization:

```text
Mouse moved node from x=42 to x=91.
```

Better synchronization:

```text
AddCallNode(Behavior/CoinPickup, Audio.Play, clip=coin)
```

## 8.2 Transaction Structure

A transaction should contain:

```json
{
  "transactionId": "tx_000001",
  "workspaceId": "workspace_anhos",
  "projectId": "project_anhos",
  "authorId": "user_mehmet",
  "sourceView": "saga.view.simple_blocks",
  "targetArtifact": "Behaviors/CoinPickup",
  "operationType": "SetBehaviorParameter",
  "payload": {
    "parameter": "scoreReward",
    "oldValue": 1,
    "newValue": 5
  },
  "baseVersion": "artifact_version_41",
  "resultVersion": "artifact_version_42",
  "validation": {
    "status": "Passed",
    "diagnostics": []
  },
  "timestampUtc": "2026-05-27T00:00:00Z"
}
```

## 8.3 Transaction Categories

Recommended categories:

| Category | Examples |
|---|---|
| Scene transaction | create entity, move entity, set component property |
| Behavior transaction | add node, connect port, set parameter, change event |
| Script transaction | source patch, C# behavior edit, generated binding update |
| Asset transaction | import asset, change asset ref, approve asset |
| Package transaction | manifest update, publish profile update |
| Extension transaction | install extension, enable node library |
| Review transaction | approve, reject, request changes |
| Lock transaction | acquire/release lock |
| Comment transaction | add/resolve comment |

## 8.4 Transactions Must Be Validated

Before applying a transaction:

1. Verify identity.
2. Verify permission.
3. Verify artifact visibility.
4. Verify lock status.
5. Verify schema compatibility.
6. Verify semantic validity.
7. Verify diagnostics threshold.
8. Apply transaction.
9. Append transaction log.
10. Notify subscribed views.

## 8.5 Transaction Broadcast

After a transaction is applied, each client receives it if allowed.

A Simple View may render:

```text
Coin reward changed to 5.
```

A Pro View may render:

```text
Behavior/CoinPickup: SetBehaviorParameter(scoreReward, 5)
```

A Code View may render:

```text
Source patch applied to CoinPickup.cs line 42.
```

A Team Room may render:

```text
Mehmet changed CoinPickup reward: 1 → 5.
```

---

# 9. Locking and Conflict Strategy

## 9.1 Why Not Full CRDT First

Game projects are not plain text documents. They contain:

- scene graphs;
- entity/component state;
- behavior graphs;
- C# source files;
- asset references;
- binary assets;
- package manifests;
- generated artifacts;
- runtime binding metadata.

Attempting full real-time CRDT/OT for every artifact too early would likely consume the project.

V1 should use:

```text
Presence + locks + transactions + validation + conflict reports
```

## 9.2 Lock Levels

Recommended lock levels:

| Lock Level | Example |
|---|---|
| Project | rare, dangerous, migration operations |
| Scene | editing Map_A globally |
| Entity | editing Coin_04 |
| Component | editing Transform of Coin_04 |
| Behavior | editing CoinPickup behavior |
| Node | editing one behavior node |
| Asset | editing Character_Michael.fbx |
| Source region | editing a C# behavior method |
| Package manifest | publish/build config |

Early implementation should start with artifact-level locks:

- scene;
- entity;
- behavior;
- asset;
- source file/source region.

## 9.3 Lock Record

```json
{
  "lockId": "lock_123",
  "artifact": "Behaviors/CoinPickup",
  "lockType": "Behavior",
  "ownerUserId": "user_mehmet",
  "ownerView": "saga.view.simple_blocks",
  "mode": "exclusive_edit",
  "createdAtUtc": "2026-05-27T00:00:00Z",
  "expiresAtUtc": null,
  "reason": "Editing reward logic"
}
```

## 9.4 Lock UX

Users should see:

```text
CoinPickup is being edited by Mehmet.
Open read-only
Request control
Follow user
Comment
```

## 9.5 Conflict Handling

If two users edit the same artifact without valid lock or based on stale version:

- reject second transaction;
- show diff/conflict;
- allow manual rebase;
- never silently merge unsafe changes.

Conflict result examples:

```text
Rejected: CoinPickup changed from version 41 to 42 while you were editing.
Review changes and reapply.
```

---

# 10. Project Slice Model

## 10.1 Purpose

Enterprise users should not receive the entire project if they only need a small part.

Rule:

> **Sensitive content must not be sent to clients that are not allowed to access it. UI hiding is not sufficient security.**

## 10.2 Project Slice Definition

A project slice is a filtered subset of a project.

```text
Full Project
    ↓ permission filter
Project Slice
    ↓ user editor
```

A slice may include:

- selected scenes;
- selected entities;
- selected assets;
- selected behavior graphs;
- selected source regions;
- allowed node libraries;
- allowed build/test reports;
- redacted references;
- placeholder assets;
- proxy metadata.

## 10.3 Slice Example — Level Designer

Allowed:

```text
Scenes/Map_A
Prefabs/Map_A/*
Behaviors/Map_A_Triggers/*
Assets/Environment/Map_A/*
Limited Blocks Library
Task List
Comments
Preview Build Access
```

Hidden:

```text
Server source code
Economy config
Unreleased final boss map
Other teams' maps
Native extensions
Signing keys
Full package manifests
```

## 10.4 Slice Example — External Contractor

Allowed:

```text
Assigned character asset
Material preview scene
Upload/review panel
Comments
Export validation report
```

Hidden:

```text
Full project source
Story scripts
Other character assets
Game economy
Server systems
Publish configs
Internal tools
```

## 10.5 Redaction Rules

When a hidden artifact is referenced by a visible artifact, the user may receive:

```text
RestrictedAssetRef(id=asset_secret_001, displayName="Restricted Asset")
```

or a proxy placeholder.

The system must preserve integrity without leaking sensitive names/content.

---

# 11. Permission and Policy Model

## 11.1 Core Security Rule

Every operation must answer:

```text
Who is the user?
What role do they have?
What task/slice are they assigned?
What artifact do they want to access?
What operation are they attempting?
Is the artifact visible to them?
Is the operation allowed?
Does it require review?
Should the result be redacted?
```

## 11.2 Role Examples

Possible roles:

- Owner;
- Technical Director;
- Programmer;
- Gameplay Designer;
- Level Designer;
- Artist;
- Audio Designer;
- QA Tester;
- Build Engineer;
- External Contractor;
- Viewer.

## 11.3 Permission Examples

Permissions may include:

```text
CanViewScene
CanEditScene
CanViewAsset
CanEditAsset
CanImportAsset
CanViewCSharp
CanEditCSharp
CanUseSimpleBlocks
CanUseProGraph
CanEditServerLogic
CanEditReplicationPolicy
CanInstallExtension
CanApproveChange
CanPublishBuild
CanViewDiagnostics
CanViewAuditLog
```

## 11.4 Policy Examples

```text
Designers cannot edit server-authoritative movement logic.
External contractors cannot access full source code.
Only leads can install native extensions.
Publish requires zero critical diagnostics.
Shipping profile requires approved script changes.
Hot-path scripts cannot allocate per tick.
Economy value changes require lead approval.
Restricted story content is hidden until assigned.
```

## 11.5 Policy Output

Every policy check should produce:

```json
{
  "status": "Allowed",
  "requiresApproval": false,
  "redactions": [],
  "diagnostics": []
}
```

or:

```json
{
  "status": "Denied",
  "reason": "User cannot edit server-authoritative movement logic.",
  "diagnosticCode": "SAGA_POLICY_0042"
}
```

---

# 12. Team Room / Collaboration Dashboard

## 12.1 Purpose

Team Room is the simple collaboration overview editor.

It is not the main game editor. It is a workspace dashboard showing who is doing what, what is locked, what changed, what needs review, and whether the project is healthy.

## 12.2 Team Room Panels

Recommended panels:

- Online users;
- Presence map;
- Active artifacts;
- Locks;
- Tasks;
- Recent changes;
- Review requests;
- Comments;
- Diagnostics summary;
- Build/test status;
- Publish gate status;
- Restricted work summary;
- Activity timeline.

## 12.3 Example Team Room Output

```text
Project: Anhos
Workspace: Main Team Workspace

Online:
    Arda       Pro View       Editing Scripts/DoorLogic.cs
    Mehmet     Simple View    Editing Behaviors/CoinPickup
    Ayşe       Asset View     Reviewing Character_Michael
    Can        QA View        Testing Build #142

Locks:
    CoinPickup.behavior       Mehmet
    Character_Michael.fbx     Ayşe

Recent Changes:
    Mehmet changed CoinPickup reward: 1 → 5
    Arda added C# node: Door.OpenWithKey
    Ayşe replaced coat texture

Warnings:
    2 critical diagnostics
    1 publish gate failure
    3 script changes need review
```

## 12.4 Enterprise Redaction In Team Room

If a user lacks permission, Team Room should show safe summaries:

```text
Mehmet is editing a restricted behavior.
Ayşe is reviewing a restricted asset.
A lead approval is pending.
```

No sensitive artifact names should leak through the dashboard.

---

# 13. Audit Log

## 13.1 Purpose

Audit log records important actions for accountability.

Unlike presence, audit is durable.

## 13.2 Audit Events

Audit should record:

- transaction applied;
- transaction rejected;
- lock acquired/released;
- permission denied;
- project slice issued;
- source file accessed;
- restricted artifact accessed;
- approval granted/rejected;
- extension installed/enabled;
- publish attempted;
- publish passed/failed;
- diagnostics overridden if allowed.

## 13.3 Audit Entry Example

```json
{
  "auditId": "audit_000001",
  "workspaceId": "workspace_anhos",
  "userId": "user_designer_07",
  "action": "SetBehaviorParameter",
  "artifact": "Behaviors/CoinPickup",
  "sourceView": "saga.view.simple_blocks",
  "before": { "scoreReward": 1 },
  "after": { "scoreReward": 5 },
  "policyResult": "Allowed",
  "requiresApproval": false,
  "timestampUtc": "2026-05-27T00:00:00Z"
}
```

## 13.4 Enterprise Audit Requirements

Enterprise audit should be:

- append-only;
- queryable;
- exportable;
- linked to transaction ids;
- linked to publish reports;
- permission-filtered;
- tamper-evident eventually.

Do not attempt cryptographic audit hardening in the first implementation, but do not design against it.

---

# 14. Review and Approval Workflow

## 14.1 Purpose

Some changes should not directly enter production or publishable state.

Examples:

- economy balance change;
- server logic change;
- native extension install;
- publish profile change;
- restricted story content change;
- C# source patch from a non-programmer;
- hot-path behavior modification.

## 14.2 Approval States

```text
Draft
PendingReview
ChangesRequested
Approved
Rejected
Merged/Applied
Published
```

## 14.3 Review Object

```json
{
  "reviewId": "review_001",
  "targetTransactions": ["tx_000101", "tx_000102"],
  "requestedBy": "user_designer_07",
  "reviewers": ["user_lead_01"],
  "status": "PendingReview",
  "diagnostics": [],
  "createdAtUtc": "2026-05-27T00:00:00Z"
}
```

## 14.4 Integration With Publish Gate

Publish gate may require:

```text
No critical diagnostics
All required reviews approved
No stale script bindings
No unapproved native extensions
No restricted content in target package
No unresolved merge conflicts
```

---

# 15. Source Code Visibility

## 15.1 Core Rule

If source code must be protected, it must not be sent to unauthorized clients.

Bad:

```text
Send full source to client.
Hide files in UI.
```

Good:

```text
Server sends only authorized source regions or compiled/proxy interfaces.
```

## 15.2 Source Access Levels

Possible levels:

| Level | Meaning |
|---|---|
| No access | source not sent |
| Metadata only | function names/signatures, no implementation |
| Opaque node access | usable as block, implementation hidden |
| Read-only source | can inspect but not edit |
| Editable source region | can edit assigned regions |
| Full source access | programmer/lead access |

## 15.3 C# + Blocks Interaction

A designer may see:

```text
Open Door With Key
inputs: player, door
```

while the programmer sees:

```csharp
[SagaNode("Doors/Open Door With Key")]
public static void OpenDoorWithKey(Player player, Door door)
{
    if (player.Inventory.Has("key"))
    {
        door.Open();
    }
}
```

This allows visual authoring without leaking implementation.

---

# 16. Relationship To C# ↔ Visual Blocks Architecture

The collaboration model should build on the source-preserving C# ↔ blocks architecture.

Key integration points:

- C# source file remains canonical for code.
- Block view is projection, not separate truth.
- Source patches are transactions.
- Advanced C# can appear as opaque node.
- Permission may allow block usage without source visibility.
- Enterprise can expose node metadata without exposing C# implementation.
- Audit can record visual edits as source patch transactions.

Example:

```text
Designer changes block parameter
    ↓
Source-preserving patch generated
    ↓
Policy checks whether designer can affect that source region
    ↓
Patch becomes transaction
    ↓
Audit records who changed what
```

---

# 17. Data Model Summary

## 17.1 Core Entities

```text
Workspace
Project
User
Role
Permission
ProjectSlice
EditorView
ViewCapabilityManifest
Session
PresenceRecord
Artifact
Transaction
Lock
AuditEvent
ReviewRequest
DiagnosticReport
PublishGateReport
```

## 17.2 Artifact Types

```text
Scene
Entity
Component
Behavior
ScriptSource
Asset
Prefab
PackageManifest
ExtensionManifest
BuildProfile
DiagnosticReport
```

## 17.3 Event Streams

Potential event streams:

```text
presence.events
transaction.events
audit.events
lock.events
review.events
diagnostics.events
publish.events
```

Early implementation can be simpler and in-process. The model should still be event-aware.

---

# 18. Security Model Levels

## 18.1 Level 0 — Local Trust

- full project local;
- no real access control;
- suitable for solo/friend work.

## 18.2 Level 1 — UI Restrictions

- project mostly local;
- editor hides some panels;
- not secure against malicious users;
- suitable only for convenience.

## 18.3 Level 2 — Permissioned Project Slices

- server sends only allowed artifacts;
- source code can be withheld;
- usable enterprise baseline.

## 18.4 Level 3 — Server-Side Build/Preview

- sensitive build steps happen server-side;
- users submit changes but do not receive full build context;
- useful for contractors and confidential projects.

## 18.5 Level 4 — Remote/Streamed Editor

- editor runs remotely;
- user receives only visual stream and sends input;
- highest security but high cost and complexity.

Recommended first serious enterprise target:

```text
Level 2: Permissioned Project Slices
```

---

# 19. MVP Proofs

## 19.1 MVP 1 — Presence and Cursor Proof

Goal:

```text
Multiple users can see each other in the same project without sharing editor layout.
```

Requirements:

- 3 users connect to same workspace;
- each user has local editor view;
- online users visible;
- cursor/selection visible in scene or graph context;
- user color/name shown;
- presence is ephemeral;
- no project mutation from cursor updates.

Exit criteria:

- users can see who is online and where they are working;
- no shared-screen/editor-host assumption exists.

## 19.2 MVP 2 — Transaction Proof

Goal:

```text
A change from one view appears correctly in another view.
```

Scenario:

1. Simple View changes CoinPickup reward from 1 to 5.
2. Transaction is created.
3. Pro View receives transaction.
4. Pro View displays updated behavior.
5. Team Room displays recent change.

Exit criteria:

- transaction log deterministic;
- target artifact version changes;
- both views agree on project state.

## 19.3 MVP 3 — Lock Proof

Goal:

```text
Two users cannot destructively edit the same behavior at once.
```

Requirements:

- lock behavior artifact;
- second user sees read-only state;
- request control option exists;
- lock release updates everyone.

## 19.4 MVP 4 — Multi-View Proof

Goal:

```text
Simple View and Pro View edit the same behavior safely.
```

Scenario:

1. Simple View creates high-level behavior.
2. Pro View sees detailed behavior graph.
3. Pro View adds advanced node.
4. Simple View sees opaque/advanced block.
5. Simple View does not misrepresent advanced logic.

## 19.5 MVP 5 — Project Slice Proof

Goal:

```text
A user only receives authorized artifacts.
```

Scenario:

- User A can access Map_A only.
- User B can access Scripts only.
- User C can access Asset Review only.
- Restricted files are not sent to unauthorized clients.
- Team Room redacts restricted artifact names.

## 19.6 MVP 6 — Audit Proof

Goal:

```text
Every meaningful change is accountable.
```

Requirements:

- audit entries for transactions;
- audit entries for permission denied;
- audit entries for locks;
- audit entries for review approval;
- audit entries linked to publish report.

---

# 20. Suggested Implementation Stages

## Stage 0 — Architecture Decision

Deliverables:

- this architecture document;
- decision that collaboration syncs semantic transactions, not UI state;
- decision that editor views are personal;
- decision that enterprise requires project slicing, not UI hiding.

## Stage 1 — Local Presence Model

Deliverables:

- presence record type;
- in-memory session registry;
- online user list;
- basic cursor/selection events;
- no persistence.

## Stage 2 — Transaction Log Foundation

Deliverables:

- transaction type model;
- artifact version model;
- deterministic transaction append;
- transaction replay for simple artifacts;
- Team Room recent changes feed.

## Stage 3 — Artifact Locking

Deliverables:

- lock model;
- lock acquire/release;
- lock conflict diagnostics;
- read-only state for locked artifacts.

## Stage 4 — Two-View Collaboration Proof

Deliverables:

- Simple Blocks View proof;
- Pro Graph View proof;
- shared behavior artifact;
- cross-view transaction projection;
- opaque advanced block handling.

## Stage 5 — Team Room Dashboard V1

Deliverables:

- online users;
- active artifacts;
- locks;
- recent changes;
- diagnostics summary;
- review queue placeholder.

## Stage 6 — Role and Permission Skeleton

Deliverables:

- user roles;
- permission checks for basic operations;
- denied transaction diagnostics;
- role-scoped view capabilities.

## Stage 7 — Project Slice Prototype

Deliverables:

- artifact visibility filter;
- restricted artifact redaction;
- client receives only allowed artifacts;
- slice diagnostics.

## Stage 8 — Audit Log V1

Deliverables:

- transaction audit;
- denied access audit;
- lock audit;
- publish/report links placeholder;
- exportable audit JSON.

## Stage 9 — Review / Approval V1

Deliverables:

- review request object;
- approve/reject;
- transaction gated by review;
- Team Room review queue.

## Stage 10 — Enterprise Controlled Workspace Prototype

Deliverables:

- role-scoped project slices;
- source visibility levels;
- redacted presence;
- audit log;
- approval workflow;
- publish gate integration.

---

# 21. Major Risks

## 21.1 Shared Editor Misdesign

Risk:

```text
Everyone is forced into one user's editor layout.
```

Consequence:

- custom editor becomes unusable for teams;
- enterprise access control fails;
- users hate the workflow.

Mitigation:

```text
Personal editor views only. Shared project model always.
```

## 21.2 UI Hiding Mistaken For Security

Risk:

```text
Full project is downloaded but hidden in UI.
```

Consequence:

- source/content can leak.

Mitigation:

```text
Permissioned project slices. Unauthorized artifacts not sent.
```

## 21.3 Transaction Model Too Low-Level

Risk:

```text
Transactions describe UI gestures instead of project meaning.
```

Consequence:

- other views cannot interpret changes;
- audit logs become useless.

Mitigation:

```text
Semantic transactions only.
```

## 21.4 Overbuilding Enterprise Too Early

Risk:

```text
SSO, cloud orgs, billing, advanced approvals before collaboration core works.
```

Consequence:

- project stalls.

Mitigation:

```text
Build collaboration core first. Leave enterprise hooks clean.
```

## 21.5 CRDT/OT Rabbit Hole

Risk:

```text
Trying to merge every artifact type in real time like Google Docs.
```

Consequence:

- enormous complexity.

Mitigation:

```text
Use locks + transactions + conflict reports first.
```

---

# 22. Final Model Statement

The final intended model is:

> **Saga Collaborative Workspace is a multi-view, permission-controlled game production workspace where each user keeps a personal editor view, presence shows live collaboration state, real project changes are submitted as semantic transactions, and enterprise control is achieved through project slices, permissions, audit logs, review workflows, and publish gates.**

The key product rule:

```text
Do not share one user's editor.
Share the project model.
```

The key engineering rule:

```text
Do not synchronize UI gestures.
Synchronize semantic project transactions.
```

The key enterprise rule:

```text
Do not merely hide sensitive data in the UI.
Do not send unauthorized data to the client.
```

If Saga follows these rules, collaboration can scale from friend co-op editing to controlled enterprise production without replacing the underlying architecture.
