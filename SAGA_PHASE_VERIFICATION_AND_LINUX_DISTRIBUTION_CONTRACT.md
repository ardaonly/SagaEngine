# SagaEngine Phase Verification and Linux Distribution Contract

> Internal execution contract for Codex and maintainers.  
> This file is not a marketing roadmap.  
> This file defines how phases are verified and what must exist before SagaEngine can be considered a distributable Linux technical preview.

---

## 0. Purpose

This document defines the verification system for SagaEngine's long-term productization phases.

The goal is not to make the repository look complete.

The goal is to make the repository verifiably buildable, runnable, packageable, and distributable on Linux.

The final target is:

```txt
build/dist/linux/Saga/
```

containing a usable Linux Saga distribution with:

```txt
Saga product shell
SagaEditor shell
Runtime executable
Server executable where enabled
Public CLI tools
Starter sample project
Package pipeline output
SDE package or runtime-facing SDE artifacts where applicable
C# scripting support
Visual Blocks projection/edit support for the compatible subset
Profiles/customization system
Plugin/extension surface
Public docs
Known limitations
Quick verification command
```

A phase is not complete because a document says it is complete.

A phase is complete only when evidence exists and verification passes.

---

## 1. Core Rule

```txt
DONE = implementation + verification + evidence + limitations + reproducible result
```

Invalid proof:

```txt
roadmap text
claim text
closure note only
generated placeholder
planned report
old recovery note
AI-written completion statement
Codex saying "done" without verification
```

Valid proof:

```txt
source code
tests
build logs
CLI logs
smoke tests
packaged artifact
fresh checkout verification
manual verification note
known limitations
phase evidence folder
```

---

## 2. Phase Status Model

Only the following statuses are allowed:

| Status | Meaning |
|---|---|
| `Not Started` | No real implementation work has started. |
| `In Progress` | Files are being changed, but there is no complete verification evidence yet. |
| `Implemented-Unverified` | Code/docs exist, but the phase has not passed the required gate. |
| `Verified` | Required evidence exists and the verification gate passed. |
| `Blocked` | Work cannot continue because of a missing dependency or unresolved decision. |
| `Rejected` | Work was attempted but failed verification or introduced false claims. |
| `Archived` | Historical phase. Preserved for context, not part of active product truth. |

Only `Verified` means complete.

Codex may set:

```txt
Not Started
In Progress
Implemented-Unverified
Blocked
```

Codex must not set:

```txt
Verified
Rejected
```

`Verified` and `Rejected` require maintainer review or an explicit verification script result.

---

## 3. Required Tracking Files

Create and maintain these files:

```txt
docs/internal/PHASE_STATUS_MATRIX.md
docs/internal/phase-evidence/
scripts/verify-phase
scripts/verify-quick
scripts/scan-claims
scripts/check-boundaries
```

`TEMP.md` remains the internal execution plan.

This document defines how the execution plan is judged.

`PHASE_STATUS_MATRIX.md` is the single source of truth for current phase status.

---

## 4. Phase Status Matrix Format

`docs/internal/PHASE_STATUS_MATRIX.md` must use this structure:

```md
# SagaEngine Phase Status Matrix

> Internal status file.  
> This file records verified reality, not roadmap intent.

| Phase | Name | Status | Evidence Path | Required Gate | Last Verified Commit | Notes |
|---:|---|---|---|---|---|---|
| 1 | Worktree Recovery | Not Started | `docs/internal/phase-evidence/PHASE_01/` | repo-gate | `-` | `-` |
| 2 | Public Documentation Identity Reset | Not Started | `docs/internal/phase-evidence/PHASE_02/` | docs-gate | `-` | `-` |
| 3 | Internal Archive Cleanup | Not Started | `docs/internal/phase-evidence/PHASE_03/` | docs-gate | `-` | `-` |
| 4 | Toolchain Boundary Cleanup | Not Started | `docs/internal/phase-evidence/PHASE_04/` | tools-gate | `-` | `-` |
| 5 | Canonical Build Baseline | Not Started | `docs/internal/phase-evidence/PHASE_05/` | build-gate | `-` | `-` |
| 6 | Test Taxonomy | Not Started | `docs/internal/phase-evidence/PHASE_06/` | test-gate | `-` | `-` |
| 7 | CI-Ready Local Gate | Not Started | `docs/internal/phase-evidence/PHASE_07/` | verify-gate | `-` | `-` |
| 8 | MultiplayerSandbox Truth Reset | Not Started | `docs/internal/phase-evidence/PHASE_08/` | sample-truth-gate | `-` | `-` |
| 9 | StarterArena Sample Definition | Not Started | `docs/internal/phase-evidence/PHASE_09/` | sample-design-gate | `-` | `-` |
| 10 | First Playable Runtime Loop | Not Started | `docs/internal/phase-evidence/PHASE_10/` | runtime-smoke-gate | `-` | `-` |
| 11 | C# Gameplay Script v1 | Not Started | `docs/internal/phase-evidence/PHASE_11/` | scripting-gate | `-` | `-` |
| 12 | Server-Authoritative Sample v1 | Not Started | `docs/internal/phase-evidence/PHASE_12/` | server-gate | `-` | `-` |
| 13 | C# Compatibility Profile v1 | Not Started | `docs/internal/phase-evidence/PHASE_13/` | csharp-blocks-gate | `-` | `-` |
| 14 | Read-Only Blocks Projection | Not Started | `docs/internal/phase-evidence/PHASE_14/` | projection-gate | `-` | `-` |
| 15 | Block Operation Contract | Not Started | `docs/internal/phase-evidence/PHASE_15/` | patch-contract-gate | `-` | `-` |
| 16 | First Safe Block Edit | Not Started | `docs/internal/phase-evidence/PHASE_16/` | source-patch-gate | `-` | `-` |
| 17 | Two-Way Authoring v1 | Not Started | `docs/internal/phase-evidence/PHASE_17/` | authoring-loop-gate | `-` | `-` |
| 18 | SDE Current Contract Stabilization | Not Started | `docs/internal/phase-evidence/PHASE_18/` | sde-boundary-gate | `-` | `-` |
| 19 | SDE Artifact and Manifest Contracts | Not Started | `docs/internal/phase-evidence/PHASE_19/` | sde-artifact-gate | `-` | `-` |
| 20 | Saga Schema Package Boundary | Not Started | `docs/internal/phase-evidence/PHASE_20/` | sde-schema-gate | `-` | `-` |
| 21 | Product Shell v1 | Not Started | `docs/internal/phase-evidence/PHASE_21/` | shell-gate | `-` | `-` |
| 22 | Editor Shell v1 | Not Started | `docs/internal/phase-evidence/PHASE_22/` | editor-gate | `-` | `-` |
| 23 | Customizable Editor Profiles | Not Started | `docs/internal/phase-evidence/PHASE_23/` | customization-gate | `-` | `-` |
| 24 | Plugin / Extension Surface v1 | Not Started | `docs/internal/phase-evidence/PHASE_24/` | plugin-gate | `-` | `-` |
| 25 | Local Team Workspace | Not Started | `docs/internal/phase-evidence/PHASE_25/` | collaboration-gate | `-` | `-` |
| 26 | Semantic Transactions | Not Started | `docs/internal/phase-evidence/PHASE_26/` | transaction-gate | `-` | `-` |
| 27 | Team Room v1 | Not Started | `docs/internal/phase-evidence/PHASE_27/` | team-room-gate | `-` | `-` |
| 28 | Permissions / Roles v1 | Not Started | `docs/internal/phase-evidence/PHASE_28/` | permissions-gate | `-` | `-` |
| 29 | Project Slices | Not Started | `docs/internal/phase-evidence/PHASE_29/` | slice-gate | `-` | `-` |
| 30 | Audit / Review / Approval | Not Started | `docs/internal/phase-evidence/PHASE_30/` | audit-gate | `-` | `-` |
| 31 | Package Pipeline v1 | Not Started | `docs/internal/phase-evidence/PHASE_31/` | package-gate | `-` | `-` |
| 32 | Public SDK Boundary | Not Started | `docs/internal/phase-evidence/PHASE_32/` | sdk-boundary-gate | `-` | `-` |
| 33 | Installable Technical Preview Candidate | Not Started | `docs/internal/phase-evidence/PHASE_33/` | install-gate | `-` | `-` |
| 34 | Extension Packaging Boundary | Not Started | `docs/internal/phase-evidence/PHASE_34/` | extension-package-gate | `-` | `-` |
| 35 | Multiplayer Production Direction Proof | Not Started | `docs/internal/phase-evidence/PHASE_35/` | multiplayer-proof-gate | `-` | `-` |
| 36 | Enterprise Workflow Direction Proof | Not Started | `docs/internal/phase-evidence/PHASE_36/` | enterprise-proof-gate | `-` | `-` |
| 37 | Full Authoring Loop Proof | Not Started | `docs/internal/phase-evidence/PHASE_37/` | full-loop-gate | `-` | `-` |
| 38 | Distributable Engine Candidate | Not Started | `docs/internal/phase-evidence/PHASE_38/` | distribution-gate | `-` | `-` |
```

