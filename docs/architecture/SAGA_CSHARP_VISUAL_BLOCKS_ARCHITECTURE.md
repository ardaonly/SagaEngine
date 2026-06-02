# SagaEngine Source-Preserving C# ↔ Visual Blocks Architecture

## Document Status

**Status:** Architecture proposal / product-technical design document
**Scope:** SagaEngine scripting, visual authoring, editor integration, C# source preservation, block projection, and future extension system
**Primary goal:** Define a realistic model where users can write C#, view/edit supported regions as visual blocks, and return to C# without losing the original source text when no block edit occurred.

This document is not a full implementation roadmap. It is a serious architectural definition for the desired model, including required systems, boundaries, non-goals, risks, test strategy, and staged adoption.

---

# 1. Executive Summary

SagaEngine should not create a separate user-facing scripting language as its primary authoring model. The correct long-term model is:

> **C# is the canonical source. Visual blocks are a source-preserving projection of Saga-compatible C# regions. Saga Behavior IR is a semantic analysis and validation layer, not the primary text source. Runtime executes compiled C#, not interpreted visual blocks.**

This means the user writes normal-looking C# gameplay code. Saga analyzes the parts that follow the supported **Saga-compatible C# profile**, projects them into visual blocks, and can return to the original C# text without rewriting it. If the user does not edit the block graph, the C# file must remain byte-identical.

When the user edits through blocks, Saga should not regenerate the whole C# file. It should apply minimal source patches to the relevant source spans while preserving the rest of the file exactly: comments, formatting, whitespace, naming, using order, unsupported code regions, and surrounding structure.

This model gives SagaEngine a strong product identity:

- Programmers keep real C#.
- Designers can work visually.
- Visual scripting is not a toy layer detached from code.
- Runtime performance remains close to normal C# because blocks are not interpreted in hot paths.
- Advanced C# remains possible as opaque/custom nodes.
- Saga extension packages can expose both C# APIs and visual nodes from the same codebase.

The hard truth: this is a compiler/editor tooling project. It is not a small visual scripting feature. It requires Roslyn-style syntax analysis, source-span mapping, a graph projection model, source-preserving patching, strict diagnostics, deterministic tests, and careful versioning.

---

# 2. Product Intent

## 2.1 What Saga Wants To Enable

SagaEngine should support multiple user skill levels without splitting the project into incompatible authoring worlds.

The target user flows are:

### Flow A — Programmer-first

1. User writes C# gameplay logic.
2. Saga detects supported Saga-compatible regions.
3. Editor shows those regions as visual blocks.
4. User can inspect or lightly edit the behavior visually.
5. Returning to C# preserves original code unless the user made a real block edit.

### Flow B — Designer-first

1. User creates behavior through blocks.
2. Saga creates C# source using a stable generated region or source-backed behavior file.
3. Programmer can inspect and modify the code.
4. Supported edits remain projectable back to blocks.

### Flow C — Library/extension author

1. Developer writes C# methods/classes with Saga metadata attributes.
2. Saga extracts node schemas from those APIs.
3. Visual users can use those methods as blocks.
4. Advanced implementation remains C# and does not need to expand into blocks.

### Flow D — Mixed team

1. Designer edits simple gameplay flow as blocks.
2. Programmer writes advanced C# helper methods.
3. The helper methods appear as opaque/custom visual nodes.
4. Publish checks validate compatibility, unsupported regions, runtime binding, and diagnostics.

---

# 3. Core Design Decision

## 3.1 Canonical Source

The canonical source for gameplay scripts should be:

```text
C# source files
```

Not:

```text
Visual graph files
```

and not:

```text
Saga Behavior IR files
```

The reason is simple: the desired requirement is text-level preservation:

```text
C# written by user
    → visual blocks
    → back to C#
    → identical source if no edits occurred
```

If the IR becomes the primary source and C# is regenerated from it, exact source preservation becomes nearly impossible beyond trivial cases. The generator would naturally normalize formatting, remove comments, change style, or reorder expressions.

## 3.2 Role of Saga Behavior IR

Saga Behavior IR still exists, but its role is not to replace the C# file.

