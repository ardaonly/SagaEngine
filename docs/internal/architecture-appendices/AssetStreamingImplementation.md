# Asset Streaming System — Implementation Note

> Last updated: 2026-05-18
> Status: Implementation-history appendix
> Location: `docs/internal/architecture-appendices/AssetStreamingImplementation.md`
> Scope: Implemented runtime asset streaming architecture, design decisions, known constraints, follow-up risks, and runtime/asset-pipeline boundary alignment.

---

## 0. Document Status

It records the implemented asset streaming architecture for SagaEngine's runtime/resource layer.

This document should remain useful as implementation history and design rationale.

It must not be treated as the current architecture index entry point or source
of truth for product direction.

Current ownership truth belongs in:

- `architecture/RUNTIME.md`
- `architecture/ASSETS_AND_PACKAGES.md`
- `architecture/PUBLISH.md`
- `architecture/EDITOR.md`

Runtime streaming is one part of the content pipeline.

It is not the entire content pipeline.

---

## 1. Related Documents

This document describes the runtime asset streaming implementation.

It does not own source asset import, cook, package staging, publish readiness, editor asset UX, or stale artifact analysis.

Related current documents:

| Document                            | Purpose                                                                                             |
| ----------------------------------- | --------------------------------------------------------------------------------------------------- |
| `architecture/RUNTIME.md`           | Runtime package and manifest consumption, startup validation, asset manifest loading                |
| `architecture/ASSETS_AND_PACKAGES.md` | Source asset, package, manifest, and asset pipeline boundaries                                   |
| `architecture/PUBLISH.md`           | Publish report and package blocker boundary                                                        |
| `architecture/EDITOR.md`            | Editor authoring and asset UX boundary                                                             |
| `architecture/TESTING_AND_EVIDENCE.md` | Focused verification boundary                                                                  |
| `DependencyGraph.md`                | Runtime/tool/editor/asset-pipeline ownership boundaries                                             |

---

## 2. Purpose

The asset streaming system provides runtime-facing asset loading and residency management.

Its main responsibilities are:

* asynchronous asset loading,
* priority-based streaming requests,
* reference-counted residency,
* runtime asset lookup,
* memory budget awareness,
* file-backed or package-backed asset sources,
* typed asset access for meshes and textures,
* asset registry manifest consumption,
* graceful failure reporting for missing or invalid assets,
* runtime diagnostics for package/manifest/artifact load failures.

The system is designed for runtime use.

It is not the full editor import/cook pipeline.

It is also not the package staging system, publish readiness system, asset authoring UI, or stale artifact analyzer.

---

## 3. Ownership

The asset streaming system belongs to the engine/runtime resource layer.

It may own:

* runtime asset handles,
* asset residency cache,
* streaming request queues,
* streaming priority logic,
* runtime asset source loading,
* runtime manifest consumption,
* typed runtime wrappers for supported asset classes,
* runtime diagnostics for streaming failures,
* runtime package/asset manifest validation where needed for safe loading.

It must not own:

* editor import UX,
* drag-and-drop authoring workflows,
* asset inspector UI,
* project content browser UI,
* broad third-party asset normalization,
* production asset cooking policy,
* package staging,
* publishing workflows,
* collaboration session state,
* product project lifecycle,
* Forge build/cook orchestration.

Correct ownership:

```txt
Runtime asset streaming
  owns runtime loading, residency, runtime manifest consumption, and runtime load diagnostics

AssetPipeline
  owns source import, conversion, validation, cook output, asset metadata, and asset manifests

Forge
  owns build workflow orchestration, cook invocation, package staging, and report aggregation

  owns stale artifact and source → cooked artifact relationship analysis

SagaEditor
  owns content browser, asset inspector, import/reimport UX, and diagnostics presentation

Saga product shell
  owns project lifecycle and user-facing product workflow
```

Incorrect ownership:

```txt
Runtime streaming owns import UX, cook pipeline, product workflow, package staging, stale analysis, and editor asset panels
```

Runtime streaming must stay focused on runtime loading and residency so import, cooking, publishing, and product workflow decisions remain owned by their dedicated layers.

---

## 4. Runtime / Asset Pipeline Boundary

Runtime asset streaming consumes runtime-ready asset artifacts.

It does not own source asset import or cook behavior.

Correct flow:

```txt
Source asset
    ↓
AssetPipeline import/validate
    ↓
Asset metadata + import settings
    ↓
AssetPipeline cook
    ↓
Cooked runtime-ready artifact
    ↓
Asset manifest
    ↓
Forge package staging
    ↓
Runtime package manifest
    ↓
Runtime asset streaming / residency
```

Runtime streaming may load:

* asset manifests,
* cooked artifacts,
* runtime-ready metadata,
* streaming group information,
* residency hints,
* dependency information,
* budget metadata.

Runtime streaming must not own:

* source asset import,
* importer registry,
* source format parsing for authoring formats,
* texture compression pipeline,
* mesh optimization pipeline,
* audio conversion pipeline,
* editor content browser behavior,
* asset reimport workflow,
* package staging,
* publish readiness.

Shipping/runtime packages should load cooked artifacts, not arbitrary source assets.

Development/editor-preview profiles may allow special debug paths, but those paths must be explicit and diagnostics-visible.

Runtime should not silently fall back to source asset loading in a shipping package.

