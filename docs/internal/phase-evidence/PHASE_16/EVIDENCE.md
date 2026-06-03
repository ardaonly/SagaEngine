# Phase 16 Evidence

## Status

Implemented-Unverified

## Phase Scope

Phase 16 adds the first safe block edit apply path for SagaScript/SagaWeaver
metadata. The implemented path is intentionally narrow:

- `sagascript plan-block-edit` now surfaces `targetSourceHash`.
- `sagascript apply-block-edit` consumes a passed `block_patch_preview_v1.json`.
- Only `StringLiteralEdit` is applied.
- The patched C# source is written as a copied output file under
  `<out>/patched-source/`.
- The original source file is not overwritten by default.
- `block_patch_apply_v1.json` records original hash, patched hash,
  `changedSpanOnly`, diagnostics, source preservation, and non-claims.

## Evidence Summary

The SagaScript CLI tests cover:

- accepted `StringLiteralEdit` preview metadata with target source hash;
- copied-source apply output;
- unchanged original source bytes;
- exact-span-only replacement;
- comment, whitespace, and using-order preservation outside the target span;
- stale source hash rejection;
- malformed preview span rejection;
- failed preview rejection for opaque/read-only targets.

No runtime, server, editor, package/distribution, SDE, StarterArena gameplay, or
CSharpScriptHost behavior was changed.

## Changed Files

See `changed_files.txt`.

## Verification Commands

See `commands.log`.

## Command Results

The recorded local checks passed in this batch. The phase remains
`Implemented-Unverified` because maintainer verification has not occurred.

## Required Files

- `EVIDENCE.md`: present
- `commands.log`: present
- `changed_files.txt`: present
- `known_limitations.md`: present
- `verification_result.json`: present

## Manual Checks

- [x] Public docs do not claim full Visual Blocks editing.
- [x] Known limitations are documented.
- [x] The apply path writes a patched copy instead of overwriting source.
- [x] Unsupported behavior is not hidden.
- [x] No phase is marked `Verified`.

## Known Limitations

See `known_limitations.md`.

## Verification Decision

Implemented-Unverified

## Decision Reason

SagaScript has a real copy-output `apply-block-edit` command for one safe
`StringLiteralEdit` from a passed Phase 15 preview artifact. It rejects stale,
malformed, failed-preview, opaque, and unsupported inputs. It is not maintainer
verified and is not a full Visual Blocks editor or arbitrary C# patch system.