Saga Behavior IR is used for:

- semantic analysis;
- visual block projection;
- diagnostics;
- type validation;
- runtime binding metadata;
- publish checks;
- extension/node metadata;
- optional generated artifacts;
- editor graph layout metadata.

It is not the authoritative text representation.

## 3.3 Correct Mental Model

```text
C# Source File
    ↓ parse/analyze
Roslyn Syntax Tree + Semantic Model
    ↓ source spans + supported constructs
Saga Projection Model
    ↓
Visual Blocks
    ↓ edits
Source-Preserving Patch System
    ↓
C# Source File
```

The visual blocks are a projection over real source code, not an independent replacement for it.

---

# 4. Definitions

## 4.1 Normal C#

Normal C# means the full C# language as commonly used by programmers. It may contain:

- LINQ;
- lambdas;
- async/await;
- generics;
- reflection;
- inheritance;
- pattern matching;
- attributes;
- comments;
- custom formatting;
- partial classes;
- local functions;
- expression-bodied members;
- extension methods;
- unsafe code if allowed by project settings.

Normal C# can compile and run, but it is not always projectable into visual blocks.

## 4.2 Saga-compatible C#

Saga-compatible C# is a supported subset/profile of C# that Saga can reliably project into visual blocks and patch back into source.

It is still valid C#. It is not a separate language.

Example:

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

This can become:

```text
On Player Interact
    If player.Inventory.Has("key")
        Door.Open()
```

## 4.3 Advanced C#

Advanced C# means valid C# that Saga can compile and execute but cannot fully expand into editable visual blocks.

Examples:

- reflection-heavy code;
- complex LINQ pipelines;
- nested lambdas;
- dynamic dispatch;
- unsupported async flows;
- unsupported generics;
- metaprogramming patterns;
- arbitrary unsafe code.

Advanced C# should remain supported, but it appears in the block graph as an opaque code region or custom node.

## 4.4 Opaque C# Node

An opaque C# node is a visual representation of code that Saga cannot or should not expand internally.

Example:

```csharp
[SagaNode("Combat/Calculate Damage")]
public static int CalculateDamage(Player attacker, Enemy target)
{
    var baseDamage = attacker.Strength * 2;
    var armorReduction = target.Armor / 3;
    return Math.Max(1, baseDamage - armorReduction);
}
```

Block view:

```text
Calculate Damage
    attacker: Player
    target: Enemy
    returns: int
```

The implementation remains C#.

## 4.5 Source-Preserving Roundtrip

Source-preserving roundtrip means:

```text
input C# bytes == output C# bytes
```

when no visual edit occurred.

This is stricter than semantic equivalence.

## 4.6 Semantic Roundtrip

Semantic roundtrip means behavior is preserved, but exact source text may change.

Example:

```csharp
if(player.HasItem("key")) Door.Open();
```

could become:

```csharp
if (player.HasItem("key"))
{
    Door.Open();
}
```

This is not enough for the desired model. Saga must target source preservation at least for no-edit flows and minimal patch preservation for edited flows.

---

# 5. Non-Goals

This system must not claim impossible or dangerous guarantees.

## 5.1 Not Every C# Feature Becomes Editable Blocks

Saga should not promise that arbitrary C# can be expanded into clean visual blocks.

Unsupported C# should remain:

- normal C#;
- opaque visual node;
- read-only block region;
- or advanced code region.

## 5.2 Not A New Primary User Language

Saga should not force users to write a custom Saga language as the main gameplay language.

A custom internal DSL may exist for generated artifacts, graph metadata, or internal packages, but user-facing scripting should primarily remain C#.

## 5.3 Not Runtime Graph Interpretation For Hot Paths

Visual blocks must not imply runtime interpretation of block graphs in performance-critical paths.

Runtime should execute:

- compiled C#;
- cached delegates/function handles;
- native C++ engine systems for hot paths;
- precomputed runtime binding metadata.

## 5.4 Not A Full IDE Replacement At V1

Saga should not attempt to replace Rider, Visual Studio, or VS Code in early versions.

It should provide:

- block projection;
- diagnostics;
- source-backed visual editing;
- script validation;
- package/build integration.

Full refactoring tools, advanced debugger replacement, and language server depth are later concerns.

## 5.5 Not Enterprise Governance At The Core Scripting Phase

Enterprise features such as SSO, organization roles, cloud policy, approval chains, and audit dashboards are later governance layers. The scripting model should not be blocked by them.

However, source preservation and deterministic diagnostics should be designed so enterprise governance can be added later.

---

# 6. Architectural Principles

## 6.1 C# Must Remain Trustworthy

Users must trust that opening block view will not rewrite their code.

Required guarantee:

```text
No visual edit → no source mutation.
```

This should be tested byte-for-byte.

## 6.2 Blocks Are A View, Not A Replacement

Visual blocks are a projection over recognized source regions.

The block graph should store UI state such as:

- node layout;
- collapsed/expanded state;
- selected view mode;
- grouping;
- comments;
- visual metadata.

But the source C# remains the canonical behavior definition.

## 6.3 Edits Must Be Local

A block edit should patch only the smallest valid source span.

Bad:

```text
Regenerate entire method/class/file.
```

Good:

```text
Patch only the changed expression/statement/block.
Preserve everything else exactly.
```

## 6.4 Unsupported Code Must Not Break The System

Unsupported code is normal. It should not be treated as failure.

The editor should show:

- expandable regions;
- partially expandable regions;
- opaque code nodes;
- unsupported diagnostics;
- exact reason why a region cannot be projected.

## 6.5 Performance Critical Code Should Be Explicit

Saga should classify behavior execution tier:

| Tier | Meaning | Intended Use |
|---|---|---|
| Tier 0 | interpreted/debug graph | editor preview, diagnostics |
| Tier 1 | cached runtime plan | low-frequency gameplay |
| Tier 2 | compiled C# | normal gameplay scripts |
| Tier 3 | native C++ engine system | hot paths: networking, movement, replication, physics |

Visual blocks should normally compile to Tier 2 or bind to Tier 3 systems.

## 6.6 Diagnostics Must Be First-Class

The system must explain:

- why a region is Saga-compatible;
- why a region is unsupported;
- whether no-edit roundtrip is safe;
- whether block edit patching is supported;
- whether runtime binding is valid;
- whether an extension node is safe;
- whether generated metadata is stale.

---

# 7. High-Level System Components

## 7.1 Saga Script Analyzer

Responsible for reading C# and determining which regions are Saga-compatible.

Responsibilities:

- parse C# files;
- identify `[SagaBehavior]`, `[SagaNode]`, `[SagaLibrary]`, and related metadata;
- classify methods/classes/expressions/statements;
- build source-span mapping;
- build a semantic model;
- detect unsupported constructs;
- produce diagnostics;
- emit projection descriptors.

Inputs:

- `.cs` files;
- project script manifest;
- Saga API metadata;
- node library metadata;
- analyzer configuration.

Outputs:

- projection model;
- diagnostics;
- source map;
- behavior metadata;
- node metadata;
- compatibility report.

## 7.2 Saga Projection Model

Represents the bridge between C# syntax and visual blocks.

It should contain:

- behavior id;
- source file path;
- source span ranges;
- syntax node kind;
- semantic operation kind;
- projected block kind;
- input/output ports;
- type information;
- edit capability;
- unsupported reason if any;
- stable identity for nodes.

The projection model should not replace the C# file.

## 7.3 Saga Behavior IR

A semantic representation of supported behavior.

Example concepts:

- event entry;
- sequence;
- branch;
- loop;
- call;
- assignment;
- variable read;
- variable write;
- constant;
- return;
- comparison;
- arithmetic operation;
- engine API call;
- custom node call.

IR responsibilities:

- validation;
- diagnostics;
- runtime metadata generation;
- build/publish checks;
- optional graph execution for editor preview;
- code generation only when necessary.

## 7.4 Visual Block Graph

The editor-facing visual model.

Responsibilities:

- render projected blocks;
- preserve visual layout metadata;
- provide supported edits;
- expose unsupported regions as opaque nodes;
- allow high-level/low-level expansion where supported;
- surface diagnostics inline.

