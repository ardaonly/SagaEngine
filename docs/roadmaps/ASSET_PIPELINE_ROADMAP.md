# Saga Asset Pipeline Roadmap

> Last updated: 2026-05-15
> Status: Proposed production roadmap
> Target: A production-grade asset pipeline that connects editor import, source asset metadata, validation, cooking, artifact manifests, runtime asset streaming, SDE references, Forge build/package orchestration, diagnostics, collaboration, and publish gates.
> Scope: Source assets, imported assets, asset metadata, asset ids, asset registry, import/cook workflows, cooked artifacts, asset manifests, dependency tracking, runtime-ready formats, editor UX, Forge integration, runtime streaming consumption, diagnostics, collaboration behavior, and packaging/publishing rules.

---

## 0. Roadmap Convention

- `[x]` — Shipped. The note after the item names the files, modules, tests, generated artifacts, or integration points that represent the completed work.
- `[ ]` — Open. The item describes the finished production state, not temporary scaffolding.
- Shipped asset pipeline work must include implementation, diagnostics, tests, and verification evidence.
- Open asset pipeline work must describe observable editor/build/runtime behavior, not vague architecture intent.
- Runtime asset streaming must remain separate from editor import and cooking.
- Asset import UX must not become runtime loading policy.
- Forge coordinates build/cook/package steps but does not own importer internals.
- SDE may reference asset artifacts, but SDE must not own asset import/cook implementation.
- Runtime consumes runtime-ready artifacts; it must not become a general file format normalization system.

A runtime loader that slowly becomes an importer, cooker, validator, file watcher, preview database, and product workflow has stopped being a loader.

It has become a hostage situation with textures.

---

## 1. Document Purpose

This document defines Saga's asset pipeline roadmap.

Saga already separates runtime streaming from editor import/cook responsibilities. This roadmap turns that separation into a complete product pipeline:

```txt
Source Asset
    ↓
Editor Import
    ↓
Asset Metadata + Stable AssetId
    ↓
Validation
    ↓
Cook Plan
    ↓
Cooked Runtime Artifact
    ↓
Asset Manifest
    ↓
Forge Build/Package
    ↓
Runtime Asset Streaming
    ↓
Diagnostics / Collaboration / Publish Gate
```

The target is not simply loading files.

The target is a predictable, diagnosable, reproducible asset path where editor authoring, build/cook tooling, runtime streaming, collaboration, and publishing all agree on what an asset is and which artifact represents it.

Correct model:

```txt
Editor import/cook pipeline produces runtime-ready assets.
Forge coordinates validation, cook, staging, and package layout.
Runtime streaming consumes runtime-ready assets through manifests.
Saga product workflow owns publish decisions.
```

Incorrect model:

```txt
Runtime sees a random PNG, FBX, WAV, glTF, or Blender file and figures life out at 60 FPS.
```

That is not flexibility.

That is a frame hitch wearing a cape.

---

## 2. Companion Documents

| Document | Purpose |
|---|---|
| `AssetStreamingImplementation.md` | Implemented runtime asset streaming architecture and current design decisions |
| `ENGINE_ROADMAP.md` | Runtime resource ownership, streaming consumption, diagnostics, server/runtime systems |
| `EDITOR_ROADMAP.md` | Content browser, asset inspector, import UX, authoring diagnostics |
| `FORGE_ROADMAP.md` | Build workflow frontend, validation, cook steps, package staging |
| `SDE_ROADMAP.md` | Deterministic data compiler, artifact references, dependency manifests |
| `SHARED_ROADMAP.md` | Neutral asset/artifact/package/diagnostic contracts |
| `PRISM_ROADMAP.md` | Artifact relationship analysis, stale generated/cooked artifact detection |
| `COLLABORATION_ROADMAP.md` | Asset claims, locks, conflicts, change streams, shared editing UX |
| `DIAGNOSTICS_ROADMAP.md` | Runtime diagnostics, structured reports, health/resource visibility |
| `DependencyGraph.md` | Dependency ownership and allowed module directions |
| `SAGA_PRODUCT_ROADMAP.md` | Product shell, project lifecycle, preview/build/publish workflow |

---

## 3. Product Definition

- [ ] Define Saga Asset Pipeline as the production asset path from source files to runtime-ready artifacts.

  Done means:

  - source assets can be imported,
  - stable asset ids are assigned,
  - asset metadata is stored and validated,
  - cook plans are generated deterministically where practical,
  - cooked artifacts are emitted,
  - asset manifests map asset ids to runtime artifacts,
  - Forge validates/cooks/stages/packages assets,
  - runtime streaming loads only runtime-ready assets,
  - diagnostics explain import/cook/runtime failures,
  - publish is blocked by invalid or stale asset state.

- [ ] Keep asset pipeline separate from runtime streaming implementation.

  Done means:

  - runtime streaming loads cooked/runtime-ready assets,
  - editor import/cook handles source asset conversion,
  - runtime does not own broad third-party file normalization,
  - runtime does not own import UX or content browser behavior.