If a cooked artifact is missing or stale, the correct owners are:

```txt
```

not runtime streaming.

Runtime streaming can report the load failure.

It should not pretend to fix the asset pipeline.

---

## 5. Architecture Summary

The runtime asset streaming system is structured around these concepts:

```txt
AssetId
  stable runtime identifier for an asset

AssetManifest
  registry of known assets and their metadata

AssetSource
  file-backed or package-backed source of raw/cooked asset bytes

AssetStreamRequest
  request to load or increase residency of an asset

AssetStreamingScheduler
  prioritizes and dispatches load work

ResidencyCache
  keeps loaded assets alive while referenced

AssetHandle<T>
  typed runtime access to a loaded asset

AssetDiagnostic
  reports missing, invalid, failed, stale-reference, or budget-blocked assets
```

High-level flow:

```txt
Runtime requests AssetId
        ↓
AssetRegistry resolves metadata
        ↓
Streaming scheduler queues request
        ↓
AssetSource reads cooked/runtime-ready asset data
        ↓
Typed loader decodes runtime format
        ↓
ResidencyCache stores loaded result
        ↓
Runtime receives typed AssetHandle<T>
```

Package-oriented flow:

```txt
Runtime starts with package manifest
        ↓
Package manifest optionally references asset identity manifest
        ↓
Package manifest references asset manifest
        ↓
Asset identity manifest maps AssetKey to AssetId
        ↓
Asset manifest references cooked artifacts
        ↓
Runtime validates manifest/artifact metadata
        ↓
Streaming system loads requested AssetId
```

---

## 6. Runtime Streaming Responsibilities

The runtime streaming layer is responsible for loading assets that are already in a runtime-consumable form.

It should support:

* loading cooked mesh assets,
* loading cooked texture assets,
* loading runtime material data,
* resolving asset ids through manifests,
* applying priority to streaming requests,
* keeping assets resident while referenced,
* releasing unused assets when possible,
* respecting memory budget pressure,
* reporting missing or invalid assets,
* validating runtime-consumed manifest entries,
* producing structured runtime diagnostics.

The runtime streaming layer should avoid:

* arbitrary editor import behavior,
* expensive format normalization,
* destructive source asset mutation,
* editor UI callbacks,
* project database ownership,
* content authoring workflows,
* package staging workflows,
* publish readiness logic,
* stale artifact regeneration.

Runtime streaming should be lean.

Editor import and cooking can be heavier.

Runtime streaming consumes runtime-ready content.

It does not manufacture runtime-ready content from whatever file happened to be nearby.

---

## 7. Core Components

### 7.1 AssetId

`AssetId` is the stable runtime identifier for an asset.

Expected properties:

* stable across runtime sessions,
* serializable,
* comparable,
* loggable,
* valid/invalid state is explicit,
* does not depend on filesystem path identity alone.

Example responsibility:

```txt
AssetId identifies what asset the runtime wants.
It does not describe how the editor imported it.
```


Implementation ownership still remains separate:

```txt
AssetId contract may be shared.
Runtime asset registry implementation stays in Engine.
Asset metadata/import implementation stays in AssetPipeline/editor tooling.
```

Current AssetId allocation policy:

```txt
AssetId type
  std::uint64_t

Invalid/null AssetId
  0

Valid AssetId range
  1 through UINT64_MAX
```

Policy rules:

* runtime never generates AssetIds,
* runtime consumes AssetIdentityManifest data produced by build/package-side ownership,
* the first allocation policy applies to one identity manifest package identity set,
* no multi-package AssetId merge policy is defined yet,
* if a previous valid identity manifest exists, its AssetKey to AssetId mappings are reused,
* new AssetKeys receive new numeric AssetIds,
* new AssetKeys are sorted lexicographically before allocation,
* new IDs are assigned from `max(existing AssetId) + 1`,
* deleted asset IDs are not reused in the first policy,
* AssetKey rename is treated as delete plus new asset in the first policy,
* rename-preserving migration is future work,
* duplicate AssetKey, duplicate AssetId, invalid ID, overflow, or malformed previous identity manifest is a deterministic failure,
* no deterministic hash fallback is defined,
* the allocation source of truth is the previous valid identity manifest plus the current package asset manifest key set.

Ownership:

```txt
AssetPipeline / package generation side
  owns future AssetId assignment and AssetIdentityManifest emission

Forge
  may orchestrate validation, staging, and report aggregation

Runtime
  consumes AssetIdentityManifest and validates package coverage
```

This policy exists to prevent asset identity allocation behavior from being invented inside generator implementation work. It does not add a generator, package staging step, editor asset browser behavior, rename migration, or runtime fallback path.

Generator readiness / boundary status:

The runtime side can consume and validate AssetIdentityManifest data, but the
tool-side JSON writer, package staging, and Forge orchestration path are still
not implemented.

Current implemented boundary:

* `Tools/AssetPipeline` now provides a narrow `SagaAssetPipelineLib` target,
* `SagaAssetPipeline/Identity/AssetIdentityGenerator.hpp` owns the pure
  in-memory identity allocation policy,
* the generator reuses previous valid mappings,
* new AssetKeys are allocated lexicographically from `max(existing AssetId) + 1`,
* deleted AssetIds are not reused,
* AssetKey rename is still treated as delete plus new identity,
* duplicate current AssetKey, duplicate previous AssetKey, duplicate AssetId,
  invalid AssetId 0, and numeric overflow are deterministic generation failures,
