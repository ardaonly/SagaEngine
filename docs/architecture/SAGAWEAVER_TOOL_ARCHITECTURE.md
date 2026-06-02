# SagaWeaver Tool Architecture

## Document Status

**Status:** Product-technical architecture document / implementation planning specification
**Scope:** C# source analysis, source-preserving visual block projection, minimal source patching, node metadata extraction, runtime binding manifest generation, diagnostics, editor/build integration, and future collaboration readiness
**Intended repository location:** `docs/architecture/SAGAWEAVER_TOOL_ARCHITECTURE.md`
**Related systems:** SagaScript, Saga Editor, Saga Project Model, Saga Runtime, Saga Publish Gate, Saga Collaborative Workspace, Forge, Prism, SDE
**Primary goal:** Define a serious and realistic toolchain that makes SagaEngine's C# ↔ Visual Blocks model possible without destroying source trust, runtime performance, editor consistency, or long-term product architecture.

---

# 1. Executive Summary

SagaWeaver is a proposed compiler/editor tooling layer for SagaEngine.

Its purpose is not to create a toy visual scripting system. Its purpose is to make this model technically possible:

```text
C# source is canonical.
Visual blocks are a source-preserving projection.
Block edits become minimal source patches.
Advanced C# remains supported as opaque code.
Runtime executes compiled C#, not interpreted block graphs.
Editor views consume generated projection metadata.
Publish/collaboration systems consume deterministic diagnostics and transactions.
```

SagaWeaver exists because SagaEngine's long-term product vision depends on a difficult promise:

> Designers should be able to work visually without forcing programmers to abandon real C# source code.

That promise is easy to say and hard to implement. Without a dedicated toolchain, the system will likely collapse into one of these failures:

```text
1. Blocks become a separate toy language detached from runtime behavior.
2. Opening block view rewrites or damages user C#.
3. The editor shows simplified logic that lies about what the code really does.
4. Runtime interprets graphs in hot paths and loses performance.
5. Collaboration cannot understand changes because edits are UI gestures instead of semantic source/project transactions.
6. Publish gates cannot validate script compatibility deterministically.
7. Visual and code workflows diverge into separate realities.
```

SagaWeaver prevents that by becoming the single responsible pipeline for:

```text
C# analysis
Saga-compatible profile classification
source-span mapping
visual projection model generation
minimal patch generation
node metadata extraction
runtime binding metadata generation
script diagnostics
publish/collaboration integration artifacts
```

The tool should start small. The first target is not a full visual scripting editor.

The first target is:

```text
DoorLogic.cs
    ↓ analyze
[SagaBehavior] method discovered
    ↓ project
read-only block model generated
    ↓ roundtrip
no source mutation, byte-identical
    ↓ patch
"key" → "gold_key" changes only that literal span
    ↓ validate
edited C# still compiles
    ↓ bind
runtime binding manifest generated
```

If this proof works, SagaEngine gains a credible foundation for source-preserving visual authoring. If it does not work, the larger Scratch-like / Unreal-like / C# authoring vision should not proceed.

---

# 2. Why SagaWeaver Exists

## 2.1 The Problem

SagaEngine wants a very specific kind of authoring system:

```text
Scratch-like high-level blocks for beginners/designers
Unreal-like pro graph for technical users
C# source for programmers
low-level runtime/diagnostic visibility for engine users
custom editor views for different users and roles
```

The problem is that these views must not become separate sources of truth.

A bad architecture would create separate systems:

```text
VisualGraph.behavior
DoorLogic.cs
RuntimeBinding.json
EditorBlockLayout.json
CollaborationTransaction.json
```

where each file gradually represents a different version of the same behavior.

That creates long-term corruption:

```text
Blocks show one thing.
C# does another.
Runtime binds a third thing.
Collaboration audits a fourth thing.
Publish gate validates stale metadata.
```

SagaWeaver prevents this by making the C# source file the primary truth for script-backed behavior.

## 2.2 The Core Danger

The most dangerous failure is not a compiler error.

The most dangerous failure is a trust failure.

If a programmer opens a visual block view and Saga rewrites the file, removes comments, changes formatting, reorders `using` statements, normalizes whitespace, or alters unsupported regions, the feature is dead.

The core rule:

```text
No block edit → no source mutation.
```

This must be tested byte-for-byte.

## 2.3 The Product Reason

SagaWeaver is not just an internal convenience tool. It supports SagaEngine's product identity.

The intended product statement:

```text
Saga lets designers work in visual blocks while programmers keep real C#.
Blocks are source-preserving projections, not a separate scripting language.
Runtime executes compiled C# and native engine systems, not visual graphs in hot paths.
```

That positioning is much stronger than:

```text
Saga has visual scripting.
```

Almost every engine can claim some form of visual scripting. Saga's differentiator should be:

```text
source-preserving visual projection over real C#
```

---

# 3. Non-Goals

SagaWeaver must avoid scope traps.

## 3.1 Not A Full IDE Replacement