- [ ] Keep asset pipeline product-aware without turning Forge or runtime into the product shell.

  Done means:

  - Saga owns product lifecycle and publish entry points,
  - Forge coordinates asset build/cook/package steps,
  - Editor owns authoring UI,
  - Runtime consumes artifacts,
  - shared contracts describe asset/artifact identity.

---

## 4. Ownership Model

### 4.1 Saga Product Ownership

- [ ] `Saga` owns product-level asset workflow entry points.

  Done means Saga can expose:

  - project asset status,
  - import entry points through mounted editor mode,
  - preview/build/package/publish actions,
  - global asset validation failures,
  - publish-blocking asset diagnostics,
  - product-level asset pipeline health state.

- [ ] `Saga` does not own importer, cooker, runtime loader, or build internals.

  Done means Saga consumes services/reports rather than becoming a hidden asset pipeline implementation.

---

### 4.2 SagaEditor Ownership

- [ ] `SagaEditor` owns asset authoring UX.

  Done means the editor owns:

  - content browser,
  - import dialogs,
  - drag-and-drop import behavior,
  - asset inspector,
  - import settings UI,
  - asset preview surfaces,
  - asset validation display,
  - editor-side cook/preview commands,
  - asset diagnostics navigation.

- [ ] `SagaEditor` does not own runtime asset truth.

  Done means editor UI state is not the source of runtime asset identity or package layout.

---

### 4.3 Asset Pipeline Ownership

- [ ] Create a dedicated asset pipeline service/module when implementation size justifies it.

  Candidate ownership:

```txt
SagaAssetPipeline
```

  or initially:

```txt
Editor asset import/cook services + Forge cook orchestration + Shared contracts
```

  Done means asset pipeline implementation owns:

  - import configuration,
  - source asset scanning,
  - asset metadata generation,
  - importer selection,
  - cook planning,
  - cook tool invocation rules,
  - cooked artifact emission,
  - asset dependency tracking,
  - import/cook diagnostics,
  - asset manifest generation.

- [ ] Keep asset pipeline independent from runtime streaming internals.

  Done means the pipeline can produce runtime-ready artifacts without including runtime loader private headers.

---

### 4.4 Forge Ownership

- [ ] Forge owns asset build/cook/package orchestration.

  Done means Forge can:

  - validate asset metadata,
  - build cook plans,
  - invoke asset cook tools,
  - collect cook diagnostics,
  - stage cooked artifacts,
  - write package manifests,
  - fail builds on invalid/stale assets,
  - produce CI-friendly asset reports.

- [ ] Forge does not own importer internals.

  Done means Forge invokes approved cook/import tools or services; it does not become a mesh importer, texture compressor, audio converter, and material compiler internally.

---

### 4.5 Runtime Ownership

- [ ] Runtime owns loading and residency of runtime-ready asset artifacts.

  Done means runtime owns:

  - asset handles,
  - asset registry consumption,
  - manifest lookup,
  - streaming requests,
  - priority-based loading,
  - residency cache,
  - typed runtime asset access,
  - runtime streaming diagnostics.

- [ ] Runtime does not own editor import/cook pipeline.

  Forbidden runtime responsibilities:

  - drag-and-drop asset import,
  - broad source format normalization,
  - destructive source asset mutation,
  - editor import settings UI,
  - package publish decisions,
  - source asset database ownership.

---

### 4.6 SagaShared Ownership

- [ ] `SagaShared` owns neutral asset/artifact contracts.

  Done means `SagaShared` may contain:

  - `AssetId`,
  - `AssetKind`,
  - `AssetRef`,
  - `ArtifactId`,
  - `ArtifactRef`,
  - `AssetImportStatus`,
  - `AssetCookStatus`,
  - `AssetDiagnosticPayload`,
  - `AssetManifestDescriptor`,
  - `CookedArtifactDescriptor`,
  - `AssetDependencyDescriptor`.

- [ ] `SagaShared` does not own asset pipeline implementation.

  Forbidden in `SagaShared`:

  - importers,
  - file format parsers,
  - texture compression implementation,
  - mesh optimization implementation,
  - runtime cache implementation,
  - editor asset panels,
  - build/cook process orchestration,
  - filesystem watchers.

---

### 4.7 SDE Ownership

- [ ] SDE may reference asset artifacts through stable contracts.

  Done means SDE source/data can reference:

  - asset ids,
  - artifact refs,
  - schema ids,
  - package refs,
  - dependency manifests,
  - validation diagnostics.

- [ ] SDE does not own asset import/cook implementation.

  Done means SDE can validate references and emit artifact dependencies, but it does not decode source meshes, compress textures, import audio, or manage editor asset previews.

---

### 4.8 Prism Ownership

- [ ] Prism owns asset/artifact relationship analysis.

  Done means Prism can detect:

  - stale cooked artifacts,
  - missing artifact outputs,
  - source asset hash mismatch,
  - invalid generated/cooked artifact relationships,
  - asset references pointing to missing artifacts,
  - package manifest inconsistencies.