The visual graph should not silently mutate source.

## 7.5 Source-Preserving Patch Engine

The hardest and most important component.

Responsibilities:

- apply minimal text edits to C# source;
- preserve unaffected byte ranges exactly;
- preserve comments and whitespace outside edited spans;
- update source maps after edits;
- reject unsafe edits;
- produce preview diffs;
- support rollback;
- integrate with undo/redo.

This component determines whether the whole feature feels professional or fragile.

## 7.6 Runtime Binding Generator

Build-time component that resolves visual/C# behavior metadata into runtime-safe binding tables.

Responsibilities:

- resolve function ids;
- validate node APIs;
- avoid string lookup in hot paths;
- generate runtime metadata;
- connect C# script assemblies to engine APIs;
- produce diagnostics for missing or stale bindings.

## 7.7 Saga Extension Metadata System

Supports Scratch-like extensions for Saga.

An extension can expose:

- C# APIs;
- visual nodes;
- editor panels;
- runtime systems;
- server systems;
- asset importers;
- diagnostics rules;
- templates;
- publish checks.

Early versions should start with static/built-in extensions before dynamic plugin loading.

---

# 8. C# Compatibility Profile

## 8.1 Supported V1 Constructs

V1 should support a narrow but useful subset.

Recommended V1 supported constructs:

- method declarations marked `[SagaBehavior]`;
- method parameters with supported Saga types;
- local variables with simple supported types;
- `if` / `else`;
- simple `foreach` over supported collections;
- direct method calls;
- property reads/writes;
- field reads/writes where allowed;
- constants;
- enum values;
- simple arithmetic;
- simple boolean expressions;
- return statements;
- custom node calls through `[SagaNode]` APIs.

Supported types should initially be limited:

- `bool`;
- `int`;
- `float`;
- `double` only if needed;
- `string` with restrictions;
- `EntityId`;
- `PlayerRef`;
- `AssetRef<T>`;
- `ItemId`;
- engine-defined value types;
- explicitly registered script structs;
- enums.

## 8.2 Unsupported Or Deferred Constructs

V1 should reject or mark as opaque:

- complex LINQ;
- arbitrary lambdas;
- async/await;
- reflection;
- dynamic;
- unsafe;
- pointer operations;
- arbitrary generics;
- complex inheritance dispatch;
- local functions;
- expression trees;
- preprocessor-heavy regions;
- unsupported exception flows;
- arbitrary file/network access in gameplay logic.

This is not a weakness. It is the cost of reliable visual projection.

## 8.3 Compatibility Result Types

Every analyzed region should receive a classification:

| Classification | Meaning |
|---|---|
| `FullyProjectable` | Can be shown and edited as blocks |
| `ReadOnlyProjectable` | Can be shown as blocks but not safely edited |
| `PartiallyProjectable` | Some child regions become blocks; unsupported children become opaque |
| `OpaqueNode` | Exposed as a single block with C# implementation hidden |
| `Unsupported` | Cannot be represented safely |
| `Invalid` | Does not compile or violates Saga rules |

---

# 9. Source Preservation Model

## 9.1 Required No-Edit Guarantee

For every supported C# file:

```text
Read C#
Project to blocks
Switch back to C#
Save without edits
```

must produce:

```text
exact same bytes
```

This must be a hard invariant.

## 9.2 Edit Rules

When visual block edits occur:

1. The system computes the exact source span affected.
2. It verifies the span is safe to patch.
3. It generates replacement text only for that span.
4. It preserves all other text exactly.
5. It updates source maps.
6. It re-runs analysis.
7. It reports compatibility changes.

## 9.3 Patch Granularity

Patch granularity should be as narrow as possible:

| Edit | Preferred Patch Scope |
|---|---|
| change literal value | literal token only |
| change function argument | argument expression only |
| add call inside block | containing statement block |
| change condition | condition expression only |
| add branch | containing if/else structure |
| reorder statements | containing statement list |
| change method signature | method declaration span |

## 9.4 Formatting Policy

The patch engine must not run global formatting automatically.

Rules:

- no whole-file formatting on block save;
- no using reordering unless explicitly requested;
- no comment deletion;
- no region deletion;
- no line ending normalization unless project policy explicitly allows it;
- generated patches should infer local indentation from surrounding source.

## 9.5 Diff Preview

For any block edit that changes source, the editor should be able to show:

```text
Before / After source diff
```

This is essential for programmer trust.

---

# 10. Visual Block Model

## 10.1 Block Categories

Recommended categories:

- Event blocks;
- Flow blocks;
- Condition blocks;
- Variable blocks;
- Function call blocks;
- Engine API blocks;
- Custom C# node blocks;
- Opaque code blocks;
- Composite/high-level blocks;
- Diagnostic blocks.

## 10.2 Expandable Abstractions

Saga should support high-level nodes that can expand into lower-level behavior where safe.

Example:

```text
Spawn Enemy Wave
    ↓ expand
For Each Spawn Point
    Allocate Entity
    Set Transform
    Attach AI Brain
    Mark Replication Dirty
```

This is powerful, but should be constrained.

Rules:

- not every node must be expandable;
- expandable nodes must declare their expansion model;
- expansion must be deterministic;
- expansion must be versioned;
- source patching of expanded nodes must be carefully controlled.

## 10.3 Opaque Code Nodes

Unsupported advanced C# should not disappear.

It should appear as:

```text
Advanced C# Region
```

or:

```text
Custom Node: Calculate Damage
```

The visual system should display:

- method name;
- inputs;
- outputs;
- summary;
- compatibility status;
- whether it can be expanded;
- source location.

---

# 11. Saga Extension / Library Model

## 11.1 Purpose

Saga should allow developers to create extension packages that add engine/editor/gameplay capabilities.

This is the more serious version of a Scratch extension model.

## 11.2 C# Node API

Example:

```csharp
[SagaLibrary("Saga.Inventory")]
public static class InventoryNodes
{
    [SagaNode("Inventory/Give Item")]
    public static void GiveItem(Player player, ItemId item, int amount)
    {
        player.Inventory.Give(item, amount);
    }

    [SagaNode("Inventory/Has Item")]
    public static bool HasItem(Player player, ItemId item)
    {
        return player.Inventory.Contains(item);
    }
}
```

This should expose:

```text
Inventory
    Give Item
    Has Item
```

## 11.3 Extension Manifest

A Saga extension manifest may eventually include:

```json
{
  "schemaVersion": 1,
  "id": "Saga.Inventory",
  "version": "0.1.0",
  "displayName": "Inventory",
  "csharpAssembly": "Saga.Inventory.dll",
  "nodeMetadata": "nodes.inventory.json",
  "editorPanels": [],
  "runtimeSystems": [],
  "diagnosticRules": [],
  "publishChecks": []
}
```

Early versions should not start with complex dynamic loading. Start with built-in/static extensions and manifest validation.

## 11.4 Extension Safety

Extensions must declare:

- runtime tier;
- deterministic/non-deterministic behavior;
- editor-only/runtime availability;
- server/client availability;
- permissions;
- asset dependencies;
- supported platforms;
- compatibility version.

---

# 12. Runtime Performance Model

## 12.1 Core Rule

Visual blocks must not be the runtime hot-path execution format.

Correct runtime path:

```text
C# source
    ↓ compile
C# assembly
    ↓ bind
cached runtime calls
    ↓ execute
```

Block projection and source preservation happen in editor/build tooling, not every frame.

## 12.2 Avoid In Runtime Hot Paths

Avoid:

- string-based node lookup;
- dynamic invocation per node;
- boxing every value into object;
- heap allocation per node execution;
- runtime graph traversal for server tick logic;
- reflection in hot paths;
- script-side movement/replication loops unless explicitly optimized.

## 12.3 Acceptable Runtime Costs

Acceptable:

- compiled C# method calls;
- cached delegates;
- precomputed binding tables;
- explicit managed/native boundary calls;
- event dispatch with stable ids;
- low-frequency gameplay behavior.

## 12.4 Hot Path Policy

The following should remain C++/engine-native or highly optimized systems:

- packet handling;
- server-authoritative movement core;
- replication filtering;
- physics loop;
- transform update for large numbers of entities;
- broad-phase queries;
- low-level memory/resource tracking;
- networking session management.

Visual/C# scripting can configure or call into those systems, but should not replace them in v1.

---

# 13. Editor UX Requirements

## 13.1 User Must See Compatibility Status

For every script method, the editor should show:

```text
Fully block-compatible
Partially block-compatible
Advanced C# only
Invalid / compile error
```

## 13.2 User Must Understand Why

Diagnostics should say:

- unsupported construct;
- exact source location;
- suggested rewrite;
- whether it can be converted to opaque node;
- whether runtime still compiles.

Bad diagnostic:

```text
Cannot convert to blocks.
```

Good diagnostic:

```text
This method uses a LINQ Select lambda at line 42. Saga Blocks V1 cannot expand arbitrary lambdas. The method can still be exposed as an opaque C# node.
```

## 13.3 Block Edit Must Show Source Impact

Before applying a visual edit, editor should optionally show:

- affected source range;
- generated patch;
- before/after diff;
- compatibility changes.

## 13.4 Unsupported Code Should Remain Visible

Never hide unsupported code from the user.

Unsupported code should become:

- opaque node;
- read-only source block;
- inline code preview;
- diagnostic marker.

---

# 14. File and Artifact Model

## 14.1 Source Files

Primary files:

```text
Scripts/**/*.cs
```

These are the canonical behavior sources.

## 14.2 Visual Metadata Files

Visual graph layout should be stored separately:

```text
.saga/VisualBlocks/**/*.sagablocks.json
```

This avoids polluting C# source with layout metadata.

Visual metadata should include:

- behavior id;
- source file path;
- method identity;
- node layout;
- collapsed states;
- user annotations;
- visual grouping;
- last compatible source hash;
- schema version.

## 14.3 Analyzer Artifacts

Build/editor artifacts:

```text
Build/SagaScript/projection_report.json
Build/SagaScript/behavior_ir.json
Build/SagaScript/node_metadata.json
Build/SagaScript/runtime_bindings.json
Build/Reports/sagascript_diagnostics.json
```

These are generated artifacts, not primary source.

## 14.4 Stable Identity

Each behavior and node needs stable identity.

Potential identity components:

- project id;
- source file normalized path;
- class full name;
- method signature;
- optional explicit `[SagaId("...")]`;
- source span hash;
- generated stable node ids.

Explicit ids are preferable for long-lived behaviors.

---

# 15. Testing Strategy

This feature requires unusually strong tests. Weak tests will produce a fragile editor.

## 15.1 No-Edit Roundtrip Tests

For every fixture:

```text
input.cs
    parse
    project blocks
    return to source
output.cs
```

Assert:

```text
input bytes == output bytes
```

Fixtures must include:

- comments;
- weird spacing;
- CRLF and LF line endings;
- regions;
- attributes;
- using order;
- partial classes;
- unsupported methods;
- supported methods;
- nested classes if allowed;
- expression-bodied members;
- generic classes;
- mixed supported/unsupported methods.

## 15.2 Projection Classification Tests

Test classification:

- fully projectable;
- partially projectable;
- opaque;
- unsupported;
- invalid.

## 15.3 Minimal Patch Tests

For each block edit, assert:

- only expected source span changed;
- unaffected byte ranges are identical;
- resulting C# compiles;
- projection remains valid or reports expected diagnostics.

## 15.4 Diagnostics Tests

Every unsupported construct should have:

- stable diagnostic code;
- severity;
- exact location;
- human-readable message;
- suggested action when possible.

## 15.5 Runtime Binding Tests

Validate:

- node function ids resolve;
- missing node APIs fail clearly;
- stale metadata is detected;
- runtime binding avoids string lookup in hot paths;
- generated binding manifest is deterministic.

## 15.6 Performance Tests

Not MMO-scale early, but enough to prevent obvious regressions:

- analyzer time on sample project;
- projection time;
- block view load time;
- patch application time;
- runtime overhead compared to equivalent handwritten C# method;
- no per-frame graph interpretation in compiled path.