* no JSON writer, file I/O, package mutation, Forge step, runtime dependency,
  editor integration, or deterministic hash fallback was added.

Missing pieces before generated AssetIdentityManifest files can enter package
staging:

* Forge AssetPipelineAdapter is not present,
* Forge AssetValidateStep is not present,
* Forge AssetCookStep is not present,
* Forge AssetPackageStep is not present,
* Forge asset/package manifest writer infrastructure is not present,
* AssetIdentityManifest JSON writer is not present,
* AssetIdentityManifest generator executable/service entrypoint is not present.

Recommended next implementation boundary:

```txt
AssetPipeline / package generation side
    AssetIdentityManifest JSON writer or generator executable/service
        ↓
Forge
    orchestrates validation/staging/report aggregation only
        ↓
Runtime
    consumes the generated manifest and validates startup coverage
```

Forge should not become the importer, cooker, or identity generator implementation
owner. Runtime should not generate AssetIds. The next generator slice should
stay inside the AssetPipeline boundary and add only the missing writer or
tool entrypoint needed before Forge orchestration.

---

### 7.2 AssetManifest

The asset manifest maps known asset ids to runtime metadata.

It may contain:

* asset id,
* asset kind,
* runtime path,
* source/cooked hash,
* dependency list,
* memory estimate,
* streaming group,
* platform/profile compatibility,
* version/schema information.

The manifest should be produced by the asset pipeline or build/cook process.

Runtime should consume it.

Runtime should not casually invent manifest truth at load time unless explicitly in development mode.

Current package identity contract:

```txt
PackageManifest.assetIdentityManifest
  optional single package-relative path to an AssetIdentityManifest

AssetIdentityManifest
  schemaVersion = 1
  mappings[] = { assetKey, assetId }
```

Example package manifest field:

```json
{
  "assetIdentityManifest": "Manifests/asset_identity.json"
}
```

Example asset identity manifest:

```json
{
  "schemaVersion": 1,
  "mappings": [
    { "assetKey": "texture.hero.diffuse", "assetId": 1001 }
  ]
}
```

Contract rules:

* `assetIdentityManifest` is optional for backward compatibility.
* if present, it must be a non-escaping package-relative path,
* if present, the referenced manifest must load and validate deterministically,
* `AssetId` value `0` is reserved for invalid/null state,
* valid numeric `AssetId` values must be greater than zero,
* the first implemented slice supports one identity manifest only,
* no multi-manifest merge semantics are defined yet,
* no deterministic hash fallback is defined,
* runtime consumes this contract but does not generate it.

Identity-backed bootstrap flow:

```txt
PackageManifest
    ↓
assetIdentityManifest
    ↓
AssetIdentityManifestLoader
    ↓
StaticAssetIdResolver
    ↓
RuntimeAssetRegistryBootstrapper explicit-resolver path
```

Identity-backed preflight validation uses the same identity loader and package
asset planning path without mutating `AssetRegistry`.

The first validation slice requires coverage for every packaged asset manifest
entry when `ValidatePackageAssetsFromPackageIdentityManifest` is called.
Extra identity mappings are allowed in this slice; exact package membership
policy remains future package/build validation work.

Runtime startup validation uses the identity-backed preflight path when a
package manifest references `assetIdentityManifest`. Legacy package manifests
without that field keep the existing referenced asset manifest validation path.

The identity manifest exists so packaged runtime asset manifests can keep stable package-facing `AssetKey` values while runtime hot paths use numeric `AssetId` values. Asset identity assignment and manifest generation remain future AssetPipeline/Forge responsibilities.

Runtime startup or package load should validate:

* asset manifest version,
* asset identity manifest version when referenced,
* asset id format,
* AssetKey to AssetId mapping coverage where identity-backed bootstrap is used,
* asset kind,
* cooked artifact path,
* cooked artifact hash where configured,
* dependency list,
* platform/profile compatibility,
* streaming group metadata,
* memory/budget metadata where available.

Invalid manifest state should produce structured runtime diagnostics.

Examples:

```txt
Runtime.Asset.ManifestMissing
Runtime.Asset.ManifestUnsupportedVersion
Runtime.Asset.ArtifactMissing
Runtime.Asset.ArtifactHashMismatch
Runtime.Asset.UnsupportedKind
Runtime.Asset.DependencyMissing
Runtime.Asset.BudgetExceeded
```

---

### 7.3 AssetSource

`AssetSource` abstracts where asset bytes come from.

Possible implementations:

* loose file source,
* package/archive source,
* memory source for tests,
* development hot-reload source,
* future remote/cache source.

Rules:

* asset source returns bytes or failure,
* asset source does not own typed decoding policy,
* asset source reports useful diagnostics,
* asset source should not expose platform-specific file handles through public runtime API.

`AssetSource` may read cooked/runtime-ready asset bytes.

It should not become an authoring importer by accident.

### 7.3.1 Virtual File System Boundary

`IVirtualFileSystem` is the runtime content path boundary.

Its purpose is to let runtime code request content by normalized virtual path instead of depending directly on physical native filesystem paths.

Current MVP rules:

* virtual paths use `/`,
* valid virtual file paths are absolute, for example `/content/meshes/tree.bin`,
* mount points are normalized absolute prefixes,
* the longest matching mount point wins,
* `.` / `..`, empty path segments, relative paths, and backslashes are rejected deterministically,
* native directory and memory backends are available as basic implementations.

Boundary:

```txt
IVirtualFileSystem
  owns virtual path normalization, mount lookup, and backend dispatch

IAssetSource
  owns asset-byte loading for streaming requests

FileAssetSource
  remains the current native file-backed asset source
```

The VFS MVP does not change package manifests, asset manifests, streaming behavior, package staging, Forge orchestration, editor UX, or asset identity generation.

`FileAssetSource` is not converted in this slice. It may later become either a VFS backend or a VFS consumer after package/content mounting semantics are defined.

`VirtualFileAssetSource` is the first VFS-backed `IAssetSource` adapter.

It consumes:

* `IVirtualFileSystem`,
* an `AssetId` + `AssetKind` to virtual path resolver.

It does not change `AssetRegistry.sourcePath` semantics, asset manifest format, package manifest format, startup validation, package mounting, or `FileAssetSource`.

Current `AssetRegistry.sourcePath` behavior:

```txt
AssetManifestRegistryAdapter resolves asset manifest paths against the package/runtime manifest context and stores the resolved cooked/runtime path in AssetRegistryEntry::sourcePath.
```

This field is not currently a VFS virtual path contract. It remains the path consumed by the active asset source implementation for the current runtime registration path.

Current status mapping:

```txt
VFS Ok
  -> StreamingStatus::Ok

VFS AssetNotFound / MountNotFound
  -> StreamingStatus::AssetNotFound

VFS InvalidPath / BackendError
  -> StreamingStatus::SourceError
```

The resolver owns the policy of producing virtual paths. A later slice must decide whether runtime asset registry paths are virtual paths, native paths, or a separate artifact-location contract.

---

### 7.4 AssetStreamRequest

A stream request describes runtime demand for an asset.

Expected fields:

* asset id,
* asset kind,
* priority,
* requester/context id where useful,
* desired residency level,
* cancellation token or request handle,
* diagnostic context.

Priority may be influenced by:

* camera distance,
* gameplay criticality,
* preload step,
* player proximity,
* UI/runtime preview need,
* explicit blocking request,
* memory budget pressure.

---

### 7.5 AssetStreamingScheduler

The streaming scheduler prioritizes asset loading work.

It should support:

* request queueing,
* priority ordering,
* cancellation,
* deduplication,
* bounded concurrent work,
* budget-aware scheduling,
* diagnostics for queue pressure.

It should not block the main runtime thread unnecessarily.

Blocking loads may exist for critical boot/runtime cases, but they must be explicit.

Hidden blocking IO is just a frame hitch wearing a fake badge.

---

### 7.6 ResidencyCache

The residency cache owns loaded runtime asset instances.

It should support:

* reference-counted residency,
* weak lookup,
* explicit release,
* eviction candidates,
* memory usage tracking,
* asset kind grouping,
* diagnostics for leaks or budget pressure.

The cache should not own editor import state.

It owns loaded runtime assets, not the history of humanity's fight with file formats.

---

### 7.7 AssetHandle

`AssetHandle<T>` provides typed runtime access to a loaded asset.

Expected behavior:

* references a valid loaded asset or explicit failure/null state,
* keeps asset resident while held,
* can expose asset metadata,
* avoids raw ownership ambiguity,
* supports diagnostics when access fails.

Typed handle examples:

```txt
AssetHandle<MeshAsset>
AssetHandle<TextureAsset>
AssetHandle<MaterialAsset>
```

Important rule:

```txt
A live AssetHandle must keep its asset valid.
```

---

### 7.8 AssetDiagnostic

Asset streaming diagnostics should report:

* missing asset,
* unsupported asset kind,
* invalid manifest entry,
* failed file read,
* failed decode,
* memory budget rejection,
* dependency missing,
* dependency cycle,
* version/schema mismatch,
* cancelled request.

Diagnostics should include:

* asset id,
* asset path if available,
* asset kind,
* package id where available,
* manifest path where available,
* artifact path where available,
* error code,
* readable message,
* recoverability flag,
* source system if known.

Runtime asset streaming diagnostics should use the shared diagnostics/resource model where available.

Runtime streaming should emit evidence, not try to fix authoring/build errors.

---

## 8. Implemented Design Decisions

### 8.1 Runtime Streaming Is Separate From Editor Import

Decision:

```txt
Runtime streaming consumes runtime-ready assets.
Editor import/cook produces runtime-ready assets.
```

Updated ownership wording:

```txt
Runtime streaming consumes runtime-ready cooked artifacts.
AssetPipeline owns source import/cook behavior.
Editor owns asset authoring UX.
Forge owns build/package orchestration.
```

Reason:

* runtime must stay predictable,
* editor import must handle messy third-party content,
* asset conversion can be expensive,
* import diagnostics belong to authoring tools,
* runtime should not carry every importer edge case,
* shipping package loading should be strict and manifest-driven.

Preserve this decision.

Do not turn runtime streaming into Blender, glTF validator, texture compressor, material converter, file watcher, package stager, publish checker, and project UI at the same time.

The universe has suffered enough.

---

### 8.2 Priority-Based Streaming

Decision:

```txt
Streaming requests carry priority.
Scheduler chooses work based on priority and budget pressure.
```