SagaWeaver is not a replacement for:

```text
Rider
Visual Studio
VS Code
OmniSharp
full language server infrastructure
advanced refactoring engine
C# debugger
NuGet ecosystem manager
```

Early versions should only provide:

```text
analysis
projection
diagnostics
source-span mapping
minimal patching
node metadata
runtime binding metadata
```

## 3.2 Not Arbitrary C# To Blocks

SagaWeaver must not claim:

```text
Every C# program can become editable blocks.
```

Correct claim:

```text
Saga-compatible C# regions can be projected into blocks.
Advanced C# remains valid C# and may appear as opaque/read-only nodes.
```

Unsupported C# is not a failure. It is normal.

## 3.3 Not Runtime Graph Interpretation

SagaWeaver must not create a runtime model where hot gameplay code is interpreted as visual graph nodes per frame.

Correct runtime path:

```text
C# source
    ↓ compile
C# assembly
    ↓ runtime binding manifest
cached function/delegate calls
    ↓
C++ engine systems for hot paths
```

## 3.4 Not A Visual Editor UI

SagaWeaver is not the block editor itself.

It produces artifacts consumed by the editor:

```text
projection model
source map
diagnostics
node metadata
binding manifest
patch preview
```

The Saga Editor renders and edits. SagaWeaver analyzes and patches.

## 3.5 Not A General-Purpose Collaboration System

SagaWeaver does not implement presence, locks, Team Room, roles, audit, or project slices.

It should produce artifacts that collaboration can use:

```text
source patch transactions
diagnostic reports
behavior/node identities
source region metadata
reviewable diffs
```

## 3.6 Not A Security Boundary By Itself

SagaWeaver may support source visibility levels and metadata-only node exposure, but enterprise security must be enforced by workspace/project-slice services.

SagaWeaver can help produce safe metadata. It cannot by itself guarantee that unauthorized clients never receive sensitive data.

---

# 4. Core Architecture Decision

## 4.1 Canonical Source

For script-backed gameplay behavior, the canonical source is:

```text
C# source file
```

Not:

```text
visual graph file
behavior IR file
generated JSON
editor layout metadata
```

## 4.2 Projection, Not Replacement

Visual blocks are a projection over C# source.

Correct mental model:

```text
C# Source
    ↓ Roslyn parse/semantic model
SagaWeaver Analysis
    ↓ source spans + classification
Saga Projection Model
    ↓
Visual Blocks
    ↓ optional supported edits
Minimal Source Patch
    ↓
C# Source
```

## 4.3 IR Role

Saga Behavior IR may exist, but it is not the text authority.

IR is used for:

```text
semantic analysis
validation
diagnostics
block projection
runtime metadata generation
publish checks
optional preview/simulation
```

IR must not become the primary source that regenerates user C#.

## 4.4 Runtime Rule

Runtime should execute:

```text
compiled C#
cached delegates / function handles
native C++ engine hot-path systems
precomputed binding metadata
```

Runtime should not execute:

```text
visual block graphs as the normal hot path
string-based node lookup every frame
reflection per node execution
heap-heavy dynamic dispatch per node
```

---

# 5. Tool Positioning

## 5.1 Name

Recommended internal architecture name:

```text
SagaWeaver
```

Recommended CLI/user-facing command name:

```text
sagascript
```

Reason:

```text
SagaWeaver describes the actual role:
    weaving C# source, block projection, source patching, node metadata, and runtime binding.

sagascript is easier as a command-line tool name:
    sagascript analyze
    sagascript project-blocks
    sagascript patch
    sagascript emit-bindings
```

## 5.2 Repository Position

Early implementation should live inside the SagaEngine monorepo:

```text
Tools/SagaScript/
```

or:

```text
Tools/SagaWeaver/
```

Recommended initial layout:

```text
Tools/
  SagaScript/
    src/
      SagaWeaver.Core/
      SagaWeaver.Saga/
      SagaWeaver.Cli/
    tests/
      SagaWeaver.Core.Tests/
      SagaWeaver.Saga.Tests/
    fixtures/
      Roundtrip/
      Patch/
      NodeExtraction/
      Unsupported/
```

## 5.3 Separate Repository Later

Do not start as a separate repository.

A separate repository becomes reasonable only after these proofs pass:

```text
1. No-edit C# → blocks → C# byte-identical roundtrip.
2. Minimal literal patch proof.
3. [SagaNode] metadata extraction.
4. Runtime binding manifest generation.
5. Deterministic diagnostic report.
```

Before those exist, a separate repository adds overhead without product value.

## 5.4 Reusable Core, Saga-Specific Adapter

The architecture should be split:

```text
SagaWeaver.Core
    reusable C# analysis/projection/patching primitives

SagaWeaver.Saga
    Saga-specific attributes, types, rules, diagnostics, runtime binding

SagaWeaver.Cli
    command-line interface and artifact generation

SagaEditor integration
    consumes projection/source-map/diagnostic artifacts

SagaRuntime integration
    consumes runtime binding metadata
```