- [ ] Prism does not cook assets.

Prism observes and analyzes.

It does not become the kitchen.

---

## 5. Dependency Rules

### 5.1 Allowed Dependencies

Allowed dependency directions:

```txt
Saga → asset pipeline service API
Saga → Forge build/publish service boundary
Saga → SagaShared asset/artifact contracts

SagaEditor → asset pipeline service API
SagaEditor → SagaShared asset/artifact contracts
SagaEditor → Forge validation/cook boundary
SagaEditor → runtime preview asset handles where approved

Forge → asset cook/import tool executables or service boundary
Forge → SagaShared asset/artifact/diagnostic contracts where approved
Forge → SDE executable for data/artifact validation where required

Runtime → SagaShared asset/artifact contracts
Runtime → cooked asset artifacts
Runtime → asset manifest files

Server → SagaShared asset/artifact contracts
Server → cooked server-needed artifacts

SDE → standalone compiler-safe libraries
SDE output → references asset/artifact ids

Prism → source asset metadata
Prism → artifact manifests
Prism → package manifests
Prism → generated/cooked output metadata
```

---

### 5.2 Forbidden Dependencies

Forbidden dependency directions:

```txt
Runtime → SagaEditor import UI
Runtime → asset importer private implementation
Runtime → Forge internals
Runtime → SDE parser/AST internals
SagaShared → asset pipeline implementation
Forge → runtime streaming internals
Prism → asset cooker internals
SDE → asset importer internals
Asset pipeline → product shell widgets
Asset pipeline → editor UI widgets unless in editor adapter layer
```

Forbidden shortcuts:

```txt
Runtime imports arbitrary source files in shipping mode.
Editor UI state becomes asset registry truth.
Forge implements every file format importer internally.
SDE compiles source asset binary formats.
AssetId is just a filesystem path.
Cooked artifact hash is ignored at runtime.
Publish succeeds with stale cooked assets.
```

---

## 6. Core Concepts

### 6.1 Source Asset

- [ ] Define source asset model.

  Done means a source asset can describe:

  - source path,
  - source hash,
  - detected asset kind,
  - importer id,
  - import settings ref,
  - owner package/project,
  - last imported time or deterministic import version,
  - source diagnostics.

Examples:

```txt
Textures/terrain_albedo.png
Models/player.fbx
Audio/hit_01.wav
Materials/player_material.sde
```

Source assets are authoring inputs.

They are not automatically runtime assets.

---

### 6.2 AssetId

- [ ] Define stable `AssetId` contract.

  Done means `AssetId` is:

  - stable across sessions,
  - serializable,
  - comparable,
  - loggable,
  - explicit about invalid/null state,
  - not dependent on filesystem path identity alone.

- [ ] Define asset id generation policy.

  Done means asset ids can be assigned by:

  - project asset database,
  - import manifest,
  - package metadata,
  - deterministic path/hash strategy where approved,
  - explicit user/plugin-provided ids where safe.

Filesystem paths move.

Stable asset identity should not panic every time someone renames a folder.

---

### 6.3 Asset Metadata

- [ ] Define asset metadata model.

  Done means metadata can describe:

  - asset id,
  - display name,
  - asset kind,
  - source asset refs,
  - import settings,
  - cook settings,
  - dependencies,
  - tags/labels,
  - platform compatibility,
  - validation status,
  - artifact refs,
  - preview status.

---

### 6.4 Asset Kind

- [ ] Define asset kinds.

  Required initial kinds:

```txt
Texture
Mesh
Material
Shader
Audio
Animation
Skeleton
Scene
Prefab
Script
Graph
SDEData
Package
Font
UI
Unknown
```

- [ ] Define kind-specific import/cook rules.

  Done means texture, mesh, audio, material, graph, and script assets do not pretend to share the same pipeline just because they are all files.

---

### 6.5 Imported Asset

- [ ] Define imported asset state.

  Done means imported asset state records:

  - source asset ref,
  - asset id,
  - importer id/version,
  - import settings hash,
  - source hash,
  - imported metadata hash,
  - import diagnostics,
  - dependency list.

Imported asset state is the bridge between messy source files and controlled cook artifacts.

---

### 6.6 Cooked Artifact

- [ ] Define cooked artifact contract.

  Done means every cooked artifact has:

  - artifact id,
  - artifact kind,
  - source asset id,
  - runtime path/package path,
  - artifact hash,
  - cook profile,
  - target platform,
  - schema/format version,
  - dependencies,
  - memory estimate,
  - diagnostics summary.

Cooked artifacts are what runtime should load.

Source assets are what humans edit.

Confusing these two is how runtime code becomes a file-format support group.

---

### 6.7 Asset Manifest

- [ ] Define asset manifest format.

  Done means the manifest maps:

  - asset id,
  - asset kind,
  - artifact ref,
  - runtime path,
  - dependencies,
  - memory estimate,
  - streaming group,
  - platform/profile compatibility,
  - version/schema information,
  - diagnostics summary.