Reason:

* gameplay-critical assets should load before distant or optional assets,
* runtime preview needs responsive loading,
* memory and IO are finite,
* deterministic diagnostics are easier when request priority is explicit.

Expected priority classes:

```txt
Critical
High
Normal
Low
Background
```

---

### 8.3 Reference-Counted Residency

Decision:

```txt
Loaded assets remain resident while referenced.
Unused assets become eviction candidates.
```

Reason:

* prevents premature unload,
* supports predictable runtime handles,
* allows memory pressure handling,
* keeps ownership understandable.

Important rule:

```txt
A live AssetHandle must keep its asset valid.
```

---

### 8.4 Manifest-Driven Runtime Lookup

Decision:

```txt
Runtime resolves assets through manifests instead of ad hoc source paths.
```

Reason:

* stable ids survive path changes,
* cooked assets can differ from source assets,
* platform-specific variants can be selected,
* build/publish validation can reason about missing assets,
* runtime/server startup validation can reject invalid package state clearly.

Runtime should not silently guess asset truth from source folders.

The manifest is the runtime contract.

---

### 8.5 Typed Runtime Wrappers

Decision:

```txt
Runtime exposes typed asset handles instead of unstructured byte blobs.
```

Reason:

* call sites are safer,
* diagnostics are clearer,
* asset kind mismatch can be detected,
* runtime systems avoid repeating decode logic.

---

## 9. Current Runtime Asset Types

The current system is expected to support or evolve around these runtime-facing asset categories:

```txt
MeshAsset
TextureAsset
MaterialAsset
ShaderAsset
SceneAsset
PrefabAsset
AudioAsset
ScriptArtifact
GraphArtifact
```

Current known focus:

* mesh streaming,
* texture streaming,
* runtime resource lookup,
* manifest-backed asset identity,
* residency management.

Future asset types should be added through explicit typed loaders and manifest metadata, not random one-off code paths.

Random one-off loaders are how content pipelines become folklore.

---

## 10. Mesh Streaming Notes

Mesh streaming should handle runtime-ready mesh data.

Expected runtime mesh data:

* vertex buffers,
* index buffers,
* vertex layout,
* bounds,
* submesh ranges,
* material slots,
* optional LOD metadata,
* optional collision reference.

Runtime mesh streaming should not own:

* DCC import policy,
* arbitrary exporter normalization,
* artist-facing validation UI,
* mesh optimization pipeline,
* editor preview thumbnails.

Those belong to import/cook tooling and editor UX.

---

## 11. Texture Streaming Notes

Texture streaming should handle runtime-ready texture data.

Expected runtime texture data:

* dimensions,
* format,
* mip levels,
* usage flags,
* platform compatibility,
* memory estimate,
* optional streaming group.

Runtime texture streaming should not own:

* source image authoring,
* texture compression decisions,
* editor import presets,
* material authoring UI,
* source image repair.

Texture format support should be explicit.

If the runtime accepts a texture, it should know exactly what it is accepting.

Broad trial-and-error loading belongs in editor import tooling, not the runtime streaming path.

---

## 12. Import Format Note

The current hand-rolled parsing approach for formats such as:

```txt
glTF
DDS
KTX2
JSON
```

may be acceptable for a controlled runtime subset.

It is not enough for broad production authoring compatibility.

For production import/cook workflows, Saga should eventually introduce a dedicated asset import pipeline that can handle:

* real-world exporter variation,
* format validation,
* importer diagnostics,
* conversion,
* compression,
* dependency extraction,
* metadata generation,
* platform-specific cooking,
* source-to-runtime artifact mapping.

Runtime streaming should stay lean.

Editor/tool import can be heavier.

Clear distinction:

```txt
Runtime loader
  handles controlled runtime-ready formats

AssetPipeline importer/cooker
  handles messy source formats and produces runtime-ready artifacts
```

---

## 13. Asset Manifest Consumption

The runtime asset system should resolve assets through explicit manifests rather than guessing project source folders.

Runtime startup or package load should validate:

* asset manifest version,
* asset id format,
* asset kind,
* cooked artifact path,
* cooked artifact hash where configured,
* dependency list,
* platform/profile compatibility,
* streaming group metadata,
* memory/budget metadata where available.

Invalid manifest state should produce structured runtime diagnostics.

Examples:

```txt
Runtime.Asset.ManifestMissing
Runtime.Asset.ManifestUnsupportedVersion
Runtime.Asset.ArtifactMissing
Runtime.Asset.ArtifactHashMismatch
Runtime.Asset.UnsupportedKind
Runtime.Asset.DependencyMissing
Runtime.Asset.BudgetExceeded
```

The runtime may support development convenience modes, but those modes must be explicit.

Shipping/runtime package behavior should be manifest-driven and strict.

---

## 14. Cooked Artifact Rule

Shipping/runtime packages should load cooked artifacts, not arbitrary source assets.

Development/editor-preview profiles may allow special debug paths, but those paths must be explicit and diagnostics-visible.

Runtime should not silently fall back to source asset loading in a shipping package.

If a cooked artifact is missing or stale, the correct owner is the build/asset pipeline:

```txt
```

not runtime streaming.

Runtime streaming can report:

```txt
Runtime.Asset.ArtifactMissing
Runtime.Asset.ArtifactHashMismatch
Runtime.Asset.UnsupportedVersion
Runtime.Asset.DependencyMissing
```

