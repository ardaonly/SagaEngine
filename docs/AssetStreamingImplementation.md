# Asset Streaming System — Implementation Note

> Last updated: 2026-05-15
> Status: Implementation note
> Location: `docs/AssetStreamingImplementation.md`
> Related roadmap: `ENGINE_ROADMAP.md`
> Related systems: Runtime resources, asset registry, residency cache, streaming scheduler, runtime preview, editor import/cook pipeline, asset pipeline, package manifests, build/publish pipeline, Prism stale artifact analysis.
> Scope: Implemented runtime asset streaming architecture, design decisions, known constraints, follow-up risks, and runtime/asset-pipeline boundary alignment.

---

## 0. Document Status

This document is not an active roadmap.

It records the implemented asset streaming architecture for SagaEngine's runtime/resource layer.

Active future work should be tracked in:

```txt
ENGINE_ROADMAP.md
ASSET_PIPELINE_ROADMAP.md
BUILD_PUBLISH_PIPELINE_ROADMAP.md
FORGE_ROADMAP.md
PRISM_ROADMAP.md
```

This document should remain useful as implementation history and design rationale.

It must not be treated as the source of truth for future product direction.

It must also not become the full asset pipeline roadmap.

Runtime streaming is one part of the content pipeline.

It is not the entire content pipeline.

---

## 1. Related Documents

This document describes the runtime asset streaming implementation.

It does not own source asset import, cook, package staging, publish readiness, editor asset UX, or stale artifact analysis.

Related roadmap documents:

| Document                            | Purpose                                                                                             |
| ----------------------------------- | --------------------------------------------------------------------------------------------------- |
| `ENGINE_ROADMAP.md`                 | Runtime/server package and manifest consumption, runtime startup validation, asset manifest loading |
| `ASSET_PIPELINE_ROADMAP.md`         | Source asset import, metadata, cook, cooked artifacts, asset manifests, asset diagnostics           |
| `BUILD_PUBLISH_PIPELINE_ROADMAP.md` | Validation/cook/package/publish workflow                                                            |
| `FORGE_ROADMAP.md`                  | Build workflow orchestration and asset cook invocation                                              |
| `PRISM_ROADMAP.md`                  | Stale cooked artifact detection and source asset → cooked artifact relationship analysis            |
| `EDITOR_ROADMAP.md`                 | Content browser, asset inspector, import/reimport UX, asset diagnostics display                     |
| `SHARED_ROADMAP.md`                 | Neutral asset ids, artifact refs, package manifests, diagnostics payloads                           |
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
* SDE schema/data compilation,
* broad third-party asset normalization,
* production asset cooking policy,
* package staging,
* publishing workflows,
* collaboration session state,
* product project lifecycle,
* Prism stale artifact analysis,
* Forge build/cook orchestration.

Correct ownership:

```txt
Runtime asset streaming
  owns runtime loading, residency, runtime manifest consumption, and runtime load diagnostics

AssetPipeline
  owns source import, conversion, validation, cook output, asset metadata, and asset manifests

Forge
  owns build workflow orchestration, cook invocation, package staging, and report aggregation

Prism
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
AssetPipeline / Forge / Prism
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
Package manifest references asset manifest
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

`AssetId` may eventually become a shared neutral contract if editor, runtime, asset pipeline, Forge, Prism, and package manifests all need the same stable identifier.

Implementation ownership still remains separate:

```txt
AssetId contract may be shared.
Runtime asset registry implementation stays in Engine.
Asset metadata/import implementation stays in AssetPipeline/editor tooling.
```

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
* preload phase,
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
Prism owns stale cooked artifact analysis.
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
AssetPipeline / Forge / Prism
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

## 20. SDE Boundary

SDE remains a standalone deterministic data compiler.

Asset streaming may consume SDE-produced runtime artifacts.

Allowed examples:

* compiled graph artifact reference,
* schema id,
* generated runtime data artifact,
* artifact hash,
* diagnostic payload reference.

Forbidden dependency direction:

```txt
SDE → SagaEngine
SDE → SagaEditor
SDE → SagaServer
SDE → SagaShared
SDE → SagaCollaboration
SDE → Forge
SDE → Prism
SDE → SagaTools
```

Runtime asset streaming should consume outputs.

It should not pull SDE compiler internals into the runtime.

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
Prism owns stale artifact relationship analysis.
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
Prism owns stale relationship analysis.
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
Prism detects stale cooked artifacts.
Forge blocks publish/package where required.
AssetPipeline regenerates cooked artifacts.
Runtime reports load/manifest/artifact failures.
```

---

## 29. Relationship to Roadmaps

This document records the implementation note for runtime asset streaming.

Active roadmap ownership:

| Area                                                 | Owner document                      |
| ---------------------------------------------------- | ----------------------------------- |
| Runtime resource roadmap                             | `ENGINE_ROADMAP.md`                 |
| Source asset import/cook                             | `ASSET_PIPELINE_ROADMAP.md`         |
| Editor import UX / asset inspector / content browser | `EDITOR_ROADMAP.md`                 |
| Shared ids/artifact contracts                        | `SHARED_ROADMAP.md`                 |
| Collaboration resource ownership                     | `COLLABORATION_ROADMAP.md`          |
| Tool ecosystem                                       | `TOOLS_ROADMAP.md`                  |
| SDE compiler artifacts                               | `SDE_ROADMAP.md`                    |
| Build/cook/package/publish pipeline                  | `BUILD_PUBLISH_PIPELINE_ROADMAP.md` |
| Forge build/cook/package orchestration               | `FORGE_ROADMAP.md`                  |
| Prism stale cooked artifact analysis                 | `PRISM_ROADMAP.md`                  |
| Dependency boundaries                                | `DependencyGraph.md`                |
| Diagnostics/reporting model                          | `DIAGNOSTICS_ROADMAP.md`            |

This file should not track future task progress unless it is documenting completed implementation decisions.

Roadmap tasks belong in the roadmap documents.

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
* Prism stale artifact analysis,
* Forge build/cook orchestration,
* asset collaboration conflict resolution,
* asset schema language design.

Those belong to:

* `ASSET_PIPELINE_ROADMAP.md`,
* `FORGE_ROADMAP.md`,
* `PRISM_ROADMAP.md`,
* `EDITOR_ROADMAP.md`,
* `BUILD_PUBLISH_PIPELINE_ROADMAP.md`,
* `COLLABORATION_ROADMAP.md`,
* `SDE_ROADMAP.md` where schema/artifact definitions are involved.

Runtime streaming loads runtime-ready assets.

It does not own the entire content pipeline.

---

## 31. Decision Summary

Preserve these decisions:

```txt
Runtime asset streaming consumes runtime-ready cooked artifacts.
AssetPipeline owns source import/cook behavior.
Forge owns build/package orchestration.
Prism owns stale cooked artifact analysis.
SagaEditor owns asset authoring UX.
Runtime/server validate manifests and artifacts at startup/load time.
Runtime should not silently load arbitrary source assets in shipping packages.
Invalid asset package state should produce structured diagnostics.
Runtime loading and residency stay in Engine.
Editor import/cook stays out of runtime streaming.
SDE remains standalone and may produce artifacts consumed by runtime.
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