- [ ] Define manifest ownership.

  Done means manifests are produced by build/cook/package pipeline and consumed by runtime/server/editor preview.

Runtime may consume manifest truth.

Runtime should not invent manifest truth in shipping mode.

---

### 6.8 Asset Registry

- [ ] Define asset registry ownership.

  Done means the asset registry provides:

  - asset id lookup,
  - asset metadata lookup,
  - artifact lookup,
  - dependency lookup,
  - validation state lookup,
  - package membership lookup.

- [ ] Separate editor registry from runtime registry.

  Done means editor registry may know source assets/import settings, while runtime registry knows runtime-ready artifacts and streaming metadata.

---

### 6.9 Asset Dependencies

- [ ] Define asset dependency model.

  Done means dependencies can represent:

  - texture used by material,
  - material used by mesh/prefab,
  - skeleton used by animation,
  - audio bank references,
  - graph/script asset references,
  - SDE data references,
  - package-level dependencies.

- [ ] Detect dependency cycles where invalid.

  Done means cook/build fails clearly when dependency cycles make artifact generation invalid.

---

## 7. Import Pipeline

### 7.1 Import Discovery

- [ ] Add source asset discovery.

  Done means editor/build tools can discover source assets under approved project content roots.

Expected roots:

```txt
<Project>/Assets/
<Project>/Content/
<Project>/Scripts/
<Project>/.sde/
```

- [ ] Support ignore/exclude rules.

  Done means generated/build/cache/vendor directories are not accidentally imported.

---

### 7.2 Importer Selection

- [ ] Define importer registry.

  Done means source assets are routed to importers by:

  - extension,
  - content sniffing where safe,
  - explicit import settings,
  - asset kind override,
  - plugin/package importer registration.

- [ ] Reject ambiguous importer selection.

  Done means assets with multiple possible importers require explicit choice or deterministic priority rules.

---

### 7.3 Import Settings

- [ ] Define import settings model.

  Done means import settings can describe:

  - importer id,
  - importer version,
  - target asset kind,
  - scale/unit settings,
  - texture color space,
  - compression intent,
  - mesh optimization intent,
  - audio conversion intent,
  - material mapping rules,
  - platform/profile overrides.

- [ ] Version import settings.

  Done means old import settings can migrate or fail clearly instead of silently changing output.

---

### 7.4 Import Diagnostics

- [ ] Emit structured import diagnostics.

  Required diagnostics:

```txt
Asset.Import.UnsupportedFormat
Asset.Import.InvalidSource
Asset.Import.MissingDependency
Asset.Import.AmbiguousImporter
Asset.Import.InvalidSettings
Asset.Import.VersionMismatch
Asset.Import.SourceChanged
```

- [ ] Display import diagnostics in editor Problems panel.

  Done means users can click diagnostics and navigate to the relevant asset/source/import setting.

---

## 8. Cook Pipeline

### 8.1 Cook Plan

- [ ] Define cook plan model.

  Done means cook plan contains:

  - cook profile,
  - target platform,
  - input assets,
  - output artifacts,
  - dependencies,
  - tool invocations,
  - cache keys,
  - diagnostics,
  - failure policy.

- [ ] Keep cook plan deterministic.

  Done means identical source/import/settings/profile inputs produce identical planned outputs where practical.

---

### 8.2 Cook Profiles

- [ ] Define cook profiles.

  Required profiles:

```txt
editor-preview
dev-client
dev-server
shipping-client
shipping-server
test
```

- [ ] Support platform-specific cook settings.

  Done means texture/audio/mesh/package formats can differ by platform/profile without corrupting asset identity.

---

### 8.3 Cook Steps

- [ ] Add cook step categories.

  Required categories:

```txt
ValidateSource
ImportMetadata
ConvertTexture
ConvertMesh
ConvertAudio
CompileMaterial
CompileShader
CompileGraph
CompileScript
GenerateManifest
StageArtifact
PackageArtifact
```

- [ ] Define cook step failure policy.

  Done means build fails or warns according to asset kind, profile, severity, and package requirement.

---

### 8.4 Cook Cache

- [ ] Define cook cache keys.

  Done means cache keys include:

  - source hash,
  - import settings hash,
  - cook settings hash,
  - importer version,
  - cooker version,
  - toolchain version,
  - target platform,
  - cook profile,
  - dependency hashes.

- [ ] Reject stale cache hits.

  Done means stale cooked artifacts are detected instead of silently reused.

---

### 8.5 Cook Diagnostics

- [ ] Emit structured cook diagnostics.

  Required diagnostics:

```txt
Asset.Cook.Failed
Asset.Cook.MissingInput
Asset.Cook.MissingOutput
Asset.Cook.StaleArtifact
Asset.Cook.DependencyCycle
Asset.Cook.UnsupportedPlatform
Asset.Cook.InvalidProfile
Asset.Cook.CacheMismatch
Asset.Cook.BudgetExceeded
```

