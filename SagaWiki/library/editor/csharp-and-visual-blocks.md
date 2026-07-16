---
title: C# and Visual Blocks
status: current
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# C# and Visual Blocks

C# source is the behavior source of truth. Visual Blocks are an editor projection and authoring surface over that source. They do not define an independent behavior graph, bytecode format, or second runtime virtual machine.

## Many views, one behavior

Block definitions, instances, stacks, slots, snap rules, and block IR support bounded authoring. Compiled assemblies and binding manifests belong to the scripting tool/runtime path. Their authority is bounded:

- block values express an authoring projection;
- generated C# is derived when a visual operation produces source;
- compiled assemblies execute validated source inputs;
- binding descriptors connect script and native contracts;
- read-only product descriptors record evidence without becoming source.

No artifact may silently override the C# source it was derived from.

## Block and node classification

The editor block library owns palette descriptors, categories, slots, instances, snap rules, and lowering inputs. Script tooling may extract node-library metadata from C# source. Metadata must classify a node honestly as runtime-backed, projection-only, or unsupported/placeholder according to available evidence. A visible node name or generated `node_metadata.json` does not prove inventory, audio, spawning, scheduling, or any other runtime feature by itself.

## Safe patch application

A source patch must identify the expected file, span, and prior content or hash. Application should reject stale or ambiguous input instead of mutating whatever text currently occupies the range. The review surface should show the proposed semantic change and diagnostics before commit to source.

Editor tooling must not make unannounced source edits in the background. Compile feedback can shorten the loop, but it does not weaken staleness checks or source ownership.

## API levels

High-level scripting contracts are the default authoring surface. Low-level lifecycle, handle, and host interfaces exist for controlled integration. Unsafe or native interop capability should be explicit, narrow, and validated rather than implied by ordinary script access.

## Current surface boundary

The public `VisualBlocksEditor` surface is intentionally limited to `SagaEditor/VisualBlocks/Blocks/**`. Compiler, debugger, editor, graph, import, node, pin, type-system, evaluation, and runtime-host placeholder surfaces are not public contracts. Product integration emits read-only descriptor/evidence data from private implementation.

| Surface | Contract reading |
| --- | --- |
| Block category, definition, kind, slot, instance, stack, script, snap rules, block IR, library, and lowering | The evidenced block-authoring contract. |
| Read-only descriptor/evidence generation | Private product integration; derived evidence only. |
| Compiler, debugger, graph editor/document, imports, nodes, pins, type system, evaluation, and runtime host | Absent from the Visual Blocks public contract. |

See [C# and Visual Blocks contracts](../reference/csharp-and-visual-blocks-contracts.md) for compatibility categories, projection/graph/IR boundaries, node levels, exact hash/span patching, staleness, compile, and runtime rules.

The separate native/managed execution boundary is documented in [Runtime scripting host and lifecycle](../reference/runtime-scripting-host-and-lifecycle.md).