---

# 16. Suggested Implementation Stages

This is not the full roadmap, but the realistic order of construction.

## Stage 0 — Decision Record

Deliverable:

- architecture doc;
- explicit canonical source decision;
- non-goals;
- compatibility profile draft;
- source preservation guarantee.

Exit criteria:

- team agrees C# is canonical source;
- IR is semantic/supporting model;
- blocks are projection;
- no-edit roundtrip is hard invariant.

## Stage 1 — Read-Only C# Projection Prototype

Goal:

```text
C# → blocks → C# with no edit = byte-identical
```

Deliverables:

- parse selected C# files;
- identify `[SagaBehavior]` methods;
- project simple methods into read-only block model;
- store no source changes;
- produce projection diagnostics.

No block editing yet.

Exit criteria:

- no-edit roundtrip byte-identical tests pass;
- unsupported code does not crash analyzer;
- projection report is deterministic.

## Stage 2 — Saga-compatible C# Profile V1

Goal:

Define what is actually block-compatible.

Deliverables:

- supported construct table;
- unsupported construct diagnostics;
- type compatibility rules;
- method classification;
- behavior metadata output.

Exit criteria:

- each supported construct has tests;
- each unsupported construct has diagnostics;
- profile is narrow but useful.

## Stage 3 — Visual Block Graph Read Model

Goal:

Render projected behavior as block graph.

Deliverables:

- block schema;
- node/port model;
- source-span links;
- visual metadata file format;
- block layout persistence.

Exit criteria:

- block view opens supported methods;
- unsupported regions appear as opaque nodes;
- source locations are traceable from blocks.

## Stage 4 — Source-Preserving Patch Prototype

Goal:

Allow very small block edits and patch C# locally.

Initial edits:

- change numeric/string literal;
- change boolean condition operator;
- change function argument;
- add simple method call inside supported block;
- remove simple method call.

Deliverables:

- patch engine;
- diff preview;
- rollback;
- source map update;
- patch diagnostics.

Exit criteria:

- minimal patch tests pass;
- unaffected source bytes remain identical;
- edited output compiles;
- unsupported edits are rejected cleanly.

## Stage 5 — C# Node Library Extraction

Goal:

Allow C# APIs to become block nodes.

Deliverables:

- `[SagaLibrary]`;
- `[SagaNode]`;
- input/output port extraction;
- node category metadata;
- node diagnostics;
- node metadata artifact.

Exit criteria:

- C# method can appear as visual node;
- node metadata deterministic;
- invalid node signatures fail clearly.

## Stage 6 — Runtime Binding Manifest

Goal:

Make generated node/behavior metadata usable at runtime without slow lookup.

Deliverables:

- function id system;
- binding manifest;
- script assembly binding;
- runtime diagnostics;
- missing/stale binding detection.

Exit criteria:

- compiled C# behavior runs;
- visual-origin behavior has no graph interpreter overhead in normal runtime path;
- runtime binding tests pass.

## Stage 7 — Editable Blocks V1

Goal:

Make simple visual editing trustworthy.

Deliverables:

- supported edit operations;
- undo/redo integration;
- source patch preview;
- compatibility status updates;
- user-facing diagnostics.

Exit criteria:

- simple designer-authored logic can be edited visually;
- programmer-written formatting is preserved outside changed spans;
- editor never silently rewrites whole file.

## Stage 8 — High-Level / Low-Level Expansion Prototype

Goal:

Support selected composite nodes.

Deliverables:

- composite node schema;
- expansion metadata;
- read-only expansion view;
- optional editable expansion for selected nodes;
- diagnostics for non-expandable nodes.

Exit criteria:

- high-level node can expand to lower-level blocks;
- expansion is deterministic and versioned;
- no false promise that every node expands.

## Stage 9 — Extension Package V1

Goal:

Support built-in/static Saga extensions.

Deliverables:

- extension manifest;
- node metadata packaging;
- C# assembly packaging;
- validation;
- publish-check integration.

Exit criteria:

- extension can add C# APIs and visual nodes;
- package validation catches missing/stale node metadata;
- no dynamic plugin complexity required yet.