But regeneration belongs elsewhere.

---

## 15. Memory Budget Strategy

Runtime streaming should respect platform/profile memory budgets.

Expected budget categories:

* total asset memory,
* texture memory,
* mesh memory,
* audio memory,
* streaming staging memory,
* temporary decode memory.

Budget behavior should include:

* request rejection or delay under pressure,
* eviction of unused assets,
* diagnostic output,
* per-kind memory accounting,
* high-water mark tracking.

Budget diagnostics should answer:

```txt
What asset consumed memory?
What budget was exceeded?
What assets are resident?
What assets are eviction candidates?
What request was delayed or rejected?
```

If the budget system cannot answer those questions, it is not a budget system.

It is vibes with counters.

---

## 16. Dependency Handling

Asset streaming must eventually support dependency-aware loading.

Example dependencies:

```txt
SceneAsset → MeshAsset
SceneAsset → MaterialAsset
MaterialAsset → TextureAsset
PrefabAsset → MeshAsset
PrefabAsset → ScriptArtifact
GraphArtifact → SchemaArtifact
```

Expected behavior:

* dependencies are listed in manifest metadata,
* missing dependencies fail clearly,
* dependency cycles are detected,
* dependency loading can be prioritized,
* dependency failure propagates to the parent request.

Runtime should not silently replace failed dependencies unless explicitly configured.

Silent fallback is how bugs get promoted to art direction.

---

## 17. Failure Behavior

Asset streaming failures must be explicit.

Required failure cases:

* asset id not found,
* manifest missing,
* asset file missing,
* asset kind mismatch,
* asset version mismatch,
* unsupported platform variant,
* failed decode,
* missing dependency,
* dependency cycle,
* memory budget exceeded,
* request cancelled,
* source unavailable.

Each failure should produce:

* error code,
* asset id,
* asset kind,
* source path if available,
* manifest path if available,
* artifact path if available,
* package id if available,
* readable message,
* recoverability flag,
* diagnostic category.

Runtime must not crash on normal asset failure.

Normal asset failure is expected.

Crashing because a texture is missing is not “strict”.

It is melodrama.

---

## 18. Runtime Preview Integration

Editor runtime preview may consume the asset streaming system.

Rules:

* preview requests runtime assets through approved runtime APIs,
* preview does not own residency cache internals,
* preview failures are reported back to editor diagnostics,
* stopping preview releases preview-owned asset handles,
* editor import/cook state remains separate,
* preview uses editor-preview/runtime-ready artifacts where possible.

Correct flow:

```txt
Editor preview requests runtime asset
        ↓
Runtime asset streaming loads runtime-ready asset
        ↓
Preview renders or fails with diagnostic
```

Incorrect flow:

```txt
Preview panel manually parses source asset and mutates residency cache
```

No.

Bad panel.

---

## 19. Collaboration Boundary

Asset streaming may interact with collaboration indirectly.

Examples:

* collaborator imports an asset,
* asset cook produces new runtime artifact,
* manifest changes,
* runtime preview reloads asset,
* publish gate checks asset status.

But asset streaming does not own collaboration.

Rules:

* collaboration state belongs to `SagaCollaboration`,
* neutral collaboration contracts may live in `SagaShared`,
* editor collaboration UI belongs to `SagaEditor`,
* runtime streaming may consume stable artifact/resource ids,
* runtime streaming must not include editor-private collaboration headers.

Forbidden:

```txt
Asset streaming owns locks
Asset streaming owns participant state
Asset streaming owns session lifecycle
Asset streaming imports Editor/include/SagaEditor/Collaboration
```

---




Allowed examples:

* compiled graph artifact reference,
* schema id,
* generated runtime data artifact,
* artifact hash,
* diagnostic payload reference.

Forbidden dependency direction:

```txt
```

Runtime asset streaming should consume outputs.


A compiler is allowed to produce artifacts.

It is not invited to move into the engine and rearrange the furniture.

---

## 21. Threading and Async Rules

Asset streaming is async by default.

Expected rules:

* IO work happens off the main runtime thread where possible,
* decode work is bounded,
* GPU upload or renderer-facing handoff occurs on the correct thread,
* asset handle publication is synchronized,
* cancellation is safe,
* failed async jobs publish diagnostics,
* shutdown waits for or cancels streaming jobs safely.

Thread ownership must be explicit.

Forbidden:

* publishing partially decoded assets,
* mutating residency cache from arbitrary threads without synchronization,
* destroying assets while live handles exist,
* blocking the main thread without explicit critical reason.

---

## 22. Hot Reload and Development Mode

Development mode may support asset reload.

Hot reload should be treated carefully.

Expected behavior:

* file/source change is detected,
* asset is reloaded through a controlled path,
* old asset remains valid until replacement is ready,
* failure keeps old valid asset when possible,
* dependent resources are invalidated or refreshed,
* diagnostics report reload result.

Hot reload should not be required for production runtime.

Development convenience must not corrupt production assumptions.

Yes, shocking: the thing that helps during development should not become a runtime landmine.

Hot reload must also respect the runtime/asset-pipeline boundary:

```txt
Development source change
    ↓
AssetPipeline/import/cook or controlled dev path
    ↓
Updated runtime-ready artifact/manifest
    ↓
Runtime reloads through explicit runtime API
```

