# SagaEngine 0.1 Technical Preview Execution Roadmap

## Document Status

**Status:** Product execution roadmap / Phase 6–65 technical preview plan
**Target:** Target 1 / Technical Preview — SagaEngine 0.1 Technical Preview
**Phase range:** Phase 6 through Phase 65
**Intended path:** `docs/roadmaps/SAGAENGINE_0_1_TECHNICAL_PREVIEW_ROADMAP.md`
**Primary goal:** Turn SagaEngine from a diagnostics-backed engine foundation into a concrete, distributable technical preview where `MultiplayerSandbox` can be opened, scripted, viewed through C# and visual blocks, launched locally, observed through diagnostics, packaged, and launched outside the editor.

---

# 1. Executive Summary

SagaEngine has moved beyond a pure engine-recovery track. The next target is not “add systems forever.” The next target is a **smaller complete product proof**.

The 0.1 Technical Preview must prove this claim:

```text
Saga can create, validate, launch, diagnose, script, visually inspect/edit, package,
and lightly collaborate on one small multiplayer project through its own workflow.
```

The technical preview is not beta status, not an enterprise platform, and not
an Unreal/Unity competitor. It is a serious first product artifact that
demonstrates the core Saga identity:

```text
One project.
Many abstraction levels.
Real C#.
Source-preserving visual projection.
Real server/client runtime path.
Real diagnostics.
Real package proof.
Basic local collaboration.
Evidence-backed claims.
```

The target demo project is:

```text
MultiplayerSandbox Product Proof
```

---

# 2. Source Architecture Inputs

This roadmap consolidates the execution pattern from the following architecture documents:

```text
sagaengine_diagnostics_migration_spec.md
saga_post_diagnostics_product_mvp_roadmap.md
SAGA_MASTER_PRODUCT_EXECUTION_PLAN.md
SAGA_AUXILIARY_TOOLCHAIN_ARCHITECTURE.md
saga_csharp_visual_blocks_architecture.md
saga_collaborative_workspace_architecture.md
```

The source documents define these hard rules:

```text
Diagnostics must produce runtime reliability evidence.
Tools must produce versioned artifacts.
C# source is canonical.
Visual blocks are projections, not replacement source.
No visual edit means no source mutation.
Simple view must not lie.
Editor views are personal.
Project model is shared.
Collaboration uses semantic transactions, not UI gestures.
Every claim must map to evidence.
```

---

# 3. Current Starting Point

This roadmap assumes Phases 0–5 of the diagnostics track are complete or accepted with recorded evidence.

Current completed diagnostics foundation:

```text
Phase 0 — Diagnostics migration audit
Phase 1 — SagaDiagnostics foundation consolidation
Phase 2 — Operational reports + optional ZoneServer observability
Phase 3 — Crash-context/reliability reports + health rules
Phase 4 — Safe SagaStressArena v1 + bounded report artifact proof
Phase 5 — Memory/resource snapshot foundation + StressArena integration
```

Current non-claims that remain active:

```text
No production MMO readiness.
No complete raw CTest health claim.
No real OS crash safety.
No real network BotClient swarm.
No full NetworkChaos yet.
No full StateValidation yet.
No FaultBoundary yet.
No beta product status.
No enterprise readiness.
No complete visual scripting.
No arbitrary C# roundtrip.
```

---

# 4. Technical Preview Claim Level

The target claim for Phase 65 is:

```text
SagaEngine 0.1 Technical Preview
```

Allowed claim:

```text
SagaEngine can open and validate a MultiplayerSandbox project, run a local server/client
preview, validate and summarize diagnostics, analyze and minimally patch Saga-compatible C#
behavior source through a source-preserving visual projection, show basic Simple/Pro/C# views,
coordinate basic local presence/locks/transactions, package the project, and launch a packaged proof.
```

Forbidden claims:

```text
beta product status
release-candidate status
production MMO readiness
enterprise collaboration readiness
arbitrary C# to visual blocks
complete visual scripting
Unity/Unreal replacement
Roblox Studio replacement
cloud workspace platform
full security model
full collaboration
MMO-scale proof
```

---

# 5. Execution Model

Each phase must close with evidence.

Every phase must include:

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

A phase cannot close with:

```text
probably works
seems done
mostly implemented
should be fine
```

The default verification pattern:

```text
git diff --check
touched-file trailing whitespace scan
focused target build
focused CTest/test run
relevant regression tests
architecture/boundary tests when dependencies change
docs rg checks for claim/non-claim wording
```

---

# 6. Phase Overview

```text
Block A — Diagnostics Closure                        Phase 6–12
Block B — Product Spine / Project Truth              Phase 13–19
Block C — Playable Vertical Slice                    Phase 20–25
Block D — Diagnostics Visibility Product Layer       Phase 26–28
Block E — Packaging / Publish Proof                  Phase 29–33
Block F — C# Scripting / SagaWeaver MVP              Phase 34–43
Block G — Editor Authoring Spine                     Phase 44–49
Block H — Collaboration MVP                          Phase 50–56
Block I — View Capability / Product Honesty          Phase 57–59
Block J — Technical Preview Candidate / Distribution Phase 60–65
```

---

# Block A — Diagnostics Closure

## Phase 6 — NetworkChaos Policy Foundation

**Status:** Implemented as deterministic local/direct policy foundation evidence
on 2026-05-31. This does not start Phase 7.

### Purpose

Add deterministic NetworkChaos policy contracts without claiming real network chaos.

### Deliverables

```text
NetworkChaosConfig
NetworkChaosDecision
NetworkChaosLayer
deterministic seed support
drop / duplicate / defer decisions
bounded defer queue
net.chaos.* metrics
direct_zone_packet_chaos_smoke scenario if safe
NetworkChaosLayerTests
```

### Tests

```text
disabled policy delivers all frames
drop policy deterministic
duplicate-once deterministic
defer releases deterministically if implemented
queue cap prevents unbounded growth
metrics emitted through diagnostics
SagaStressArena chaos smoke report contains net.chaos.* metrics
```

### Non-Goals

```text
real socket-level packet manipulation
OS network shaping
real network BotClient
MMO-scale network stress
unbounded queues
sleep-based latency in tests
```

### Exit Criteria

A direct/local packet chaos smoke scenario produces reports with deterministic chaos metrics.

### Evidence

```text
NetworkChaosLayerTests
SagaStressArenaTests direct_zone_packet_chaos_smoke
docs/architecture/NETWORK_CHAOS_POLICY_FOUNDATION.md
```

---

## Phase 7 — SagaChaosLab v1