## Stage 10 — Team Workflow Readiness

Goal:

Prepare for collaboration without implementing full real-time shared editing.

Deliverables:

- script behavior ids;
- patch/diff model;
- conflict detection concept;
- lock/checkpoint metadata;
- script diagnostics report suitable for review.

Exit criteria:

- block edits produce reviewable C# diffs;
- source changes are auditable;
- future collaboration is not blocked by unstable identity.

---

# 17. Major Risks

## 17.1 Scope Explosion

This system can become a compiler, IDE, visual scripting editor, package manager, and collaboration platform at the same time.

Mitigation:

- Stage 1 must be read-only.
- Editing must start with tiny patches.
- Unsupported code must be accepted as normal.
- Do not attempt arbitrary C# block conversion.

## 17.2 User Trust Failure

If Saga rewrites user code unexpectedly, programmers will stop trusting the editor.

Mitigation:

- no-edit byte-identical invariant;
- diff preview;
- local patching only;
- rollback;
- deterministic tests.

## 17.3 Visual Graph Spaghetti

If every C# construct becomes a block, visual graphs become unreadable.

Mitigation:

- opaque nodes;
- abstraction levels;
- collapse/expand;
- block compatibility profile;
- do not represent every syntax detail visually.

## 17.4 Performance Misdesign

If runtime executes block graphs dynamically, performance will suffer.

Mitigation:

- compiled C# runtime path;
- cached function handles;
- no hot-path graph interpretation;
- native engine systems for Tier 3 logic.

## 17.5 Identity Instability

If node identities change on every edit, visual layout, diffs, collaboration, and diagnostics become unstable.

Mitigation:

- explicit behavior ids;
- stable node ids;
- source map strategy;
- careful handling of renamed methods and moved code.

---

# 18. Minimum Viable Proof

Before building the full system, Saga needs a small but convincing proof.

## MVP Scenario

Input C#:

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

Editor shows:

```text
On Player Interact
    If player.Inventory.Has("key")
        Door.Open()
```

Required proof:

1. Opening block view does not mutate source.
2. Returning to C# without edits is byte-identical.
3. Changing `"key"` to `"gold_key"` in block view patches only that literal.
4. The rest of the file remains byte-identical.
5. The edited source compiles.
6. Runtime executes compiled C#.
7. Projection diagnostics are deterministic.

This MVP is small but proves the core philosophy.

---

# 19. Product Positioning

If implemented correctly, this becomes a defining SagaEngine feature.

Suggested product language:

> **Saga uses source-preserving C# visual projection. Designers can work in blocks, programmers keep real C#, and runtime executes compiled code instead of interpreted block graphs.**

Avoid overclaiming:

Bad claim:

> Every C# program can become editable blocks.

Correct claim:

> Saga-compatible C# regions can be edited as visual blocks. Advanced C# remains supported as opaque reusable code nodes.

Bad claim:

> Visual scripting has zero performance cost in every case.

Correct claim:

> Visual blocks compile to the same C# runtime path as supported script code, so block authoring does not require interpreted runtime overhead.

---

# 20. Final Architecture Decision

SagaEngine should implement the desired model as:

```text
C# canonical source
    + source-preserving analyzer/projection
    + visual blocks over supported regions
    + opaque nodes for advanced C#
    + minimal source patching for block edits
    + compiled C# runtime path
    + extension metadata for custom nodes
```

The correct guarantee is:

```text
No block edit → byte-identical C# roundtrip.
Supported block edit → minimal source patch.
Unsupported C# → preserved as C# and optionally exposed as opaque node.
Runtime → compiled C#, not interpreted blocks.
```

This model is hard, but coherent. It gives SagaEngine a serious identity: not just another engine with visual scripting, but a source-preserving authoring platform where code and blocks can coexist without destroying each other.

The main engineering rule is non-negotiable:

> **Never sacrifice source trust for visual convenience.**

If users believe Saga may rewrite their code unexpectedly, the feature fails. If users trust that Saga can project, explain, patch, and preserve their C#, the feature becomes one of the strongest reasons to use the engine.