Runtime should not silently convert arbitrary source formats during hot reload unless the profile explicitly allows a development-only path.

---

## 23. Diagnostics Alignment

Runtime asset streaming diagnostics should use the shared diagnostics/resource model where available.

Runtime asset diagnostics should be able to reference:

* `AssetId`,
* asset kind,
* package id,
* asset manifest entry,
* cooked artifact path,
* dependency asset id,
* streaming group,
* memory/budget estimate,
* runtime load state.

Runtime diagnostics should be consumable by:

* Saga product preview status,
* SagaEditor runtime preview Problems panel,
* diagnostics reports,
* CI/runtime startup validation where applicable.

Runtime streaming should emit evidence, not try to fix authoring/build errors.

Suggested diagnostic families:

```txt
Runtime.Asset.*
Runtime.Package.*
Runtime.Artifact.*
Asset.Manifest.*
Asset.Runtime.*
Asset.Streaming.*
Asset.Budget.*
```

Examples:

```txt
Runtime.Asset.ManifestMissing
Runtime.Asset.ArtifactMissing
Runtime.Asset.ArtifactHashMismatch
Runtime.Asset.DependencyMissing
Runtime.Asset.BudgetExceeded
Runtime.Asset.UnsupportedKind
Runtime.Package.UnsupportedVersion
```

---

## 24. Testing Requirements

Asset streaming should have tests for:

* valid asset lookup,
* missing asset lookup,
* manifest load failure,
* asset kind mismatch,
* asset load success,
* asset load failure,
* dependency load success,
* dependency missing,
* dependency cycle,
* cache hit,
* cache release,
* eviction candidate selection,
* memory budget pressure,
* cancelled request,
* concurrent duplicate requests,
* shutdown with pending requests,
* package manifest references asset manifest,
* invalid asset manifest version,
* missing cooked artifact,
* artifact hash mismatch where configured.

Recommended test categories:

```txt
Unit tests
  AssetId
  AssetManifest
  AssetRegistry
  ResidencyCache
  StreamRequestQueue

Integration tests
  package manifest → asset manifest → source → typed load → residency handle
  manifest → source → typed load → residency handle

Failure tests
  malformed asset
  missing dependency
  invalid manifest
  missing cooked artifact
  artifact hash mismatch
  budget exceeded
```

---

## 25. Diagnostics Requirements

Asset streaming diagnostics should be visible to:

* runtime diagnostics,
* editor Problems panel during preview/import integration,
* asset pipeline tooling,
* publish validation,
* build/package diagnostics reports where applicable.

Minimum diagnostic fields:

```txt
AssetId
AssetKind
DiagnosticSeverity
DiagnosticCode
Message
SourcePath
ManifestPath
ArtifactPath
PackageId
Recoverability
RequestContext
```

Diagnostic categories:

```txt
AssetManifest
AssetLookup
AssetSource
AssetDecode
AssetDependency
AssetBudget
AssetResidency
AssetStreamingScheduler
RuntimePackage
RuntimeArtifact
```

Diagnostics should be structured enough that the user does not need to guess whether the problem is:

```txt
bad source asset
stale cooked artifact
missing package entry
runtime decode failure
memory budget rejection
```

Those are different failures.

Treating them all as “failed to load” is lazy diagnostics.

---

## 26. Recommended Runtime File Layout

Recommended target layout:

```txt
Engine/include/SagaEngine/Assets/
  AssetId.hpp
  AssetKind.hpp
  AssetManifest.hpp
  AssetMetadata.hpp
  AssetRegistry.hpp
  AssetDiagnostic.hpp

Engine/include/SagaEngine/Resources/
  AssetHandle.hpp
  ResidencyCache.hpp
  ResourceBudget.hpp
  StreamingPriority.hpp
  StreamRequest.hpp
  StreamRequestHandle.hpp
  AssetStreamingScheduler.hpp

Engine/include/SagaEngine/Assets/Types/
  MeshAsset.hpp
  TextureAsset.hpp
  MaterialAsset.hpp

Engine/src/SagaEngine/Assets/
  AssetRegistry.cpp
  AssetManifest.cpp
  AssetDiagnostic.cpp

Engine/src/SagaEngine/Resources/
  ResidencyCache.cpp
  AssetStreamingScheduler.cpp
  ResourceBudget.cpp

Engine/src/SagaEngine/Assets/Types/
  MeshAssetLoader.cpp
  TextureAssetLoader.cpp
  MaterialAssetLoader.cpp
```

This layout is illustrative, not mandatory.

The important part is ownership separation:

```txt
Runtime loading and residency stay in Engine.
Editor import/cook stays out of runtime streaming.
AssetPipeline owns source import/cook behavior.
Forge owns package staging and build orchestration.
```

---

## 27. Known Constraints

Current or expected constraints:

* runtime loader likely supports a controlled subset of source/runtime formats,
* broad real-world asset import compatibility is not guaranteed,
* hand-rolled parsing is risky for production authoring,
* memory budget behavior must be tested under pressure,
* dependency loading must be explicit,
* async lifetime rules must be hardened,
* runtime package/asset manifest validation must become strict enough for shipping profiles,
* diagnostics must be good enough to debug asset failures without stepping through every loader like a medieval punishment.

---

## 28. Follow-Up Risks

### 28.1 Runtime Import Creep