**Status:** Implemented as bounded direct/local profile-runner evidence on
2026-05-31. This does not start Phase 8.

### Purpose

Create a tool-level chaos profile runner around the deterministic NetworkChaos policy.

### Deliverables

```text
Tools/SagaChaosLab
chaos_profile.json schema
chaos_report.json
profile validation
direct harness integration with SagaStressArena
safe smoke defaults
manual-only heavier profiles
```

### Tests

```text
valid chaos profile loads
invalid profile fails deterministically
direct chaos smoke emits chaos_report.json
profile cannot request unbounded delay/queue
report fields deterministic
```

### Non-Goals

```text
distributed chaos cloud
OS-level network shaping
real internet condition emulator
production networking readiness
```

### Exit Criteria

A bounded chaos profile can be run locally and summarized as a machine-readable report.

### Evidence

```text
SagaChaosLabTests
SagaStressArenaTests direct_zone_packet_chaos_smoke
NetworkChaosLayerTests
docs/architecture/SAGACHAOSLAB_V1.md
```

---

## Phase 8 — Server Lifecycle Diagnostics Expansion

### Purpose

Extend diagnostics beyond ZoneServer metrics into server lifecycle and logical ownership events.

### Deliverables

```text
ServerStarted
ServerShutdownRequested
SessionConnected
SessionDisconnected
EntityCreated
EntityDestroyed
TickSlow
LifetimeLeakDetected
session/connection/entity lifetime records
server lifecycle report section
focused server lifecycle diagnostics tests
```

### Tests

```text
server start/shutdown emits metrics/log events
session connect/disconnect marks lifetime records
entity create/destroy marks lifetime records
disconnected session leak is reported
diagnostics absent preserves existing behavior
```

### Non-Goals

```text
broad server rewrite
production account/session service
full replication observer lifecycle if replication is not ready
```

### Exit Criteria

A server lifecycle smoke test proves that core lifecycle events are observable and reportable.

---

## Phase 9 — Real Transport Stress Smoke

**Status:** Deferred with evidence as of 2026-05-31. The server TCP seam exists,
but a safe smoke needs an official test client or transport adapter first.

### Purpose

Move SagaStressArena from direct harness proof toward bounded real transport smoke if safe.

### Deliverables

```text
transport_smoke tier
fake/bot client only if real transport path is ready
connect/send/disconnect bounded scenario
stress_transport_smoke_report.json
transport diagnostics counters
```

### Tests

```text
client connects to local server
movement/input is sent
disconnect is observed
diagnostics report includes transport events
timeout/cap prevents machine overload
```

### Non-Goals

```text
bot swarm
100/500/1000 client claims
load benchmark
distributed stress
unbounded reconnect storm
```

### Exit Criteria

A bounded real transport smoke exists, or the phase explicitly defers it with evidence and keeps the direct harness as the accepted proof.

### Current Decision

Phase 9 is deferred with evidence. The direct/local harness remains the accepted
proof until an official client/test transport seam owns handshake response and
server-frame writing. A raw protocol-mirroring TCP mini-client is out of scope.

---

## Phase 10 — SagaStateCheck Foundation

**Status:** Implemented as a standalone deterministic state validation
foundation as of 2026-05-31. Live authoritative movement snapshot integration
is deferred until a safe read-only enumerator exists.

### Purpose

Start server state validation without overclaiming full replication correctness.

### Deliverables

```text
Tools/SagaStateCheck or Server validation module
StateSnapshot contract
EntitySnapshot contract
EntityChecksum
DesyncReport
state_check_report.json
documented future authoritative movement snapshot seam
```

### Tests

```text
matching snapshots pass
mismatched entity position fails
missing entity fails
ownership mismatch fails
desync report deterministic
state_check_report.json deterministic
```

### Non-Goals

```text
full replication validation
anti-cheat system
production desync recovery
large-world validation
```

### Exit Criteria

A small deterministic server state mismatch produces a machine-readable desync report.

---

## Phase 11 — FaultBoundary Audit + Minimal Runtime Fault Contract

**Status:** Implemented as minimal classification/reporting as of 2026-05-31.
Recommended recovery actions are reported, not executed.

### Purpose

Define and implement a minimal fault boundary contract only where real subsystem boundaries exist.

### Deliverables

```text
FaultBoundary
FaultPolicy
RecoveryAction
FaultReport
Runtime/Server fault architecture note
minimal tests for policy classification
diagnostics fault report section
```

### Tests

```text
fault policy classifies recoverable/unrecoverable faults
diagnostics receives fault event
recovery action is reported, not over-executed
no subsystem fake recovery claim
operational report output deterministic
```

### Non-Goals

```text
full fault-isolation framework
chat/inventory/combat recovery if those subsystems do not exist
production fault tolerance
```

### Exit Criteria

FaultBoundary v0 exists as a tested contract and does not claim production recovery.

---

## Phase 12 — Diagnostics Closure Checkpoint

**Status:** Implemented as a documentation checkpoint as of 2026-05-31.
Block A closes only as diagnostics foundation for Technical Preview product
work.

### Purpose

Close the diagnostics track as a foundation for Technical Preview product work.

### Deliverables

```text
docs/architecture/POST_DIAGNOSTICS_PRODUCT_OPENING_CHECKPOINT.md
diagnostics implemented/deferred matrix
allowed claims
blocked claims
next product phase decision
```

### Tests / Verification

```text
diagnostics focused tests pass
stress/chaos/state tests pass or deferred with evidence
docs non-claims verified
architecture tests pass
```

### Non-Goals

```text
product launch
editor MVP
enterprise readiness
complete raw CTest claim unless actually run and passed
```

### Exit Criteria

The project has a clear, evidence-backed decision that Product Spine / Project
Truth planning can begin without claiming production readiness.

---

# Block B — Product Spine / Project Truth

## Block B Phase 13-19 Status

Phase 13-19 establish the Technical Preview project truth spine:

```text
docs/internal/product-history/SAGA_PRODUCT_DEFINITION.md
docs/internal/product-history/SAGA_PROJECT_MODEL_INVENTORY_PHASE14.md
docs/product/SAGAPROJ_SCHEMA_V0.md
Tools/SagaProjectKit
samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj
```

Accepted evidence for this block is focused:

```text
dotnet build Tools/SagaProjectKit/SagaProjectKit.csproj --configuration Release --nologo --verbosity minimal -m:1
python3 Tools/SagaProjectKit/tests/test_sagaprojectkit_cli.py
```

This status means `.sagaproj` project truth, validate/resolve/doctor reports,
and the MultiplayerSandbox fixture exist. It does not start Phase 20, launch
runtime/server processes, stage packages, publish packages, integrate the
editor, or replace `saga.project.json`.

