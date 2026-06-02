# Editor Workflow Usability Phase 76-81

Phase 76-81 adds backend-neutral editor workflow state for Block C:

- Phase 76: Project Browser Improvements
- Phase 77: Behavior Inspector v1
- Phase 78: Diagnostics Panel v1
- Phase 79: Source Diff / Review Panel v1
- Phase 80: Scene / Asset Browser v1
- Phase 81: Simple/Pro/C# View Navigation State

The implementation lives in `Editor/include/SagaEditor/Authoring` and
`Editor/src/SagaEditor/Authoring`. The headers are plain C++ model surfaces.
They do not expose Qt types, Qt widgets, Qt macros, or public Qt panel ABI.

## Ownership Rules

C# source remains canonical. SagaScript owns source mutation. The Editor does
not write C# directly, format source, regenerate source, apply patches, or roll
back patches directly.

Phase 79 consumes SagaScript-owned report artifacts:

- `patch_preview.json`
- `patch_apply_report.json`
- `patch_diff_report.json`
- `patch_review_report.json`
- `patch_rollback_report.json`

Approved review is metadata only in the editor model. It does not mean the patch
was applied. Editor apply and rollback action descriptors are disabled with the
reason: `SagaScript-owned source mutation only.`

## Workflow Models

`ProjectBrowserWorkflowView` reads the `.sagaproj` technical preview subset and
reports project identity, root paths, script folders, launch profiles, package
profiles, and the `Scenes`, `Scripts`, `Assets`, `Packages`, and `Reports`
sections. It reports missing manifests, invalid JSON, missing folders, and
missing report roots deterministically without creating directories or launching
apps.

`ScriptBehaviorInspectorView` now consumes projection, source map, runtime
binding, node metadata, compatibility profile, and optional script artifact
validation evidence. `ProjectionOnly` is not runtime proof. `Deferred` is not
runtime proof. Runtime-backed nodes without explicit runtime evidence are shown
as missing proof, not assumed ready.

`DiagnosticsPanelView` groups diagnostics into critical, warning, and info
groups from `diagnostics_summary.json`, project browser diagnostics,
SagaScript artifact diagnostics, script validation diagnostics, and behavior
inspector warnings. Refresh recomputes from files and does not write files.

`SceneAssetBrowserInventoryView` is inventory/audit only. When no accepted scene
or entity source-of-truth files exist, it reports `MissingSourceOfTruth` and
does not invent scene editing, entity placement, prefab editing, asset import,
or runtime asset execution.

`ViewNavigationWorkflowState` models Simple, Pro, and C# source view state using
the existing SagaViewKit capability truth. It preserves selected behavior, node,
artifact, and source link across transitions. Simple View discloses opaque,
deferred, and unsupported content and cannot mark unsupported regions editable.
C# Source View exposes canonical source path/span/hash as read-only visibility.

## Deferred

This pass does not deliver a full editor MVP, full visual scripting, arbitrary
C# to blocks, graph editing, drag/drop node editing, runtime gameplay, server
gameplay, ClientHost work, Qt panel implementation, Qt public ABI expansion,
asset import pipeline, scene editing, prefab editing, entity placement, or Hedef
3 work.

## Evidence

Focused verification:

```sh
nix-shell --run "cmake --build build/RelWithDebInfo-0.0.9 --target EditorAuthoringSpineTests SagaArchitectureTests -j 1"
nix-shell --run "ctest --test-dir build/RelWithDebInfo-0.0.9 -R 'EditorAuthoringSpineTests|EditorQtPublicAbiBoundaryTests' --output-on-failure -j 1"
```