Risk:

```txt
Runtime streaming slowly becomes editor import/cook pipeline.
```

Impact:

* runtime becomes heavy,
* failures become harder to diagnose,
* platform-specific cook logic leaks into runtime,
* broad source format compatibility becomes runtime burden.

Mitigation:

```txt
Keep import/cook in AssetPipeline/editor/tools.
Keep runtime streaming focused on runtime-ready assets.
Runtime consumes cooked artifacts and manifests.
```

---

### 28.2 Shared Ownership Confusion

Risk:

```txt
Asset ids, manifests, diagnostics, and artifact refs get split randomly across Engine, Editor, Shared, and Tools.
```

Impact:

* duplicate contracts,
* incompatible ids,
* broken publish validation,
* fragile runtime/editor integration.

Mitigation:

```txt
Neutral contracts may live in SagaShared.
Runtime implementation stays in Engine.
Editor/tool implementation stays with owning systems.
AssetPipeline owns source import/cook implementation.
Forge owns package staging.
```

---

### 28.3 Weak Diagnostics

Risk:

```txt
Asset failures report only "failed to load".
```

Impact:

* slow debugging,
* bad editor UX,
* unsafe publish decisions,
* user confusion.

Mitigation:

```txt
Every failure includes asset id, kind, source, manifest/artifact refs, reason, and recoverability.
```

---

### 28.4 Budget System Drift

Risk:

```txt
Residency cache ignores real memory pressure.
```

Impact:

* runtime memory spikes,
* platform instability,
* non-deterministic asset eviction,
* hard-to-reproduce crashes.

Mitigation:

```txt
Add memory accounting, budget diagnostics, and eviction tests.
```

---

### 28.5 Stale Cooked Artifact Confusion

Risk:

```txt
Runtime load failure is mistaken for asset pipeline freshness validation.
```

Impact:

* runtime gets blamed for stale build outputs,
* editor/product publish state becomes misleading,
* package validation becomes incomplete.

Mitigation:

```txt
Forge blocks publish/package where required.
AssetPipeline regenerates cooked artifacts.
Runtime reports load/manifest/artifact failures.
```

---

## 29. Relationship to Current Ownership Docs

This document records the implementation note for runtime asset streaming.

Current ownership references:

| Area                                                 | Owner document                      |
| ---------------------------------------------------- | ----------------------------------- |
| Runtime resource behavior                            | `architecture/RUNTIME.md`           |
| Source asset import/cook                             | `architecture/ASSETS_AND_PACKAGES.md` |
| Editor import UX / asset inspector / content browser | `architecture/EDITOR.md`            |
| Shared ids/artifact contracts                        | `architecture/SOURCE_OF_TRUTH_MAP.md` |
| Collaboration resource ownership                     | `architecture/SAGA_COLLABORATION_CURRENT_BOUNDARY.md` |
| Tool ecosystem                                       | focused tool contracts and local reports |
| Build/cook/package/publish boundary                  | `architecture/PUBLISH.md`           |
| Forge build/cook/package orchestration               | `architecture/PUBLISH.md`           |
| Dependency boundaries                                | `DependencyGraph.md`                |
| Diagnostics/reporting model                          | `architecture/TESTING_AND_EVIDENCE.md` |

This file should not track future task progress unless it is documenting completed implementation decisions.

Implementation history and runtime design rationale belong here.

---

## 30. Non-Goals

This implementation note does not define:

* source asset import pipeline,
* asset reimport workflow,
* importer registry,
* cook profiles,
* texture/mesh/audio conversion implementation,
* package staging rules,
* publish readiness rules,
* editor content browser UX,
* asset metadata authoring UX,
* Forge build/cook orchestration,
* asset collaboration conflict resolution,
* asset schema language design.

Those belong to current architecture documents:

* `architecture/ASSETS_AND_PACKAGES.md`,
* `architecture/PUBLISH.md`,
* internal/proposed toolchain appendices,
* `architecture/EDITOR.md`,
* `architecture/SAGA_COLLABORATION_CURRENT_BOUNDARY.md`,
  definitions are involved.

Runtime streaming loads runtime-ready assets.

It does not own the entire content pipeline.

---

## 31. Decision Summary

Preserve these decisions:

```txt
Runtime asset streaming consumes runtime-ready cooked artifacts.
AssetPipeline owns source import/cook behavior.
Forge owns build/package orchestration.
SagaEditor owns asset authoring UX.
Runtime/server validate manifests and artifacts at startup/load time.
Runtime should not silently load arbitrary source assets in shipping packages.
Invalid asset package state should produce structured diagnostics.
Runtime loading and residency stay in Engine.
Editor import/cook stays out of runtime streaming.
Collaboration state is not owned by runtime streaming.
```

The runtime asset system should be strict about what it consumes.

It should not become an importer, cooker, editor tool, package stager, publish validator, and runtime cache all at once.

---

## 32. Final Rule

Runtime asset streaming should stay:

```txt
small enough to reason about,
strict enough to fail safely,
async enough to avoid frame stalls,
diagnostic enough to debug,
manifest-driven enough to support packages,
and separate enough from editor import/cook tooling to avoid architectural soup.
```

Asset streaming loads runtime assets.

It does not own the entire content pipeline.

That boundary keeps runtime loading separate from editor import, cooking, packaging, stale analysis, and publishing responsibilities.
