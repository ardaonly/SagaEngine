# SagaEngine Small-Team Alpha Execution Roadmap

## Document Status

**Status:** Product execution roadmap / Phase 66–95 small-team alpha plan
**Target:** Target 2 / Small-Team Alpha — Small-Team Alpha
**Phase range:** Phase 66 through Phase 95
**Intended path:** `docs/roadmaps/SAGAENGINE_SMALL_TEAM_ALPHA_ROADMAP.md`
**Precondition:** Target 1 / Technical Preview — `SagaEngine 0.1 Technical Preview` / Phase 6–65 has been accepted or explicitly accepted with documented residual debt.
**Primary goal:** Move SagaEngine from a credible technical preview into a small-team usable alpha where a small technical team can build and iterate on a tiny multiplayer project without constantly bypassing Saga’s editor, tools, packaging, diagnostics, and collaboration workflow.

---

# 1. Executive Summary

Target 1 / Technical Preview proves the product spine:

```text
project -> script -> visual projection -> local preview -> diagnostics -> package -> evidence
```

Target 2 / Small-Team Alpha must make that spine usable by a small team.

The target claim after Phase 95 is:

```text
A small technical team can use Saga to build a tiny multiplayer project with
Simple/Pro/C# views, diagnostics, package proof, persistent presence/locks,
reviewable changes, basic roles, and team-oriented workflows.
```

This is still not enterprise. It is still not product beta. It is a **small-team alpha candidate**.

The core shift from Target 1 / Technical Preview to Target 2 / Small-Team Alpha:

```text
Target 1 / Technical Preview: Can Saga prove the end-to-end product spine?
Target 2 / Small-Team Alpha: Can a small team actually use that spine without constantly bypassing it?
```

---

# 2. Source Architecture Inputs

This roadmap follows these architecture rules:

```text
One project, many abstraction levels.
C# source remains canonical.
Blocks are source-preserving projections.
Simple view must not lie.
Runtime hot paths remain native C++.
Tools communicate through versioned artifacts.
Editor views are personal.
The project model is shared.
Collaboration uses semantic transactions, not UI gestures.
Claims must map to evidence.
```

Primary input documents:

```text
SAGA_MASTER_PRODUCT_EXECUTION_PLAN.md
saga_post_diagnostics_product_mvp_roadmap.md
saga_csharp_visual_blocks_architecture.md
saga_collaborative_workspace_architecture.md
SAGA_AUXILIARY_TOOLCHAIN_ARCHITECTURE.md
SAGAENGINE_0_1_TECHNICAL_PREVIEW_ROADMAP.md
```

---

# 3. Target 2 / Small-Team Alpha Product Claim Level

Allowed claim after successful Phase 95:

```text
SagaEngine Small-Team Alpha Candidate:
A small technical team can use Saga to iterate on a tiny multiplayer project with
real project validation, local preview, C# behavior authoring, source-preserving block
edits for supported regions, diagnostics visibility, package/publish proof, persistent
workspace state, artifact locks, comments, reviewable changes, and basic role checks.
```

Forbidden claims:

```text
product beta
release candidate
enterprise-ready
production MMO-ready
secure collaboration platform
full cloud workspace
full project slicing
full source redaction
arbitrary C# to blocks
full visual scripting
full pro graph editor
full asset pipeline
full multiplayer scale
Unity/Unreal replacement
Roblox Studio replacement
```

---

# 4. Small-Team Alpha Exit Criteria

Target 2 / Small-Team Alpha is accepted only if a small technical team can do the following without bypassing Saga for the core flow:

```text
1. Open MultiplayerSandbox.
2. See project health, diagnostics, package status, and team presence.
3. Edit a simple supported behavior through blocks.
4. Inspect and patch the same behavior through C# source/diff views.
5. Use more than one built-in gameplay node.
6. Validate runtime bindings and node metadata.
7. Run local server/client preview.
8. Package and publish-check the project.
9. Use persistent locks and comments to coordinate.
10. Request/review a change.
11. See Team Room activity/review queue.
12. Produce an alpha acceptance report.
```

The alpha can still be rough. It cannot be fake.

---

# 5. Stage Overview

```text
Block A — Alpha Opening and Evidence Discipline       Phase 66–68
Block B — C# / Blocks Authoring Expansion             Phase 69–75
Block C — Editor Workflow Usability                   Phase 76–81
Block D — Asset / Package / Launch / Performance      Phase 82–86
Block E — Collaboration Persistence and Review        Phase 87–92
Block F — Alpha Acceptance and Closure                Phase 93–95
```

---

# 6. Execution Rules

Each phase must close with:

```text
Implemented:
Evidence:
Tests:
Reports/artifacts:
Docs:
Non-goals preserved:
Known debt:
Claim level:
Next phase:
```

Every new feature must help one of these:

```text
create
run
debug
package
collaborate on
review
or verify MultiplayerSandbox
```

A feature that does not support the product spine should be deferred.

---

# Block A — Alpha Opening and Evidence Discipline

## Phase 66 — Small-Team Alpha Opening Checkpoint

**Status:** Implemented as Target 2 / Small-Team Alpha opening evidence discipline on 2026-05-31.
This does not start Phase 69 or Target 3 / Enterprise-Evolvable Foundation.

### Purpose

Open Target 2 / Small-Team Alpha only after the technical preview is accepted or explicitly accepted with documented residual debt.

### Deliverables

```text
docs/architecture/SMALL_TEAM_ALPHA_OPENING_CHECKPOINT.md
Phase 65 closure summary
allowed alpha claims
blocked alpha claims
residual technical preview debt
alpha scope freeze
```

### Tests / Verification

```text
DocGuard verifies no beta/enterprise claims
Technical Preview acceptance evidence referenced
critical blockers classified
```

### Non-Goals

```text
new editor features
new scripting features
new collaboration implementation
enterprise policy
```

### Exit Criteria

The project has a clear decision that small-team alpha work can begin.

### 2026-05-31 Evidence

`Tools/SagaAlphaGate` adds `opening-check`, which references Phase 65
Technical Preview closure evidence and writes
`small_team_alpha_opening_report.json`. The report records allowed alpha
claims, blocked claims, residual Technical Preview debt, and deterministic
diagnostics.

---

## Phase 67 — Alpha Acceptance Test Plan

**Status:** Implemented as an acceptance scenario matrix on 2026-05-31.

### Purpose

Define the executable acceptance path for Target 2 / Small-Team Alpha.

### Deliverables

```text
docs/testing/SAGA_SMALL_TEAM_ALPHA_ACCEPTANCE_TEST_PLAN.md
acceptance scenario matrix
required reports
required tools
required editor workflows
manual vs automated classification
```

### Acceptance Scenario

```text
two or three simulated users
MultiplayerSandbox project
simple block edit
C# diff review
local preview
diagnostics summary
package/publish-check
persistent lock
comment
review request
Team Room status
```

### Tests / Verification

```text
docs claim scan
acceptance script placeholders or test targets inventoried
```

### Non-Goals

```text
full automation of every UI step
enterprise acceptance
release-candidate gate
```

### Exit Criteria

The alpha has a concrete pass/fail definition before more features are added.

### 2026-05-31 Evidence

`docs/testing/SAGA_SMALL_TEAM_ALPHA_ACCEPTANCE_TEST_PLAN.md` defines the
Small-Team Alpha scenario matrix, required reports, and manual/automated/
later-phase classifications. `sagaalphagate acceptance-plan` emits the same
matrix as deterministic JSON evidence.

---

## Phase 68 — Performance Budgets and Usability Budgets v0

**Status:** Implemented as v0 budget definitions and deterministic reporting on
2026-05-31.

### Purpose

Prevent alpha expansion from becoming slow, unstable, or vague.

### Deliverables

```text
docs/architecture/SAGA_PERFORMANCE_BUDGETS.md
editor open budget
project validation budget
script analysis budget
preview launch budget
package check budget
diagnostics summary budget
collaboration update budget
```

### Reports

```text
performance_budget_report.json
or budget checks inside existing diagnostic/publish reports
```

### Tests

```text
budget report emits deterministic fields
missing measurement is explicit, not silently passed
no production performance claim
```

### Non-Goals

```text
full profiler
benchmark suite
load/performance readiness
```

### Exit Criteria

The alpha has initial budgets and can tell when a workflow regresses.

### 2026-05-31 Evidence

`docs/architecture/SAGA_PERFORMANCE_BUDGETS.md` defines initial workflow
budgets and report semantics. `sagaalphagate budget-report` writes
`performance_budget_report.json`, reports missing measurements explicitly, and
emits deterministic budget violation diagnostics without claiming production
performance readiness.

---

# Block B — C# / Blocks Authoring Expansion

## Policy Mini-Block Before Phase 69

**Status:** Implemented as policy-only evidence on 2026-05-31. This does not
start Phase 69.

Before Block B implementation starts, six architecture policies define the
safe boundary for node truth, source patch ownership, editor UI, client
preview, generated artifacts, and JSON artifact envelopes:

```text
docs/architecture/SAGA_GAMEPLAY_NODE_LIBRARY_V1.md
docs/architecture/SOURCE_PATCH_APPLY_POLICY.md
docs/architecture/EDITOR_UI_IMPLEMENTATION_STRATEGY.md
docs/architecture/CLIENT_PREVIEW_AND_RUNTIME_UI_STRATEGY.md
docs/architecture/SAGA_GENERATED_FILES_POLICY.md
docs/architecture/SAGA_ARTIFACT_CONTRACTS_V1.md
```

DocGuard requires these files before Phase 69-75 evidence can pass.

## Phase 69 — C# Node Library Extraction v1

**Status:** Implemented as metadata-only SagaScript extraction evidence on
2026-05-31.

### Purpose

Allow selected C# APIs to appear as visual nodes through metadata.

### Deliverables

```text
[SagaLibrary]
[SagaNode]
node_metadata.json
input/output port extraction
node category metadata
node diagnostics
```

### Tests

```text
valid C# method becomes node metadata
invalid signature fails clearly
node metadata output deterministic
unsupported API appears as diagnostic
```

### Non-Goals

```text
dynamic plugin system
marketplace
arbitrary reflection-heavy node extraction
runtime graph interpretation
```

### Exit Criteria

Built-in C# APIs can become visual nodes without hand-written block definitions.

### 2026-05-31 Evidence

`sagascript extract-nodes` emits `node_library_report.json` from
`[SagaLibrary]` and `[SagaNode]` metadata with stable ids, source spans, api
domain, api level, capability, deterministic diagnostics, and source hashes.

---

## Phase 70 — Built-In Gameplay Node Pack v1

**Status:** Implemented as tool-local metadata fixtures on 2026-05-31.

### Purpose

Add enough built-in nodes for a tiny real gameplay loop.

### Deliverables

```text
Inventory.Has
Door.Open
Score.Add
Audio.PlayEvent
Entity.SetTag or equivalent
```

### Tests

```text
node metadata generated
projection uses nodes
invalid node config fails
runtime binding metadata valid
```

### Non-Goals

```text
complete gameplay framework
inventory system production readiness
audio engine feature set
```

### Exit Criteria

A designer can build a pickup/door/reward behavior with more than one useful node.

### 2026-05-31 Evidence

Tool-local Gameplay.High node fixtures cover `Inventory.Has`, `Door.Open`,
`Score.Add`, `Audio.PlayEvent`, and `Entity.SetTag` as `ProjectionOnly`
metadata. They are not runtime behavior proof.

---

## Phase 71 — Trigger / Spawn / Timer Node Pack v1

**Status:** Implemented as deferred/read-only metadata and projection evidence
on 2026-05-31.

### Purpose

Add a second small node set for scenario gameplay.

### Deliverables

```text
Trigger.OnEnter or equivalent
Spawn.Entity or controlled placeholder
Timer.Delay or deterministic timer node
Destroy/Deactivate entity node if safe
```

### Tests

```text
nodes extract deterministically
unsupported runtime use fails clearly
behavior projection displays nodes
publish gate validates node metadata
```

### Non-Goals

```text
full ECS authoring
production spawning system
complex async/timer framework
```

### Exit Criteria

MultiplayerSandbox can express a tiny gameplay loop beyond door/pickup.

### 2026-05-31 Evidence

Tool-local Gameplay.Low fixtures cover `Trigger.OnEnter`, `Spawn.Entity`,
`Timer.Delay`, and `Entity.DestroyOrDeactivate` as `Deferred` metadata.
Projection and Simple View honesty checks disclose deferred nodes as read-only.

---

## Phase 72 — ReplaceStringLiteral Source Patch Apply v1

### Purpose

Add the first source-changing SagaScript patch path while preserving source
trust.

### Supported Patches

```text
ReplaceStringLiteral only
```

### Deliverables

```text
patch-apply command
required expectedOldText validation
backup and same-directory temp write
exact post-apply byte-diff verification
automatic rollback on failed post-check
stale artifact diagnostics without regeneration
```

### Tests

```text
ReplaceStringLiteral changes only expected span
unsupported edit rejected
formatting outside span preserved
rollback restores original bytes
stale artifacts reported but not regenerated
```

### Non-Goals

```text
method regeneration
class regeneration
arbitrary graph editing
format rewriting
operations beyond ReplaceStringLiteral
editor-side C# writes
artifact regeneration after apply
```

### Exit Criteria

SagaScript can apply one reviewed string-literal edit without breaking
programmer trust.

### 2026-05-31 Evidence

`sagascript patch-apply` applies only `ReplaceStringLiteral` requests with
source hash, `expectedOldText`, editable-policy, exact UTF-8 span, backup,
same-directory temp write, post-apply byte-diff verification, rollback, and
stale-artifact reporting. `patch-preview` remains non-mutating.

---

## Phase 73 — Undo/Redo + Diff Preview for Block Patches

