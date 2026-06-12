# C# / Visual Authoring Boundary

> Status: Current compact boundary

This is the canonical short boundary for Saga C# and future Visual Blocks
authoring. It summarizes what current contracts allow and what the larger
proposed SagaWeaver/C# architecture notes must not be read to promise.

## Current Contract

- C# source is canonical.
- Visual Blocks are a source-preserving projection over compatible C# source
  spans.
- Runtime executes compiled C# and runtime/server artifacts, not a visual graph
  VM.
- The current projection contract is metadata/report oriented.
- Patch planning is preview-first.
- The only bounded apply path writes a patched copy for a validated string
  literal edit; it does not overwrite the original source by default.
- The editor may request, preview, review, and display source patch artifacts,
  but the editor must not write C# source directly.

## Supported Evidence Surface

Current evidence should be read through these compact contracts:

- [C# Visual Blocks Compatibility Profile](CSHARP_VISUAL_BLOCKS_COMPATIBILITY_PROFILE.md)
- [Visual Block Patch Contract](VISUAL_BLOCK_PATCH_CONTRACT.md)
- [Source Patch Application Policy](SOURCE_PATCH_APPLY_POLICY.md)
- [Authoring Authority Model](AUTHORING_AUTHORITY_MODEL.md)

The compatibility profile proves bounded analysis/projection metadata and
source-byte preservation checks for fixtures. It does not prove a complete
visual scripting product.

## Explicit Non-Claims

- No complete Visual Blocks editor UI.
- No arbitrary C# to blocks conversion.
- No perfect C# roundtrip.
- No editor-owned C# writes.
- No source-file regeneration workflow.
- No runtime visual graph execution.
- No full IDE, refactoring engine, or language server.
- No package-wide authoring bridge claim unless a focused contract proves it.

## Proposed Appendices

The larger SagaWeaver and C# Visual Blocks architecture files are proposed
direction appendices. They preserve design intent, risks, and future merge
ideas. They do not supersede this boundary or current product status.
