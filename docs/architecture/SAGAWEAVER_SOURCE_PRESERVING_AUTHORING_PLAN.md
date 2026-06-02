# SagaWeaver Source-Preserving Authoring Architecture and Implementation Plan

## Document Status

**Status:** Product-technical architecture / implementation planning specification
**Intended repository location:** `docs/architecture/SAGAWEAVER_SOURCE_PRESERVING_AUTHORING_PLAN.md`
**Related systems:** SagaEngine, Saga Editor, Saga Project Model, Saga Runtime, Saga Publish Gate, Saga Workspace, SagaViewKit, SagaPolicyKit, SagaDocGuard
**Primary goal:** Define how SagaWeaver should be built as the dedicated source-preserving C# ↔ Visual Blocks toolchain for SagaEngine.

---

# 1. Executive Summary

SagaWeaver is the toolchain responsible for making SagaEngine's most important authoring promise technically real:

```text
Programmers keep real C# source.
Designers can work through visual blocks.
The editor does not rewrite or damage user code.
Runtime executes compiled C#, not visual graphs.
Publish, diagnostics, and collaboration can trust deterministic script artifacts.
```

SagaWeaver is not a general visual scripting toy. It is not a C# generator. It is not a replacement for Rider, Visual Studio, VS Code, OmniSharp, or a full language server.

SagaWeaver is a **source-preserving authoring pipeline**.

Its first serious proof is deliberately small:

```text
DoorLogic.cs
    -> analyze
    -> discover [SagaBehavior]
    -> generate read-only block projection
    -> verify no-edit roundtrip keeps bytes identical
    -> patch "key" -> "gold_key" through exact source span replacement
    -> verify unaffected bytes remain identical
    -> verify edited C# still compiles
    -> emit runtime_bindings.json
    -> emit deterministic diagnostics
```

If this proof works, Saga has a credible foundation for Simple Blocks View, Pro Graph View, C# Source View, source diff/review, publish validation, and future collaboration transactions.

If this proof fails, Saga should not expand into a larger visual scripting system.

---

# 2. Product Claim

Allowed claim after the first successful SagaWeaver proof:

```text
SagaWeaver can analyze Saga-compatible C# behavior source, generate a read-only visual block projection, preserve source exactly when no edit occurs, apply a narrow supported literal patch through exact source spans, regenerate deterministic script metadata, and emit runtime binding diagnostics.
```

Forbidden claims:

```text
full visual scripting
arbitrary C# to blocks
perfect C# <-> blocks conversion
generated C# replacement workflow
runtime graph interpreter
full IDE
full C# language server
full refactoring engine
security boundary
enterprise source redaction by itself
Unity/Unreal visual scripting replacement
```

Correct product sentence:

```text
SagaWeaver lets visual blocks make C# more approachable without making C# less trustworthy.
```

---

# 3. Core Principles

## 3.1 C# Source Is Canonical

For script-backed gameplay behavior, the canonical source is:

```text
Scripts/**/*.cs
```

Not:

```text
projection_report.json
source_map.json
behavior_ir.json
node_metadata.json
runtime_bindings.json
.sagablocks.json
editor layout metadata
```

Generated artifacts may describe, index, project, validate, bind, or review C# source. They must not become the primary truth for user-authored behavior.

## 3.2 Projection, Not Replacement

Visual blocks are projections over C# source.

Correct model:

```text
C# Source
    -> Roslyn syntax/semantic model
    -> SagaWeaver analysis
    -> source spans + compatibility classification
    -> projection model
    -> visual blocks
    -> optional supported source patch
    -> same C# source file, minimally modified
```

Wrong model:

```text
Blocks
    -> regenerate C# file
    -> replace programmer's source
```

## 3.3 No Visual Edit Means No Source Mutation

Opening, inspecting, projecting, or closing block view must not mutate C# source.

This must be tested byte-for-byte.

Required invariant:

```text
input_bytes == output_bytes after no-edit projection
```

## 3.4 Patch The Smallest Safe Span

Supported block edits must patch the smallest valid source span.

Allowed early patch:

```text
"key" -> "gold_key"
```

Forbidden early behavior:

```text
rewrite method
rewrite class
normalize formatting
reorder using statements
remove comments
regenerate file
silently modify unsupported region
```

## 3.5 Unsupported C# Is Normal

SagaWeaver must not treat unsupported C# as failure by default.