### Purpose

Make the Phase 72 source patch path reviewable and reversible through
SagaScript reports.

### Deliverables

```text
patch-diff command
patch-review command
patch-rollback command from passed apply report
source diff preview
report-only review approval/rejection
strict hash-gated rollback from backup
```

### Tests

```text
diff preview does not mutate source
review approval does not apply source
rollback restores previous bytes
rollback rejects stale current source
rollback rejects mismatched backup
failed rollback restores current bytes
diff preview stable
```

### Non-Goals

```text
full IDE undo stack
redo source mutation
multi-file refactor undo
collaborative merge undo
editor UI
Qt UI
graph editing
```

### Exit Criteria

A user can safely inspect and reverse a supported block edit.

### 2026-05-31 Evidence

`sagascript patch-diff` and `patch-review` are non-mutating report commands.
`sagascript patch-rollback` restores only from a passed Phase 72 apply report
when the current source hash and backup hash match the report, then verifies
the restored hash and exact backup bytes.

---

## Phase 74 — Saga-Compatible C# Profile v2

### Purpose

Make supported/unsupported C# behavior explicit enough for teams.

### Deliverables

```text
supported constructs table
unsupported constructs diagnostics
opaque-region rules
block compatibility report
profileVersion field
```

### Tests

```text
supported if/call/literal projected
unsupported LINQ/lambda/async/pattern classified
opaque node reason stable
Simple View cannot edit unsupported region
```

### Non-Goals

```text
full C# language support
arbitrary conversion
IDE-grade analyzer
```

### Exit Criteria

A team can understand what Saga can and cannot project.

---

## Phase 75 — Runtime Binding + Publish Integration for Node Metadata

### Purpose

Ensure visual-origin and C#-origin behavior metadata is validated before package/publish.

### Deliverables

```text
runtime_bindings.json validation
node_metadata.json validation
publish gate integration
stale metadata detection
missing node binding diagnostics
```

### Tests

```text
valid node metadata passes publish gate
missing node metadata blocks publish
stale binding blocks publish
compiled behavior still runs through compiled C# path
```

### Non-Goals

```text
runtime graph interpreter
hot reload
dynamic plugin loading
```

### Exit Criteria

Authoring artifacts cannot become stale without publish/pipeline detection.

---

# Block C — Editor Workflow Usability

## Phase 76 — Project Browser Improvements

### Purpose

Make navigating MultiplayerSandbox practical.

### Deliverables

```text
project tree
Scenes/Scripts/Assets/Packages/Reports sections
validation status badges
open artifact action
```

### Tests

```text
project browser loads .sagaproj
missing artifact shows diagnostic
open action resolves correct path
no project mutation on browse
```

### Non-Goals

```text
full asset browser
drag/drop import
cloud project browser
```

### Exit Criteria

Users can find the major project artifacts without leaving Saga.

---

## Phase 77 — Behavior Inspector v1

### Purpose

Make behavior state understandable in one editor panel.

### Deliverables

```text
behavior identity
source file path
projection status
runtime binding status
node metadata status
diagnostic list
supported/opaque region summary
```

### Tests

```text
valid behavior shows green status
unsupported code shows opaque status
missing binding shows diagnostic
panel state deterministic
```

### Non-Goals

```text
full graph editor
debugger
runtime variable inspector
```

### Exit Criteria

A user can diagnose a behavior without manually reading five JSON files.

---

## Phase 78 — Diagnostics Panel v1

### Purpose

Surface runtime/tool diagnostics inside the editor.

### Deliverables

```text
diagnostics panel
latest diagnostics_summary.json reader
critical/warning/info grouping
open source/report link
refresh action
```

### Tests

```text
panel reads summary
critical diagnostics shown
missing report handled
refresh deterministic
```

### Non-Goals

```text
full dashboard
remote telemetry
live streaming diagnostics
```

### Exit Criteria

Diagnostics become part of the normal authoring workflow.

---

## Phase 79 — Source Diff / Review Panel v1

### Purpose

Make source-preserving block edits reviewable.

### Deliverables

```text
source diff panel
patch preview display
review status placeholder
apply/cancel hooks
link to transaction/comment if available
```

### Tests

```text
diff panel displays expected changed span
cancel leaves source unchanged
apply updates source
unsupported patch shows diagnostic
```

### Non-Goals

```text
full code editor
merge tool
Git integration
```

### Exit Criteria

Patch edits are visible enough for team trust.

---

## Phase 80 — Scene / Asset Browser v1

### Purpose

Add enough scene/asset navigation for a small project.

### Deliverables

```text
scene list
entity list
asset manifest list
asset status
open/select artifact
basic missing asset diagnostics
```