This prevents the tool from becoming too tightly coupled to the engine while still serving Saga first.

---

# 6. Relationship To Existing Saga Tools

## 6.1 Forge

Forge is the build/gate orchestrator.

SagaWeaver should integrate through Forge as a tool/gate:

```bash
forge gate run \
  --name sagascript \
  --tool sagascript \
  --diagnostics Build/Reports/sagascript_diagnostics.json
```

Forge should not become the analyzer.

Correct split:

```text
Forge = runs tools, collects reports, enforces gates.
SagaWeaver = analyzes C#, projects blocks, emits script artifacts.
```

## 6.2 Prism

Prism is code/repository analysis and snapshot extraction.

SagaWeaver should not become Prism.

Correct split:

```text
Prism = broad code/repo snapshot and analysis graph.
SagaWeaver = C# gameplay script projection and patching.
```

Possible future integration:

```text
Prism may ingest SagaWeaver diagnostics and projection artifacts.
Prism may show script behavior graphs in a repository-level analysis view.
```

## 6.3 SDE

SDE is system/model/schema definition.

SagaWeaver should not become SDE.

Correct split:

```text
SDE = project/system definition and model graph.
SagaWeaver = C# source/blocks/binding tooling.
```

Possible future integration:

```text
SDE may define behavior schemas, project rules, or view capability metadata.
SagaWeaver may consume generated schema metadata.
```

But SagaWeaver must not require SDE to parse a C# file or produce basic diagnostics.

---

# 7. Major Components

## 7.1 SagaWeaver Analyzer

### Purpose

Parse C# scripts and classify methods, classes, expressions, statements, and attributes.

### Responsibilities

```text
Read script files.
Parse using Roslyn.
Build syntax and semantic models.
Find [SagaBehavior], [SagaNode], [SagaLibrary], and related attributes.
Classify supported, partially supported, opaque, unsupported, and invalid regions.
Build source-span mappings.
Emit diagnostics.
Emit behavior and node descriptors.
```

### Inputs

```text
Scripts/**/*.cs
script manifest
Saga API metadata
node library metadata
analyzer configuration
compiler references
language version
project compatibility profile
```

### Outputs

```text
analysis_report.json
compatibility_report.json
sagascript_diagnostics.json
```

---

## 7.2 SagaWeaver Projection

### Purpose

Convert classified C# regions into a visual block projection model.

### Responsibilities

```text
Represent [SagaBehavior] methods as event/flow/call/condition blocks.
Represent supported expressions as editable block structures.
Represent unsupported regions as opaque/read-only nodes.
Attach source-span links to every projected node.
Attach type information and edit capability.
Preserve stable node identity where possible.
```

### Output

```text
projection_report.json
```

Example projection shape:

```json
{
  "schemaVersion": 1,
  "behaviors": [
    {
      "behaviorId": "DoorLogic.OnPlayerInteract(Player)",
      "sourceFile": "Scripts/DoorLogic.cs",
      "classification": "FullyProjectable",
      "entryBlock": {
        "kind": "Event",
        "name": "On Player Interact",
        "children": [
          {
            "kind": "If",
            "condition": {
              "kind": "Call",
              "target": "player.Inventory.Has",
              "arguments": [
                {
                  "kind": "StringLiteral",
                  "value": "key",
                  "editCapability": "PatchLiteral"
                }
              ]
            },
            "then": [
              {
                "kind": "Call",
                "target": "Door.Open"
              }
            ]
          }
        ]
      }
    }
  ]
}
```

---

## 7.3 SagaWeaver Source Map

### Purpose

Map projected blocks back to exact C# source spans.

### Responsibilities

```text
Track source file path.
Track byte offsets.
Track line/column ranges.
Track syntax node kind.
Track semantic operation kind.
Track editable spans.
Track unsafe patch regions.
Track source hash/version.
```

### Output

```text
source_map.json
```