Correct behavior:

```text
advanced C# remains valid C#
unsupported region becomes opaque/read-only in visual projection
diagnostic explains why it cannot be edited visually
source link is preserved
```

## 3.6 Runtime Does Not Interpret Visual Graphs

Runtime path should be:

```text
C# source
    -> compile
    -> C# assembly
    -> runtime_bindings.json
    -> cached function/delegate handles
    -> C++ native engine systems for hot paths
```

Runtime should not execute visual block graphs as the normal gameplay hot path.

## 3.7 Tools Communicate Through Versioned Artifacts

SagaWeaver should emit deterministic artifacts consumed by editor, publish gate, build tools, CI, and collaboration systems.

It should not directly depend on Qt widgets, editor panels, Diligent rendering, runtime internals, or workspace server implementation.

---

# 4. Non-Goals

SagaWeaver is not:

```text
a full IDE
a full language server
a debugger
a refactoring engine
a NuGet project manager
a runtime graph interpreter
a visual editor UI
a collaboration server
a permissions/security system
a complete C# to visual blocks converter
a source code generator that replaces user C#
```

SagaWeaver may produce artifacts used by those systems, but it must not own them.

---

# 5. Repository Strategy

## 5.1 Monorepo First

SagaWeaver should start inside the SagaEngine monorepo.

Recommended layout:

```text
Tools/
  SagaWeaver/
    src/
      SagaWeaver.Core/
      SagaWeaver.Saga/
      SagaWeaver.Cli/
    tests/
      SagaWeaver.Core.Tests/
      SagaWeaver.Saga.Tests/
      SagaWeaver.Cli.Tests/
    fixtures/
      DoorLogic/
      Roundtrip/
      Patch/
      Unsupported/
      NodeExtraction/
      RuntimeBinding/
```

## 5.2 Core / Saga / CLI Split

```text
SagaWeaver.Core
    reusable C# source analysis, source map, patch, diagnostic primitives

SagaWeaver.Saga
    Saga-specific attributes, behavior profile, node rules, runtime binding schema

SagaWeaver.Cli
    command-line interface and artifact output
```

## 5.3 Separate Repo Later

SagaWeaver may become a separate repository only if:

```text
1. SagaWeaver.Core works without private SagaEngine internals.
2. CLI behavior is stable.
3. JSON schemas are versioned.
4. Tests run independently.
5. Saga integration is an adapter, not a private-header dependency.
6. There is real reuse value outside SagaEngine.
```

Do not create a separate repository before the first source-preserving proof works.

---

# 6. Recommended Technology

## 6.1 Language

```text
C#
```

Reason:

```text
C# source analysis should use the C# ecosystem.
Roslyn is the practical parser/semantic model foundation.
CLI tooling and JSON output are straightforward.
Editor/build integration remains clean through artifacts.
```

## 6.2 Core Dependencies

Recommended:

```text
Microsoft.CodeAnalysis.CSharp
System.CommandLine
System.Text.Json
Microsoft.Extensions.Logging
xUnit or NUnit
Verify or snapshot-style golden testing where appropriate
```

Optional later:

```text
Spectre.Console for readable CLI output
JsonSchema.Net or NJsonSchema for schema validation
DiffPlex for readable diff previews
```

Avoid early:

```text
GUI dependencies
Qt bindings
runtime engine linking
large web stack
LLM-dependent code analysis
custom C# parser
```

---

# 7. Major Components

## 7.1 Analyzer

Purpose:

```text
Read C# scripts, parse them, build semantic model if possible, discover Saga behavior and node declarations, classify supported and unsupported regions.
```

Responsibilities:

```text
load script files
detect parse errors
detect compile/semantic errors when references are available
find [SagaBehavior] methods
find [SagaNode] methods
find [SagaLibrary] classes
collect source spans
collect symbol identities
emit analysis_report.json
emit diagnostics
```

## 7.2 Compatibility Classifier

Purpose:

```text
Classify which C# regions are editable, projectable, opaque, or invalid for Saga Blocks V1.
```

Initial classifications:

```text
FullyProjectable
PartiallyProjectable
OpaqueSupportedCSharp
UnsupportedInvalid
ParseFailed
CompileFailed
```

Initial supported constructs:

```text
method declaration marked [SagaBehavior]
if statement
simple boolean condition
method call
string literal
numeric literal
boolean literal
simple enum literal
simple return where safe
```