## Phase 13 — Product Opening Checkpoint

### Purpose

Translate diagnostics closure into a product execution opening.

### Deliverables

```text
SAGA_PRODUCT_DEFINITION.md or update existing product docs
Technical Preview claim level
MultiplayerSandbox chosen as first product proof
artifact contracts inventory
repo path decisions
```

### Tests

```text
docs claim scan passes
required source docs referenced
no beta/enterprise overclaim
```

### Non-Goals

```text
new runtime features
editor features
packaging implementation
```

### Exit Criteria

Product work begins from a frozen, explicit claim level.

---

## Phase 14 — Saga Project Model Inventory

### Purpose

Audit existing project/package/runtime/editor metadata before introducing `.sagaproj`.

### Deliverables

```text
project model inventory doc
existing manifest/schema map
existing package profile map
required MVP fields list
migration/debt list
```

### Tests

```text
docs rg checks
no code unless inventory reveals trivial boundary guard need
```

### Non-Goals

```text
new schema implementation
editor rewrite
asset database
```

### Exit Criteria

The exact MVP project model is known before implementation.

---

## Phase 15 — `.sagaproj` Schema v0

### Purpose

Create the canonical Saga project manifest.

### Deliverables

```text
.sagaproj schema v0
SagaProjectDescriptor
SagaProjectManifest loader
schemaVersion handling
projectId
engine compatibility
scene references
asset references
script folder references
launch profile references
package profile references
diagnostics folder
```

### Tests

```text
valid manifest loads
missing required fields fail
unknown schema version fails
paths remain project-relative
project open does not mutate files
```

### Non-Goals

```text
full scene editor
cloud workspace
enterprise permissions
asset database
full scripting system
```

### Exit Criteria

A sample `.sagaproj` can be loaded and validated deterministically.

---

## Phase 16 — SagaProjectKit v1: Validate

### Purpose

Add the first product tool for project validation.

### Deliverables

```text
Tools/SagaProjectKit
sagaproject validate
project_validation_report.json
shared artifact envelope
consistent diagnostics format
```

### Tests

```text
valid project report Passed
invalid project report Failed
diagnostics include code/severity/category/path
exit codes deterministic
```

### Non-Goals

```text
GUI project wizard
cloud project registry
full editor integration
```

### Exit Criteria

`MultiplayerSandbox.sagaproj` can be validated by a standalone tool.

---

## Phase 17 — SagaProjectKit v1: Resolve

### Purpose

Resolve project-relative paths into a deterministic product view.

### Deliverables

```text
sagaproject resolve
project_resolution.json
script/assets/package/diagnostics/launch path resolution
stable output ordering
```

### Tests

```text
relative paths resolve deterministically
missing path diagnostics clear
output is stable across runs
does not mutate project files
```

### Non-Goals

```text
file generation
package staging
editor opening
```

### Exit Criteria

Other tools can consume one canonical project resolution artifact.

---

## Phase 18 — SagaProjectKit v1: Doctor

### Purpose

Add project health checks.

### Deliverables

```text
sagaproject doctor
project_doctor_report.json
missing folder/file checks
compatibility warnings
diagnostics directory check
```

### Tests

```text
missing Scripts folder warns/errors as configured
missing launch profile detected
missing package profile detected
report stable
```

### Non-Goals

```text
automatic project repair
template generator unless trivial
```

### Exit Criteria

A broken demo project produces useful repair diagnostics without mutation.

---

## Phase 19 — MultiplayerSandbox Project Manifest

### Purpose

Create the canonical sample project used by the entire MVP.

### Deliverables

```text
samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj
launch_profiles.json
package_profiles.json
Scripts/ folder
Assets/ or manifest references
Diagnostics output folder convention
README for sample path
```

### Tests

```text
SagaProjectKit validate passes
resolve passes
doctor passes or emits only expected warnings
```

### Non-Goals

```text
finished game
art content
full scene editing
```

### Exit Criteria

All future MVP phases have a real project target.

---

# Block C — Playable Vertical Slice

## Block C Phase 20-25 Status

Phase 20-25 establish the first bounded MultiplayerSandbox preview path:

```text
Tools/MultiplayerSandboxHeadless
Tools/SagaLaunchLab
samples/MultiplayerSandbox/launch_profiles.json
docs/internal/product-history/MULTIPLAYER_SANDBOX_PLAYABLE_VERTICAL_SLICE_PHASE20_25.md
docs/architecture/RUNTIME_SERVER_LAUNCH_CONTRACT_PHASE21.md
```

Accepted evidence for this block is focused:

```text
MultiplayerSandboxHeadlessTests
SagaLaunchLab CLI tests
```

This status means the server-only headless preview and acceptance report path
exist. Runtime client preview and two-client local preview remain deferred
until a bounded ClientHost preview/report seam exists.

## Phase 20 — Headless MultiplayerSandbox v0

### Purpose

Prove MultiplayerSandbox can run without editor dependency.

### Deliverables

```text
headless server/runtime sample path
minimal entities
server-authoritative movement proof
diagnostics report path
focused headless sample test
```

### Tests

```text
server host starts
entity exists
input queues before tick
state mutates only during tick
diagnostics report exists
```

### Non-Goals

```text
visual client
full gameplay
online services
```

### Exit Criteria

The sample project has a server-side gameplay proof.

---

## Phase 21 — Runtime/Server Launch Contract

### Purpose

Standardize launch arguments and diagnostics/report path propagation.

### Deliverables

```text
SagaServer launch contract
SagaRuntime launch contract
project/package path args
diagnostics output args
launch profile arg mapping
startup failure diagnostics
```

### Tests

```text
valid args accepted
missing manifest fails deterministically
diagnostics path passed through
startup report deterministic
```

### Non-Goals

```text
process orchestration tool
editor button integration
installer
```

### Exit Criteria

SagaLaunchLab can depend on stable CLI contracts.

---

## Phase 22 — SagaLaunchLab v1: Server Launch

### Purpose

Create the local preview launcher for server-only runs.

### Deliverables

```text
Tools/SagaLaunchLab
sagalaunch server
launch_preview_report.json
stdout/stderr capture
exit code capture
timeout
clean shutdown
```

### Tests

```text
server launch report emitted
invalid executable path fails
timeout failure deterministic
clean shutdown recorded
```

### Non-Goals

```text
multi-client preview
editor UI
cloud launch
```

### Exit Criteria

A local server preview can be launched reproducibly by a tool.

---

## Phase 23 — SagaLaunchLab v1: Server + Client Preview