---

## 5. Evidence Folder Format

Each phase must have:

```txt
docs/internal/phase-evidence/PHASE_XX/
  EVIDENCE.md
  commands.log
  changed_files.txt
  known_limitations.md
  verification_result.json
```

Optional files:

```txt
manual_checklist.md
screenshots/
package_manifest.json
build_report.json
test_output.log
```

---

## 6. EVIDENCE.md Template

```md
# Phase XX Evidence

## Status

Implemented-Unverified

## Phase Scope

Describe the exact scope attempted in this phase.

## Changed Files

Generated from:

```bash
git diff --name-only
```

## Verification Commands

List exact commands run.

## Command Results

Include command result summaries and paths to full logs.

## Required Files

List required files and whether they exist.

## Manual Checks

- [ ] Public docs do not overclaim.
- [ ] Known limitations are documented.
- [ ] No placeholder is presented as shipped behavior.
- [ ] Runtime/editor/tool behavior was manually checked if required.
- [ ] Unsupported behavior is not hidden.

## Known Limitations

Link to `known_limitations.md`.

## Verification Decision

One of:

```txt
Implemented-Unverified
Verified
Blocked
Rejected
```

## Decision Reason

Explain why.
```

---

## 7. Required Verification Gates

### 7.1 Repo Gate

Used by:

```txt
Phase 1
```

Required commands:

```bash
git status --short
git diff --check
```

Required evidence:

```txt
worktree is explainable
commit groups are clear
generated files are ignored or justified
```

---

### 7.2 Docs Gate

Used by:

```txt
Phase 2
Phase 3
```

Required commands:

```bash
git diff --check
scripts/scan-claims README.md docs samples Tools
```

Banned public claims:

```txt
production-ready
enterprise-ready
release candidate
public beta
full visual scripting
arbitrary C# to blocks
complete engine
AI-native
revolutionary
```

Required evidence:

```txt
public docs describe current capabilities
public docs describe known limitations
internal recovery/history docs are not public onboarding docs
```

---

### 7.3 Tools Gate

Used by:

```txt
Phase 4
```

Required evidence:

```txt
public tools are listed separately
internal tools are listed separately
public tools have real CLI examples
internal tools are not marketed as product features
```

---

### 7.4 Build Gate

Used by:

```txt
Phase 5
```

Required commands:

```bash
scripts/build-default
scripts/verify-quick
```

Required evidence:

```txt
canonical build path exists
build failure reasons are readable
default build does not require unrelated experimental systems
```

---

### 7.5 Test Gate

Used by:

```txt
Phase 6
```

Required evidence:

```txt
quick tests are defined
focused tests are defined
integration tests are defined
heavy/stress/manual tests are opt-in
```

---

### 7.6 Verify Gate

Used by:

```txt
Phase 7
```

Required commands:

```bash
scripts/verify-quick
```

Required evidence:

```txt
fresh checkout verification path exists
failure output is readable
heavy/stress tests are not run by default
```

---

### 7.7 Sample Truth Gate

Used by:

```txt
Phase 8
```

Required evidence:

```txt
MultiplayerSandbox is classified honestly
fixture/generated output is separated
sample README does not claim unsupported behavior
```

---

### 7.8 Runtime Smoke Gate

Used by:

```txt
Phase 10
```

Required commands:

```bash
scripts/verify-quick
```

Additional expected command, adjusted to actual CLI name:

```bash
saga run samples/StarterArena
```

Required evidence:

```txt
sample project loads
runtime launches
player can move
restart works
quit works
```

---

### 7.9 Scripting Gate

Used by:

```txt
Phase 11
```

Required evidence:

```txt
C# scripts compile
runtime script binding works
sample behavior uses C# where intended
script diagnostics are readable
```

Required sample scripts:

```txt
Door.cs
Pickup.cs
DamageZone.cs
GameRules.cs
```

---

### 7.10 Server Gate

Used by:

```txt
Phase 12
```

Required evidence:

```txt
local server launches
local client connects
server owns authoritative state
invalid input is rejected
deterministic server smoke exists
```

---

### 7.11 C# Blocks Gate

Used by:

```txt
Phase 13
Phase 14
Phase 15
Phase 16
Phase 17
```

Required fixtures:

```txt
tests/fixtures/csharp_blocks/projectable/
tests/fixtures/csharp_blocks/partially_projectable/
tests/fixtures/csharp_blocks/advanced_opaque/
tests/fixtures/csharp_blocks/unsupported/
```

Required proof:

```txt
compatible C# projects to blocks
partially compatible C# becomes mixed editable/read-only projection
advanced C# becomes opaque/read-only source-linked blocks
unsupported C# produces diagnostics
no-edit roundtrip is byte-identical
block edit creates minimal C# patch
unrelated comments/formatting are preserved
patched C# compiles
runtime uses compiled C#, not visual graph execution
```

---

### 7.12 SDE Boundary Gate

Used by:

```txt
Phase 18
Phase 19
Phase 20
```

Required commands, adjusted to actual repository scripts:

```bash
python3 Tools/Scripts/check_tools_isolation.py
python3 Tools/SystemDefinitionEngine/build.py --tests
conan create Tools/SystemDefinitionEngine
```

Required proof:

```txt
SDE builds standalone
SDE links through SDE::Core
SDE core does not include SagaEngine/SagaEditor/SagaServer headers
SDE does not become runtime/editor/server/C# host
same input produces deterministic output
SDE artifacts/manifests are documented
Saga-specific semantics live outside SDE core
```

---

### 7.13 Shell and Editor Gate

Used by:

```txt
Phase 21
Phase 22
Phase 23
Phase 24
```

Required proof:

```txt
product shell opens a project
product shell routes edit/play/package workflows
editor opens StarterArena
scene hierarchy is visible
inspector shows entity/component data
content browser works
problems panel shows diagnostics
runtime preview starts
profile switching changes layout/tool visibility
profile switching does not change project truth
sample plugin loads
plugin can be disabled without corrupting the project
```

---

### 7.14 Collaboration Gate

Used by:

```txt
Phase 25
Phase 26
Phase 27
```

Required proof:

```txt
two local users can be simulated
resource lock conflict is detected
semantic transaction is logged
unsafe transaction is rejected
Team Room shows users/resources/locks/conflicts/review queue
unresolved conflict blocks package/publish where required
```

---

### 7.15 Enterprise Gate

Used by:

```txt
Phase 28
Phase 29
Phase 30
Phase 36
```

Required proof:

```txt
unauthorized edit is rejected by service/backend logic, not UI hiding only
role checks exist
project slices restrict visible resources
restricted data is not sent to unauthorized clients where applicable
dangerous operation requires approval
audit log records restricted operation
publish gate checks review/audit state
enterprise workflow fixture passes
```

---

### 7.16 Package Gate

Used by:

```txt
Phase 31
```

Required output:

```txt
build/packages/StarterArena/
```

Required proof:

```txt
fresh build produces package
package contains runtime executable
package contains assets
package contains scripts or compiled script output
package contains manifests
package runs from package directory
package does not require source tree paths
```

---

### 7.17 SDK Boundary Gate

Used by:

```txt
Phase 32
```

Required proof:

```txt
public headers are inventoried
experimental headers are inventoried
internal/private headers are separated
Qt/SDL/Diligent leaks are removed from inappropriate public boundaries
samples use public/approved APIs only
include boundary tests pass
```

---

### 7.18 Install Gate

Used by:

```txt
Phase 33
```

Required output:

```txt
build/dist/linux/Saga/
```

Required package contents:

```txt
bin/Saga
bin/SagaEditor
bin/SagaRuntime
bin/SagaServer                 # if server target is enabled
bin/sagaproject                # if public
bin/sagapack                   # if public
bin/sagascript                 # if public
bin/sde                        # if public/exposed
lib/
include/
share/saga/
samples/StarterArena/
docs/
licenses/
VERIFY.txt
KNOWN_LIMITATIONS.md
```

