---
title: Scripting
status: current
owner: SagaWiki
generated_html: pages/scripting.html
---

# Scripting

C# source is the intended canonical managed behavior source. SagaScript owns analysis, compilation, generated artifacts, and source-authoring workflows. Runtime Scripting owns engine-side runtime contracts; EditorScripting and VisualBlocksEditor own editor integration and authoring behavior.

## Visual Blocks

Visual Blocks use the `SagaEditor::VisualBlocks` namespace and share the C# compilation/runtime semantic path rather than creating a second source of truth. There is no second runtime graph VM: C# remains the behavior source consumed by the runtime.

Block-edit evaluation and diff generation do not silently mutate source. An applicable patch is bounded by its recorded source span and base-source hash; changed spans, hash mismatch, or otherwise stale evidence must be rejected before an explicit apply. Generated graphs, source maps, patches, and reports are derived artifacts. Current code does not establish arbitrary lossless C# round-tripping, secure untrusted-code execution, or a finished visual-authoring and debugging product.

## Compatibility boundary

Projection metadata distinguishes source that can be represented, source that is only partly representable, advanced or unsafe regions that must remain opaque and read-only, and unsupported input that must produce diagnostics. High-level and low-level views are authoring classifications over the same C# language, not separate languages or runtimes. Valid advanced C# remains source even when Visual Blocks cannot safely edit it.