### Tests

```text
scene list loads
entity list stable
asset manifest parsed
missing asset surfaced
```

### Non-Goals

```text
full level editor
asset import pipeline
material editor
prefabs
```

### Exit Criteria

A user can inspect the project’s small content set inside Saga.

---

## Phase 81 — Simple/Pro/C# View Navigation State

### Purpose

Make multi-view authoring practical.

### Deliverables

```text
view switcher
Simple View -> Pro View -> C# Source link
preserve selected behavior/artifact
view state metadata
```

### Tests

```text
switching views preserves selected behavior
Simple View opaque node links to Pro/C#
C# source view links back to projection
view state does not mutate project truth
```

### Non-Goals

```text
full custom workspace layout system
multi-monitor editor
enterprise role-scoped views
```

### Exit Criteria

A user can move between abstraction levels without losing context.

### Phase 76-81 Evidence Status

Backend-neutral editor workflow models now cover Project Browser Improvements,
Behavior Inspector v1, Diagnostics Panel v1, Source Diff / Review Panel v1,
Scene / Asset Browser v1, and Simple/Pro/C# View Navigation State without Qt
public ABI expansion.

The evidence is model/report state only. C# source remains canonical.
SagaScript owns source mutation. The Editor does not write C# directly and does
not apply or rollback patches directly. `ProjectionOnly` is not runtime proof.
`Deferred` is not runtime proof. Scene/entity browsing remains an inventory
audit and reports `MissingSourceOfTruth` when no accepted scene/entity artifacts
exist.

Focused checks:

```sh
nix-shell --run "cmake --build build/RelWithDebInfo-0.0.9 --target EditorAuthoringSpineTests SagaArchitectureTests -j 1"
nix-shell --run "ctest --test-dir build/RelWithDebInfo-0.0.9 -R 'EditorAuthoringSpineTests|EditorQtPublicAbiBoundaryTests' --output-on-failure -j 1"
```

This status does not claim a full editor MVP, full visual scripting, arbitrary
C# to blocks, editable graph, drag/drop node editing, runtime gameplay, server
gameplay, ClientHost work, Qt panel implementation, asset import, scene editing,
prefab editing, entity placement, or Target 3 / Enterprise-Evolvable Foundation work.

---

# Block D — Asset / Package / Launch / Performance

## Phase 82 — Asset Workflow Validation v1

### Purpose

Make assets safe enough for small-team workflows.

### Deliverables

```text
asset manifest validator improvements
asset identity check
missing/stale asset diagnostics
asset_validation_report.json
```

### Tests

```text
valid assets pass
missing asset fails
duplicate asset id fails
stale asset identity fails
report deterministic
```

### Non-Goals

```text
full importer
asset streaming
editor asset pipeline
```

### Exit Criteria

Small teams can trust package/publish checks to catch obvious asset issues.

### 2026-05-31 Evidence

`Tools/SagaPackager` adds `asset-validate`, which writes
`asset_workflow_report.json` without importing, cooking, or mutating assets. The
report validates `.sagaproj` asset references, duplicate ids, project-relative
paths, asset root existence, placeholder-only roots, and optional generated
package manifest evidence from a supplied stage report.

The current MultiplayerSandbox asset root is placeholder-only. Missing accepted
asset manifest/source truth is reported as `MissingSourceOfTruth`, not as
`Passed`. Scene/entity source truth remains missing and honest.

---

## Phase 83 — Small-Team Package Profiles

### Purpose

Support development, review, and playable package profiles.

### Deliverables

```text
dev package profile
review package profile
playable package profile
profile inheritance if safe
profile diagnostics
```

### Tests

```text
each profile validates
missing profile fails
review profile includes diagnostics
playable profile launch command valid
```

### Non-Goals

```text
commercial release packaging
store upload
patcher/updater
```

### Exit Criteria

Small teams can produce the right package for the right purpose.

### 2026-05-31 Evidence

`Tools/SagaPackager` adds `profile-matrix`, which writes
`package_profile_matrix_report.json` over existing package profiles. The current
real profile remains `technical-preview-server-headless`; it links to
`local-server-headless`, validates through existing package profile rules, and
is stage-supported by the bounded server/headless package path.

Development, review, and playable profiles are not invented in this pass. New
profile claims must remain deferred unless backed by real profile fields and
commands.

---

## Phase 84 — Launch Profile Matrix v1

### Purpose

Make common preview modes reproducible.

### Deliverables

```text
server-only profile
one-client profile
two-client profile
review profile
diagnostics profile
launch_profile_matrix_report.json
```

### Tests

```text
each profile resolves
invalid executable/path fails
timeouts deterministic
diagnostics paths captured
```