Required proof:

```txt
fresh install/unpack works
getting started works
sample opens
sample plays
sample packages
quick verification passes from distribution folder
```

---

### 7.19 Distribution Gate

Used by:

```txt
Phase 38
```

Required command:

```bash
scripts/package-linux-saga
```

Required output:

```txt
build/dist/linux/Saga/
build/dist/linux/Saga.tar.zst
build/dist/linux/Saga.sha256
```

Required proof:

```txt
full engine/toolchain build completes
Linux distribution folder is created under build/dist/linux/Saga
archive is created
checksum is created
distribution runs outside source tree
StarterArena opens from distribution
StarterArena plays from distribution
StarterArena packages from distribution
known limitations are included
public docs contain no fake maturity claims
quick verification passes
```

---

## 8. Final Linux Distribution Layout

The final Linux distribution must use this target layout:

```txt
build/
  dist/
    linux/
      Saga/
        bin/
          Saga
          SagaEditor
          SagaRuntime
          SagaServer
          sagaproject
          sagapack
          sagascript
          sde
        lib/
          *.so
        include/
          SagaEngine/
          SagaEditor/
          SagaTools/
          SDE/
        share/
          saga/
            templates/
            profiles/
            schemas/
            plugins/
            cmake/
        samples/
          StarterArena/
            project file
            scenes/
            scripts/
            assets/
            README.md
        docs/
          README.md
          GETTING_STARTED.md
          CURRENT_CAPABILITIES.md
          KNOWN_LIMITATIONS.md
          BUILDING_FROM_SOURCE.md
          SCRIPTING.md
          VISUAL_BLOCKS.md
          PACKAGING.md
          CUSTOMIZATION.md
        licenses/
        VERIFY.txt
        VERSION
        BUILD_INFO.json
        KNOWN_LIMITATIONS.md
      Saga.tar.zst
      Saga.sha256
```

If a binary is not implemented yet, it must not be shipped as a fake placeholder.

If a tool is internal, it must not appear in the public distribution by default.

---

## 9. Maximum Customization Philosophy Gate

SagaEngine may only claim “maximum customization direction” when these capabilities are real enough to demonstrate:

### 9.1 Editor Profiles

Required:

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

Proof:

```txt
profile switch changes editor layout/panels/tools
profile switch does not change project truth
profiles are not permissions
```

### 9.2 Plugin / Extension Surface

Required:

```txt
panel plugin
tool command plugin
block/node library plugin
plugin manifest
disable/unload behavior
```

Proof:

```txt
sample plugin loads
sample plugin can be disabled
project remains valid without plugin
public/private extension API is separated
```

### 9.3 Source-Preserving C# / Blocks Authoring

Required:

```txt
C# canonical source
Visual Blocks projection
read-only opaque unsupported C#
minimal patch application
source maps/source spans
compile after patch
```

Proof:

```txt
supported block edit patches C#
advanced C# is not faked as editable blocks
no unsupported source is hidden
runtime uses compiled C#
```

### 9.4 SDE as External Definition Compiler

Required:

```txt
SDE standalone build
SDE::Core package
deterministic artifacts
diagnostics
manifests
no Saga core dependency
```

Proof:

```txt
SDE can build without Saga modules
same input produces same output
Saga consumes SDE outputs through contracts
```

### 9.5 Product Shell Routing

Required:

```txt
Edit
Play
Package
Team
Diagnostics
```

Proof:

```txt
shell routes workflows
subsystem failures are visible
shell does not own every subsystem internally
```

### 9.6 Collaboration and Enterprise Foundation

Required:

```txt
local team workspace
semantic transactions
roles
project slices
audit/review/approval
publish blockers
```

Proof:

```txt
unauthorized edits are rejected
restricted data is not only hidden by UI
dangerous operations require approval
publish checks blockers
```

If these are incomplete, public docs must say:

```txt
SagaEngine has a customization-first direction, but the full customization stack is not complete yet.
```

Do not claim maximum customization as shipped until the above gate passes.

---

## 10. Linux Distribution Script Contract

Create:

```txt
scripts/package-linux-saga
```

Expected behavior:

```bash
scripts/package-linux-saga --config Release
```

The script must:

1. run `scripts/verify-quick`,
2. build required targets,
3. stage runtime/editor/server/tool binaries,
4. stage libraries,
5. stage public headers,
6. stage templates/profiles/schemas/plugins,
7. stage StarterArena sample,
8. stage public docs,
9. stage licenses,
10. write `BUILD_INFO.json`,
11. write `KNOWN_LIMITATIONS.md`,
12. run distribution smoke checks,
13. create `Saga.tar.zst`,
14. create `Saga.sha256`.

It must fail if required binaries are missing.

It must fail if public docs contain banned maturity claims.

It must fail if the sample cannot be opened or launched.

It must fail if the package depends on source-tree-only paths.

---

## 11. BUILD_INFO.json Contract

The distribution must include:

```json
{
  "product": "SagaEngine",
  "channel": "technical-preview",
  "platform": "linux",
  "version": "0.0.0",
  "commit": "unknown",
  "buildType": "Release",
  "builtAtUtc": "unknown",
  "includedTargets": [],
  "publicTools": [],
  "samples": [],
  "knownLimitationsFile": "KNOWN_LIMITATIONS.md",
  "verification": {
    "verifyQuick": "unknown",
    "distributionSmoke": "unknown",
    "claimScan": "unknown"
  }
}
```

The script must fill this with real values where possible.

Unknown fields are allowed only when they are honest.

---

## 12. Public Documentation Rules for Distribution

The Linux distribution docs must include:

```txt
GETTING_STARTED.md
CURRENT_CAPABILITIES.md
KNOWN_LIMITATIONS.md
SCRIPTING.md
VISUAL_BLOCKS.md
PACKAGING.md
CUSTOMIZATION.md
```

The docs must not claim:

```txt
production-ready
enterprise-ready
public beta
release candidate
complete editor
full visual scripting
arbitrary C# to blocks
complete MMO
```

Allowed claim after Phase 38:

```txt
SagaEngine is a distributable Linux technical preview.
```

Not allowed after Phase 38 unless separately proven:

```txt
SagaEngine is production-ready.
SagaEngine is enterprise-ready.
SagaEngine has complete visual scripting.
SagaEngine can convert arbitrary C# to blocks.
SagaEngine is a complete MMO engine.
```

---

## 13. Final Acceptance: Distributable Linux Saga

SagaEngine reaches the target state only when all of the following are true:

```txt
[ ] Phase 1-38 are Verified or explicitly marked Archived/Deferred with reason.
[ ] scripts/verify-quick passes.
[ ] scripts/package-linux-saga passes.
[ ] build/dist/linux/Saga/ exists.
[ ] build/dist/linux/Saga.tar.zst exists.
[ ] build/dist/linux/Saga.sha256 exists.
[ ] distribution runs outside the source tree.
[ ] StarterArena opens from the distribution.
[ ] StarterArena plays from the distribution.
[ ] StarterArena packages from the distribution.
[ ] C# scripts compile in the supported workflow.
[ ] Visual Blocks projection works for the supported subset.
[ ] Supported block edit patches C# source safely.
[ ] Unsupported C# appears as read-only/opaque, not fake editable.
[ ] SDE remains standalone.
[ ] Public SDK boundary is clean enough for sample use.
[ ] Editor profiles demonstrate customization.
[ ] Sample plugin demonstrates extension capability.
[ ] Known limitations are visible.
[ ] Public docs have no fake maturity claims.
```

Only then may the project say:

```txt
SagaEngine has a distributable Linux technical preview.
```

---

## 14. Final Positioning

The final product direction is:

```txt
SagaEngine is a customization-first game engine and toolchain technical preview.

It uses C# as canonical source for scripting, Visual Blocks as a source-preserving
projection over compatible code, SDE as a standalone deterministic definition
compiler, and a product shell that routes editor, runtime, package, team, and
diagnostics workflows.

Its long-term direction is server-authoritative multiplayer, deep editor
customization, source-preserving visual authoring, and collaboration/enterprise
workflows backed by evidence rather than claims.
```

Short version for public docs after verification:

```txt
SagaEngine is a Linux-distributable technical preview of a customization-first
game engine and toolchain.
```

Do not make the public claim stronger than the verification evidence.
