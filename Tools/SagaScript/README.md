# SagaScript Toolchain

SagaScript is the standalone C# metadata validation and manifest emission tool
for the SagaScript ecosystem.

Current scope:

- Parse C# source with Roslyn.
- Validate SagaScript attributes and conservative v1 type support.
- Inspect block-callable bindings.
- Emit JSON-compatible binding, capability, artifact, and diagnostics reports.
- Compile validated script source into .NET 10 script assemblies.
- Analyze `[SagaBehavior]` metadata for the Block F source-preserving
  SagaWeaver MVP.
- Extract `[SagaLibrary]` and `[SagaNode]` metadata into a deterministic node
  library report for the Phase 69-71 authoring expansion.
- Project supported `Gameplay + High` and `Gameplay + Low` C# into read-only
  block metadata with exact source spans.
- Emit `analysis_report.json`, `runtime_bindings.json`, `source_map.json`,
  `projection_report.json`, and `node_metadata.json`.
- Emit `node_library_report.json` from `extract-nodes`.
- Preview one safe `ReplaceStringLiteral` patch without mutating source.
- Apply one safe `ReplaceStringLiteral` patch through SagaScript with source
  hash validation, backup, same-directory temp write, exact byte-span
  verification, rollback on failed post-check, and stale-artifact reporting.
- Emit non-mutating diff and review reports for one `ReplaceStringLiteral`
  patch, and roll back a passed Phase 72 apply report through SagaScript-only
  hash-gated backup restore.

Current non-goals:

- No runtime or server execution.
- No editor UI.
- No Forge or Prism implementation ownership.
- No complete visual node authoring.
- No general C# conversion.
- No behavior-equivalent C# regeneration.
- No general source rewrite or multi-operation patch apply.
- No editor-side C# source writes.
- No broad undo/redo stack or redo mutation.
- No live code reload, debug tooling, or hardened script sandbox.

Example:

```sh
Tools/SagaScript/sagascript validate --source Scripts --json
Tools/SagaScript/sagascript inspect-bindings --source Scripts --out Build/Manifests/script_bindings.inspect.json
Tools/SagaScript/sagascript emit-manifests --source Scripts --out Build/Manifests
Tools/SagaScript/sagascript compile --source Scripts --out Build/Manifests --artifacts-out Build/Artifacts/Scripts --project-root .
Tools/SagaScript/sagascript analyze --source Scripts --out Build/SagaScript
Tools/SagaScript/sagascript emit-bindings --source Scripts --out Build/SagaScript
Tools/SagaScript/sagascript project-blocks --source Scripts --out Build/SagaScript
Tools/SagaScript/sagascript extract-nodes --source Scripts --out Build/SagaScript
Tools/SagaScript/sagascript patch-preview --source Scripts --source-map Build/SagaScript/source_map.json --request patch_request.json --out Build/SagaScript/patch_preview.json
Tools/SagaScript/sagascript patch-apply --source Scripts --source-map Build/SagaScript/source_map.json --request patch_request.json --out Build/SagaScript/patch_apply_report.json
Tools/SagaScript/sagascript patch-diff --source Scripts --source-map Build/SagaScript/source_map.json --request patch_request.json --out Build/SagaScript/patch_diff_report.json
Tools/SagaScript/sagascript patch-review --diff Build/SagaScript/patch_diff_report.json --decision Approved --reviewer reviewer://local --out Build/SagaScript/patch_review_report.json
Tools/SagaScript/sagascript patch-rollback --apply-report Build/SagaScript/patch_apply_report.json --out Build/SagaScript/patch_rollback_report.json
```

`compile` requires a .NET 10 SDK. When the SDK is missing, the launcher emits a
structured `Script.Toolchain.DotNetSdkMissing` diagnostic instead of allowing the
.NET SDK to print a raw target-framework error. SagaScript does not downgrade the
target framework automatically.

Compile outputs:

- `Build/Artifacts/Scripts/SagaProjectScripts.scripts.dll`
- `Build/Artifacts/Scripts/SagaProjectScripts.scripts.runtimeconfig.json`
- `Build/Artifacts/Scripts/SagaProjectScripts.scripts.pdb`
- `Build/Manifests/script_bindings.json`
- `Build/Manifests/script_capabilities.json`
- `Build/Manifests/script_artifacts.json`
- `Build/Manifests/artifact_manifest.scripts.json`
- `Build/Manifests/sagascript_diagnostics.json`