Example:

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
          "text": "\"key\"",
          "patchCapability": "ReplaceLiteral"
        }
      ]
    }
  ]
}
```

---

## 7.4 SagaWeaver Patch Engine

### Purpose

Apply supported block edits as minimal source patches.

This is the hardest component.

### Responsibilities

```text
Receive block edit request.
Resolve edit to exact source span.
Validate source hash/base version.
Validate patch capability.
Generate replacement text.
Produce diff preview.
Reject unsafe patch.
Apply patch only to affected span.
Preserve all other bytes exactly.
Re-run analysis after patch.
Emit updated diagnostics.
Support rollback/undo integration.
```

### Initial Supported Edits

```text
Change string literal.
Change numeric literal.
Change boolean literal.
Change enum value.
Change simple function argument.
Change simple condition operator.
```

### Explicitly Deferred Edits

```text
Arbitrary statement insertion.
Complex method rewriting.
Class restructuring.
Whole-file formatting.
Using reorder.
Automatic style normalization.
Arbitrary graph-to-code generation.
```

### Patch Rule

```text
Patch the smallest valid source span.
Never rewrite the whole file.
Never format unrelated code.
Never delete comments outside the edited span.
```

---

## 7.5 SagaWeaver Node Extractor

### Purpose

Extract visual node metadata from C# APIs.

### Responsibilities

```text
Find [SagaLibrary] classes.
Find [SagaNode] methods.
Validate method signatures.
Extract categories, display names, inputs, outputs, types, defaults, descriptions.
Classify runtime tier.
Classify client/server/editor availability.
Emit deterministic node metadata.
Emit diagnostics for invalid node declarations.
```

Example C#:

```csharp
[SagaLibrary("Saga.Inventory")]
public static class InventoryNodes
{
    [SagaNode("Inventory/Give Item")]
    public static void GiveItem(Player player, ItemId item, int amount)
    {
        player.Inventory.Give(item, amount);
    }
}
```

Generated metadata:

```json
{
  "schemaVersion": 1,
  "libraries": [
    {
      "id": "Saga.Inventory",
      "nodes": [
        {
          "id": "Saga.Inventory.GiveItem(Player,ItemId,int)",
          "displayName": "Give Item",
          "category": "Inventory",
          "inputs": [
            { "name": "player", "type": "Player" },
            { "name": "item", "type": "ItemId" },
            { "name": "amount", "type": "int" }
          ],
          "output": null,
          "runtimeTier": "CompiledCSharp"
        }
      ]
    }
  ]
}
```

---

## 7.6 SagaWeaver Runtime Binding Generator

### Purpose

Generate runtime-safe binding metadata so runtime execution does not depend on slow editor graph interpretation.

### Responsibilities

```text
Assign stable function ids.
Resolve behavior methods.
Resolve node APIs.
Validate assemblies and symbols.
Detect stale node metadata.
Detect missing functions.
Emit runtime_bindings.json.
Avoid string lookup in hot paths.
Support cached delegate/function handle binding.
```

### Output

```text
runtime_bindings.json
```

Example:

```json
{
  "schemaVersion": 1,
  "assembly": "Anhos.Gameplay.dll",
  "behaviors": [
    {
      "behaviorId": "DoorLogic.OnPlayerInteract(Player)",
      "functionId": "fn_8B3E0A11",
      "typeName": "DoorLogic",
      "methodName": "OnPlayerInteract",
      "signature": "void(Player)",
      "runtimeTier": "CompiledCSharp"
    }
  ],
  "nodes": [
    {
      "nodeId": "Saga.Inventory.GiveItem(Player,ItemId,int)",
      "functionId": "fn_A17C92D0"
    }
  ]
}
```

---

## 7.7 SagaWeaver Diagnostics

### Purpose

Produce deterministic diagnostics that explain compatibility, source preservation, patch safety, runtime binding, and publish readiness.

### Diagnostic Requirements

Each diagnostic should have:

```text
stable code
severity
source file
line/column or source span
message
technical reason
suggested action
related behavior/node id
machine-readable category
```

Bad diagnostic:

```text
Cannot convert to blocks.
```

Good diagnostic:

```text
SAGA_SCRIPT_0042:
This method uses a LINQ Select lambda at Scripts/DoorLogic.cs:42.
Saga Blocks V1 cannot expand arbitrary lambdas.
The method can still compile and may be exposed as an opaque C# node.
```

### Diagnostic Categories

```text
Parse
Compile
Compatibility
Projection
SourcePreservation
Patch
NodeMetadata
RuntimeBinding
Publish
SecurityVisibility
PerformancePolicy
```

---

# 8. Generated Artifacts

SagaWeaver should write generated artifacts under build/output folders.

Recommended output:

```text
Build/
  SagaScript/
    analysis_report.json
    projection_report.json
    source_map.json
    behavior_ir.json
    node_metadata.json
    runtime_bindings.json
    compatibility_report.json
    patch_preview.json

  Reports/
    sagascript_diagnostics.json