### Non-Goals

```text
matchmaking
cloud launch
large multi-client stress
```

### Exit Criteria

A team can run common local workflows without remembering manual command chains.

### 2026-05-31 Evidence

`Tools/SagaLaunchLab` adds `profile-matrix`, which writes
`launch_profile_matrix_report.json`. The existing `local-server-headless`
profile is represented as the bounded server/headless runnable path, with
deterministic missing executable/path diagnostics when checked.

One-client and two-client launch expectations are reported as `Deferred`
because no bounded ClientHost/runtime preview report seam exists. This does not
touch ClientHost, Runtime gameplay, Server gameplay, real transport stress,
multi-client stress, or long preview runs.

---

## Phase 85 — Performance Budget Reports v1

### Purpose

Make alpha workflows measurable.

### Deliverables

```text
project validation timing
script analysis timing
projection timing
launch preview timing
package validation timing
diagnostics summary timing
performance_budget_report.json
```

### Tests

```text
budget report generated
missing measurement explicit
budget violation diagnostic deterministic
no production performance claim
```

### Non-Goals

```text
full profiler
benchmark lab
production performance readiness
```

### Exit Criteria

Small-team workflows have measurable regression signals.

### 2026-05-31 Evidence

`Tools/SagaAlphaGate budget-report` remains the canonical writer for
`performance_budget_report.json`. Partial deterministic measurements can be
consumed from `alpha_budget_measurements.json`; unmeasured workflows remain
`MeasurementMissing`. Deterministic `BudgetExceeded` diagnostics are emitted for
violations.

This evidence does not claim production performance readiness, benchmark-lab
coverage, heavy stress, long soak, raw full CTest, or client-preview-derived
timing.

---

## Phase 86 — MultiplayerSandbox Gameplay Expansion v1

### Purpose

Expand the sample from proof into a tiny game loop.

### Deliverables

```text
pickup
door/trigger
score/reward
simple spawn or reset
two-client/local preview compatibility
diagnostics markers
```

### Tests

```text
gameplay loop triggers
server authoritative state changes
diagnostics record event
packaged proof still launches
```

### Non-Goals

```text
finished game
polished art
AI/combat/inventory system depth
```

### Exit Criteria

MultiplayerSandbox feels like a tiny playable product proof, not only a wiring test.

### 2026-05-31 Evidence

Phase 86 is deferred, not implemented. `Tools/SagaAlphaGate` adds
`gameplay-expansion-blockers`, which writes a blocker report for pickup,
door/trigger, score/reward, spawn/reset, two-client/local preview, and gameplay
diagnostics markers.

The blockers require Runtime gameplay, Server gameplay, a bounded
ClientHost/runtime preview seam, and accepted gameplay source truth. This pass
does not modify gameplay systems, server authority, Runtime/client code,
scripts, scene/entity data, or package/runtime behavior.

---

# Block E — Collaboration Persistence and Review

## Phase 87 — Workspace Persistence v1

### Purpose

Persist collaboration workspace state across sessions.

### Deliverables

```text
workspace_session.json
session resume
user display metadata
active artifact memory
last known presence summary
```

### Tests

```text
session saves
session resumes
stale session handled
project files not corrupted
```

### Non-Goals

```text
cloud workspace
accounts
authentication
enterprise identity
```

### Exit Criteria

Local/team sessions do not vanish immediately after restart.

### 2026-05-31 Evidence

`CollaborationWorkspaceState` records durable local workspace metadata for
workspace id, project id, actors, sessions, active artifact memory, and
diagnostics. Durable state belongs under `.saga/collaboration/`; generated
reports belong under `Build/Collaboration/`.

Focused `CollaborationModelTests` cover deterministic serialization/parsing,
invalid state diagnostics, session resume metadata, and absence of private
machine, credential, account, and OS identity data.

---

## Phase 88 — Persistent Artifact Locks

### Purpose

Make locks usable for team coordination.

### Deliverables

```text
persistent lock records
lock owner
lock reason/comment
lock timeout or stale lock handling
lock conflict diagnostics
```

### Tests

```text
lock persists across session resume
second user blocked
stale lock can be cleared through defined path
lock release deterministic
```

### Non-Goals

```text
enterprise permissions
distributed lock server
automatic merge
```

### Exit Criteria

Small teams can avoid destructive simultaneous edits over time.

### 2026-05-31 Evidence

Persistent artifact locks now model artifact path, behavior id, node id, source
span, patch report, and diagnostic id targets. Locks remain coordination
metadata only. They do not prevent filesystem writes outside the model.

Focused tests cover deterministic acquire/release behavior, second-owner
conflict behavior, stale/expired lock classification, and generated lock report
shape.