Initial opaque constructs:

```text
LINQ
lambda
async/await
reflection
dynamic
unsafe
generic-heavy constructs
complex pattern matching
local functions
preprocessor-heavy regions
exception-heavy control flow
```

Opaque does not mean broken. It means not safely visually editable.

## 7.3 Projection Engine

Purpose:

```text
Generate a visual block projection model from supported C# regions.
```

Output:

```text
projection_report.json
```

Required fields:

```text
schemaVersion
sourceFiles
behaviors
blocks
node ids
source span references
edit capabilities
opaque region summaries
diagnostics
```

Rules:

```text
Projection must not mutate source.
Projection must not hide important behavior.
Projection must not pretend opaque code is simple logic.
Projection output must be deterministic.
```

## 7.4 Source Map Generator

Purpose:

```text
Map projected blocks to exact source byte spans.
```

Output:

```text
source_map.json
```

Required fields:

```text
schemaVersion
source file path
content hash
line/column span
byte start/end
syntax kind
semantic kind
node id
patch capability
base version/hash
```

Source maps must be precise enough for exact patching.

## 7.5 Patch Engine

Purpose:

```text
Turn supported visual edits into minimal source patches.
```

Initial supported patch:

```text
ReplaceStringLiteral
```

Near-term supported patches:

```text
ReplaceNumericLiteral
ReplaceBooleanLiteral
ReplaceEnumValue
ReplaceSimpleFunctionArgument
ReplaceSimpleConditionOperator
```

Patch process:

```text
1. Read patch_request.json.
2. Verify source file exists.
3. Verify source hash/base version.
4. Resolve node id to source span.
5. Verify patch capability.
6. Validate replacement text.
7. Produce patch_preview.json.
8. If --apply is passed, patch only the exact span.
9. Re-run analysis.
10. Emit updated diagnostics.
```

Default behavior:

```text
patch command previews only
source is modified only with explicit --apply
```

## 7.6 Node Metadata Extractor

Purpose:

```text
Extract visual node metadata from Saga C# API declarations.
```

Inputs:

```text
[SagaLibrary]
[SagaNode]
method signatures
parameter metadata
runtime tier metadata
availability metadata
```

Output:

```text
node_metadata.json
```

Rules:

```text
invalid node signatures produce diagnostics
metadata output is stable
node ids are deterministic
explicit [SagaId] may stabilize ids later
```

## 7.7 Runtime Binding Generator

Purpose:

```text
Generate deterministic runtime binding metadata for compiled C# behaviors and nodes.
```

Output:

```text
runtime_bindings.json
```

Required fields:

```text
schemaVersion
assembly identity
behavior id
function id
type name
method name
signature
runtime tier
node id -> function id mapping
diagnostics
```

Rules:

```text
no per-frame reflection requirement
stable function ids
stale metadata detection
missing binding diagnostics
publish-gate friendly output
```

## 7.8 Diagnostics System

Purpose:

```text
Explain parse, compile, compatibility, projection, patch, source preservation, node metadata, binding, publish, and visibility issues.
```

Diagnostic shape:

```json
{
  "code": "SAGA_SCRIPT_0042",
  "severity": "Warning",
  "category": "Compatibility",
  "message": "This method uses a LINQ lambda and cannot be expanded into editable Blocks V1.",
  "path": "Scripts/DoorLogic.cs",
  "line": 42,
  "column": 17,
  "behaviorId": "DoorLogic.OnPlayerInteract(Player)",
  "suggestedAction": "Keep this region as opaque C# or rewrite it using supported Saga-compatible constructs."
}
```

Severity levels:

```text
Trace
Info
Warning
Error
Fatal
```

---

# 8. Generated Artifacts

Recommended output tree:

```text
Build/
  SagaScript/
    analysis_report.json
    compatibility_report.json
    projection_report.json
    source_map.json
    behavior_ir.json
    node_metadata.json
    runtime_bindings.json
    patch_preview.json

  Reports/
    sagascript_diagnostics.json
```

Canonical source remains:

```text
Scripts/**/*.cs
```

Optional visual layout metadata:

```text
.saga/
  VisualBlocks/
    **/*.sagablocks.json
```

Visual layout metadata may store node positions and collapsed state. It must not store behavior truth.

---

# 9. CLI Design

Recommended command:

```bash
sagascript
```

## 9.1 analyze