```

These are generated artifacts, not canonical source.

Canonical source remains:

```text
Scripts/**/*.cs
```

Visual layout metadata should remain separate:

```text
.saga/
  VisualBlocks/
    **/*.sagablocks.json
```

---

# 9. CLI Design

## 9.1 Command Overview

Recommended CLI command:

```bash
sagascript
```

Recommended subcommands:

```bash
sagascript analyze
sagascript project-blocks
sagascript patch
sagascript extract-nodes
sagascript emit-bindings
sagascript validate
sagascript report
```

## 9.2 `analyze`

```bash
sagascript analyze \
  --project Anhos.sagaproj \
  --scripts Scripts \
  --out Build/SagaScript \
  --diagnostics Build/Reports/sagascript_diagnostics.json
```

Responsibilities:

```text
Parse scripts.
Classify compatibility.
Emit diagnostics.
Emit analysis/compatibility reports.
```

## 9.3 `project-blocks`

```bash
sagascript project-blocks \
  --scripts Scripts \
  --out Build/SagaScript/projection_report.json
```

Responsibilities:

```text
Generate read-only block projection.
Generate source map.
Preserve source.
```

## 9.4 `patch`

```bash
sagascript patch \
  --source Scripts/DoorLogic.cs \
  --source-map Build/SagaScript/source_map.json \
  --edit patch_request.json \
  --preview Build/SagaScript/patch_preview.json
```

Responsibilities:

```text
Validate patch request.
Produce preview diff.
Apply only if requested.
Reject unsafe edits.
```

Recommended explicit apply flag:

```bash
sagascript patch ... --apply
```

Without `--apply`, it should only preview.

## 9.5 `extract-nodes`

```bash
sagascript extract-nodes \
  --assembly Anhos.Gameplay.dll \
  --scripts Scripts \
  --out Build/SagaScript/node_metadata.json
```

Responsibilities:

```text
Extract [SagaLibrary] and [SagaNode] metadata.
Validate node API signatures.
Emit deterministic metadata.
```

## 9.6 `emit-bindings`

```bash
sagascript emit-bindings \
  --assembly Anhos.Gameplay.dll \
  --node-metadata Build/SagaScript/node_metadata.json \
  --out Build/SagaScript/runtime_bindings.json
```

Responsibilities:

```text
Generate runtime binding manifest.
Detect missing/stale methods.
Emit diagnostics.
```

## 9.7 `validate`

```bash
sagascript validate \
  --project Anhos.sagaproj \
  --profile editor-preview
```

Responsibilities:

```text
Run all relevant checks.
Return non-zero exit code on blocking diagnostics.
Produce deterministic report.
```

---

# 10. Integration With Saga Editor

## 10.1 Editor Consumption

Saga Editor should consume:

```text
projection_report.json
source_map.json
node_metadata.json
sagascript_diagnostics.json
```

The editor should not independently guess C# compatibility.

Correct:

```text
Saga Editor asks SagaWeaver for projection/diagnostics.
```

Wrong:

```text
Every editor panel parses C# differently.
```

## 10.2 Block View

Block View should use projection data to render:

```text
event blocks
flow blocks
condition blocks
function call blocks
custom node blocks
opaque code nodes
diagnostic markers
```

## 10.3 Patch Flow

Visual edit flow:

```text
User edits block.
Editor creates patch request.
SagaWeaver validates request.
SagaWeaver produces diff preview.
User confirms.
SagaWeaver applies minimal source patch.
SagaWeaver re-analyzes file.
Editor refreshes projection.
```

## 10.4 Source Trust UX

The editor should show compatibility states:

```text
Fully block-compatible
Read-only block-compatible
Partially compatible
Opaque C# node
Unsupported for blocks
Invalid / compile error
```

If a file is opened in block view and no edit occurs, the editor must not save or touch the C# file.

---

# 11. Integration With Runtime

## 11.1 Runtime Consumption

Runtime should consume:

```text
compiled C# assembly
runtime_bindings.json
package/script metadata
```

Runtime should not consume:

```text
editor visual layout
block graph UI metadata
patch preview files
source maps unless needed for diagnostics
```

## 11.2 Hot Path Policy

The following must remain C++/native or highly optimized:

```text
packet handling
server-authoritative movement
replication filtering
physics loop
large transform update loops
low-level networking session management
low-level memory/resource tracking
```

C# behavior may:

```text
configure systems
respond to events
call engine APIs
run low-frequency gameplay logic
dispatch scripted events
```

C# behavior should not:

```text
replace server tick movement core
perform per-packet logic
allocate heavily per frame
perform reflection in hot paths
depend on runtime graph traversal
```

---

# 12. Integration With Publish Gate

SagaWeaver should produce publish-readable diagnostics.

Publish gate should be able to block on:

```text
C# compile failure
missing behavior method
invalid [SagaNode] signature
stale runtime binding
unsupported required behavior
critical script diagnostic
unsafe source patch
missing script artifact
invalid node metadata
hot-path policy violation
```

Publish gate should not block on:

```text
advanced C# that is valid and intentionally opaque
read-only projection limitations
unsupported visual editing when runtime compile path is valid
```

unless project policy requires full block compatibility.

---

# 13. Integration With Collaboration

SagaWeaver supports collaboration indirectly.

## 13.1 Source Patch As Transaction

A visual edit should become:

```text
source-preserving patch
    ↓
semantic script transaction
    ↓
policy validation
    ↓
audit log
    ↓
Team Room recent change
```

Example:

```text
Simple Blocks View:
    Change CoinPickup reward 1 → 5

SagaWeaver:
    Patch literal source span

Collaboration:
    Transaction: SetBehaviorParameter(CoinPickup, scoreReward, 5)

C# View:
    Shows source diff

Team Room:
    Mehmet changed CoinPickup reward: 1 → 5
```

## 13.2 Source Region Permissions

SagaWeaver should expose source region metadata so policy systems can decide:

```text
Can this user edit this behavior?
Can this user patch this C# source region?
Can this user see the implementation?
Can this user use the node only as opaque metadata?
```

## 13.3 Audit

SagaWeaver should provide enough metadata for audit records:

```text
source file
behavior id
node id
old value
new value
source span
diagnostic status
patch hash
runtime binding impact
```

---

# 14. Compatibility Profile V1

## 14.1 Supported Constructs

The V1 Saga-compatible C# profile should be intentionally narrow.

Recommended supported constructs:

```text
[SagaBehavior] method declarations
method parameters with supported Saga types
local variables with simple supported types
if / else
simple foreach over supported collections
direct method calls
property reads/writes
field reads/writes where allowed
constants
enum values
simple arithmetic
simple boolean expressions
return statements
custom node calls through [SagaNode] APIs
```

Recommended supported types:

```text
bool
int
float
double only if explicitly allowed
string with restrictions
EntityId
PlayerRef
AssetRef<T>
ItemId
engine-defined value types
explicitly registered script structs
enums
```

## 14.2 Unsupported Or Opaque Constructs

V1 should reject, mark read-only, or represent as opaque:

```text
complex LINQ
arbitrary lambdas
async/await
reflection
dynamic
unsafe
pointer operations
arbitrary generics
complex inheritance dispatch
local functions
expression trees
preprocessor-heavy regions
unsupported exception flows
arbitrary file/network access in gameplay logic
```

## 14.3 Classification Types

Every region should receive one of:

```text
FullyProjectable
ReadOnlyProjectable
PartiallyProjectable
OpaqueNode
Unsupported
Invalid
```

---

# 15. Source Preservation Rules

## 15.1 No-Edit Roundtrip

For no-edit projection:

```text
input bytes == output bytes
```

No exceptions.

## 15.2 Formatting Rules

SagaWeaver must not:

```text
run whole-file formatting automatically
reorder using directives
normalize line endings
delete comments
delete regions
rewrite unsupported code
rename symbols
change indentation outside patch span
```

## 15.3 Patch Rules

For supported block edits:

```text
compute exact source span
verify source hash
verify patch capability
replace smallest safe span
preserve all other bytes
re-run analysis
emit diagnostics
```

## 15.4 Rejection Is Acceptable

If SagaWeaver cannot safely patch, it should reject.

A clean rejection is better than unsafe mutation.

Example:

```text
Rejected: This condition contains unsupported nested lambda expression.
Open C# Source View to edit.
```

---

# 16. Testing Strategy

SagaWeaver requires unusually strict tests.

## 16.1 Roundtrip Tests

For each fixture:

```text
input.cs
    analyze
    project
    return without edit
output.cs
```

Assert:

```text
input.cs bytes == output.cs bytes
```

Fixture categories:

```text
comments
weird spacing
CRLF
LF
using order
regions
attributes
partial classes
nested classes
supported methods
unsupported methods
expression-bodied members
generic classes
mixed supported/unsupported code
```

## 16.2 Projection Tests

Assert:

```text
if becomes If block
method call becomes Call block
literal becomes Literal block
unsupported code becomes Opaque/Unsupported diagnostic
source spans are correct
node ids are stable
```

## 16.3 Patch Tests

For each supported edit:

```text
only expected source span changed
unaffected byte ranges are identical
edited source compiles
new projection is valid
diagnostics are deterministic
```

## 16.4 Diagnostics Tests

Every diagnostic must have:

```text
stable code
severity
message
source location
suggested action where useful
```

## 16.5 Runtime Binding Tests

Assert:

```text
function ids deterministic
methods resolved
missing methods fail clearly
stale metadata detected
runtime binding manifest stable
hot path avoids string lookup policy
```

## 16.6 Performance Tests

Early performance tests should measure:

```text
analysis time on sample project
projection time
patch time
node extraction time
binding generation time
editor block load artifact size
runtime binding lookup overhead
```

The goal is not MMO-scale early. The goal is to prevent obvious design regressions.

---

# 17. Implementation Stages

## Stage SW-0 — Decision Record

### Goal

Establish SagaWeaver as the official C# ↔ Visual Blocks compiler/editor bridge.

### Deliverables

```text
SAGAWEAVER_TOOL_ARCHITECTURE.md
C# canonical source decision
no-edit byte-identical guarantee
non-goals
first proof definition
```

### Exit Criteria

```text
Team agrees C# is canonical.
Blocks are projection.
Runtime executes compiled C#.
SagaWeaver owns analysis/projection/patch/binding.
```

---

## Stage SW-1 — Project Skeleton

### Goal

Create the tool skeleton.

### Deliverables

```text
Tools/SagaScript/src/SagaWeaver.Core
Tools/SagaScript/src/SagaWeaver.Saga
Tools/SagaScript/src/SagaWeaver.Cli
Tools/SagaScript/tests/SagaWeaver.Core.Tests
Tools/SagaScript/fixtures
```

### Exit Criteria

```text
CLI runs.
Test project builds.
No engine runtime dependency.
Roslyn package wired.
```

---

## Stage SW-2 — Roslyn Parse + Behavior Discovery

### Goal

Parse C# and discover `[SagaBehavior]`.

### Deliverables

```text
C# file loader
Roslyn syntax parse
basic compile diagnostics
attribute discovery
behavior descriptor output
```

### Exit Criteria

```text
DoorLogic.cs behavior discovered.
Missing/invalid attributes produce diagnostics.
Output deterministic.
```

---

## Stage SW-3 — Read-Only Projection V1

### Goal

Project one narrow behavior into block model.

### Supported First Pattern

```csharp
[SagaBehavior]
public void OnPlayerInteract(Player player)
{
    if (player.Inventory.Has("key"))
    {
        Door.Open();
    }
}
```

### Deliverables

```text
projection_report.json
source_map.json
If block
Call block
String literal block
diagnostics
```

### Exit Criteria

```text
Projection generated.
No source file mutation.
Unsupported constructs do not crash.
```

---

## Stage SW-4 — No-Edit Byte-Identical Roundtrip

### Goal

Prove source trust.

### Deliverables

```text
roundtrip test fixtures
byte equality assertions
line-ending preservation
comments/formatting preservation tests
```

### Exit Criteria

```text
input.cs == output.cs for no-edit flow.
```

---

## Stage SW-5 — Minimal Patch Prototype

### Goal

Support first safe visual edit.

### First Edit

```text
"key" → "gold_key"
```

### Deliverables

```text
patch request schema
patch preview
literal replacement
source hash validation
post-patch analysis
```

### Exit Criteria

```text
Only literal span changes.
File compiles.
Projection updates.
Unrelated bytes preserved.
```

---

## Stage SW-6 — Saga-Compatible Profile V1

### Goal

Define and enforce the first compatibility subset.

### Deliverables

```text
supported construct table
unsupported construct diagnostics
classification model
profile tests
```

### Exit Criteria

```text
Each supported construct has tests.
Each known unsupported construct has deterministic diagnostics.
```

---

## Stage SW-7 — Node Metadata Extraction

### Goal

Extract `[SagaLibrary]` and `[SagaNode]`.

### Deliverables

```text
node_metadata.json
signature validation
category extraction
input/output port extraction
invalid node diagnostics
```

### Exit Criteria

```text
Inventory/Give Item node metadata generated.
Invalid node fails clearly.
```

---

## Stage SW-8 — Runtime Binding Manifest

### Goal

Generate runtime-safe binding metadata.

### Deliverables

```text
runtime_bindings.json
function id generator
method resolution
stale binding diagnostics
```

### Exit Criteria

```text
Compiled C# behavior can be resolved by runtime metadata.
No runtime graph interpretation required.
```

---

## Stage SW-9 — Editor Read-Only Integration

### Goal

Saga Editor renders projection.

### Deliverables

```text
Editor block view consumes projection_report.json.
Opaque/unsupported nodes visible.
Diagnostics inline.
Source location navigation.
```

### Exit Criteria

```text
DoorLogic opens in read-only blocks.
No source mutation.
```

---

## Stage SW-10 — Editable Block Parameter V1

### Goal

Allow first visual edit through editor.

### Deliverables

```text
Block edit request
diff preview
confirm/apply
undo/rollback hook
post-patch refresh
```

### Exit Criteria

```text
Editor changes "key" to "gold_key".
Only literal source span changes.
```

---

## Stage SW-11 — Publish Gate Integration

### Goal

Make script state visible to product validation.

### Deliverables

```text
sagascript_diagnostics.json consumed by publish check
blocking severity policy
runtime binding freshness check
missing script artifact check
```

### Exit Criteria

```text
Publish blocks invalid scripts.
Publish allows opaque valid C# if policy permits.
```

---

## Stage SW-12 — Collaboration Transaction Integration

### Goal

Turn source patches into reviewable transactions.

### Deliverables

```text
source patch transaction schema
behavior id mapping
source region metadata
audit-friendly patch records
Team Room recent change feed integration
```

### Exit Criteria

```text
Simple View edit appears as C# diff and collaboration transaction.
```

---

# 18. First Minimum Viable Proof

## 18.1 Input

```csharp
using Saga.Gameplay;

public sealed class DoorLogic : SagaBehaviour
{
    [SagaBehavior]
    public void OnPlayerInteract(Player player)
    {
        // Designer-readable behavior
        if (player.Inventory.Has("key"))
        {
            Door.Open();
        }
    }
}
```

## 18.2 Required Output

Projection:

```text
On Player Interact
    If player.Inventory.Has("key")
        Door.Open()
```

## 18.3 Required Guarantees

```text
Opening block view does not mutate source.
Returning to C# without edits is byte-identical.
Changing "key" to "gold_key" patches only that literal.
The rest of the file remains byte-identical.
The edited source compiles.
Runtime executes compiled C#.
Projection diagnostics are deterministic.
```

## 18.4 Failure Conditions

The proof fails if:

```text
whole file is rewritten
comments are removed
formatting is normalized
unsupported code crashes analyzer
runtime requires block graph interpretation
patch changes more than expected span
diagnostics are vague or nondeterministic
```

---

# 19. Risk Register

## 19.1 Scope Explosion

### Risk

SagaWeaver becomes:

```text
compiler
IDE
visual editor
package manager
collaboration server
runtime interpreter
plugin marketplace
```

### Mitigation

```text
Keep SagaWeaver to analysis/projection/patch/metadata/binding.
Keep editor UI separate.
Keep collaboration separate.
Keep runtime execution separate.
```

---

## 19.2 Source Trust Failure

### Risk

Opening block view rewrites C#.

### Consequence

Programmers stop trusting Saga Editor.

### Mitigation

```text
No-edit byte-identical invariant.
Diff preview for every edit.
Minimal patch only.
Rollback.
Strict tests.
```

---

## 19.3 Visual Graph Spaghetti

### Risk

Trying to represent every C# syntax detail visually.

### Consequence

Blocks become unreadable.

### Mitigation

```text
Narrow compatibility profile.
Opaque nodes.
High-level/low-level abstraction.
Read-only regions.
No arbitrary C# block conversion.
```

---

## 19.4 Runtime Performance Misdesign

### Risk

Runtime executes block graphs dynamically.

### Consequence

Hot paths become slow and allocation-heavy.

### Mitigation

```text
Compiled C# runtime path.
Runtime binding manifest.
Cached function handles.
C++ native hot paths.
No per-frame graph traversal.
```

---

## 19.5 Identity Instability

### Risk

Node and behavior ids change constantly.

### Consequence

Visual layout, diffs, collaboration, and diagnostics break.

### Mitigation

```text
Stable behavior ids.
Optional explicit [SagaId].
Source path + symbol identity.
Stable node id generation.
Careful rename/move handling.
```

---

## 19.6 Editor Coupling

### Risk

SagaWeaver becomes tightly coupled to Qt/editor widgets.

### Consequence

Tool cannot run in CI/build/publish.

### Mitigation

```text
SagaWeaver emits JSON artifacts.
Editor consumes artifacts.
No UI dependency in core tool.
```

---

## 19.7 Engine Coupling

### Risk

SagaWeaver directly links runtime engine systems.

### Consequence

Build graph becomes fragile.

### Mitigation

```text
Consume API metadata.
Emit artifacts.
Runtime consumes binding manifest.
Keep tool-side dependencies explicit.
```

---

# 20. Success Criteria

SagaWeaver is successful when:

```text
C# source trust is preserved.
Read-only projection works.
Minimal supported block edits patch exact source spans.
Advanced C# remains safe as opaque nodes.
Runtime binding metadata is deterministic.
Editor does not guess script compatibility.
Publish gate can validate script state.
Collaboration can audit source patch transactions.
Programmers trust that Saga will not rewrite their code unexpectedly.
Designers can safely edit supported gameplay parameters visually.
```

SagaWeaver is not successful merely because:

```text
a block graph appears on screen
a demo works once
the editor can generate C# from scratch
runtime can interpret blocks
```

The decisive measure is:

```text
source preservation + deterministic metadata + runtime-safe binding
```

---

# 21. Long-Term Expansion

After the first proof, SagaWeaver may expand into:

```text
more supported C# constructs
more patch operations
composite/high-level node expansion
built-in Saga extension packages
node library documentation generation
performance policy diagnostics
source visibility metadata for project slices
reviewable script change bundles
semantic behavior diffing
editor graph layout persistence integration
package-level script compatibility reports
```

Do not expand until the core guarantees are stable.

---

# 22. Final Decision

SagaWeaver should exist as the dedicated toolchain for SagaEngine's C# ↔ Visual Blocks architecture.

Its role:

```text
Analyze C#.
Project safe regions into visual blocks.
Preserve source exactly when no edit occurs.
Patch only minimal source spans when supported visual edits occur.
Expose advanced C# as opaque/read-only nodes.
Generate deterministic node metadata and runtime binding manifests.
Emit diagnostics that editor, publish, and collaboration systems can trust.
```

Its boundaries:

```text
Not the editor UI.
Not the runtime interpreter.
Not a full IDE.
Not arbitrary C# to blocks.
Not the collaboration server.
Not the enterprise security boundary.
```

Its first proof:

```text
DoorLogic.cs
    → read-only block projection
    → byte-identical no-edit roundtrip
    → "key" to "gold_key" minimal patch
    → compiled C# runtime binding
```

The engineering commandment:

```text
Never sacrifice source trust for visual convenience.
```

The product commandment:

```text
Blocks must make C# more approachable without making C# less trustworthy.
```

The strategic commandment:

```text
Do not build a bigger scripting fantasy.
Build the smallest source-preserving proof that can survive tests.
```
