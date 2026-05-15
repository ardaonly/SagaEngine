# Asset Streaming System — Implementation Note

> Last updated: 2026-05-14  
> Status: Implementation note  
> Recommended location: `docs/implementation-notes/AssetStreamingImplementation.md`  
> Related roadmap: `ENGINE_ROADMAP.md`  
> Related systems: Runtime resources, asset registry, residency cache, streaming scheduler, runtime preview, editor import/cook pipeline.  
> Scope: Implemented runtime asset streaming architecture, design decisions, known constraints, and follow-up risks.

---

## 0. Document Status

This document is not an active roadmap.

It records the implemented asset streaming architecture for SagaEngine’s runtime/resource layer.

Active future work should be tracked in:

```txt
ENGINE_ROADMAP.md
```

or, if the asset pipeline grows large enough:

```txt
ASSET_PIPELINE_ROADMAP.md
```

This document should remain useful as implementation history and design rationale.

It must not be treated as the source of truth for future product direction.

That is how implementation notes mutate into haunted roadmaps, because apparently documents also suffer identity crises.

---

## 1. Purpose

The asset streaming system provides runtime-facing asset loading and residency management.

Its main responsibilities are:

- asynchronous asset loading,
- priority-based streaming requests,
- reference-counted residency,
- runtime asset lookup,
- memory budget awareness,
- file-backed asset sources,
- typed asset access for meshes and textures,
- asset registry manifest consumption,
- graceful failure reporting for missing or invalid assets.

The system is designed for runtime use.

It is not the full editor import/cook pipeline.

---

## 2. Ownership

The asset streaming system belongs to the engine/runtime resource layer.

It may own:

- runtime asset handles,
- asset residency cache,
- streaming request queues,
- streaming priority logic,
- runtime asset source loading,
- runtime manifest consumption,
- typed runtime wrappers for supported asset classes,
- runtime diagnostics for streaming failures.

It must not own:

- editor import UX,
- drag-and-drop authoring workflows,
- asset inspector UI,
- project content browser UI,
- SDE schema/data compilation,
- broad third-party asset normalization,
- production asset cooking policy,
- publishing workflows,
- collaboration session state,
- product project lifecycle.

Correct ownership:

```txt
Runtime asset streaming
  owns runtime loading and residency

Editor import/cook pipeline
  owns authoring-time import, conversion, validation, and cook output

Saga product shell
  owns project lifecycle and user-facing product workflow
```

Incorrect ownership:

```txt
Runtime streaming owns import UX, cook pipeline, and product workflow
```

That would be convenient in the same way throwing all your tools into one bucket is convenient until you need the sharp one.

---

## 3. Architecture Summary

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
  reports missing, invalid, failed, or budget-blocked assets
```

High-level flow:

```txt
Runtime requests AssetId
        ↓
AssetRegistry resolves metadata
        ↓
Streaming scheduler queues request
        ↓
AssetSource reads asset data
        ↓
Typed loader decodes runtime format
        ↓
ResidencyCache stores loaded result
        ↓
