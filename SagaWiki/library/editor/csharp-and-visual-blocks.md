---
title: C# and Visual Blocks
status: current
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# C# and Visual Blocks

C# source is the behavior source of truth. Visual Blocks are an editor projection and authoring surface over that source. They do not define an independent behavior graph, bytecode format, or second runtime virtual machine.

## Many views, one behavior

Block descriptors, graph layout, source maps, diagnostics, compiled assemblies, and binding manifests support editing or execution. Their authority is bounded:

- graph layout records presentation;
- block/graph models express an editable projection;
- generated C# is derived when a visual operation produces source;
- compiled assemblies execute validated source inputs;
- binding descriptors connect script and native contracts;
- source maps relate generated or projected spans back to authored source.

No artifact may silently override the C# source it was derived from.

## Block and node classification

The editor block library owns palette descriptors, categories, slots, instances, snap rules, and lowering inputs. Script tooling may extract node-library metadata from C# source. Metadata must classify a node honestly as runtime-backed, projection-only, or unsupported/placeholder according to available evidence. A visible node name or generated `node_metadata.json` does not prove inventory, audio, spawning, scheduling, or any other runtime feature by itself.

## Safe patch application

A source patch must identify the expected file, span, and prior content or hash. Application should reject stale or ambiguous input instead of mutating whatever text currently occupies the range. The review surface should show the proposed semantic change and diagnostics before commit to source.

Editor tooling must not make unannounced source edits in the background. Hot reload and compile feedback can shorten the loop, but they do not weaken staleness checks or source ownership.

## API levels

High-level scripting contracts are the default authoring surface. Low-level lifecycle, handle, and host interfaces exist for controlled integration. Unsafe or native interop capability should be explicit, narrow, and validated rather than implied by ordinary script access.

## Current surface boundary

VisualBlocksEditor keeps CoreCLR hosting, managed/native bridges, assembly contexts, hot reload, and graph evaluation runners under `Private`. Their implementation presence does not change the source-of-truth rule or establish a Visual Blocks runtime.

| Surface | Contract reading |
| --- | --- |
| Descriptor, graph, IR, source-map, import, projection, and diagnostics values | Candidate editor-authoring contracts where used across module boundaries. |
| CoreCLR host, script host, assembly context, native/managed bridges, hot reload, and evaluation runners | Private runtime-host/editor-evaluation implementation; not an editor consumer contract. |
| Canvas and debugger surfaces | Remaining editor workflow surface; public visibility is not a shipped graph-runtime or stable editor-SDK claim. |

See [C# and Visual Blocks contracts](../reference/csharp-and-visual-blocks-contracts.md) for compatibility categories, projection/graph/IR boundaries, node levels, exact hash/span patching, staleness, compile, and runtime rules.

The separate native/managed execution boundary is documented in [Runtime scripting host and lifecycle](../reference/runtime-scripting-host-and-lifecycle.md).