```bash
sagascript analyze \
  --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj \
  --scripts samples/MultiplayerSandbox/Scripts \
  --out Build/SagaScript \
  --diagnostics Build/Reports/sagascript_diagnostics.json
```

Responsibilities:

```text
parse scripts
classify compatibility
discover behaviors/nodes
emit analysis and compatibility reports
emit diagnostics
```

## 9.2 project-blocks

```bash
sagascript project-blocks \
  --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj \
  --scripts samples/MultiplayerSandbox/Scripts \
  --out Build/SagaScript
```

Responsibilities:

```text
generate projection_report.json
generate source_map.json
preserve source
emit opaque nodes for unsupported code
```

## 9.3 patch

```bash
sagascript patch \
  --source samples/MultiplayerSandbox/Scripts/DoorLogic.cs \
  --source-map Build/SagaScript/source_map.json \
  --edit patch_request.json \
  --preview Build/SagaScript/patch_preview.json
```

Apply explicitly:

```bash
sagascript patch ... --apply
```

Responsibilities:

```text
validate patch request
verify source hash
produce diff preview
apply only exact source span when --apply is set
reject unsafe edits
```

## 9.4 extract-nodes

```bash
sagascript extract-nodes \
  --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj \
  --scripts samples/MultiplayerSandbox/Scripts \
  --out Build/SagaScript/node_metadata.json
```

## 9.5 emit-bindings

```bash
sagascript emit-bindings \
  --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj \
  --assembly Build/Scripts/MultiplayerSandbox.Gameplay.dll \
  --node-metadata Build/SagaScript/node_metadata.json \
  --out Build/SagaScript/runtime_bindings.json
```

## 9.6 validate

```bash
sagascript validate \
  --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj \
  --script-artifacts Build/SagaScript \
  --strict
```

Validates:

```text
analysis report exists
projection report exists
source map exists
node metadata exists
runtime bindings exist
no stale source hash
no fatal diagnostics
```

---

# 10. Core Schemas

## 10.1 patch_request.json

```json
{
  "schemaVersion": 1,
  "sourceFile": "Scripts/DoorLogic.cs",
  "baseContentHash": "sha256:...",
  "edits": [
    {
      "editId": "edit_001",
      "nodeId": "node_has_key_arg_001",
      "operation": "ReplaceStringLiteral",
      "oldValue": "key",
      "newValue": "gold_key"
    }
  ]
}
```

## 10.2 patch_preview.json

```json
{
  "schemaVersion": 1,
  "status": "Previewed",
  "sourceFile": "Scripts/DoorLogic.cs",
  "baseContentHash": "sha256:...",
  "resultContentHash": "sha256:...",
  "edits": [
    {
      "editId": "edit_001",
      "status": "Allowed",
      "byteStart": 182,
      "byteEnd": 187,
      "beforeText": "\"key\"",
      "afterText": "\"gold_key\""
    }
  ],
  "diagnostics": []
}
```

## 10.3 projection_report.json

```json
{
  "schemaVersion": 1,
  "tool": "SagaWeaver",
  "status": "Passed",
  "behaviors": [
    {
      "behaviorId": "DoorLogic.OnPlayerInteract(Player)",
      "sourceFile": "Scripts/DoorLogic.cs",
      "classification": "FullyProjectable",
      "entryBlockId": "block_event_001",
      "blocks": [
        {
          "blockId": "block_event_001",
          "kind": "Event",
          "displayName": "On Player Interact",
          "children": ["block_if_001"]
        }
      ]
    }
  ],
  "diagnostics": []
}
```

## 10.4 source_map.json

```json
{
  "schemaVersion": 1,
  "sourceFiles": [
    {
      "path": "Scripts/DoorLogic.cs",
      "contentHash": "sha256:...",
      "spans": [
        {
          "nodeId": "node_has_key_arg_001",
          "kind": "StringLiteral",
          "byteStart": 182,
          "byteEnd": 187,
          "lineStart": 7,
          "columnStart": 30,
          "lineEnd": 7,
          "columnEnd": 35,
          "patchCapability": "ReplaceStringLiteral"
        }
      ]
    }
  ]
}
```

---

# 11. Editor Integration

Saga Editor consumes SagaWeaver artifacts.

Editor responsibilities:

```text
render Simple Blocks View from projection_report.json
render Pro Graph View from richer projection/IR later
open source file from source span
show diagnostics
show opaque node reasons
request patch preview
show diff
apply patch only after explicit user action
refresh artifacts after patch
```

Editor must not:

```text
parse C# independently for source truth
invent script compatibility rules
rewrite source directly from blocks
mutate project truth through widget state
hide unsupported logic as if it were simple
```

Recommended editor panels:

```text
Behavior Inspector
Simple Blocks View
Pro Graph View
C# Source Link View
Source Diff / Review Panel
Diagnostics Panel
Runtime Binding Status Panel
```

---

# 12. Runtime Integration

Runtime consumes:

```text
compiled C# assembly
runtime_bindings.json
package manifest
script metadata
```

Runtime must not consume:

```text
visual block graph as hot-path execution model
editor layout metadata
patch preview
source map for normal execution
```

Runtime binding rules:

```text
function ids stable
missing method fails deterministically
stale binding blocks publish/runtime startup where required
runtime diagnostics include script binding failures
```

---

# 13. Publish Gate Integration

PublishGate should validate:

```text
sagascript_diagnostics.json exists
node_metadata.json exists when visual nodes are used
runtime_bindings.json exists when C# behaviors are packaged
source_map.json matches current source hashes if patchable visual editing is claimed
no fatal SagaWeaver diagnostics
no stale binding metadata
unsupported regions are either opaque/allowed or blocked by profile
```

PublishGate should block:

```text
missing runtime bindings
stale source hash
invalid node metadata
fatal script diagnostics
unapproved source patch transactions where review is required
```

---

# 14. Collaboration Integration

SagaWeaver does not implement collaboration. It emits artifacts collaboration can trust.

Source patch transaction shape:

```json
{
  "transactionId": "tx_000101",
  "operationType": "SourcePatch",
  "sourceView": "saga.view.simple_blocks",
  "targetArtifact": "Scripts/DoorLogic.cs",
  "patchPreview": "Build/SagaScript/patch_preview.json",
  "baseContentHash": "sha256:...",
  "resultContentHash": "sha256:...",
  "diagnostics": []
}
```

Collaboration rules:

```text
source patches are semantic transactions
locks may be source-file or source-region scoped
review workflow may require approval for patches from non-programmers
permission system may allow opaque node use without source visibility
enterprise source redaction is enforced by WorkspaceHub/PolicyKit, not SagaWeaver alone
```

---

# 15. Testing Strategy

## 15.1 Golden Fixtures

Required fixtures:

```text
DoorLogic_BasicSupported.cs
DoorLogic_WithCommentsAndFormatting.cs
DoorLogic_UnsupportedLinq.cs
DoorLogic_InvalidSyntax.cs
DoorLogic_InvalidNodeSignature.cs
DoorLogic_MultipleBehaviors.cs
DoorLogic_RenameSymbol.cs
DoorLogic_PatchStringLiteral.cs
```

## 15.2 Required Tests

Analyzer tests:

```text
valid behavior discovered
invalid syntax emits parse diagnostic
missing references emits compile diagnostic where applicable
[SagaBehavior] identity stable
[SagaNode] metadata extracted
```

Projection tests:

```text
basic DoorLogic projects to expected blocks
unsupported LINQ becomes opaque/read-only
projection output deterministic
source file not modified
```

Source map tests:

```text
string literal span byte offsets correct
line/column spans correct
source hash stable
node id maps to exact syntax node
```

No-edit roundtrip tests:

```text
input bytes == output bytes after projection
comments preserved
whitespace preserved
using order preserved
unsupported regions preserved
```

Patch tests:

```text
"key" -> "gold_key" patches only literal span
unaffected bytes identical
patch preview does not mutate file
--apply mutates only expected span
stale source hash rejects patch
unsupported edit rejected
rollback restores original bytes
edited source compiles
```

Runtime binding tests:

```text
behavior function id deterministic
missing method emits binding diagnostic
stale node metadata detected
runtime_bindings.json stable across runs
```

CLI tests:

```text
analyze emits expected files
project-blocks emits projection/source_map
patch preview emits patch_preview without mutation
patch --apply modifies only expected source span
validate fails on fatal diagnostics
exit codes deterministic
```

## 15.3 Test Closure Rule

A SagaWeaver phase cannot close with:

```text
looks right
works manually
blocks appear in editor
C# generated successfully once
```

It closes only with:

```text
focused tests
golden fixture output
byte-identical source proof
patch span proof
deterministic diagnostics
runtime binding proof
```

---

# 16. Implementation Phases

## Phase SW0 — SagaWeaver Scope Freeze

Deliverables:

```text
this document accepted
allowed/forbidden claims recorded
first proof fixture selected: DoorLogic.cs
monorepo-first decision recorded
```

Exit criteria:

```text
No arbitrary C# conversion claim.
No runtime graph interpreter claim.
No source regeneration workflow claim.
```

## Phase SW1 — Tool Skeleton

Deliverables:

```text
Tools/SagaWeaver solution
SagaWeaver.Core
SagaWeaver.Saga
SagaWeaver.Cli
unit test projects
fixture directory
basic sagascript --help
```

Exit criteria:

```text
solution builds
test runner works
CLI returns deterministic exit codes
```

## Phase SW2 — Saga Attribute Fixtures

Deliverables:

```text
minimal [SagaBehavior]
minimal [SagaNode]
minimal [SagaLibrary]
DoorLogic.cs fixture
invalid fixture set
```

Exit criteria:

```text
fixtures compile in test environment or compile errors are intentional and diagnosed
```

## Phase SW3 — Analyzer v0

Deliverables:

```text
parse C# files
discover [SagaBehavior]
discover [SagaNode]
emit analysis_report.json
emit diagnostics
```

Exit criteria:

```text
DoorLogic behavior discovered deterministically
invalid syntax diagnostic stable
```

## Phase SW4 — Compatibility Profile v0

Deliverables:

```text
Saga-compatible construct classifier
opaque unsupported region classifier
compatibility_report.json
```

Exit criteria:

```text
simple if/call/literal supported
LINQ/lambda classified opaque
unsupported region does not fail whole file
```

## Phase SW5 — Read-Only Projection v0

Deliverables:

```text
projection_report.json
supported blocks: event, if, call, literal
opaque node metadata
projection diagnostics
```

Exit criteria:

```text
DoorLogic projects to expected block tree
unsupported construct becomes opaque node
source file is not modified
```

## Phase SW6 — Source Map v0

Deliverables:

```text
source_map.json
content hash
byte offsets
line/column spans
patch capability metadata
```

Exit criteria:

```text
literal source span is exact
source map deterministic
```

## Phase SW7 — No-Edit Roundtrip Proof

Deliverables:

```text
byte-identical roundtrip test harness
comment/whitespace/using-order preservation tests
```

Exit criteria:

```text
input bytes == output bytes after analyze/project-blocks/open-close simulation
```

## Phase SW8 — Patch Preview v0

Deliverables:

```text
patch_request.json schema
patch_preview.json schema
ReplaceStringLiteral preview
source hash validation
```

Exit criteria:

```text
preview shows exact span replacement
preview does not mutate source
stale hash rejects patch
```

## Phase SW9 — Patch Apply v0

Deliverables:

```text
--apply support
minimal source span replacement
rollback helper
post-patch analysis
```

Exit criteria:

```text
"key" -> "gold_key" changes only literal bytes
unaffected bytes identical
edited source compiles
rollback restores original source
```

## Phase SW10 — Node Metadata Extraction v0

Deliverables:

```text
[SagaLibrary] extraction
[SagaNode] extraction
node_metadata.json
invalid node diagnostics
```

Exit criteria:

```text
valid node metadata deterministic
invalid signature fails clearly
```

## Phase SW11 — Runtime Binding Manifest v0

Deliverables:

```text
runtime_bindings.json
behavior function ids
node function ids
missing method diagnostics
stale metadata detection
```

Exit criteria:

```text
DoorLogic binding manifest deterministic
missing method fails deterministically
```

## Phase SW12 — Publish Validation Bridge

Deliverables:

```text
sagascript validate
strict mode
publish-friendly script status summary
```

Exit criteria:

```text
missing runtime bindings fail
fatal diagnostics fail
valid DoorLogic package script state passes
```

## Phase SW13 — Editor Artifact Contract

Deliverables:

```text
editor consumption contract
Behavior Inspector required fields
Simple Blocks required fields
Source Diff required fields
opaque node display contract
```

Exit criteria:

```text
editor can consume artifacts without parsing C# independently
```

## Phase SW14 — Collaboration Transaction Contract

Deliverables:

```text
source patch transaction schema
review metadata placeholder
lock scope recommendation
policy visibility metadata placeholder
```