SagaWeaver MVP outputs:

- `Build/SagaScript/analysis_report.json`
- `Build/SagaScript/runtime_bindings.json`
- `Build/SagaScript/source_map.json`
- `Build/SagaScript/projection_report.json`
- `Build/SagaScript/node_metadata.json`
- `Build/SagaScript/node_library_report.json`
- `Build/SagaScript/csharp_compatibility_profile_v2.json`
- `Build/SagaScript/script_artifact_validation_report.json`
- `Build/SagaScript/sagascript_diagnostics.json`

## Patch Apply

`patch-apply` is SagaScript-only and accepts one request operation:
`ReplaceStringLiteral`.

Required request fields:

- `operation`
- `nodeId`
- `baseSourceHash`
- `expectedOldText`
- `replacement`

Apply rejects stale hashes, missing source files, non-string targets, read-only
targets, deferred/opaque/unsupported compatibility, invalid spans, and
`expectedOldText` mismatches before writing. On success it writes
`patch_apply_report.json`, creates a sibling `.saga-backup.<operationId>` file,
writes a sibling `.saga-tmp.<operationId>` temp file before replacement, verifies
that only the approved UTF-8 byte span changed, and reports generated artifacts
as stale without regenerating them.

## Patch Diff, Review, And Rollback

`patch-diff` is non-mutating. It accepts the same single
`ReplaceStringLiteral` request shape as `patch-apply`, validates the current
source and source-map target, and emits `patch_diff_report.json` with byte and
unified text diff data.

`patch-review` is non-mutating. It records `Approved` or `Rejected` review
metadata for a passed `patch-diff` report. Approval does not apply a patch.

`patch-rollback` is SagaScript-only source mutation. It restores from a passed
`patch_apply_report.json` only when the current source hash matches
`newSourceHash` and the backup hash matches `baseSourceHash`. Rollback writes
through a same-directory temp file, verifies the restored hash and bytes, and
restores the pre-rollback source bytes if the post-check fails.

## Two-Axis API Metadata

SagaScript classifies public script-facing APIs with two explicit axes:

- domain: `Gameplay`, `Runtime`, `Server`, `Networking`, `UI`, `Diagnostics`,
  `Assets`, `Packaging`
- level: `High`, `Low`

Block F implements the `Gameplay + High` and `Gameplay + Low` projection proof.
Other domains are schema-defined for future expansion and must not be claimed as
implemented unless a focused test proves them.

## Phase 69-71 Node Library Metadata

`extract-nodes` scans C# source for `[SagaLibrary]` and `[SagaNode]` metadata
without executing gameplay code or mutating source. It writes
`node_library_report.json` with library entries, node entries, api domain, api
level, capability, source spans, deterministic diagnostics, and source hashes.

The built-in gameplay node fixtures used by tests are metadata fixtures only:

- Gameplay.High nodes are `ProjectionOnly`.
- Gameplay.Low trigger, spawn, timer, and entity lifecycle nodes are `Deferred`.

`ProjectionOnly` and `Deferred` are not runtime proof. Deferred nodes must stay
disclosed/read-only in projection and view honesty checks.

## Phase 74-75 Compatibility And Script Evidence

`compatibility-profile` writes `csharp_compatibility_profile_v2.json` with
conservative classifications for Saga-compatible C# source. Current
`EditableByPatch` support is limited to `StringLiteral` targets that can use
`ReplaceStringLiteral`; numeric, boolean, enum, condition rewrite, argument
rewrite, method insertion, and method-call insertion/removal are not editable.

`emit-bindings` now preserves read-only node metadata in `runtime_bindings.json`:
node id, kind, domain, level, capability, projection compatibility,
compatibility classification, library ids, and runtime proof state.

`validate-artifacts --artifact-root <dir> --out <report>` checks existing
SagaScript artifacts without mutating source or package contents. ProjectionOnly
and Deferred entries do not satisfy runtime proof. RuntimeBacked entries require
explicit evidence or validation fails.

These commands do not add runtime node execution, runtime gameplay, server
gameplay, Editor UI, Qt UI, graph editing, full visual scripting, or arbitrary
C# to blocks.