### Purpose

Launch one local server and one local runtime client.

### Deliverables

```text
sagalaunch preview
server + one client process
diagnostics report path collection
process log collection
launch_preview_report.json
```

### Tests

```text
server and client start
client exits cleanly or controlled timeout
diagnostics summary paths captured
failure paths deterministic
```

### Non-Goals

```text
two-client gameplay
editor integration
real matchmaking
```

### Exit Criteria

One command starts a minimal local preview.

---

## Phase 24 — Two-Client Local Preview

### Purpose

Prove local multiplayer preview shape.

### Deliverables

```text
local-2-client launch profile
two clients
server session awareness
movement proof if available
clean shutdown report
```

### Tests

```text
two clients start
server observes sessions if supported
diagnostics report includes session/client counts
shutdown clean
```

### Non-Goals

```text
internet networking
account system
lobby/matchmaking
MMO scale
```

### Exit Criteria

The MVP can honestly say it has a bounded local multiplayer preview proof.

---

## Phase 25 — Playable Preview Acceptance Test

### Purpose

Create an end-to-end acceptance test for project → launch → diagnostics.

### Deliverables

```text
MultiplayerSandbox preview acceptance test
project validation
launch profile resolution
server/client preview
diagnostics report existence
acceptance_report.json if useful
```

### Tests

```text
single command/test executes the preview path
failure diagnostics useful
no hidden manual steps
```

### Non-Goals

```text
packaging
visual scripting
collaboration
```

### Exit Criteria

Playable proof is reproducible from the repo.

---

# Block D — Diagnostics Visibility Product Layer

## Phase 26 — SagaProbe v1: Read Diagnostics Report

**Status:** Implemented as read-only SagaProbe summary evidence on
2026-05-31.

### Purpose

Make diagnostics readable by product tools and humans.

### Deliverables

```text
Tools/SagaProbe
sagaprobe summarize
diagnostics_summary.json
human-readable console summary
support operational/reliability/stress reports
```

### Tests

```text
reads operational report
reads reliability report
extracts critical diagnostics
emits deterministic summary
invalid report fails clearly
```

### Non-Goals

```text
full dashboard
Qt UI
telemetry upload
```

### Exit Criteria

A user can summarize diagnostics without opening raw JSON manually.

---

## Phase 27 — SagaProbe v1: Compare Reports

**Status:** Implemented as exact deterministic SagaProbe comparison evidence on
2026-05-31.

### Purpose

Detect regressions between diagnostic report runs.

### Deliverables

```text
sagaprobe compare
report_diff.json
metric diff
new critical diagnostics
missing expected sections
```

### Tests

```text
same report = no diff
changed metric detected
new critical violation detected
missing section detected
stable diff ordering
```

### Non-Goals

```text
statistical performance analysis
long-term metrics database
```

### Exit Criteria

Preview/stress runs can be compared as evidence.

---

## Phase 28 — Editor/CLI Diagnostics Summary Bridge

**Status:** Implemented as CLI-only `sagaprobe latest` bridge evidence on
2026-05-31. Editor shell presentation remains deferred.

### Purpose

Expose latest diagnostics summary through product CLI/editor shell.

### Deliverables

```text
Saga command or editor shell diagnostics summary bridge
latest report discovery
summary display
diagnostics_summary.json link
```

### Tests

```text
latest report selected deterministically
missing reports handled clearly
critical diagnostics surfaced
```

### Non-Goals

```text
full diagnostics viewer
dashboard UI
telemetry upload
```

### Exit Criteria

Diagnostics are visible from product workflow through the CLI bridge, not
buried in files.

---

# Block E — Packaging / Publish Proof

## Phase 29 — Package Profile v0

**Status:** Implemented as MultiplayerSandbox server/headless package profile
v0 evidence on 2026-05-31.

### Purpose

Define package profiles as product artifacts.

### Deliverables

```text
package profile schema
client/server package distinction
package output folder convention
diagnostics output path
profile validation
```

### Tests

```text
valid profile loads
missing required field fails
unknown schema version fails
project references package profile
```

### Non-Goals

```text
installer
patcher
store publishing
```

### Exit Criteria

A package profile can be validated before staging.

---

## Phase 30 — SagaPackager v1: Validate Package

**Status:** Implemented as standalone `sagapack validate` evidence on
2026-05-31.

### Purpose

Validate package inputs.

### Deliverables

```text
Tools/SagaPackager
sagapack validate
package_report.json
package manifest validation
asset manifest validation
script/runtime metadata validation placeholder
```

### Tests

```text
valid demo package passes
missing asset fails
missing script metadata fails if required
report deterministic
```

### Non-Goals

```text
actual staging
compression
installer
```

### Exit Criteria

Package readiness can be checked without launching.

---

## Phase 31 — SagaPackager v1: Stage Package

**Status:** Implemented as deterministic local `sagapack stage` evidence on
2026-05-31.

### Purpose

Create a staged package folder.

### Deliverables

```text
sagapack stage
Build/Packages/MultiplayerSandbox
package_manifest.client.json
package_manifest.server.json
asset/script metadata copy
package_report.json
```

### Tests

```text
staged output deterministic
missing input fails
staging does not mutate source project
package manifests accepted by runtime/server
```

### Non-Goals

```text
compression
signing
patching
commercial publishing
```

### Exit Criteria

The demo has a reproducible staged package.

---

## Phase 32 — SagaPublishGate v1

**Status:** Implemented as local evidence `sagapack publish-check` gate on
2026-05-31.

### Purpose

Gate publish readiness based on evidence.

### Deliverables

```text
sagapublish check or sagapack publish-check
publish_report.json
project validation requirement
package validation requirement
script diagnostics threshold
diagnostics critical failure gate
launch profile requirement
```

### Tests

```text
valid demo passes
missing asset blocks publish
script diagnostic blocks publish
critical diagnostics block publish
report deterministic
```

### Non-Goals

```text
app store upload
commercial release
cloud deploy
```

### Exit Criteria

Saga can block false publish readiness with machine-readable evidence.

---

## Phase 33 — Packaged Launch Smoke

**Status:** Implemented as server/headless packaged smoke evidence on
2026-05-31. Client packaged launch remains deferred.

### Purpose

Prove the staged package launches outside editor.

### Deliverables

```text
packaged server/client launch smoke
launch command generation
package smoke report
diagnostics output
```

### Tests

```text
staged server package launches
staged client package launches or simulated runtime host starts
diagnostics report created
shutdown clean
```

### Non-Goals

```text
installer
multi-platform distribution package
commercial release
```

### Exit Criteria

