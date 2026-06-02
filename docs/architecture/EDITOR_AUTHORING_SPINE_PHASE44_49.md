# Editor Authoring Spine Phase 44-49

## Scope

Block G adds the first backend-neutral editor authoring model over Technical
Preview project and SagaScript artifacts. The implemented surface is read-only:
it parses a minimal `.sagaproj` display subset, indexes existing SagaScript
artifacts, exposes source links, exposes read-only block projection data, and
loads patch preview review data.

No app startup wiring, Qt UI, product-shell behavior, scene mutation, entity
mutation, artifact generation, C# source mutation, or collaboration work is
part of this slice.

## Roadmap Alignment

The roadmap defines Block G as Phase 44 through Phase 49. Phase 50 is the
opening collaboration audit for Block H, so this checkpoint is recorded inside
Phase 49 evidence.

Phase 45's roadmap editing surface is deferred in this slice. The current
implementation provides a read-only project/artifact status surface only.

Phase 48's roadmap edit handoff is deferred in this slice. The current
implementation loads `patch_preview.json` as a review artifact only and does
not execute the preview.

## Project Subset Model

`TechnicalPreviewProjectView` reads only the fields needed for editor display:

- `projectId`
- `displayName`
- project root
- `scriptFolders`
- diagnostics path
- generated reports path
- launch profile references
- package profile references

The reader is not a full validator and does not duplicate SagaProjectKit's
project rules. Invalid subset parsing reports
`Editor.Project.EditorSubsetParseFailed` with text that includes
`editor subset parse failed`.

## Artifact Model

`ScriptArtifactIndex` reads existing files under `Build/SagaScript`:

- `analysis_report.json`
- `projection_report.json`
- `source_map.json`
- `runtime_bindings.json`
- `node_metadata.json`
- `patch_preview.json`

Freshness is deterministic. If `source_map.json` contains source hashes and the
source file exists, the editor model recomputes SHA-256 over the source bytes
and compares the hash. Missing source is `MissingSource`; hash mismatch is
`Stale`; missing hash data is `UnknownFreshness`. File modified time is not
used as primary truth.

## Read-Only Projection

`ScriptBehaviorProjectionView` and `ScriptBlockProjectionNodeView` consume
`projection_report.json` and `source_map.json` without using graph mutation
documents. The view preserves `apiDomain`, `apiLevel`, behavior ids, node ids,
source spans, and opaque/read-only status.

Unsupported source regions remain opaque/read-only and keep source links where
the source map provides them.

## Patch Preview Review

`ScriptPatchPreviewView` reads `patch_preview.json` and exposes review data:
operation, target node, source hash, byte span, old text, new text, and
diagnostics. It always reports `applyAvailable = false`; the editor model does
not write source files or run patch operations.

## Inspector

`ScriptBehaviorInspectorView` joins projection data with
`runtime_bindings.json` when present. It exposes behavior level/domain,
binding status, source spans, opaque reasons, and projected node details.

## Deferred

- Scene/entity/property editing from roadmap Phase 45.
- Editor execution of patch previews from roadmap Phase 48.
- Product-shell startup wiring.
- Qt panel/view implementation.
- Background SagaScript tool execution.
- Collaboration and Phase 50+ work.

## Evidence

Focused evidence is `EditorAuthoringSpineTests` plus
`EditorQtPublicAbiBoundaryTests`. The tests cover read-only project subset
loading, subset parse diagnostics, deterministic artifact freshness states,
read-only projection axes, patch preview review with no apply action, behavior
inspector binding status, and the existing Qt public ABI guard.