Runtime receives typed AssetHandle<T>
```

---

## 4. Runtime Streaming Responsibilities

The runtime streaming layer is responsible for loading assets that are already in a runtime-consumable form.

It should support:

- loading cooked mesh assets,
- loading cooked texture assets,
- loading runtime material data,
- resolving asset ids through manifests,
- applying priority to streaming requests,
- keeping assets resident while referenced,
- releasing unused assets when possible,
- respecting memory budget pressure,
- reporting missing or invalid assets.

The runtime streaming layer should avoid:

- arbitrary editor import behavior,
- expensive format normalization,
- destructive source asset mutation,
- editor UI callbacks,
- project database ownership,
- content authoring workflows.

Runtime streaming should be lean.

Editor import and cooking can be heavier.

This is one of those rare cases where “do less” is actually engineering maturity, not laziness in a trench coat.

---

## 5. Core Components

### 5.1 AssetId

`AssetId` is the stable runtime identifier for an asset.

Expected properties:

- stable across runtime sessions,
- serializable,
- comparable,
- loggable,
- valid/invalid state is explicit,
- does not depend on filesystem path identity alone.

Example responsibility:

```txt
AssetId identifies what asset the runtime wants.
It does not describe how the editor imported it.
```

---

### 5.2 AssetManifest

The asset manifest maps known asset ids to runtime metadata.

It may contain:

- asset id,
- asset kind,
- runtime path,
- source/cooked hash,
- dependency list,
- memory estimate,
- streaming group,
- platform/profile compatibility,
- version/schema information.

The manifest should be produced by the asset pipeline or build/cook process.

Runtime should consume it.

Runtime should not casually invent manifest truth at load time unless explicitly in development mode.

---

### 5.3 AssetSource

`AssetSource` abstracts where asset bytes come from.

Possible implementations:

- loose file source,
- package/archive source,
- memory source for tests,
- development hot-reload source,
- future remote/cache source.

Rules:

- asset source returns bytes or failure,
- asset source does not own typed decoding policy,
- asset source reports useful diagnostics,
- asset source should not expose platform-specific file handles through public runtime API.

---

### 5.4 AssetStreamRequest

A stream request describes runtime demand for an asset.

Expected fields:

- asset id,
- asset kind,
- priority,
- requester/context id where useful,
- desired residency level,
- cancellation token or request handle,
- diagnostic context.

Priority may be influenced by:

- camera distance,
- gameplay criticality,
- preload phase,
- player proximity,
- UI/runtime preview need,
- explicit blocking request,
- memory budget pressure.

---

### 5.5 AssetStreamingScheduler

The streaming scheduler prioritizes asset loading work.

It should support:

- request queueing,
- priority ordering,
- cancellation,
- deduplication,
- bounded concurrent work,
- budget-aware scheduling,
- diagnostics for queue pressure.

It should not block the main runtime thread unnecessarily.

Blocking loads may exist for critical boot/runtime cases, but they must be explicit.

Hidden blocking IO is just a frame hitch wearing a fake badge.

---

### 5.6 ResidencyCache

The residency cache owns loaded runtime asset instances.

It should support:

- reference-counted residency,
- weak lookup,
- explicit release,
- eviction candidates,
- memory usage tracking,
- asset kind grouping,
- diagnostics for leaks or budget pressure.

The cache should not own editor import state.

It owns loaded runtime assets, not the history of humanity’s fight with file formats.

---

### 5.7 AssetHandle

`AssetHandle<T>` provides typed runtime access to a loaded asset.

Expected behavior:

- references a valid loaded asset or explicit failure/null state,
- keeps asset resident while held,
- can expose asset metadata,
- avoids raw ownership ambiguity,
- supports diagnostics when access fails.

Typed handle examples:

```txt
AssetHandle<MeshAsset>
AssetHandle<TextureAsset>
AssetHandle<MaterialAsset>
```

---

### 5.8 AssetDiagnostic

Asset streaming diagnostics should report:

- missing asset,
- unsupported asset kind,
- invalid manifest entry,
- failed file read,
- failed decode,
- memory budget rejection,
- dependency missing,
- dependency cycle,
- version/schema mismatch,
- cancelled request.

Diagnostics should include:

- asset id,
- asset path if available,
- asset kind,
- error code,
- readable message,
- recoverability flag,
- source system if known.

---

## 6. Implemented Design Decisions

### 6.1 Runtime Streaming Is Separate From Editor Import

Decision:

```txt
Runtime streaming consumes runtime-ready assets.
Editor import/cook produces runtime-ready assets.
```

Reason:

- runtime must stay predictable,
- editor import must handle messy third-party content,
- asset conversion can be expensive,
- import diagnostics belong to authoring tools,
- runtime should not carry every importer edge case.

Preserve this decision.

Do not turn runtime streaming into Blender, glTF validator, texture compressor, material converter, file watcher, and project UI at the same time.

The universe has suffered enough.

---

### 6.2 Priority-Based Streaming

Decision:

```txt
Streaming requests carry priority.
Scheduler chooses work based on priority and budget pressure.
```

Reason:

- gameplay-critical assets should load before distant or optional assets,
- runtime preview needs responsive loading,
- memory and IO are finite,
- deterministic diagnostics are easier when request priority is explicit.

Expected priority classes:

```txt
Critical
High
Normal
Low
Background
```

---

### 6.3 Reference-Counted Residency

Decision:

```txt
Loaded assets remain resident while referenced.
Unused assets become eviction candidates.
```

Reason:

- prevents premature unload,
- supports predictable runtime handles,
- allows memory pressure handling,
- keeps ownership understandable.

Important rule:

```txt
A live AssetHandle must keep its asset valid.
```

---

### 6.4 Manifest-Driven Runtime Lookup

Decision:

```txt
Runtime resolves assets through manifests instead of ad hoc source paths.
```

Reason:

- stable ids survive path changes,
- cooked assets can differ from source assets,
- platform-specific variants can be selected,
- build/publish validation can reason about missing assets.

---

### 6.5 Typed Runtime Wrappers

Decision:

```txt
Runtime exposes typed asset handles instead of unstructured byte blobs.
```

Reason:

- call sites are safer,
- diagnostics are clearer,
- asset kind mismatch can be detected,
- runtime systems avoid repeating decode logic.

---

## 7. Current Runtime Asset Types

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

- mesh streaming,
- texture streaming,
- runtime resource lookup,
- manifest-backed asset identity,
- residency management.

Future asset types should be added through explicit typed loaders and manifest metadata, not random one-off code paths.

Random one-off loaders are how content pipelines become folklore.

---

## 8. Mesh Streaming Notes

Mesh streaming should handle runtime-ready mesh data.

Expected runtime mesh data:

- vertex buffers,
- index buffers,
- vertex layout,
- bounds,
- submesh ranges,
- material slots,
- optional LOD metadata,
- optional collision reference.

Runtime mesh streaming should not own:

- DCC import policy,
- arbitrary exporter normalization,
- artist-facing validation UI,
- mesh optimization pipeline,
- editor preview thumbnails.

Those belong to import/cook tooling and editor UX.

---

## 9. Texture Streaming Notes

Texture streaming should handle runtime-ready texture data.

Expected runtime texture data:

- dimensions,
- format,
- mip levels,
- usage flags,
- platform compatibility,
- memory estimate,
- optional streaming group.

Runtime texture streaming should not own:

- source image authoring,
- texture compression decisions,
- editor import presets,
- material authoring UI,
- source image repair.

Texture format support should be explicit.

If the runtime accepts a texture, it should know exactly what it is accepting.

“Let’s just try to load it” is not a production policy. It is a haunted demo.

---

## 10. Import Format Note

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

- real-world exporter variation,
- format validation,
- importer diagnostics,
- conversion,
- compression,
- dependency extraction,
- metadata generation,
- platform-specific cooking,
- source-to-runtime artifact mapping.

Runtime streaming should stay lean.

Editor/tool import can be heavier.

---

## 11. Memory Budget Strategy

Runtime streaming should respect platform/profile memory budgets.

Expected budget categories:

- total asset memory,
- texture memory,
- mesh memory,
- audio memory,
- streaming staging memory,
- temporary decode memory.

Budget behavior should include:

- request rejection or delay under pressure,
- eviction of unused assets,
- diagnostic output,
- per-kind memory accounting,
- high-water mark tracking.

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

## 12. Dependency Handling

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

- dependencies are listed in manifest metadata,
- missing dependencies fail clearly,
- dependency cycles are detected,
- dependency loading can be prioritized,
- dependency failure propagates to the parent request.

Runtime should not silently replace failed dependencies unless explicitly configured.

Silent fallback is how bugs get promoted to art direction.

---

## 13. Failure Behavior

Asset streaming failures must be explicit.

Required failure cases:

- asset id not found,
- manifest missing,
- asset file missing,
- asset kind mismatch,
- asset version mismatch,
- unsupported platform variant,
- failed decode,
- missing dependency,
- dependency cycle,
- memory budget exceeded,
- request cancelled,
- source unavailable.

Each failure should produce:

- error code,
- asset id,
- asset kind,
- source path if available,
- readable message,
- recoverability flag,
- diagnostic category.

Runtime must not crash on normal asset failure.

Normal asset failure is expected.

Crashing because a texture is missing is not “strict”. It is melodrama.

---

## 14. Runtime Preview Integration

Editor runtime preview may consume the asset streaming system.

Rules:

- preview requests runtime assets through approved runtime APIs,
- preview does not own residency cache internals,
- preview failures are reported back to editor diagnostics,
- stopping preview releases preview-owned asset handles,
- editor import/cook state remains separate.

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

## 15. Collaboration Boundary

Asset streaming may interact with collaboration indirectly.

Examples:

- collaborator imports an asset,
- asset cook produces new runtime artifact,
- manifest changes,
- runtime preview reloads asset,
- publish gate checks asset status.

But asset streaming does not own collaboration.

Rules:

- collaboration state belongs to `SagaCollaboration`,
- neutral collaboration contracts may live in `SagaShared`,
- editor collaboration UI belongs to `SagaEditor`,
- runtime streaming may consume stable artifact/resource ids,
- runtime streaming must not include editor-private collaboration headers.

Forbidden:

```txt
Asset streaming owns locks
Asset streaming owns participant state
Asset streaming owns session lifecycle
Asset streaming imports Editor/include/SagaEditor/Collaboration
```

---

## 16. SDE Boundary

SDE remains a standalone deterministic data compiler.

Asset streaming may consume SDE-produced runtime artifacts.

Allowed examples:

- compiled graph artifact reference,
- schema id,
- generated runtime data artifact,
- artifact hash,
- diagnostic payload reference.

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

## 17. Threading and Async Rules

Asset streaming is async by default.

Expected rules:

- IO work happens off the main runtime thread where possible,
- decode work is bounded,
- GPU upload or renderer-facing handoff occurs on the correct thread,
- asset handle publication is synchronized,
- cancellation is safe,
- failed async jobs publish diagnostics,
- shutdown waits for or cancels streaming jobs safely.

Thread ownership must be explicit.

Forbidden:

- publishing partially decoded assets,
- mutating residency cache from arbitrary threads without synchronization,
- destroying assets while live handles exist,
- blocking the main thread without explicit critical reason.

---

## 18. Hot Reload and Development Mode

Development mode may support asset reload.

Hot reload should be treated carefully.

Expected behavior:

- file/source change is detected,
- asset is reloaded through a controlled path,
- old asset remains valid until replacement is ready,
- failure keeps old valid asset when possible,
- dependent resources are invalidated or refreshed,
- diagnostics report reload result.

Hot reload should not be required for production runtime.

Development convenience must not corrupt production assumptions.

Yes, shocking: the thing that helps during development should not become a runtime landmine.

---

## 19. Testing Requirements

Asset streaming should have tests for:

- valid asset lookup,
- missing asset lookup,
- manifest load failure,
- asset kind mismatch,
- asset load success,
- asset load failure,
- dependency load success,
- dependency missing,
- dependency cycle,
- cache hit,
- cache release,
- eviction candidate selection,
- memory budget pressure,
- cancelled request,
- concurrent duplicate requests,
- shutdown with pending requests.

Recommended test categories:

```txt
Unit tests
  AssetId
  AssetManifest
  AssetRegistry
  ResidencyCache
  StreamRequestQueue

