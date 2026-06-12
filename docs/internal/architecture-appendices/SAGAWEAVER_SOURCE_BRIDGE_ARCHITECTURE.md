# SagaWeaver Authoring Bridge Architecture

## Document Status

**Status:** Proposed appendix / v2 markerless bridge direction
**Current appendix location:** `docs/internal/architecture-appendices/SAGAWEAVER_SOURCE_BRIDGE_ARCHITECTURE.md`
**Related systems:** SagaEngine, SagaEditor, Saga Package System, SDE, SagaRuntime, SagaServer, SagaTools, Forge, Prism, Publish Gate, Collaboration/Workspace
**Primary goal:** Define SagaWeaver as a markerless, source-preserving C# ↔ Visual Blocks authoring bridge where users never have to manually mark "this is a block" and do not have to write `[SagaBehavior]` as part of the default workflow.

This document is a proposed direction appendix. The current compact boundary is
[C# / Visual Authoring Boundary](../../architecture/SAGA_CSHARP_VISUAL_AUTHORING_BOUNDARY.md).
This appendix does not claim that the markerless bridge, full Visual Blocks
editor UI, arbitrary C# to blocks, runtime visual graph execution, full IDE
behavior, source redaction security, or package-aware authoring bridge is
currently implemented.

---

# 1. Executive Summary

SagaWeaver exists to make Saga's central authoring promise technically real:

```text
Users can write normal C#.
Users can author through Visual Blocks.
Users do not manually mark code as "block code".
The editor does not rewrite or damage user code.
Runtime executes compiled C#, not visual graphs.
SagaWeaver automatically infers which C# regions can be safely represented as blocks.
```

The default workflow must be **markerless**.

Users should not be required to write:

```csharp
[SagaBehavior]
public void OnPlayerInteract(Player player) { }
```

Users should not be required to write:

```csharp
[SagaNode]
public static bool HasItem(Player player, string itemId) { }
```

Users should not be required to say:

```text
this method is a block
this line is a block
this file should become blocks
```

Instead, SagaWeaver should infer behavior entrypoints and block-projectable regions from:

```text
project authoring roots
scene/entity bindings
known Saga event contracts
known Saga API contracts
package-provided node metadata
SDE-emitted authoring artifacts
C# syntax and semantic model
deterministic compatibility rules
stable symbol identity rules
compile context metadata
source visibility policy
patch permission policy
```

The key rule:

```text
Block compatibility is inferred, not declared by the user.
```

Visual Blocks are not the source of truth.

C# remains the source of truth.

---

# 2. One-Sentence Architecture

```text
SagaWeaver is Saga's markerless, source-preserving C# ↔ Visual Blocks bridge: it discovers behavior entrypoints from project bindings and event contracts, infers block-projectable C# regions through syntax/semantic analysis and node metadata, emits deterministic block projections and source maps, patches only safe minimal source spans, exposes advanced C# as opaque/read-only regions, and generates runtime bindings for compiled C# execution.
```

---

# 3. Product Claim

Allowed claim:

```text
SagaWeaver can automatically analyze Saga project C# source, discover behavior entrypoints from project/editor metadata and known event contracts, infer which regions are safely projectable to Visual Blocks, preserve source exactly when no edit occurs, apply supported block edits as minimal source patches, and emit deterministic runtime binding metadata.
```

Forbidden claims:

```text
users must write [SagaBehavior] for normal authoring
users must label code as "block code"
full arbitrary C# to blocks
perfect C# <-> blocks conversion
generated C# replacement workflow
runtime visual graph interpreter
full IDE
full C# language server
full refactoring engine
Lua/Python roundtrip with C# blocks
security boundary by itself
enterprise source redaction by itself
silent behavior remapping after rename/move
structural block edits before source-preserving insertion points exist
uncontrolled patch application without permission/review policy
```

Correct product sentence:

```text
SagaWeaver makes C# visually approachable without making C# less trustworthy.
```

---

# 4. Core Principles

## 4.1 C# Source Is Canonical

Canonical gameplay script source is:

```text
Scripts/**/*.cs
```

Not:

```text
projection_report.json
source_map.json
behavior_index.json
node_metadata.json
runtime_bindings.json
patch_preview.json
.sagablocks.json
editor layout metadata
```

Generated artifacts may describe, index, project, validate, bind, review, or patch C# source.

They must not become the primary behavior truth.

---

## 4.2 No User-Authored Block Markers

Default authoring must not require users to mark block regions.

Forbidden as default workflow:

```csharp
[SagaBehavior]
public void OnPlayerInteract(Player player) { }

[SagaNode]
public static void OpenDoor(Door door) { }
```

Forbidden user burden:

```text
mark this method as block
mark this branch as visual
tell editor this is block-compatible
write special comments for block conversion
```

Correct model:

```text
User writes normal C# or creates behavior through editor.
SagaWeaver discovers behavior candidates.
SagaWeaver infers block-projectable regions.
SagaEditor renders the inferred projection.
```

Attributes may exist only as optional advanced/internal compatibility surfaces, not as the default product workflow.

---

## 4.3 Behavior Discovery and Block Projection Are Different

These are separate steps:

```text
Behavior discovery:
  Which class/method is a gameplay behavior entrypoint?

Block projection:
  Which syntax regions inside that behavior can be safely shown/edited as Visual Blocks?
```

A method may be a behavior but only partly projectable.

A method may be valid C# but not block-editable.

A method may be shown in Blocks View with opaque regions.

---

## 4.4 Projection, Not Replacement

Correct model:

```text
C# Source
  -> Roslyn syntax/semantic model
  -> behavior entrypoint discovery
  -> compatibility classification
  -> source spans
  -> projection model
  -> Visual Blocks
  -> optional supported source patch
  -> same C# source file, minimally modified
```

Wrong model:

```text
Blocks
  -> regenerate C# file
  -> replace user source
```

SagaWeaver must not become a source generator that owns the file.

---

## 4.5 No Edit Means No Source Mutation

Opening, inspecting, projecting, or closing Blocks View must not mutate C# source.

Required invariant:

```text
input_bytes == output_bytes after no-edit projection
```

This includes:

```text
comments
whitespace
line endings
using order
unsupported regions
formatting
preprocessor regions
```

---

## 4.6 Patch the Smallest Safe Span

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

---

## 4.7 Unsupported C# Is Normal

Unsupported-for-blocks does not mean invalid.

Correct behavior:

```text
advanced C# remains valid C#
unsupported region becomes opaque/read-only in visual projection
diagnostic explains why it cannot be visually edited
source link is preserved
runtime can still compile it when C# is valid
```

Opaque is not a failure state.

Opaque means:

```text
visible
valid
source-linked
not safely editable as blocks
```

---

## 4.8 Runtime Does Not Interpret Visual Graphs

Runtime path should be:

```text
C# source
  -> compile
  -> C# assembly
  -> runtime_bindings.json
  -> cached function/delegate handles
  -> runtime/server execution
```

Runtime should not execute Visual Block graphs as the normal gameplay hot path.

Visual Blocks are an authoring surface.

Compiled C# is the execution path.

---

## 4.9 Blocks View Must Be Behaviorally Honest

Blocks View must not make a behavior look simpler than it really is.

If a behavior contains opaque C# regions, Blocks View must show visible opaque placeholders.

Forbidden:

```text
hide opaque code
collapse advanced C# into a misleading simple block
show only editable regions and imply that this is the whole behavior
```

Required:

```text
show editable block islands
show read-only block regions
show opaque C# placeholders
preserve source links
show diagnostics explaining why a region is opaque
```

Principle:

```text
Blocks View is allowed to simplify authoring.
Blocks View is not allowed to lie about behavior.
```

---

## 4.10 Structural Edits Are Deferred Until Source-Preserving Insertion Is Proven

Early SagaWeaver should support narrow source patches, not broad structural rewrites.

Allowed early edit:

```text
replace string literal
replace numeric literal
replace boolean literal
replace enum value
replace simple known argument
```

Deferred edits:

```text
add if block
delete statement block
insert method call
reorder statements
wrap region in condition
extract block to helper method
convert opaque C# into editable block graph
```

Rule:

```text
No structural block edits before insertion-point source maps, formatting preservation,
semantic validation, rollback, and post-edit compile validation are proven by tests.
```

This protects source trust.

---

## 4.11 Behavior Identity Must Be Stable

Behavior identity must not depend only on the current C# class/method name.

A behavior has two identities:

```text
project behavior identity
  stable behaviorId used by project/editor/runtime artifacts

C# symbol identity
  current fully-qualified type/method/signature in source
```

Example:

```text
behaviorId:
  game.behavior.door_interaction

current C# symbol:
  Game.Doors.DoorInteraction.OnPlayerInteract(Game.Player)
```

If the C# symbol is renamed or moved, the behaviorId should not silently change.

SagaWeaver must report stale or missing symbol diagnostics instead of guessing.

---

## 4.12 Generated Scaffolding Must Be Separate From User-Owned Source

Editor-created C# skeletons may exist, but generated code must be separated from user-owned source.

Recommended split:

```text
Scripts/Behaviors/DoorInteraction.cs
  user-owned source

Scripts/Behaviors/Generated/DoorInteraction.generated.cs
  tool-owned generated scaffolding
```

Rules:

```text
SagaWeaver must not regenerate user-owned source.
SagaWeaver may regenerate generated scaffolding only inside generated boundaries.
Generated files must be clearly marked.
User files must remain source-preserved.
```

---

## 4.13 SagaWeaver Artifacts Need Visibility Classification

SagaWeaver artifacts may contain sensitive source-derived information:

```text
file paths
method names
type names
literal values
source spans
opaque region summaries
diagnostics
patch previews
```

Therefore artifacts should have visibility levels:

```text
private-source
  source_map.json, patch_preview.json, detailed diagnostics

team-authoring
  projection_report.json, behavior_index.json, compatibility summary

runtime-safe
  runtime_bindings.json with minimized source detail

publish-report
  sanitized publish status and fatal diagnostics
```

SagaWeaver is not the security boundary, but its artifacts must be classifiable by visibility policy.

---

## 4.14 Patch Apply Requires Permission and Review Policy

Patch preview and patch apply are different authority levels.

Recommended policy:

```text
designer:
  may request patch preview
  may apply safe patches only when project policy allows

programmer:
  may apply patches directly in development policy

team / enterprise:
  may require review before source mutation

CI / publish:
  must reject unreviewed patch transactions when policy requires review
```

Rule:

```text
No source mutation from Blocks View without explicit apply and policy authorization.
```

---

# 5. Source of Behavior Identity

Because default authoring is markerless, SagaWeaver needs deterministic behavior identity sources.

Allowed behavior identity sources:

```text
project behavior manifest
scene/entity/component bindings
known Saga event contracts
SDE-emitted authoring artifacts
editor-created behavior metadata
source root conventions
optional advanced attributes
```

Priority order should be deterministic.

Recommended order:

```text
1. explicit project/editor behavior manifest
2. scene/entity/component binding
3. SDE authoring artifact
4. known event contract match
5. source root + convention candidate
6. optional advanced attribute fallback
```

Important rule:

```text
Block projection is never based on the user saying "this is a block".
It is inferred from C# code structure and compatibility rules.
```

---

# 6. Behavior Manifest Model

The default editor workflow may create a sidecar behavior manifest.

Example:

```json
{
  "schemaVersion": 1,
  "behaviorId": "game.behavior.door_interaction",
  "displayName": "Door Interaction",
  "sourceFile": "Scripts/Behaviors/DoorInteraction.cs",
  "typeName": "Game.Doors.DoorInteraction",
  "entrypoints": [
    {
      "event": "OnPlayerInteract",
      "method": "OnPlayerInteract",
      "methodSignature": "System.Void OnPlayerInteract(Game.Player)",
      "parameters": [
        {
          "name": "player",
          "type": "Game.Player"
        }
      ]
    }
  ],
  "authoring": {
    "defaultView": "blocks",
    "allowSourceView": true,
    "allowOpaqueRegions": true
  }
}
```

Corresponding C# source:

```csharp
namespace Game.Doors;

namespace Game.Doors;

public partial class DoorInteraction
{
    public void OnPlayerInteract(Player player)
    {
        if (player.HasItem("key"))
        {
            Door.Open();
        }
    }
}
```

The user does not write `[SagaBehavior]`.

The manifest tells SagaWeaver:

```text
DoorInteraction.OnPlayerInteract is a behavior entrypoint.
Analyze it.
Classify it.
Project safe regions to blocks.
Generate runtime bindings.
```


---

# 7. Fully Qualified Symbol Identity, Rename, and Move Handling

Markerless discovery must not become guess-based discovery.

Behavior manifests and binding artifacts should use deterministic symbol identity.

Required identity fields:

```json
{
  "behaviorId": "game.behavior.door_interaction",
  "sourceFile": "Scripts/Behaviors/DoorInteraction.cs",
  "typeName": "Game.Doors.DoorInteraction",
  "method": "OnPlayerInteract",
  "methodSignature": "System.Void OnPlayerInteract(Game.Player)"
}
```

## 7.1 Stable Project Identity vs Source Symbol Identity

```text
behaviorId
  stable project/editor/runtime identity

fully-qualified symbol identity
  current C# type + method + signature

source span identity
  exact file/hash/span for projection and patching
```

The behaviorId should remain stable across safe source edits.

The C# symbol identity may change when the user renames or moves code.

SagaWeaver must detect that difference.

## 7.2 Rename and Move Diagnostics

If a behavior manifest points to a symbol that no longer exists, SagaWeaver must not silently bind to a random candidate.

Required diagnostic:

```text
BehaviorSymbolNotFound
  behaviorId: game.behavior.door_interaction
  expected: Game.Doors.DoorInteraction.OnPlayerInteract(Game.Player)
```

If SagaWeaver finds possible candidates, it may report them:

```text
Possible candidates:
  Game.Doors.DoorController.OnPlayerInteract(Game.Player)
  Game.Interactions.DoorInteraction.OnPlayerInteract(Game.Player)
```

But it must not auto-remap without an explicit editor/project transaction.

## 7.3 Overload Handling

Method name alone is not enough.

Invalid manifest identity:

```json
{
  "typeName": "DoorInteraction",
  "method": "OnPlayerInteract"
}
```

Required manifest identity:

```json
{
  "typeName": "Game.Doors.DoorInteraction",
  "method": "OnPlayerInteract",
  "methodSignature": "System.Void OnPlayerInteract(Game.Player)"
}
```

This prevents ambiguous overload binding.

## 7.4 Rename Transaction

A rename/move should be represented as a behavior identity transaction:

```json
{
  "transactionType": "BehaviorSymbolRelink",
  "behaviorId": "game.behavior.door_interaction",
  "oldSymbol": "Game.Doors.DoorInteraction.OnPlayerInteract(Game.Player)",
  "newSymbol": "Game.Doors.DoorController.OnPlayerInteract(Game.Player)",
  "approvedBy": "editor-or-user"
}
```

This keeps behavior identity stable while source symbols evolve.

---

# 8. Generated Scaffolding and User-Owned Source Boundary

Markerless authoring may create C# skeletons.

The editor may generate initial source, but source preservation begins once the user owns that file.

Recommended structure:

```text
Scripts/
  Behaviors/
    DoorInteraction.cs

  Behaviors/Generated/
    DoorInteraction.generated.cs
```

## 8.1 User-Owned Source

User-owned files:

```text
Scripts/Behaviors/**/*.cs
```

Rules:

```text
no whole-file regeneration
no formatting normalization
no automatic using reorder
no comment removal
only explicit source-span patches
```

## 8.2 Tool-Owned Generated Source

Generated files:

```text
Scripts/Behaviors/Generated/**/*.generated.cs
```

Rules:

```text
must include generated-file header
may be regenerated by tools
must not contain user-authored behavior truth
must not hide source mutations
```

Generated scaffold example:

```csharp
// <auto-generated />
namespace Game.Doors;

public partial class DoorInteraction
{
    // Generated binding hooks may live here.
}
```

User-authored file example:

```csharp
namespace Game.Doors;

namespace Game.Doors;

public partial class DoorInteraction
{
    public void OnPlayerInteract(Player player)
    {
        if (player.HasItem("key"))
        {
            Door.Open();
        }
    }
}
```

## 8.3 Source Preservation Rule

```text
SagaWeaver preserves user-owned source.
SagaWeaver may regenerate only explicitly generated files.
```

---

# 9. Roslyn Compile Context

SagaWeaver cannot reliably infer semantics from syntax alone.

It needs a compile context.

Recommended artifact:

```text
sagaweaver_compile_context.json
```

Example:

```json
{
  "schemaVersion": 1,
  "targetFramework": "net10.0",
  "languageVersion": "preview",
  "nullable": "enable",
  "defineConstants": [
    "SAGA_EDITOR",
    "SAGA_CLIENT"
  ],
  "references": [
    "Build/Saga/Assemblies/SagaRuntime.dll",
    "Build/Saga/Packages/Saga.Inventory.dll",
    "Build/Saga/Packages/Saga.Quest.dll"
  ],
  "sourceRoots": [
    "Scripts/",
    "Scripts/Behaviors/",
    "Scripts/Behaviors/Generated/"
  ],
  "generatedSources": [
    "Scripts/Behaviors/Generated/**/*.generated.cs"
  ]
}
```

## 9.1 Compile Context Responsibilities

Compile context tells SagaWeaver:

```text
target framework
language version
nullable mode
conditional compilation symbols
package assemblies
game assemblies
generated source roots
user-owned source roots
```

## 9.2 Failure Behavior

If compile context is missing or incomplete, SagaWeaver may still parse syntax, but semantic projection must degrade safely.

Possible status:

```text
SyntaxOnlyProjection
SemanticProjection
SemanticAnalysisFailed
```

Rule:

```text
If SagaWeaver cannot prove semantic safety, the region becomes opaque/read-only.
```


---

# 10. Auto Behavior Discovery

SagaWeaver should implement behavior discovery as a pipeline.

## 7.1 Input Sources

```text
project manifest
scene/entity/component bindings
behavior manifests
SDE authoring artifacts
script source roots
known event contract registry
package-provided event contracts
C# syntax and semantic model
```

## 7.2 Behavior Candidate Rules

SagaWeaver may identify behavior candidates from:

```text
public instance methods matching known event signatures
methods referenced by scene/entity bindings
methods listed in behavior manifests
methods in authoring roots that match Saga event names
methods declared in SDE authoring artifacts
optional advanced attributes
```

Example known event contract:

```json
{
  "eventId": "saga.event.player_interact",
  "methodName": "OnPlayerInteract",
  "parameters": [
    { "name": "player", "type": "Player" }
  ],
  "returnType": "void"
}
```

Candidate C#:

```csharp
public void OnPlayerInteract(Player player)
{
    if (player.HasItem("key"))
    {
        Door.Open();
    }
}
```

## 7.3 Ambiguity Handling

If SagaWeaver finds multiple possible matches, it should not guess silently.

It must emit diagnostics:

```text
AmbiguousBehaviorCandidate
MissingEventContract
UnsupportedEntrypointSignature
DuplicateBehaviorId
UnboundBehaviorMethod
```

Editor may then ask the user to choose a binding, but the user is still not marking "this is a block".

They are selecting gameplay binding, not block status.

---

# 11. Auto Block Projection

After behavior entrypoints are discovered, SagaWeaver infers block-projectable regions.

## 8.1 Inputs

```text
C# syntax tree
C# semantic model
known Saga API contracts
known event contracts
package node metadata
SDE block descriptors
compatibility profile
source map builder
```

## 8.2 Output

```text
projection_report.json
source_map.json
compatibility_report.json
sagascript_diagnostics.json
```

## 8.3 Inference Rule

A C# region is block-projectable only if SagaWeaver can prove:

```text
syntax shape is supported
semantic symbols are known or safely representable
control flow is supported
data flow is simple enough for current profile
source span can be mapped exactly
patch capability is known or region can be read-only
runtime binding can be generated or source link can be preserved
```

If SagaWeaver cannot prove safety, the region becomes opaque/read-only.

---

# 12. Known Node Registry

SagaWeaver needs a known node registry.

A node registry maps C# calls, properties, events, and operations to Visual Block metadata.

Sources:

```text
built-in Saga API metadata
package-provided node metadata
SDE block descriptors
compiled C# API metadata
manual package descriptor files
```

Important rule:

```text
Application users do not manually annotate their gameplay code as blocks.
Package authors may provide node metadata through manifests or descriptors.
```

## 9.1 Node Metadata Without Attributes

Package descriptor example:

```json
{
  "schemaVersion": 1,
  "nodes": [
    {
      "nodeId": "inventory.has_item",
      "displayName": "Has Item",
      "typeName": "InventoryApi",
      "method": "HasItem",
      "parameters": [
        { "name": "player", "type": "Player" },
        { "name": "itemId", "type": "string" }
      ],
      "returnType": "bool",
      "category": "Inventory"
    }
  ]
}
```

C# API:

```csharp
public static class InventoryApi
{
    public static bool HasItem(Player player, string itemId)
    {
        return player.Inventory.Contains(itemId);
    }
}
```

No `[SagaNode]` is required.

## 9.2 Optional Attribute Fallback

Attributes may remain for low-level package authors or tests:

```csharp
[SagaLibrary("Inventory")]
public static class InventoryNodes
{
    [SagaNode("Has Item")]
    public static bool HasItem(Player player, string itemId) { }
}
```

But architecture must treat this as optional convenience, not default workflow.

---

# 13. Compatibility Classifier

Purpose:

```text
Classify each relevant C# region as editable block, read-only block, opaque C#, invalid, parse-failed, or compile-failed.
```

Classifications:

```text
EditableBlock
  Safe to show and edit visually.

ReadOnlyBlock
  Safe to show visually but not edit.

OpaqueCSharp
  Valid C# but not safely representable as normal blocks.

UnsupportedInvalid
  C# construct violates Saga authoring profile or cannot be used in this context.

ParseFailed
  File cannot be parsed.

CompileFailed
  File cannot compile with available references.
```

Initial editable constructs:

```text
known event entrypoint
if statement
simple boolean condition
known method call
string literal
numeric literal
boolean literal
simple enum literal
simple return where safe
assignment to known simple property where safe
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


---

# 14. Editable Islands and Mixed-Mode Methods

A behavior method does not have to be entirely blocks or entirely C#.

A method may contain:

```text
editable block islands
read-only block regions
opaque C# regions
invalid regions
```

Example:

```csharp
namespace Game.Doors;

public partial class DoorInteraction
{
    public void OnPlayerInteract(Player player)
    {
        if (player.HasItem("key"))
        {
            Door.Open();
        }

        var enemies = world.Entities
            .Where(e => e.Team != player.Team)
            .OrderBy(e => e.DistanceTo(player))
            .ToList();
    }
}
```

SagaWeaver may classify:

```text
if (player.HasItem("key")) { Door.Open(); }
  -> EditableBlockRegion

LINQ/lambda query
  -> OpaqueCSharpRegion
```

Blocks View must show both:

```text
[On Player Interact]
  [If player Has Item "key"]
    [Open Door]

  [Advanced C# Region: LINQ/lambda]
    read-only / open source
```

## 14.1 Editable Island Rules

An editable island must have:

```text
exact source spans
stable node ids
known patch capabilities
semantic safety
post-patch validation
```

## 14.2 Opaque Region Rules

An opaque region must have:

```text
source link
diagnostic reason
visibility in Blocks View
no visual edit affordance
```

## 14.3 Mixed-Mode Rule

```text
A behavior may be partially editable through blocks while preserving advanced C# as opaque source-linked regions.
```


---

# 15. Projection Engine

Purpose:

```text
Generate deterministic Visual Block projection models from discovered behavior entrypoints.
```

Projection must include:

```text
behavior id
entrypoint id
source file
source hash
block tree/graph
node ids
source span references
edit capabilities
opaque region summaries
diagnostics
```

Projection must not:

```text
mutate source
hide important logic
pretend opaque code is editable
invent behavior not present in source/binding metadata
```

Example C#:

```csharp
public partial class DoorInteraction
{
    public void OnPlayerInteract(Player player)
    {
        if (player.HasItem("key"))
        {
            Door.Open();
        }
    }
}
```

Example projection:

```text
[On Player Interact]
  [If player Has Item "key"]
    [Open Door]
```

---

# 16. Source Map Generator

Purpose:

```text
Map projected blocks to exact C# source byte spans.
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
behavior id
patch capability
base source version/hash
```

Example:

```json
{
  "nodeId": "node_has_item_literal_001",
  "kind": "StringLiteral",
  "byteStart": 151,
  "byteEnd": 156,
  "lineStart": 5,
  "columnStart": 28,
  "lineEnd": 5,
  "columnEnd": 33,
  "patchCapability": "ReplaceStringLiteral"
}
```

Source maps are the bridge between Visual Blocks and exact source bytes.

Without precise source maps, SagaWeaver must not patch.

---

# 17. Patch Engine

Purpose:

```text
Turn supported Visual Block edits into minimal source patches.
```

Initial supported patch:

```text
ReplaceStringLiteral
```

Near-term patches:

```text
ReplaceNumericLiteral
ReplaceBooleanLiteral
ReplaceEnumValue
ReplaceSimpleFunctionArgument
ReplaceSimpleConditionOperator
ReplaceKnownNodeArgument
```

Patch process:

```text
1. Receive patch_request.json from editor/tool.
2. Verify source file exists.
3. Verify source hash/base version.
4. Resolve node id to source span.
5. Verify patch capability.
6. Validate replacement text.
7. Produce patch_preview.json.
8. If --apply is explicit, patch only exact span.
9. Re-run analysis.
10. Emit updated diagnostics and source map.
```

Default behavior:

```text
preview only
no source mutation without explicit apply
```

---

# 18. Runtime Binding Generator

Purpose:

```text
Generate deterministic runtime binding metadata for discovered behavior entrypoints and known nodes.
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
entrypoint id
function id
type name
method name
signature
runtime tier
node id -> function id mapping
source hash
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

Runtime binding generation does not mean runtime executes blocks.

Runtime executes compiled C#.

---

# 19. Artifact Architecture

Recommended output tree:

```text
Build/
  SagaWeaver/
    behavior_index.json
    sagaweaver_compile_context.json
    analysis_report.json
    compatibility_report.json
    projection_report.json
    source_map.json
    node_metadata.json
    runtime_bindings.json
    patch_preview.json

  Reports/
    sagaweaver_diagnostics.json
```

Canonical source remains:

```text
Scripts/**/*.cs
```

Optional layout metadata:

```text
.saga/
  VisualBlocks/
    **/*.sagablocks.layout.json
```

Layout metadata may store:

```text
node positions
collapsed state
panel layout hints
designer notes
```

Layout metadata must not store behavior truth.


---

# 20. Artifact Visibility and Source Leakage Policy

SagaWeaver artifacts may expose source-derived information.

Examples:

```text
file paths
type names
method names
literal values
source spans
diagnostics
opaque region summaries
patch previews
```

Therefore every SagaWeaver artifact should be assigned a visibility class.

## 20.1 Recommended Visibility Classes

```text
private-source
  source_map.json
  patch_preview.json
  detailed source diagnostics

team-authoring
  projection_report.json
  behavior_index.json
  compatibility_report.json

runtime-safe
  runtime_bindings.json with minimized source details

publish-report
  sanitized publish status and fatal diagnostic summary
```

## 20.2 Visibility Rules

```text
source_map.json should not be broadly shared by default
patch_preview.json should be treated as source mutation material
projection_report.json should not contain unnecessary source snippets
runtime_bindings.json should avoid leaking full source spans unless required
diagnostics should support redacted modes
```

## 20.3 Security Boundary Rule

```text
SagaWeaver is not the security boundary.
But SagaWeaver artifacts must be classifiable and redactable by policy.
```


---

# 21. CLI Architecture

Recommended command:

```bash
sagaweaver
```

Commands:

```text
discover
  discover behavior entrypoints from manifests, bindings, event contracts, and C# source

analyze
  parse scripts, build semantic model, classify compatibility, emit analysis reports

project-blocks
  generate projection_report.json and source_map.json without mutating source

patch
  preview/apply safe minimal source patches

extract-nodes
  extract package-provided node metadata from descriptors/assemblies/source

emit-bindings
  emit runtime_bindings.json for compiled assemblies and discovered entrypoints

validate
  validate artifacts, source hashes, runtime bindings, fatal diagnostics, and package node metadata
```

All commands must have deterministic exit codes and machine-readable diagnostics.

---

# 22. Editor Integration

SagaEditor consumes SagaWeaver artifacts.

Editor responsibilities:

```text
create behavior manifests when user creates behavior visually
bind entity/component events to behavior entrypoints
render Simple Blocks View from projection_report.json
render Pro Graph View from richer projection/IR later
open source file from source span
show diagnostics
show opaque node reasons
request patch preview
show diff
apply patch only after explicit user action
refresh artifacts after patch
show runtime binding status
```

Editor must not:

```text
ask user to mark code as block
require [SagaBehavior] for default workflow
parse C# independently for source truth
invent script compatibility rules
rewrite source directly from widgets
hide unsupported logic as if it were simple
```

---

# 23. Package System Integration

SagaPackageSystem resolves enabled packages.

SagaWeaver consumes package-provided metadata from the resolved graph.

Package contributions may include:

```text
known event contracts
known node metadata
SDE block descriptors
C# node libraries
runtime binding requirements
diagnostic rules
```

Flow:

```text
Project manifest
  -> PackageSystem resolves enabled packages
  -> SDE compiles package definitions
  -> SagaWeaver consumes event/node metadata
  -> SagaWeaver discovers behavior entrypoints
  -> SagaWeaver projects safe C# regions to blocks
  -> SagaEditor renders blocks
  -> Runtime executes compiled C#
```

SagaWeaver must not decide which packages are enabled.

---

# 24. SDE Integration

SDE and SagaWeaver meet through artifacts.

SDE may emit:

```text
event contract descriptors
node descriptors
asset schemas
graph schemas
editor surface declarations
validation rules
```

SagaWeaver consumes relevant outputs:

```text
event contracts
node descriptors
block descriptor metadata
validation metadata
```

Correct split:

```text
SDE:
  validates package definitions and structured authoring contracts

SagaWeaver:
  validates C# source, behavior discovery, source spans, block projection, and source patches
```

SDE does not parse C#.

SagaWeaver does not compile `.sde`.

---

# 25. Runtime and Server Integration

Runtime/server consume:

```text
compiled C# assembly
runtime_bindings.json
package manifest
script metadata
```

Runtime/server must not consume:

```text
visual block graph as execution model
editor layout metadata
patch preview
source map for normal execution
```

Runtime path:

```text
C# source
  -> compile
  -> assembly
  -> runtime_bindings.json
  -> cached function/delegate handles
  -> runtime/server execution
```

---

# 26. Diagnostics

Diagnostics must explain why a region is or is not block-projectable.

Example:

```json
{
  "code": "SAGAWEAVER_COMPAT_0042",
  "severity": "Warning",
  "category": "Compatibility",
  "message": "This region uses a LINQ lambda and is shown as opaque C# because Blocks V1 cannot safely edit it.",
  "path": "Scripts/Behaviors/DoorInteraction.cs",
  "line": 18,
  "column": 21,
  "behaviorId": "game.behavior.door_interaction",
  "suggestedAction": "Keep this region as C# or rewrite it using supported Saga-compatible constructs."
}
```

Important diagnostic categories:

```text
Discovery
Compatibility
Projection
SourceMap
Patch
RuntimeBinding
PackageNode
StaleArtifact
Publish
```

---

# 27. Publish Gate Integration

PublishGate should validate:

```text
behavior_index.json exists
projection_report.json exists where Blocks View is claimed
source_map.json matches current source hashes
node_metadata.json exists when package nodes are used
runtime_bindings.json exists when C# behaviors are packaged
no fatal SagaWeaver diagnostics
unsupported regions are either opaque/allowed or blocked by policy
no stale binding metadata
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

# 28. Collaboration Integration

SagaWeaver does not implement collaboration.

It emits reviewable patch artifacts.

Patch transaction shape:

```json
{
  "transactionId": "tx_000101",
  "operationType": "SourcePatch",
  "sourceView": "saga.view.simple_blocks",
  "targetArtifact": "Scripts/Behaviors/DoorInteraction.cs",
  "patchPreview": "Build/SagaWeaver/patch_preview.json",
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

# 29. Patch Permissions and Review Policy

Patch preview and patch apply must be controlled separately.

## 29.1 Patch Preview

Patch preview is non-mutating.

Users may be allowed to request preview even when they cannot apply.

```text
preview:
  reads source map
  validates patch capability
  creates patch_preview.json
  does not mutate source
```

## 29.2 Patch Apply

Patch apply mutates source.

It requires:

```text
explicit apply action
valid source hash
valid source span
patch capability
user/project permission
policy approval when required
post-patch validation
diagnostic report
```

## 29.3 Recommended Policy

```text
designer:
  may preview patches
  may apply safe patches only when project policy allows

programmer:
  may apply source patches directly in development policy

team project:
  may require source patch review

enterprise project:
  may require signed/reviewed patch transaction before apply
```

## 29.4 Publish Gate Rule

PublishGate may reject:

```text
unreviewed patch transaction
patch applied by unauthorized role
stale patch preview
source hash mismatch
post-patch compile failure
```

Principle:

```text
Blocks can propose source changes.
Policy decides who may apply them.
```


---

# 30. Testing Strategy

Required fixtures:

```text
DoorInteraction_BasicSupported.cs
DoorInteraction_WithCommentsAndFormatting.cs
DoorInteraction_UnsupportedLinq.cs
DoorInteraction_InvalidSyntax.cs
DoorInteraction_MultipleEntrypoints.cs
DoorInteraction_AmbiguousEventCandidate.cs
DoorInteraction_PatchStringLiteral.cs
PackageNode_DescriptorBased.cs
PackageNode_InvalidDescriptor.cs
PackageEventContract_PlayerInteract.json
DoorInteraction_RenameMoved.cs
DoorInteraction_MixedModeEditableIsland.cs
DoorInteraction_GeneratedScaffold.cs
DoorInteraction_VisibilityRedaction.cs
```

Required test families:

```text
markerless behavior discovery tests
event contract matching tests
scene/entity binding discovery tests
manifest-based discovery tests
ambiguity diagnostics tests
compatibility classifier tests
projection golden tests
source map span tests
no-edit byte-identical roundtrip tests
patch preview tests
patch apply tests
rollback tests
post-patch compile tests
node metadata descriptor tests
runtime binding tests
stale hash tests
CLI exit code tests
package node bridge tests
SDE artifact bridge tests
publish validation tests
symbol rename/move diagnostics tests
fully-qualified overload resolution tests
generated scaffolding boundary tests
compile context tests
editable island mixed-mode tests
artifact visibility/redaction tests
patch permission tests
structural edit rejection tests
```

Closure rule:

```text
A SagaWeaver milestone cannot close because blocks appear on screen.
It closes only when markerless discovery, source preservation, exact patch spans, deterministic artifacts, and runtime-safe binding are proven by tests.
```

---

# 31. MVP Vertical Slice

The first serious proof should be markerless.

Input C#:

```csharp
public partial class DoorInteraction
{
    public void OnPlayerInteract(Player player)
    {
        if (player.HasItem("key"))
        {
            Door.Open();
        }
    }
}
```

Input behavior manifest:

```json
{
  "behaviorId": "game.behavior.door_interaction",
  "sourceFile": "Scripts/Behaviors/DoorInteraction.cs",
  "typeName": "Game.Doors.DoorInteraction",
  "entrypoints": [
    {
      "event": "OnPlayerInteract",
      "method": "OnPlayerInteract",
      "methodSignature": "System.Void OnPlayerInteract(Game.Player)"
    }
  ]
}
```

Proof:

```text
DoorInteraction.cs
  -> discover behavior from manifest/event contract
  -> analyze C# without [SagaBehavior]
  -> infer block-projectable if/call/literal region
  -> generate read-only block projection
  -> verify no-edit roundtrip keeps bytes identical
  -> patch "key" -> "gold_key" through exact source span replacement
  -> verify unaffected bytes remain identical
  -> verify edited C# still compiles
  -> emit runtime_bindings.json
  -> emit deterministic diagnostics
```

If this proof fails, Saga should not expand into a larger visual scripting system.

---

# 32. Implementation Notes

## SW0 — Scope Freeze

```text
markerless-by-default decision recorded
no required [SagaBehavior] default workflow
no user-authored block markers
first proof fixture selected
```

## SW1 — Tool Skeleton

```text
Tools/SagaWeaver solution
Core/Saga/CLI split
unit tests
fixtures
sagaweaver --help
```

## SW2 — Behavior Manifest and Event Contract Fixtures

```text
behavior manifest schema
event contract schema
DoorInteraction.cs markerless fixture
ambiguous fixture
invalid fixture
```

## SW3 — Markerless Discovery v0

```text
discover behavior entrypoints from manifest
discover event contract matches
emit behavior_index.json
emit discovery diagnostics
```

## SW4 — Symbol Identity and Compile Context v0

```text
fully-qualified symbol identity support
method signature identity support
sagaweaver_compile_context.json
rename/move stale symbol diagnostics
```

## SW5 — Analyzer v0

```text
parse C# files
build semantic model where references are available
analyze discovered behavior entrypoints
emit analysis_report.json
```

## SW6 — Compatibility Profile v0

```text
supported construct classifier
opaque unsupported region classifier
compatibility_report.json
```

## SW7 — Read-Only Projection v0

```text
projection_report.json
event/if/call/literal blocks
opaque node metadata
source file not modified
```

## SW8 — Source Map v0

```text
source_map.json
content hash
byte offsets
line/column spans
patch capability metadata
```

## SW9 — No-Edit Roundtrip Proof

```text
input bytes == output bytes after projection
comments/whitespace/using order preserved
```

## SW10 — Patch Preview v0

```text
patch_request.json
patch_preview.json
ReplaceStringLiteral preview
stale hash rejection
```

## SW11 — Patch Apply v0

```text
--apply support
minimal span replacement
rollback helper
post-patch analysis
edited source compiles
```

## SW12 — Descriptor-Based Node Metadata v0

```text
node descriptor schema
package-provided node metadata
no required [SagaNode]
invalid descriptor diagnostics
```

## SW13 — Runtime Binding Manifest v0

```text
runtime_bindings.json
stable behavior/function ids
missing method diagnostics
stale metadata detection
```

## SW14 — Package and SDE Bridge v0

```text
consume package node metadata
consume SDE event/node descriptors
validate resolved metadata
```

## SW15 — Publish Validation Bridge

```text
sagaweaver validate
strict mode
publish-friendly script status summary
```

## SW16 — Editor Artifact Contract

```text
Behavior Inspector required fields
Simple Blocks required fields
Source Diff required fields
opaque node display contract
```

## SW17 — Editable Islands and Opaque Honesty v0

```text
mixed-mode method classification
editable island source maps
opaque region visible placeholders
Blocks View honesty diagnostics
```

## SW18 — Patch Policy and Artifact Visibility v0

```text
artifact visibility classification
redacted diagnostics mode
patch permission checks
review transaction metadata
```

## SW19 — MVP Closure

```text
markerless DoorInteraction proof passes end-to-end
no-edit source preservation proven
minimal patch proven
runtime binding proven
publish validation bridge proven
closure report written
```

---

# 33. Failure Modes

## 27.1 Marker Creep

Failure:

```text
Saga slowly starts requiring users to write attributes/comments to get blocks.
```

Mitigation:

```text
markerless discovery tests
default workflow tests without attributes
forbidden claim checks
editor-created manifests
event contract matching
```

## 27.2 Source Trust Failure

Failure:

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

## 27.3 Arbitrary C# Conversion Trap

Failure:

```text
Project tries to convert every C# construct into editable blocks.
```

Mitigation:

```text
compatibility profile
opaque nodes
explicit unsupported diagnostics
```

## 27.4 Runtime Graph Interpreter Drift

Failure:

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

## 27.5 Ambiguous Discovery

Failure:

```text
SagaWeaver guesses the wrong behavior entrypoint.
```

Mitigation:

```text
deterministic discovery priority
ambiguity diagnostics
editor binding resolution
no silent guessing
```


## 33.6 Symbol Identity Drift

Failure:

```text
Renamed or moved C# symbols silently rebind to the wrong behavior.
```

Mitigation:

```text
stable behaviorId
fully-qualified method signatures
stale symbol diagnostics
explicit relink transaction
no silent guessing
```

## 33.7 Generated Code Boundary Violation

Failure:

```text
SagaWeaver regenerates or rewrites user-owned source as if it were generated scaffolding.
```

Mitigation:

```text
generated source roots
user-owned source roots
generated-file header checks
boundary tests
```

## 33.8 Source Leakage Through Artifacts

Failure:

```text
Projection, diagnostics, source maps, or patch previews expose source details to users/pipelines that should not see them.
```

Mitigation:

```text
artifact visibility classes
redacted diagnostics
publish-safe summaries
policy-controlled source maps
```

## 33.9 Unauthorized Source Mutation

Failure:

```text
A visual block edit mutates C# source without permission or review.
```

Mitigation:

```text
preview/apply split
patch permission policy
review transaction metadata
publish gate validation
```


---

# 34. Final Decision

SagaWeaver should be built as SagaEngine's **markerless source-preserving C# ↔ Visual Blocks authoring bridge**.

Its role:

```text
Discover behavior entrypoints without requiring [SagaBehavior].
Infer block-projectable regions without requiring user-authored block markers.
Classify advanced C# as opaque/read-only instead of pretending it is editable.
Project safe regions into Visual Blocks.
Preserve source exactly when no edit occurs.
Patch only minimal source spans when supported edits occur.
Generate deterministic source maps, node metadata, diagnostics, and runtime bindings.
Bridge package-provided event/node metadata into authoring.
Maintain stable behavior identity across source changes.
Separate generated scaffolding from user-owned source.
Support mixed-mode methods with editable islands and opaque C# regions.
Classify artifact visibility and patch permissions.
Keep runtime execution on compiled C#.
```

Its boundary:

```text
Not the editor UI.
Not runtime graph execution.
Not a full IDE.
Not arbitrary C# to blocks.
Not package dependency resolver.
Not Lua/Python host.
Not the collaboration server.
Not the enterprise security boundary.
```

Engineering commandment:

```text
Never make users mark code as blocks.
Infer block projection from code and metadata.
```

Source commandment:

```text
Never sacrifice C# source trust for visual convenience.
```

Runtime commandment:

```text
Blocks are authoring projection.
Compiled C# is execution.
```

Product commandment:

```text
SagaWeaver should feel automatic to users and strict to engineers.
```

Identity commandment:

```text
Behavior identity is stable project truth.
C# symbol identity is current source location.
SagaWeaver must not silently confuse them.
```

Visibility commandment:

```text
Source-derived artifacts must be classified before they are shared.
```

---

# 35. Understanding Check: How C# and Visual Blocks Transition Works

## 29.1 User Writes Normal C#

Default C#:

```csharp
public partial class DoorInteraction
{
    public void OnPlayerInteract(Player player)
    {
        if (player.HasItem("key"))
        {
            Door.Open();
        }
    }
}
```

The user did not write:

```csharp
[SagaBehavior]
[SagaNode]
```

The user did not say:

```text
this is a block
```

## 29.2 Editor or Project Metadata Defines Gameplay Binding

Behavior manifest:

```json
{
  "behaviorId": "game.behavior.door_interaction",
  "sourceFile": "Scripts/Behaviors/DoorInteraction.cs",
  "typeName": "Game.Doors.DoorInteraction",
  "entrypoints": [
    {
      "event": "OnPlayerInteract",
      "method": "OnPlayerInteract",
      "methodSignature": "System.Void OnPlayerInteract(Game.Player)"
    }
  ]
}
```

This says:

```text
Game.Doors.DoorInteraction.OnPlayerInteract(Game.Player) is a gameplay behavior entrypoint.
The stable behavior identity is game.behavior.door_interaction.
```

It does not say:

```text
everything inside is editable blocks.
```

## 29.3 SagaWeaver Analyzes C#

SagaWeaver parses and analyzes:

```csharp
if (player.HasItem("key"))
{
    Door.Open();
}
```

It recognizes:

```text
if statement
known condition call: player.HasItem("key")
string literal: "key"
known action call: Door.Open()
```

## 29.4 SagaWeaver Creates Blocks Automatically

Editor may show:

```text
[On Player Interact]
  [If player Has Item "key"]
    [Open Door]
```

This is automatic projection.

The user did not mark it.

## 29.5 If User Edits a Supported Block

User changes:

```text
"key" -> "gold_key"
```

SagaWeaver finds the exact source span for `"key"` through `source_map.json`.

It previews:

```text
before: "key"
after:  "gold_key"
```

Then, only after explicit apply, it patches the exact literal span.

Result:

```csharp
if (player.HasItem("gold_key"))
{
    Door.Open();
}
```

No full-file regeneration occurs.

## 29.6 If C# Is Too Advanced

C#:

```csharp
var enemies = world.Entities
    .Where(e => e.Team != player.Team)
    .OrderBy(e => e.DistanceTo(player))
    .ToList();
```

SagaWeaver shows:

```text
[Advanced C# region: LINQ/lambda]
  read-only / opaque
  open source
```

Meaning:

```text
valid C#
visible in editor
not safely editable as Visual Blocks
runtime can still compile it if C# is valid
```

## 29.7 Runtime Executes C#, Not Blocks

Runtime path:

```text
C# source
  -> compile
  -> assembly
  -> runtime_bindings.json
  -> cached function/delegate handles
  -> runtime/server execution
```

Final model:

```text
C# = truth
Visual Blocks = inferred authoring view
source_map.json = exact bridge between block node and C# bytes
patch_preview.json = safe edit proposal
runtime_bindings.json = runtime callable binding metadata
runtime = compiled C# execution
```