---

## Phase 89 — Artifact Comments v1

### Purpose

Allow lightweight discussion on project artifacts.

### Deliverables

```text
comments on behavior/scene/asset
thread id
author id
timestamp/sequence
resolved/unresolved state
comments_report.json
```

### Tests

```text
comment append deterministic
resolve comment deterministic
comments visible in Team Room/Inspector
comments do not mutate semantic artifact unless intended
```

### Non-Goals

```text
full chat
notifications
rich text
cloud sync
```

### Exit Criteria

Teams can leave review/context notes inside Saga.

### 2026-05-31 Evidence

Artifact comments now attach to artifact paths, behavior ids, node ids, source
spans, patch reports, and diagnostic ids. Comment resolve and reopen are
metadata-only transitions.

Focused tests cover deterministic comment ids, target resource ids,
resolve/reopen state, comments report shape, and source preservation.

---

## Phase 90 — Review Request Workflow v1

### Purpose

Turn changes into reviewable units.

### Deliverables

```text
review request
review status
requested reviewers
approve/reject
linked transaction/diff
review_report.json
```

### Tests

```text
review request created
approve/reject deterministic
linked diff visible
rejected review blocks configured publish/check if enabled
```

### Non-Goals

```text
enterprise approval chain
legal compliance
branching system
```

### Exit Criteria

Small teams can request review for behavior/project changes.

### 2026-05-31 Evidence

Review requests now model patch diff/review/apply report references, behavior
or source-span targets, requested reviewers, deterministic status transitions,
and stale source/report state.

`ApprovedMetadataOnly` records review approval as metadata only. It does not
invoke SagaScript, apply a patch, mutate C# source, enable editor apply actions,
or change package/publish behavior.

---

## Phase 91 — Basic Roles and Dangerous Operation Checks

### Purpose

Add lightweight team roles without enterprise policy overbuild.

### Roles

```text
Owner
Programmer
Designer
Artist
QA
```

### Deliverables

```text
role model
dangerous operation list
permission check diagnostics
role_check_report.json
```

### Dangerous Operations Examples

```text
delete scene
delete behavior source
change package profile
override lock
approve publish gate
modify runtime binding metadata manually
```

### Tests

```text
owner can approve dangerous operation
designer blocked from package profile mutation
QA can comment/review but not edit behavior if configured
diagnostics deterministic
```

### Non-Goals

```text
enterprise RBAC
project slices
source redaction
security guarantee
```

### Exit Criteria

The team workflow has basic guardrails for destructive actions.

### 2026-05-31 Evidence

Role assignments and dangerous-operation checks are implemented as local
metadata diagnostics. Supported roles remain `Owner`, `Programmer`, `Designer`,
`Artist`, and `QA`.

Focused tests cover allowed, blocked, unknown actor, and unknown operation
results for the roadmap dangerous-operation list. This is not security
enforcement.

---

## Phase 92 — Team Room Review Queue + Status v1

### Purpose

Upgrade Team Room from awareness panel to team workflow panel.

### Deliverables

```text
review queue
open comments
active locks
recent transactions
diagnostics status
package/publish status
role/dangerous-operation diagnostics
```

### Tests

```text
review queue displays pending request
lock/comment/transaction feed visible
critical diagnostics surfaced
publish status displayed
state deterministic
```

### Non-Goals

```text
full project management
time tracking
enterprise audit UI
```

### Exit Criteria

A team lead can understand current project health and pending review work from one panel.

### 2026-05-31 Evidence

`TeamRoomDashboardView` now summarizes local actors, presence, locks, comments,
review queue entries, metadata transactions, conflicts, diagnostics review
state, dangerous-operation checks, and package/publish placeholders.

The evidence remains backend-neutral model/report state with no Qt dependency.
Block E does not start Phase 93+ or Target 3 / Enterprise-Evolvable Foundation work.

---

# Block F — Alpha Acceptance and Closure

## Phase 93 — Small-Team Alpha Acceptance Script

### Purpose

Turn the alpha scenario into evidence.

### Deliverables

```text
small_team_alpha_acceptance script or test harness
project validation
script analyze/projection/patch
editor workflow smoke or adapter test
local preview
diagnostics summary
package/publish check
collaboration lock/comment/review flow
acceptance_report.json
```

### Tests

```text
acceptance path passes on primary platform
failure path identifies failing stage
artifacts collected
non-claims preserved
```

### Non-Goals

```text
full UI automation if not stable
all-platform guarantee
release candidate
```

### Exit Criteria

The small-team workflow is executable, not just documented.

---

## Phase 94 — Alpha Documentation and Onboarding

### Purpose

Make the alpha usable by a serious early user.