A packaged demo can be launched outside editor.

---

# Block F — C# Scripting / SagaWeaver MVP

Status: implemented as a source-preserving MVP through `Tools/SagaScript`.
The accepted Block F evidence is the two-axis API metadata model, focused
SagaScript CLI tests, MultiplayerSandbox script fixtures, read-only projection,
source maps, runtime binding metadata, no-edit byte preservation, and
preview-only string literal patch output. The next editor authoring phase has
not started.

Implemented API axes:

```text
domains: Gameplay, Runtime, Server, Networking, UI, Diagnostics, Assets, Packaging
levels: High, Low
```

Implemented Block F projection domains:

```text
Gameplay + High
Gameplay + Low
```

Other domains remain future schema capacity, not implemented product proof.

## Phase 34 — SagaWeaver/SagaScript Inventory

### Purpose

Audit existing C# tooling and decide the MVP slice.

### Deliverables

```text
SagaWeaver/SagaScript inventory
Roslyn/tooling status
runtime binding status
script host status
source-preservation risk list
```

### Tests

```text
docs checks
existing script tests inventoried
```

### Non-Goals

```text
new analyzer implementation
editor integration
runtime host rewrite
```

### Exit Criteria

The C# MVP slice is bounded and evidence-based.

---

## Phase 35 — C# Script Project Contract v0

### Purpose

Define how scripts live inside a Saga project.

### Deliverables

```text
Scripts/ convention
script manifest or project references
script output paths
Build/SagaScript convention
sagascript_diagnostics.json location
```

### Tests

```text
valid script folder accepted
missing script folder diagnostic
paths resolve through SagaProjectKit
```

### Non-Goals

```text
full C# API
NuGet integration
hot reload
```

### Exit Criteria

Project model knows where scripts and script artifacts live.

---

## Phase 36 — SagaScript Analyze v1

### Purpose

Analyze C# source for supported Saga metadata.

### Deliverables

```text
sagascript analyze
detect [SagaBehavior]
basic diagnostics
sagascript_diagnostics.json
```

### Tests

```text
valid [SagaBehavior] detected
missing method fails clearly
invalid syntax produces diagnostics
unsupported code is warning/opaque, not fatal unless configured
```

### Non-Goals

```text
visual projection
patching
runtime execution
```

### Exit Criteria

A script can be analyzed and diagnosed deterministically.

---

## Phase 37 — Runtime Binding Metadata v0

### Purpose

Emit metadata that runtime/publish tools can consume.

### Deliverables

```text
runtime_bindings.json
behavior id
method binding
type info
assembly/script identity
stale/missing binding diagnostics
```

### Tests

```text
binding emitted for valid behavior
invalid binding fails
stable output ordering
publish gate can read binding placeholder
```

### Non-Goals

```text
full script host
hot reload
debugger
```

### Exit Criteria

Scripts produce runtime-facing metadata.

---

## Phase 38 — Compiled C# Runtime Smoke

### Purpose

Prove one tiny compiled C# behavior path.

### Deliverables

```text
compiled script smoke
one gameplay event entry point
one or two safe engine API calls
runtime/test host binding proof
```

### Tests

```text
valid behavior compiles
runtime/test host invokes behavior or validates callable binding
invalid behavior blocks publish if configured
diagnostics deterministic
```

### Non-Goals

```text
full C# API
hot reload
debugger
runtime graph interpreter
```

### Exit Criteria

C# is not just analyzed; one small compiled behavior proof exists.

---

## Phase 39 — Source Map v0

### Purpose

Map supported behavior regions to source spans.

### Deliverables

```text
source_map.json
file identity
method span
statement/expression spans for MVP subset
stable behavior ids
```

### Tests

```text
source spans deterministic
renamed/moved unsupported cases diagnosed
span maps match original bytes
```

### Non-Goals

```text
full C# language mapping
all syntax kinds
visual editing
```

### Exit Criteria

Visual projection can reference exact source regions.

---

## Phase 40 — Read-Only Block Projection v0

### Purpose

Project supported C# regions into block metadata without editing.

### Deliverables

```text
projection_report.json
supported subset: event, branch, call, literal
opaque unsupported nodes
projection diagnostics
```

### Tests

```text
DoorLogic projects to expected blocks
unsupported construct becomes opaque/read-only
projection output deterministic
source file not modified
```

### Non-Goals

```text
editing
patching
complete visual scripting
arbitrary C# conversion
```

### Exit Criteria

Saga-compatible C# can be viewed as blocks without changing source.

---

## Phase 41 — No-Edit Roundtrip Proof

### Purpose

Prove the non-negotiable source trust invariant.

### Deliverables

```text
no-edit roundtrip test harness
byte-identical source verification
projection open/close flow
```

### Tests

```text
input bytes == output bytes after no-edit projection
comments preserved
whitespace preserved
using order preserved
unsupported regions preserved
```

### Non-Goals

```text
editing
format normalization
source regeneration
```

### Exit Criteria

No visual edit means no source mutation.

---

## Phase 42 — Minimal Source Patch Proof

Status: implemented as preview-only. The command emits `patch_preview.json` for
one `ReplaceStringLiteral` operation and does not write C# source.

### Purpose

Preview one safe block edit through exact source-span metadata.

### Deliverables

```text
literal patch preview
diff preview
patch diagnostics
source hash validation
```

### Tests

```text
"key" -> "gold_key" previews only the literal span
unaffected bytes identical
unsupported edit rejected cleanly
```

### Non-Goals

```text
arbitrary graph editing
method regeneration
class regeneration
formatting rewrite
patch apply
```

### Exit Criteria

One tiny block edit can be previewed without changing C# source.

---

## Phase 43 — SagaWeaver MVP Closure

### Purpose

Close the SagaScript/SagaWeaver MVP as evidence-backed source-preserving
metadata, not editor authoring.

### Deliverables

```text
implemented/deferred matrix
allowed claims
blocked claims
known debt
next expansion decision
```

### Tests

```text
focused SagaScript tests pass
golden fixture outputs deterministic
no-edit source preservation proven
runtime binding metadata proven
high/low gameplay projections proven
patch preview proven
```

### Non-Goals

```text
editor authoring implementation
general C# conversion
runtime graph execution
```

### Exit Criteria

Block F closes as a source-preserving C# scripting and read-only projection
foundation.

---

# Block G — Editor Authoring Spine

## Phase 44 — Editor Project Open Bridge

### Purpose

Connect editor/product shell to `.sagaproj`.

### Deliverables

```text
open project command
SagaProjectKit validation bridge
project open diagnostics
no mutation on open
```

### Tests