---

## 9. Asset Runtime Consumption

### 9.1 Runtime Manifest Loading

- [ ] Runtime loads asset manifests.

  Done means runtime can read packaged asset manifests and resolve asset ids to runtime artifact paths.

- [ ] Runtime rejects invalid manifests.

  Done means runtime produces diagnostics for:

  - missing manifest,
  - invalid manifest version,
  - missing artifact,
  - hash mismatch,
  - unsupported asset kind,
  - incompatible platform/profile.

---

### 9.2 Runtime Asset Handles

- [ ] Runtime exposes typed asset handles.

  Required initial handles:

```txt
AssetHandle<TextureAsset>
AssetHandle<MeshAsset>
AssetHandle<MaterialAsset>
AssetHandle<AudioAsset>
```

- [ ] Runtime handles explicit failure/null state.

  Done means failed asset loads do not produce ambiguous raw pointers or undefined ownership.

---

### 9.3 Runtime Residency

- [ ] Runtime manages asset residency.

  Done means runtime supports:

  - reference-counted residency,
  - weak lookup,
  - explicit release,
  - eviction candidates,
  - memory budget awareness,
  - diagnostics for leaks/budget pressure.

---

### 9.4 Runtime Streaming Groups

- [ ] Define streaming group model.

  Done means assets can be grouped by:

  - zone,
  - scene,
  - player proximity,
  - UI/menu state,
  - gameplay criticality,
  - preload set,
  - background group.

---

## 10. Editor UX

### 10.1 Content Browser

- [ ] Integrate asset pipeline with Content Browser.

  Done means the content browser can show:

  - source asset status,
  - imported asset status,
  - cook status,
  - artifact status,
  - validation diagnostics,
  - dependency info,
  - asset preview state.

---

### 10.2 Asset Inspector

- [ ] Add asset inspector integration.

  Done means users can inspect:

  - asset id,
  - asset kind,
  - source file,
  - import settings,
  - cook settings,
  - dependencies,
  - artifact refs,
  - runtime memory estimate,
  - diagnostics.

---

### 10.3 Import UX

- [ ] Add controlled import workflow.

  Done means users can:

  - import source assets,
  - choose importer where needed,
  - edit import settings,
  - preview import result,
  - see diagnostics,
  - commit or cancel import.

---

### 10.4 Reimport UX

- [ ] Add reimport workflow.

  Done means users can:

  - detect source changes,
  - reimport asset,
  - preserve asset id,
  - update imported metadata,
  - invalidate cooked artifacts,
  - trigger recook.

---

### 10.5 Asset Preview

- [ ] Add asset preview rules.

  Done means preview uses editor/runtime preview services without changing runtime package truth.

Examples:

```txt
texture preview
mesh preview
material preview
audio preview
animation preview
```

---

## 11. SDE Integration

- [ ] Allow SDE source to reference assets through stable ids/artifact refs.

  Done means SDE definitions can refer to assets without depending on editor import implementation.

Example concept:

```txt
item Sword {
    icon: asset("ui/icons/sword")
    mesh: asset("weapons/sword_mesh")
}
```

- [ ] Validate asset references.

  Done means SDE compile can emit diagnostics when:

  - asset id is unknown,
  - artifact ref is missing,
  - asset kind is incompatible,
  - asset schema version is incompatible,
  - required asset is not cooked for target profile.

- [ ] Emit asset dependency information.

  Done means SDE output manifests include asset/artifact refs needed by Forge and runtime packaging.

---

## 12. Forge Integration

- [ ] Add asset validation step.

  Done means Forge can validate:

  - source asset metadata,
  - asset ids,
  - import settings,
  - cook settings,
  - dependency graph,
  - stale artifacts,
  - manifest consistency.

- [ ] Add asset cook step.

  Done means Forge can:

  - create cook plan,
  - invoke cook tools,
  - collect diagnostics,
  - stage cooked outputs,
  - write asset manifests.

- [ ] Add asset package step.

  Done means Forge can package assets into:

  - client package,
  - server package,
  - editor-preview package,
  - test package.

- [ ] Reject invalid package placement.

  Done means Forge fails when:

  - client package misses required runtime assets,
  - server package misses server-needed graph/script/data assets,
  - editor-only assets enter shipping package incorrectly,
  - artifacts are stale or hash-mismatched.

---

## 13. Prism Integration

- [ ] Add asset/artifact relationship index.

  Done means Prism can answer:

  - which source asset produced this artifact,
  - which assets depend on this asset,
  - which SDE definitions reference this asset,
  - which package includes this artifact,
  - whether artifact is stale.

- [ ] Add stale cooked artifact detection.

  Done means Prism detects when source/import/cook inputs changed but cooked outputs were not regenerated.

- [ ] Add missing reference detection.

  Done means Prism can report broken asset references across:

  - SDE data,
  - graph assets,
  - script metadata,
  - package manifests,
  - runtime manifests.