### Deliverables

```text
Small-Team Alpha Quickstart
MultiplayerSandbox team tutorial
C# + Blocks authoring guide
Collaboration guide
Known limitations
Troubleshooting guide
```

### Tests / Verification

```text
docs commands match actual commands
DocGuard non-claim scan passes
broken links/path references checked
```

### Non-Goals

```text
marketing site
commercial docs
enterprise deployment guide
```

### Exit Criteria

A technical user can follow the docs without tribal knowledge.

---

## Phase 95 — Small-Team Alpha Closure Checkpoint

### Purpose

Accept or reject Target 2 / Small-Team Alpha honestly.

### Deliverables

```text
docs/architecture/SMALL_TEAM_ALPHA_CLOSURE_CHECKPOINT.md
implemented matrix
evidence matrix
known debt
blocked claims
Target 3 / Enterprise-Evolvable Foundation opening recommendation
```

### Required Evidence

```text
small_team_alpha_acceptance_report.json
project_validation_report.json
projection_report.json
patch/diff evidence
launch_preview_report.json
diagnostics_summary.json
publish_report.json
workspace_session_report.json
review_report.json
docguard_report.json
```

### Decision Outcomes

```text
Small-Team Alpha accepted
Small-Team Alpha partially proven
Small-Team Alpha rejected / blocked
```

### Non-Goals

```text
enterprise-evolvable claim
product beta claim
release candidate claim
production readiness claim
```

### Exit Criteria

Target 2 / Small-Team Alpha closes with evidence and a clear decision on whether Target 3 / Enterprise-Evolvable Foundation may begin.

### Implementation Evidence

Phase 93 is implemented as `SagaAlphaGate` `accept-alpha`, which reads focused
reports from `Build/SmallTeamAlpha` and emits
`small_team_alpha_acceptance_report.json`.

Phase 94 is implemented as onboarding documentation plus DocGuard-required
evidence for the quickstart, MultiplayerSandbox tutorial, C# + Blocks guide,
collaboration guide, known limitations, and troubleshooting guide.

Phase 95 is implemented as `SagaAlphaGate` `evidence-matrix` and `close-alpha`
reports plus `docs/architecture/SMALL_TEAM_ALPHA_CLOSURE_CHECKPOINT.md`.
Closure remains evidence-backed and keeps missing, blocked, deferred, and
unverified broad-health items visible.

This closure does not claim product beta, release candidate, production
readiness, Runtime gameplay, Server gameplay, ClientHost preview, editor UI,
Qt UI, raw full CTest, enterprise readiness, or Phase 96 work. Target 3 / Enterprise-Evolvable Foundation is not
started by Block F.

---

# 7. Final Target State After Phase 95

After Phase 95, the intended state is:

```text
A small technical team can use Saga to build and iterate on MultiplayerSandbox
through project validation, C# source-preserving authoring, visual block editing
for supported regions, diagnostics visibility, package/publish proof, persistent
workspace coordination, comments, reviewable changes, basic roles, and Team Room
workflow status.
```

Allowed final claim:

```text
SagaEngine Small-Team Alpha Candidate proves that Saga is not only a technical
preview, but a small-team usable, diagnostics-backed, C# + visual authoring workflow
for a tiny multiplayer project.
```

Still forbidden:

```text
product beta
release candidate
enterprise-ready
production MMO-ready
secure collaboration platform
cloud workspace platform
full source redaction
full project slicing
full visual scripting
arbitrary C# roundtrip
Unity/Unreal replacement
Roblox replacement
```

Phase 74-75 note: script compatibility profile and script artifact validation
provide read-only evidence for C# authoring boundaries and optional publish
gating. `ProjectionOnly` and `Deferred` are not runtime proof, and this evidence
does not start runtime node execution, runtime gameplay, server gameplay,
ClientHost work, Editor UI, Qt UI, graph editing, Phase 76+ work, or Target 3 / Enterprise-Evolvable Foundation
work.

---

# 8. Target 3 / Enterprise-Evolvable Foundation Opening Notes

Target 3 / Enterprise-Evolvable Foundation — Enterprise-Evolvable Foundation should not begin until Phase 95 is accepted or partially accepted with clear residual debt.

Likely Target 3 / Enterprise-Evolvable Foundation focus areas:

```text
SagaPolicyKit V1
permissioned project slices
source visibility levels
server-side artifact filtering
restricted presence redaction
audit log
review/approval workflow expansion
publish policy enforcement
workspace server prototype
enterprise-evolvable closure
```

Enterprise must be built from:

```text
project slices
policy enforcement
semantic transactions
audit logs
review workflows
publish gates
```

not from hiding UI panels after unauthorized data has already been sent.