```text
valid project opens
invalid project blocked with diagnostics
open does not mutate files
diagnostics shown/logged
```

### Non-Goals

```text
full scene editor
collaboration
visual editing
```

### Exit Criteria

Editor can open MultiplayerSandbox as a product project.

### Phase 44 Evidence

Block G adds a backend-neutral `TechnicalPreviewProjectView` that reads only
the editor display subset of `.sagaproj`. Full manifest validation remains in
SagaProjectKit. The editor subset reader reports deterministic subset parse
diagnostics and does not mutate project files.

---

## Phase 45 — Scene/Entity Minimal Editor Surface

### Purpose

Show and edit a minimal project scene/entity property.

### Deliverables

```text
scene/entity list
entity selection
simple property edit
project transaction or change record placeholder
```

### Tests

```text
scene loads from project
entity selection deterministic
property edit writes expected artifact
invalid edit rejected
```

### Non-Goals

```text
complete level editor
gizmos
asset browser
prefab system
```

### Exit Criteria

The editor has a minimal tangible project editing surface.

### Phase 45 Status

For this read-only source-trust slice, the editing part is deferred. Evidence
is limited to project/artifact status models over existing SagaScript outputs.
No scene, entity, property, or artifact writes are part of Block G.

---

## Phase 46 — C# Source View Link

### Purpose

Connect behavior source to editor workflow.

### Deliverables

```text
behavior source link
open C# source path
script diagnostics view
projection status link
```

### Tests

```text
behavior opens source
missing source diagnostic
diagnostics displayed
no source mutation
```

### Non-Goals

```text
IDE replacement
full code editor
debugger
```

### Exit Criteria

A user can locate and inspect behavior source from Saga.

### Phase 46 Evidence

`ScriptSourceLinkView` exposes source file paths, UTF-8 byte spans, line/column
metadata, source hashes, and deterministic freshness status from
`source_map.json`.

---

## Phase 47 — Simple Blocks View Read-Only

### Purpose

Render read-only visual block projection.

### Deliverables

```text
simple blocks panel/view
projection model renderer
opaque node display
source link back to C#
```

### Tests

```text
DoorLogic blocks render
opaque advanced code render
opening view does not mutate source
unsupported edits not available
```

### Non-Goals

```text
visual editing
full graph editor
node library authoring
```

### Exit Criteria

A supported C# behavior can be inspected visually in the editor.

### Phase 47 Evidence

`ScriptBehaviorProjectionView` and `ScriptBlockProjectionNodeView` consume
SagaScript projection artifacts as read-only backend-neutral models. They do
not use graph mutation documents as projection truth.

---

## Phase 48 — Simple Blocks Minimal Edit

### Purpose

Integrate the minimal source patch proof into the editor.

### Deliverables

```text
literal/parameter edit UI
diff preview
apply/cancel
rollback on failure
source diagnostics refresh
```

### Tests

```text
literal edit patches only target span
cancel does not mutate source
failed patch leaves source unchanged
diagnostics update deterministically
```

### Non-Goals

```text
arbitrary block editing
method regeneration
graph rewrite
```

### Exit Criteria

A designer-level simple edit can safely modify C#.

### Phase 48 Status

For this source-trust slice, editor-side modification is deferred. Block G only
loads `patch_preview.json` as a review artifact through
`ScriptPatchPreviewView`; no editor-side execution is included.

---

## Phase 49 — Pro View / Behavior Inspector v0

### Purpose

Provide a more detailed behavior view without becoming a full graph editor.

### Deliverables

```text
Pro Behavior Inspector
node/source span details
unsupported reason details
runtime binding metadata display
```

### Tests

```text
pro view shows supported/opaque nodes
source span links valid
runtime binding status visible
simple/pro views agree on behavior identity
```

### Non-Goals

```text
full pro graph editor
advanced graph editing
debugger
```

### Exit Criteria

Saga supports at least two abstraction levels over the same behavior truth.

### Phase 49 Evidence

`ScriptBehaviorInspectorView` joins projection, source-map, and runtime binding
artifacts for read-only inspection. This closes Block G evidence without
starting Phase 50 collaboration work.

---

# Block H — Collaboration MVP

## Phase 50 — Collaboration Model Audit

### Purpose

Map existing editor/session/project systems before implementing collaboration.

### Deliverables

```text
collaboration inventory
presence/transaction/lock MVP scope
personal views/shared project decision record
enterprise deferred list
```

### Tests

```text
docs checks
no code unless boundary guard needed
```

### Non-Goals

```text
cloud workspace
enterprise security
CRDT/OT
```

### Exit Criteria

MVP collaboration scope is bounded.

### 2026-05-31 Evidence

Closed as local/offline collaboration metadata scope in
`docs/architecture/COLLABORATION_MODEL_BLOCK_H_PHASE50_56.md`. Existing
editor collaboration-looking files are inventory only unless covered by focused
tests.

---

## Phase 51 — Local Workspace Session v0

### Purpose

Create in-memory local collaboration session state.

### Deliverables

```text
workspace id
user id
session id
join/leave
session report
```

### Tests

```text
users join/leave
duplicate session handled
session report deterministic
```

### Non-Goals

```text
network server
accounts
cloud identity
```

### Exit Criteria

Multiple local users/sessions can exist in a test harness.

### 2026-05-31 Evidence

`SagaCollaboration/CollaborationModel.hpp` adds deterministic workspace
identity, local actor, and local workspace session models. Focused tests cover
missing ids, privacy-limited metadata, and deterministic session ids.

---

## Phase 52 — Presence v0

### Purpose

Track live, ephemeral presence.

### Deliverables

```text
presence record
active artifact
cursor/selection summary
view id
presence update stream
```

### Tests

```text
presence update does not mutate project
online users deterministic
active artifact visible
restricted data not modeled as security yet
```

### Non-Goals

```text
persistent history
enterprise redaction
remote service
```

### Exit Criteria

Users can see who is present and where they are working.

### 2026-05-31 Evidence

`ArtifactPresence` models actor, opened project, active artifact, active
behavior id, view id, read-only mode, and sequence. Generated reports are kept
under `Build/Collaboration/` and tested to leave project files unchanged.

---

## Phase 53 — Artifact Locks v0

### Purpose

Prevent destructive simultaneous edits.

### Deliverables

```text
lock acquire/release
lock owner
lock conflict diagnostics
read-only state for locked artifact
```

### Tests

```text
first lock succeeds
second lock fails
release updates state
stale release fails
locked artifact edit rejected
```

### Non-Goals

```text
merge conflict resolution
approval workflow
enterprise policy
```