---

## 14. Collaboration Behavior

- [ ] Add asset collaboration metadata.

  Done means collaborative resources can describe:

  - asset id,
  - source path,
  - import settings hash,
  - cook status,
  - artifact hash,
  - claim state,
  - lock state,
  - dirty state,
  - validation state,
  - conflict state.

- [ ] Support soft claims for asset editing.

  Done means users can signal active work on:

  - source asset,
  - import settings,
  - material asset,
  - prefab/scene asset,
  - graph/script asset.

- [ ] Support hard locks for unsafe operations.

  Lock-worthy operations:

```txt
schema migration
bulk reimport
asset id migration
package manifest rewrite
cook profile migration
source asset destructive conversion
```

- [ ] Define asset conflict records.

  Done means conflicts can identify:

  - source change conflict,
  - import settings conflict,
  - artifact generation conflict,
  - dependency graph conflict,
  - package placement conflict.

---

## 15. Diagnostics

Required diagnostic families:

```txt
Asset.Source
Asset.Import
Asset.Metadata
Asset.Dependency
Asset.Cook
Asset.Manifest
Asset.Package
Asset.Runtime
Asset.Streaming
Asset.Collaboration
Asset.Stale
Asset.Budget
```

Required diagnostic fields:

```txt
diagnosticCode
severity
message
assetId
assetKind
sourcePath
artifactId
artifactPath
importerId
cookerId
cookProfile
targetPlatform
sourceHash
artifactHash
expectedHash
actualHash
dependencyRef
suggestedFix
```

Example diagnostics:

```txt
Asset.Cook.StaleArtifact
Error: Texture terrain_albedo has source hash 91af... but cooked artifact was produced from 44bc...
Run asset cook or rebuild the package.
```

```txt
Asset.Runtime.HashMismatch
Error: Runtime asset mesh/player_body failed hash validation.
Expected artifact hash b17c..., got 2a90...
```

```txt
Asset.Import.MissingDependency
Warning: Material player_cloth references missing texture cloth_normal.
```

---

## 16. Build and Publish Gates

### 16.1 Validation Gate

- [ ] Block build on required asset validation errors.

  Done means Forge rejects builds when required source/import/cook metadata is invalid.

---

### 16.2 Cook Gate

- [ ] Block package on failed cook.

  Done means runtime packages cannot be produced with missing required cooked artifacts.

---

### 16.3 Manifest Gate

- [ ] Block runtime package on invalid asset manifest.

  Done means manifest hash/version/platform/profile mismatches fail before runtime.

---

### 16.4 Publish Gate

- [ ] Saga publish workflow blocks invalid asset state.

  Done means publish fails when:

  - required assets are missing,
  - cooked artifacts are stale,
  - package manifest is invalid,
  - SDE references unknown assets,
  - runtime/server package asset sets disagree where they must match,
  - asset diagnostics exceed severity policy.

A publish system that ships stale assets is not a pipeline.

It is a suspense generator for production bugs.

---

## 17. Memory and Budget Awareness

- [ ] Add asset memory estimates.

  Done means imported/cooked metadata can include estimates for:

  - CPU memory,
  - GPU memory,
  - streaming size,
  - decompressed size,
  - package size.

- [ ] Add budget diagnostics.

  Done means cook/build/runtime can warn or fail when:

  - texture memory exceeds profile budget,
  - mesh memory exceeds budget,
  - streaming group exceeds budget,
  - package size exceeds budget,
  - runtime residency pressure exceeds threshold.

- [ ] Connect budgets to runtime diagnostics.

  Done means runtime streaming can report budget pressure using structured diagnostics/health metrics.

---

## 18. Versioning and Migration

- [ ] Version asset metadata schemas.

  Done means asset metadata includes schema version and migration behavior.

- [ ] Version cooked artifact formats.

  Done means runtime can reject incompatible artifacts clearly.

- [ ] Add migration strategy.

  Done means asset migrations can:

  - migrate metadata,
  - preserve asset ids,
  - invalidate stale cooked artifacts,
  - report migration diagnostics,
  - require collaboration locks where necessary.

---

## 19. Testing Strategy

### 19.1 Contract Tests

- [ ] Add asset contract tests.

  Required coverage:

  - `AssetId` serialization,
  - `ArtifactRef` serialization,
  - asset metadata descriptors,
  - asset manifest descriptors,
  - diagnostic payloads,
  - dependency descriptors.

---

### 19.2 Import Tests

- [ ] Add import pipeline tests.

  Required coverage:

  - valid texture import,
  - valid mesh import metadata,
  - invalid source asset,
  - ambiguous importer,
  - missing dependency,
  - import settings hash change,
  - reimport preserves asset id.

---

### 19.3 Cook Tests

- [ ] Add cook pipeline tests.

  Required coverage:

  - valid cook plan,
  - deterministic cook plan ordering,
  - stale cache rejection,
  - dependency cycle detection,
  - missing output detection,
  - platform/profile mismatch,
  - artifact manifest emission.