Exit criteria:

```text
patch preview can be represented as reviewable transaction
```

## Phase SW15 — SagaWeaver MVP Closure

Deliverables:

```text
closure report
implemented/deferred matrix
allowed claims
blocked claims
known debt
next expansion decision
```

Exit criteria:

```text
DoorLogic proof passes end-to-end
no-edit source preservation proven
minimal patch proven
runtime binding proven
publish validation bridge proven
```

---

# 17. Expansion After MVP

Only after SW15 passes:

```text
more literals and enum patches
simple method-call insertion/removal
simple condition operator patch
more built-in node extraction
behavior diffing
composite high-level node expansion
source visibility metadata for project slices
reviewable script change bundles
performance policy diagnostics
editor graph layout persistence
package-level script compatibility reports
```

Do not expand before source trust is stable.

---

# 18. Major Risks

## 18.1 Source Trust Failure

Risk:

```text
SagaWeaver rewrites or damages C# source.
```

Mitigation:

```text
no-edit byte-identical tests
minimal patch tests
no whole-file rewrite
preview before apply
```

## 18.2 Arbitrary C# Conversion Trap

Risk:

```text
Project tries to convert every C# construct into blocks.
```

Mitigation:

```text
Saga-compatible profile
opaque nodes
explicit forbidden claims
```

## 18.3 Runtime Graph Interpreter Drift

Risk:

```text
Blocks become runtime execution model.
```

Mitigation:

```text
compiled C# runtime path
runtime_bindings.json
cached handles
native C++ hot paths
```

## 18.4 Editor Coupling

Risk:

```text
SagaWeaver depends on Qt/editor widgets.
```

Mitigation:

```text
JSON artifacts only
no UI dependency in Core/Saga/Cli
```

## 18.5 Engine Coupling

Risk:

```text
SagaWeaver links directly to runtime engine internals.
```

Mitigation:

```text
consume API metadata
emit binding artifacts
runtime consumes artifacts
```

## 18.6 Identity Instability

Risk:

```text
Behavior/node ids change across runs.
```

Mitigation:

```text
stable symbol identity
source path + symbol signature
optional explicit [SagaId]
rename/move diagnostics
```

## 18.7 Diagnostics Uselessness

Risk:

```text
Diagnostics say "cannot convert" without technical reason.
```

Mitigation:

```text
stable code
category
source span
technical reason
suggested action
related behavior/node id
```

---

# 19. Success Criteria

SagaWeaver is successful when:

```text
C# source trust is preserved.
Read-only visual projection works.
No-edit roundtrip is byte-identical.
Minimal supported block edits patch exact source spans.
Advanced C# remains valid and visible as opaque/read-only nodes.
Runtime binding metadata is deterministic.
Editor does not guess script compatibility.
Publish gate can validate script state.
Collaboration can audit source patch transactions.
Programmers trust Saga will not rewrite their code unexpectedly.
Designers can safely edit supported gameplay parameters visually.
```

SagaWeaver is not successful merely because:

```text
a block graph appears on screen
a demo works once
editor can generate C# from scratch
runtime can interpret blocks
```

The decisive measure:

```text
source preservation + deterministic metadata + runtime-safe binding
```

---

# 20. Final Decision

SagaWeaver should be built as SagaEngine's dedicated source-preserving C# ↔ Visual Blocks authoring toolchain.

It should start inside the SagaEngine monorepo but be structured as if it may become a separate repository later.

Its role:

```text
Analyze C#.
Classify Saga-compatible and advanced regions.
Project safe regions into visual blocks.
Preserve source exactly when no edit occurs.
Patch only minimal source spans when supported edits occur.
Expose advanced C# as opaque/read-only nodes.
Generate deterministic node metadata and runtime binding manifests.
Emit diagnostics that editor, publish, and collaboration systems can trust.
```

Its boundary:

```text
Not the editor UI.
Not runtime graph execution.
Not a full IDE.
Not arbitrary C# to blocks.
Not the collaboration server.
Not the enterprise security boundary.
```

Engineering commandment:

```text
Never sacrifice source trust for visual convenience.
```

Product commandment:

```text
Blocks must make C# more approachable without making C# less trustworthy.
```

Execution commandment:

```text
Do not build a bigger scripting fantasy. Build the smallest source-preserving proof that can survive tests.
```