### Exit Criteria

Two users cannot destructively edit the same behavior at once.

### 2026-05-31 Evidence

Artifact locks are bounded to collaboration metadata review targets. Tests
cover first owner success, second owner rejection, deterministic release, and
locked-target transaction rejection.

---

## Phase 54 — Semantic Transaction Log v0

### Purpose

Record meaningful project changes, not UI gestures.

### Deliverables

```text
transaction id
artifact id
operation type
payload
base/result version
workspace_transactions.jsonl
```

### Tests

```text
transaction append deterministic
cursor movement not recorded as transaction
property/behavior edit recorded
replay simple transaction if feasible
```

### Non-Goals

```text
CRDT/OT
all artifact types
real-time merge
```

### Exit Criteria

Meaningful changes become durable semantic records.

### 2026-05-31 Evidence

`SemanticTransaction` and `workspace_transactions.jsonl` serialization cover
review, artifact-reviewed, note add/resolve, and diagnostic acknowledgement
operations. Cursor/presence movement is explicitly excluded from semantic
transactions.

---

## Phase 55 — Conflict Rejection v0

### Purpose

Reject unsafe edits instead of silently merging.

### Deliverables

```text
stale base version detection
locked artifact rejection
conflict diagnostics
transaction failure report
```

### Tests

```text
stale transaction fails
locked edit fails
diagnostic includes artifact/reason
project state unchanged after rejection
```

### Non-Goals

```text
automatic merge
complex diff resolution
```

### Exit Criteria

Collaboration chooses safety over silent corruption.

### 2026-05-31 Evidence

Conflict rejection records deterministic conflict categories for same patch
preview reviewed by multiple actors, stale source hash, missing target,
unsupported operation, out-of-order operation, unknown actor, and locked
artifact. Rejected operations do not append accepted transactions.

---

## Phase 56 — Team Room Dashboard v1

### Purpose

Give a team lead one place to see project activity.

### Deliverables

```text
Team Room panel/view
online users
active artifacts
locks
recent transactions
diagnostics summary
publish/package status placeholder
review queue placeholder
```

### Tests

```text
presence visible
locks visible
recent changes visible
diagnostics summary visible
state deterministic
```

### Non-Goals

```text
full project management
billing
enterprise audit UI
```

### Exit Criteria

Team lead activity is visible through a backend-neutral model/report without
starting Phase 57.

### 2026-05-31 Evidence

`TeamRoomDashboardView` summarizes local actors, presence, locks, recent
transactions, conflicts, diagnostics review status, and package/publish
placeholders. This is model/report evidence only; Qt UI is deferred.

A user can understand current project activity from one dashboard.

---

# Block I — View Capability / Product Honesty

## Phase 57 — SagaViewKit v1

### Purpose

Define what each editor view may show/edit/opaque.

### Deliverables

```text
Tools/SagaViewKit
view capability manifest schema
Simple/Pro/C# Source/Diagnostics/Team Room capability profiles
view_capability_report.json
```

### Tests

```text
valid capability manifest passes
invalid capability fails
Simple View cannot claim advanced edit capability
Pro View source-link capability accepted
report deterministic
```

### Non-Goals

```text
full permission system
enterprise policy enforcement
```

### Exit Criteria

Editor views have explicit capability truth.

### 2026-05-31 Evidence

Closed as report-only view capability infrastructure in
`docs/architecture/VIEW_CAPABILITY_PRODUCT_HONESTY_PHASE57_59.md`.
`Tools/SagaViewKit` validates view capability manifests and emits built-in
profiles for Simple, Pro, C# Source, Diagnostics, and Team Room views.

---

## Phase 58 — Simple View Must Not Lie Gate

### Purpose

Prevent simplified views from misrepresenting unsupported complexity.

### Deliverables

```text
simple-view honesty validator
opaque/advanced region requirement
unsupported edit rejection
report integration
```

### Tests

```text
advanced code cannot appear as simple editable block
opaque disclosure required
missing disclosure fails
diagnostics explain issue
```

### Non-Goals

```text
convert all advanced code
hide complexity for convenience
```

### Exit Criteria

The product has a technical guard against dishonest simple views.

### 2026-05-31 Evidence

`sagaviewkit check-simple` consumes SagaScript projection artifacts and a Simple
View profile, then reports unsupported editable nodes, missing opaque
disclosures, hidden advanced regions, missing evidence, source rewrite
capability, and patch application capability as deterministic violations.

---

## Phase 59 — SagaDocGuard v1

### Purpose

Prevent roadmap/docs from claiming more than evidence supports.

### Deliverables

```text
Tools/SagaDocGuard
claim inventory
heavy claim scanner
non-claim checker
evidence report existence checker
docguard_report.json
```

### Tests

```text
"production-ready" claim without evidence fails
required non-claims detected
missing publish/diagnostics evidence fails
report deterministic
```

### Non-Goals

```text
AI doc reasoning
perfect semantic understanding
legal compliance
```

### Exit Criteria

Docs cannot easily overclaim the MVP.

### 2026-05-31 Evidence

`Tools/SagaDocGuard` adds deterministic Markdown scanning for positive
unsupported claims, required non-claim presence, reviewed non-claim matches,
and required evidence files. It is string/regex based, does not rewrite docs,
and does not perform AI reasoning.

---

# Block J — Technical Preview Candidate / Distribution

## Phase 60 — Clean Onboarding Command

**Status:** Implemented as repository-local quickstart evidence on 2026-05-31.
This does not start Target 2 / Small-Team Alpha.

### Purpose

Create a clean first-user path.

### Deliverables

```text
quickstart command path
configure/build/open sample instructions
developer smoke script
known environment assumptions
```

### Tests

```text
quickstart commands verified on primary platform
missing dependency diagnostics clear
docs match commands
```

### Non-Goals

```text
installer
one-click launcher
commercial distribution
```

### Exit Criteria

A developer can follow documented steps to build/open the sample.

### 2026-05-31 Evidence

`Tools/SagaPreviewGate` adds `quickstart-check`, which verifies the expected
Technical Preview tools, sample manifest, launch profiles, package profiles,
scripts, and optional native binary directory. The command writes
`quickstart_report.json`.

---

## Phase 61 — Full MVP Acceptance Script

**Status:** Implemented as deterministic focused evidence orchestration on
2026-05-31. It does not run raw full CTest.

### Purpose

Create one script/test that proves the entire product spine.

### Deliverables

```text
MVP acceptance test/script
project validate
script analyze/projection/patch
launch preview
diagnostics summary
package/publish check
packaged launch smoke
```

### Tests