Integration tests
  manifest → source → typed load → residency handle

Failure tests
  malformed asset
  missing dependency
  invalid manifest
  budget exceeded
```

---

## 20. Diagnostics Requirements

Asset streaming diagnostics should be visible to:

- runtime diagnostics,
- editor Problems panel during preview/import integration,
- asset pipeline tooling,
- publish validation.

Minimum diagnostic fields:

```txt
AssetId
AssetKind
DiagnosticSeverity
DiagnosticCode
Message
SourcePath
ManifestPath
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
```

---

## 21. Recommended Runtime File Layout

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
```

---

## 22. Known Constraints

Current or expected constraints:

- runtime loader likely supports a controlled subset of source/runtime formats,
- broad real-world asset import compatibility is not guaranteed,
- hand-rolled parsing is risky for production authoring,
- memory budget behavior must be tested under pressure,
- dependency loading must be explicit,
- async lifetime rules must be hardened,
- diagnostics must be good enough to debug asset failures without stepping through every loader like a medieval punishment.

---

## 23. Follow-Up Risks

### 23.1 Runtime Import Creep

Risk:

```txt
Runtime streaming slowly becomes editor import/cook pipeline.
```

Impact:

- runtime becomes heavy,
- failures become harder to diagnose,
- platform-specific cook logic leaks into runtime,
- broad source format compatibility becomes runtime burden.