---

### 19.4 Runtime Consumption Tests

- [ ] Add runtime asset consumption tests.

  Required coverage:

  - manifest loads,
  - missing artifact diagnostic,
  - invalid hash diagnostic,
  - typed asset handle success,
  - typed asset handle failure,
  - residency reference/release behavior,
  - budget pressure diagnostic.

---

### 19.5 Forge/Prism Tests

- [ ] Add Forge/Prism asset tests.

  Required coverage:

  - Forge validates assets,
  - Forge cooks and stages artifacts,
  - Forge rejects stale artifacts,
  - Prism detects stale cooked output,
  - Prism reports broken asset references,
  - package manifest consistency check.

---

### 19.6 Editor Tests

- [ ] Add editor asset UX tests.

  Required coverage:

  - content browser displays asset status,
  - inspector displays asset id/metadata/artifacts,
  - import diagnostics route to Problems panel,
  - reimport invalidates cooked artifact,
  - preview uses runtime-ready artifact or explicit preview artifact.

---

## 20. MVP Vertical Slice

The first asset pipeline vertical slice should be a texture asset path.

Required scenario:

```txt
Import texture → cook texture → manifest → runtime load → editor preview → package validation
```

Required behavior:

1. User imports `terrain_albedo.png`.
2. Editor assigns stable `AssetId`.
3. Import settings are saved.
4. Asset metadata records source hash.
5. Forge creates cook plan for `editor-preview` and `dev-client`.
6. Cook step emits runtime-ready texture artifact.
7. Asset manifest maps `AssetId` to artifact path/hash.
8. Runtime streaming loads texture through manifest.
9. Editor preview displays loaded texture via runtime/preview asset handle.
10. Source file change marks cooked artifact stale.
11. Prism detects stale artifact.
12. Forge recook regenerates artifact and manifest.
13. Publish fails if artifact is stale or missing.

This slice is intentionally small.

It tests identity, import, cook, manifest, runtime consumption, diagnostics, and publish gates without pretending the whole asset universe has to exist on day one.

---

## 21. Initial Asset Kind Priority

Recommended implementation order:

```txt
1. Texture
2. Material
3. Mesh
4. Audio
5. Graph/SDE data asset references
6. Animation/Skeleton
7. Scene/Prefab
8. Shader pipeline expansion
```

Reason:

- textures expose import/cook/hash/runtime-preview basics,
- materials test dependency tracking,
- meshes test heavier import complexity,
- audio tests streaming/profile differences,
- graph/SDE data tests cross-pipeline references,
- animation/skeleton tests dependency and versioning pressure,
- scenes/prefabs test large composite assets,
- shaders deserve their own seriousness and should not be casually glued on.

---

## 22. Non-Goals

The Saga asset pipeline must not become:

- runtime arbitrary source file importing in shipping mode,
- a replacement for all external DCC tools,
- a hidden product shell,
- a Forge-internal file format zoo,
- a SDE compiler responsibility,
- an editor UI state serialization hack,
- a runtime loader that knows every source format,
- a package publisher that ignores stale artifacts.

The goal is a controlled source-to-runtime pipeline.

Not a magical folder where files become production-ready because everyone agreed to hope.

---

## 23. Risk Register

### 23.1 Risk: Runtime Streaming Becomes Import Pipeline

Mitigation:

- runtime loads runtime-ready artifacts,
- editor/import pipeline handles source conversion,
- Forge stages cooked artifacts,
- shipping runtime rejects source-only assets.

---

### 23.2 Risk: AssetId Is Path-Based and Fragile

Mitigation:

- stable ids are assigned at import,
- metadata tracks source path separately,
- rename/move preserves identity where possible,
- diagnostics report broken references clearly.

---

### 23.3 Risk: Cooked Artifacts Become Stale Silently

Mitigation:

- source/import/cook hashes are recorded,
- Prism detects stale artifacts,
- Forge rejects stale package builds,
- runtime can validate artifact hashes.

---

### 23.4 Risk: Forge Becomes Importer Implementation

Mitigation:

- Forge invokes cook/import tools or services,
- importer/cooker implementation has clear ownership,
- Forge owns orchestration and diagnostics aggregation.

---

### 23.5 Risk: Editor Preview Diverges From Runtime Behavior

Mitigation:

- preview uses cooked/editor-preview artifacts where possible,
- preview artifacts are clearly marked,
- runtime package artifacts remain separate,
- diagnostics identify preview/runtime mismatch.

---

### 23.6 Risk: Asset Pipeline Becomes Too Big Too Early

Mitigation:

- start with texture vertical slice,
- keep asset kind priority explicit,
- do not implement every importer at once,
- focus on identity, diagnostics, manifests, and stale artifact gates first.

---

## 24. Suggested File Targets

Expected shared contracts:

```txt
Shared/include/SagaShared/Assets/AssetId.hpp
Shared/include/SagaShared/Assets/AssetKind.hpp
Shared/include/SagaShared/Assets/AssetRef.hpp
Shared/include/SagaShared/Assets/AssetMetadata.hpp
Shared/include/SagaShared/Assets/AssetDependency.hpp
Shared/include/SagaShared/Assets/AssetImportStatus.hpp
Shared/include/SagaShared/Assets/AssetCookStatus.hpp
Shared/include/SagaShared/Assets/AssetDiagnostic.hpp
Shared/include/SagaShared/Artifacts/ArtifactId.hpp
Shared/include/SagaShared/Artifacts/ArtifactRef.hpp
Shared/include/SagaShared/Artifacts/CookedArtifactDescriptor.hpp
Shared/include/SagaShared/Assets/AssetManifestDescriptor.hpp
```

Expected asset pipeline files:

```txt
Tools/AssetPipeline/include/SagaAssetPipeline/Import/ImporterRegistry.hpp
Tools/AssetPipeline/include/SagaAssetPipeline/Import/ImportSettings.hpp
Tools/AssetPipeline/include/SagaAssetPipeline/Import/ImportResult.hpp
Tools/AssetPipeline/include/SagaAssetPipeline/Cook/CookPlan.hpp
Tools/AssetPipeline/include/SagaAssetPipeline/Cook/CookStep.hpp
Tools/AssetPipeline/include/SagaAssetPipeline/Cook/CookProfile.hpp
Tools/AssetPipeline/include/SagaAssetPipeline/Manifest/AssetManifestWriter.hpp
Tools/AssetPipeline/src/Import/ImporterRegistry.cpp
Tools/AssetPipeline/src/Cook/CookPlanner.cpp
Tools/AssetPipeline/src/Manifest/AssetManifestWriter.cpp
```

If initially kept inside editor/tooling before extraction:

```txt
Editor/include/SagaEditor/Assets/ImportService.h
Editor/include/SagaEditor/Assets/AssetMetadataService.h
Editor/include/SagaEditor/Assets/AssetPreviewService.h
Editor/src/SagaEditor/Assets/ImportService.cpp
Editor/src/SagaEditor/Assets/AssetMetadataService.cpp
Editor/src/SagaEditor/Assets/AssetPreviewService.cpp
```

Expected Forge files:

```txt
Tools/Forge/include/Forge/Steps/AssetValidateStep.hpp
Tools/Forge/include/Forge/Steps/AssetCookStep.hpp
Tools/Forge/include/Forge/Steps/AssetPackageStep.hpp
Tools/Forge/src/Steps/AssetValidateStep.cpp
Tools/Forge/src/Steps/AssetCookStep.cpp
Tools/Forge/src/Steps/AssetPackageStep.cpp
Tools/Forge/tests/AssetCookStepTests.cpp
```

Expected Prism files:

```txt
Tools/Prism/include/Prism/Assets/AssetArtifactIndex.hpp
Tools/Prism/include/Prism/Assets/AssetDependencyIndex.hpp
Tools/Prism/include/Prism/Validation/StaleCookedArtifactCheck.hpp
Tools/Prism/include/Prism/Validation/MissingAssetReferenceCheck.hpp
Tools/Prism/src/Assets/AssetArtifactIndex.cpp
Tools/Prism/src/Validation/StaleCookedArtifactCheck.cpp
```

Expected runtime files:

```txt
Engine/Public/SagaEngine/Assets/AssetManifest.hpp
Engine/Public/SagaEngine/Assets/AssetRegistry.hpp
Engine/Public/SagaEngine/Assets/AssetHandle.hpp
Engine/Public/SagaEngine/Assets/AssetDiagnostic.hpp
Engine/Private/SagaEngine/Assets/AssetManifestLoader.cpp
Engine/Private/SagaEngine/Assets/AssetRegistry.cpp
```

Expected project paths:

```txt
<Project>/Assets/
<Project>/AssetMetadata/
<Project>/Generated/Assets/
<Project>/Build/Artifacts/Assets/
<Project>/Build/Manifests/assets.json
<Project>/Build/Manifests/asset_dependencies.json
<Project>/Build/Reports/asset_diagnostics.json
```

---

## 25. Decision Summary

Preserve these decisions:

```txt
Runtime streaming consumes runtime-ready assets.
Editor import/cook handles source asset authoring and conversion.
Forge coordinates validation/cook/package workflow.
Saga owns product-level preview/build/publish entry points.
SagaShared owns neutral asset/artifact contracts only.
SDE references asset/artifact ids but does not import/cook assets.
Prism analyzes asset/artifact relationships and stale outputs.
AssetId must be stable and not just a path.
Cooked artifacts must carry hashes, profiles, versions, and dependencies.
Publish must fail on stale or missing required assets.
```

The asset pipeline should make one thing boring and reliable:

```txt
Human-edited source assets become validated runtime-ready artifacts.
```

If that path is boring, the engine can scale.

If that path is clever, hidden, and magical, the engine will spend its life debugging missing textures with a philosophical expression.