```text
acceptance script passes on primary platform
failure step emits diagnostic
artifacts collected
```

### Non-Goals

```text
complete raw CTest claim
all platforms
enterprise scenarios
```

### Exit Criteria

The MVP path is no longer a manual story; it is executable evidence.

### 2026-05-31 Evidence

`sagapreviewgate accept` orchestrates the existing focused tools for
SagaProjectKit, SagaScript, SagaViewKit, SagaLaunchLab, SagaProbe,
SagaPackager, and SagaDocGuard. The generated `mvp_acceptance_report.json`
records ordered step status and diagnostics. Preview patch evidence remains a
report artifact and does not modify C# source.

---

## Phase 62 — Cross-Platform Build Evidence

**Status:** Implemented as honest local build-matrix reporting on 2026-05-31.

### Purpose

Collect minimum platform/build evidence for technical preview.

### Deliverables

```text
Linux RelWithDebInfo evidence
Debug/Release sampled evidence
Windows/MSVC evidence if available
build_matrix_report.json or docs evidence
```

### Tests

```text
selected targets build
focused tests pass
failures documented honestly
```

### Non-Goals

```text
full CI hardening
all compilers
release-candidate guarantee
```

### Exit Criteria

Technical preview has at least credible build evidence beyond one ad-hoc run.

### 2026-05-31 Evidence

`sagapreviewgate build-matrix` writes `build_matrix_report.json`. It records
the local focused evidence path and marks platform/configuration rows without
collected evidence as `Unavailable` instead of claiming they passed.

---

## Phase 63 — Known Limitations / Non-Claims Freeze

**Status:** Implemented as Technical Preview vocabulary and DocGuard evidence
checks on 2026-05-31.

### Purpose

Freeze what the technical preview does not claim.

### Deliverables

```text
SAGA_RELEASE_VOCABULARY_AND_CLAIM_LEVELS.md
Technical Preview limitations
known debt list
blocked claims list
```

### Tests

```text
DocGuard verifies non-claims
forbidden claim scan passes
```

### Non-Goals

```text
marketing release
beta product status
enterprise promises
```

### Exit Criteria

The preview cannot accidentally be described as more mature than it is.

### 2026-05-31 Evidence

`docs/internal/product-history/SAGA_RELEASE_VOCABULARY_AND_CLAIM_LEVELS.md` freezes the allowed
Technical Preview vocabulary and required non-claims. `SagaDocGuard` now
requires Block A and Block J evidence docs in addition to Blocks B-I evidence.

---

## Phase 64 — SagaEngine 0.1 Technical Preview Package

**Status:** Implemented as a local package-folder assembly report on
2026-05-31.

### Purpose

Assemble a distributable technical preview artifact.

### Deliverables

```text
sample project
built tools/executables or build instructions
quickstart docs
diagnostics example reports
package/publish report
known limitations
```

### Tests

```text
package contains expected files
quickstart references valid paths
sample validates
packaged launch proof included
```

### Non-Goals

```text
commercial installer
auto-updater
marketplace
cloud services
```

### Exit Criteria

There is a concrete technical preview package, not just a repository state.

### 2026-05-31 Evidence

`sagapreviewgate package` assembles
`Build/TechnicalPreview/Package/SagaEngine-0.1-TechnicalPreview` from
repository-local docs, sample manifests, and gate documentation, then writes
`technical_preview_package_report.json`. This is not an installer, updater,
marketplace path, or hosted service.

---

## Phase 65 — MVP Closure Audit

**Status:** Implemented as accepted-or-blocked local closure reporting on
2026-05-31.

### Purpose

Close Target 1 / Technical Preview honestly.

### Deliverables

```text
SagaEngine 0.1 Technical Preview Closure Report
implemented matrix
evidence matrix
non-claim matrix
known debt
Target 2 / Small-Team Alpha opening recommendation
```

### Tests / Verification

```text
MVP acceptance script result recorded
DocGuard passes
PublishGate passes or justified failures documented
key focused tests pass
architecture docs updated
```

### Non-Goals

```text
Small-Team Alpha claim
Enterprise-Evolvable claim
beta product status claim
```

### Exit Criteria

Target 1 / Technical Preview is either accepted as `SagaEngine 0.1 Technical Preview` or explicitly blocked with a short blocker list.

### 2026-05-31 Evidence

`sagapreviewgate close` consumes Phase 60-64 reports and writes
`technical_preview_closure_report.json` with implemented, evidence, non-claim,
known-debt, and Target 2 / Small-Team Alpha opening recommendation fields. It only accepts Target 1 / Technical Preview
when required focused evidence is present and passed.

---

# 7. Final Target State After Phase 65

After Phase 65, the intended state is:

```text
MultiplayerSandbox can be opened, validated, scripted, viewed as C# and blocks,
minimally patched, launched locally, observed through diagnostics, packaged,
launched outside the editor, and lightly coordinated through local presence/locks/
transactions.
```

Allowed final claim:

```text
SagaEngine 0.1 Technical Preview proves the product spine of a multiplayer-first,
diagnostics-backed, source-preserving C# + visual authoring engine.
```

Still forbidden:

```text
beta product status
release-candidate status
complete visual scripting
arbitrary C# roundtrip
enterprise collaboration readiness
production MMO readiness
Unreal/Unity replacement
Roblox replacement
cloud workspace platform
full security model
```

---

# 8. Target 2 / Small-Team Alpha Opening Notes

Target 2 / Small-Team Alpha — Small-Team Alpha should begin only after Phase 65 closure.

Likely focus areas:

```text
more complete editor workflow
better C# authoring surface
more robust packaging
improved collaboration transactions
reviewable changes
team usability polish
more runtime/client stability
additional sample gameplay
```

Phase 8 status note: server lifecycle diagnostics now have a focused
direct/local evidence target, `ServerLifecycleDiagnosticsTests`, covering
bounded lifecycle events, records, derived leaks, report serialization, and
idempotent snapshot/report construction. This is diagnostics evidence only and
does not start Phase 9 real transport stress.

Target 2 / Small-Team Alpha should not start by expanding enterprise policy. It should make the small-team workflow genuinely usable first.

---

# 9. Target 3 / Enterprise-Evolvable Foundation Opening Notes

Target 3 / Enterprise-Evolvable Foundation — Enterprise-Evolvable Foundation should begin only after the small-team workflow is real.

Likely focus areas:

```text
project slices
permission enforcement
source redaction
audit log
review/approval workflow
policy gate
restricted presence
server-side build/preview
publish policy integration
```

Enterprise should be built from shared project truth, semantic transactions, and policy enforcement — not from hiding UI panels.