Mitigation:

```txt
Keep import/cook in editor/tools.
Keep runtime streaming focused on runtime-ready assets.
```

---

### 23.2 Shared Ownership Confusion

Risk:

```txt
Asset ids, manifests, diagnostics, and artifact refs get split randomly across Engine, Editor, Shared, and Tools.
```

Impact:

- duplicate contracts,
- incompatible ids,
- broken publish validation,
- fragile runtime/editor integration.

Mitigation:

```txt
Neutral contracts may live in SagaShared.
Runtime implementation stays in Engine.
Editor/tool implementation stays with owning systems.
```

---

### 23.3 Weak Diagnostics

Risk:

```txt
Asset failures report only "failed to load".
```

Impact:

- slow debugging,
- bad editor UX,
- unsafe publish decisions,
- user confusion.

Mitigation:

```txt
Every failure includes asset id, kind, source, reason, and recoverability.
```

---

### 23.4 Budget System Drift

Risk:

```txt
Residency cache ignores real memory pressure.
```

Impact:

- runtime memory spikes,
- platform instability,
- non-deterministic asset eviction,
- hard-to-reproduce crashes.

Mitigation:

```txt
Add memory accounting, budget diagnostics, and eviction tests.
```

---

## 24. Relationship to Roadmaps

This document records the implementation note for runtime asset streaming.

Active roadmap ownership:

| Area | Owner document |
|---|---|
| Runtime resource roadmap | `ENGINE_ROADMAP.md` |
| Editor import UX | `EDITOR_ROADMAP.md` |
| Shared ids/artifact contracts | `SHARED_ROADMAP.md` |
| Collaboration resource ownership | `COLLABORATION_ROADMAP.md` |
| Tool ecosystem | `TOOLS_ROADMAP.md` |
| SDE compiler artifacts | `SDE_ROADMAP.md` |
| Dependency boundaries | `DependencyGraph.md` |

This file should not track future task progress unless it is documenting completed implementation decisions.

---

## 25. Final Rule

Runtime asset streaming should stay:

```txt
small enough to reason about,
strict enough to fail safely,
async enough to avoid frame stalls,
diagnostic enough to debug,
and separate enough from editor import/cook tooling to avoid architectural soup.
```

Asset streaming loads runtime assets.

It does not own the entire content pipeline.

That boundary is not decorative.

It is the thing preventing the runtime from becoming a file-format-themed landfill.