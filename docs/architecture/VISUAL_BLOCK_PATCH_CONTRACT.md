# Visual Block Patch Contract

## Status

Phase 15 status is `Implemented-Unverified`.
Phase 16 status is `Implemented-Unverified`.

This document defines the first source-preserving block operation contract for
SagaScript read-only Visual Blocks projection artifacts. Phase 15 is
preview-only. Phase 16 adds one copy-output apply path for a passed
`StringLiteralEdit` preview. It does not overwrite the original C# file,
implement an editor UI, or make blocks editable in the Phase 14 projection
artifact.

## Command

SagaScript exposes the contract through:

```bash
sagascript plan-block-edit \
  --projection <visual_blocks_projection_v1.json> \
  --operation <operation.json> \
  --out <block_patch_preview_v1.json> \
  --json
```

The command reads a Phase 14 projection artifact and a single operation request.
It writes a dry-run `block_patch_preview_v1.json` report.

## Operation Request

The request is a JSON object:

```json
{
  "operationKind": "StringLiteralEdit",
  "targetBlockId": "visual-block://...",
  "requestedValue": "gold_key"
}
```

`operationId` may be supplied by a caller. If omitted, SagaScript derives a
deterministic id from operation kind, target block id, and requested value.

## Allowed Contract Kinds

Phase 15 recognizes these operation kinds as the initial contract vocabulary:

- `LiteralValueEdit`
- `BooleanLiteralEdit`
- `NumericLiteralEdit`
- `StringLiteralEdit`
- `MethodArgumentLiteralEdit`
- `SupportedConditionLiteralEdit`

Only `StringLiteralEdit` produces a passing preview in this phase. Other
contract-known operations are rejected until a later phase adds safe fixtures
and preview behavior.

## Rejected Operations

SagaScript rejects operations that would require source generation,
reformatting, advanced C# lowering, or editing opaque/unsupported regions.

Rejected kinds include:

- `ArbitrarySourceRewrite`
- `MethodBodyRegeneration`
- `ClassRestructure`
- `UsingRewrite`
- `FormattingRewrite`
- `UnsupportedRegionEdit`
- `OpaqueRegionEdit`
- `AdvancedCSharpLowering`
- `AsyncRewrite`
- `ReflectionRewrite`
- `UnsafeRewrite`
- `ControlFlowRestructure`

## Preview Report

`block_patch_preview_v1.json` includes:

- `schemaVersion`
- `tool`
- `command`
- `sourceFile`
- `operationId`
- `operationKind`
- `targetBlockId`
- `targetSourceHash`
- `targetSourceSpan`
- `requestedValue`
- `status`
- `patchPreview`
- `diagnostics`
- `sourcePreservation`
- `mutatesSource`
- `nonClaims`

For a passing preview, `patchPreview` describes only a minimal span replacement:

```json
{
  "mutatesSource": false,
  "replacementKind": "MinimalSpanReplacement",
  "startByte": 0,
  "endByte": 0,
  "replacementText": "\"gold_key\""
}
```

## Apply Command

Phase 16 exposes the first safe apply path through:

```bash
sagascript apply-block-edit \
  --preview <block_patch_preview_v1.json> \
  --source-root <path> \
  --out <dir> \
  --json
```

The command accepts only a passed `plan-block-edit` preview for
`StringLiteralEdit`. It validates the preview command, operation kind,
`targetSourceHash`, source root, byte span, and quoted C# string literal
replacement.

By default it writes:

- `<out>/block_patch_apply_v1.json`
- `<out>/patched-source/<relative-source-path>`

It does not overwrite the original source file.

## Apply Report

`block_patch_apply_v1.json` includes:

- `schemaVersion`
- `tool`
- `command`
- `operationId`
- `operationKind`
- `targetBlockId`
- `sourceFile`
- `patchedSourceFile`
- `targetSourceHash`
- `targetSourceSpan`
- `originalHash`
- `patchedHash`
- `changedSpanOnly`
- `status`
- `diagnostics`
- `sourcePreservation`
- `mutatesSource`
- `nonClaims`

## Source Preservation Rules

- `plan-block-edit` does not write C# source.
- A preview is allowed only when the target block exists, has a source span and
  source hash, and is classified as patch-capable compatibility metadata.
- Opaque and unsupported diagnostic blocks are rejected.
- `apply-block-edit` writes a patched copy by default and leaves the original C#
  source file unchanged.
- `apply-block-edit` rejects stale source hashes, malformed spans, failed
  previews, opaque regions, and unsupported diagnostic regions.
- `apply-block-edit` verifies that only the target byte span changed in the
  patched copy.

## Non-Claims

- No Visual Blocks editor UI is implemented.
- No block editing UI is implemented.
- No arbitrary C# to blocks conversion is claimed.
- No full visual scripting claim should be derived from this contract.
- Runtime remains compiled C#; visual graph interpretation is not introduced.
- Phase 16 does not perform in-place source mutation by default.
- Phase 16 does not implement method, class, or file regeneration.
