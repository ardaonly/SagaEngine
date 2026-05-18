# Iteration Notes

> Status: Active development note
> Current iteration: `0.0.8-dev.4`
> Purpose: Track the current implementation batch before roadmap, architecture, diagnostics, manifest, and test documents are updated.

---

## 0. Purpose

This document records the current development iteration.

It is not a changelog.

It is not a release note.

It is not a roadmap.

It is not permanent architecture truth.

It is a temporary working note used to capture what changed during the current batch.

Permanent decisions belong in:

```txt
docs/*_ROADMAP.md
docs/DependencyGraph.md
docs/SCHEMA.md
```

This file may be rewritten for each iteration.

---

## 1. Iteration

```txt
Version: 0.0.8-dev.4
Status: Draft
Date: 2026-05-18
```

Short summary:

```txt
Added narrow EditorHost-backed and EditorShell-backed EditorLab scenario adapters, an EditorLab runtime mode selector, a build-flag-gated dev bridge for mounting the Scenario Runner panel in Saga editor mode, a runtime AssetIdentityManifest path for package AssetKey to AssetId resolution, preflight coverage validation, documented AssetId allocation policy, startup gate validation, generator readiness boundary reporting, a narrow AssetPipeline identity allocation boundary scaffold, CMake target-boundary architecture tests for the current target graph, focused FileAssetSource coverage for the existing non-VFS file-backed asset source, an IVirtualFileSystem boundary MVP with native-directory and memory backends, a VFS test coverage gap audit, and a VFS-backed VirtualFileAssetSource adapter.
```

---

## 2. What Was Added

```txt
- EditorHost-backed EditorLab scenario runtime adapter.
- EditorShell-backed EditorLab scenario runtime adapter for registered panel open/close.
- EditorLab Scenario Runner runtime mode model with deterministic and connected modes.
- EditorLab Scenario Runner panel runtime mode selector.
- EditorLab Scenario Runner dock panel wrapper for editor shell mounting.
- SagaEditorLabLib dev-only library target shared by EditorLab and Saga product editor mode.
- SAGA_WITH_EDITORLAB_DEV_PANEL CMake option for optional Saga editor-mode dev panel mounting.
- SagaEditorLabBridge dev-only target for EditorLab panel registration outside SagaProductLib.
- Generic Saga product editor-mode panel provider hook that does not name EditorLab.
- EditorLab view-model and panel adapter injection path.
- Runtime AssetIdentityManifest contract and loader for explicit AssetKey to AssetId mappings.
- Optional PackageManifest assetIdentityManifest field.
- RuntimeAssetRegistryBootstrapper package-identity-manifest wiring path that builds a resolver and delegates to the existing explicit-resolver bootstrap overload.
- RuntimeAssetRegistryBootstrapper identity-backed preflight validation path for package AssetKey to AssetId coverage without registry mutation.
- RuntimeStartupGate identity-backed asset manifest validation path that uses the existing registry bootstrap preflight when assetIdentityManifest is present.
- AssetIdentityManifest contract documentation in AssetStreamingImplementation.md.
- AssetId allocation policy documentation in AssetStreamingImplementation.md before generator/staging implementation.
- AssetIdentityManifest generator readiness and missing-boundary report in AssetStreamingImplementation.md.
- Tools/AssetPipeline SagaAssetPipelineLib target for pure identity allocation code.
- SagaAssetPipeline/Identity AssetIdentityGenerator public API.
- AssetIdentityGenerator deterministic diagnostics for invalid key, duplicate input key, duplicate previous key, duplicate AssetId, invalid AssetId, and AssetId overflow.
- CMake target-boundary architecture tests that guard current SagaTargets.cmake target links without acting as a full CMake parser.
- Target-boundary coverage for SagaProductLib, SagaEditorLabBridge, Saga executable dev-panel bridge links, SagaAssetPipelineLib, and runtime/server targets.
- Unit coverage for EditorHost-backed command dispatch, public editor state reads, diagnostics counts, and injected-adapter scenario execution.
- Unit coverage for shell-backed registered panel open, registered panel close, unknown panel failure, and host-state delegation.
- Unit coverage for runtime mode listing, connected mode selection, unknown mode rejection, connected adapter execution, and deterministic default behavior with injected adapters.
- Unit coverage for the Scenario Runner dock panel stable id/title.
- Unit coverage for AssetIdentityManifest loading, package manifest identity reference validation, identity-backed registry bootstrap atomic failure paths, identity-backed preflight coverage validation, and startup gate identity validation.
- Unit coverage for AssetIdentityGenerator mapping reuse, allocation after max previous id, lexicographic new allocation, deleted-id non-reuse, duplicate input key failure, duplicate previous AssetId failure, AssetId 0 failure, UINT64_MAX overflow failure, and rename-as-new behavior.
- Unit coverage for forbidden CMake target links and the SAGA_WITH_EDITORLAB_DEV_PANEL guard around SagaEditorLabBridge usage.
- Unit coverage for FileAssetSource existing-file reads, missing-file failure, empty resolver path failure, payload metadata, and registry state cleanliness after read failure.
- IVirtualFileSystem runtime content path boundary under SagaEngine::Resources.
- VirtualFileSystem mount table with normalized absolute mount points, longest-prefix dispatch, and deterministic mount failures.
- NativeDirectoryVirtualFileSystemBackend for physical directory-backed content reads.
- MemoryVirtualFileSystemBackend for deterministic tests.
- Unit coverage for virtual path reads, native-directory reads, missing files, invalid virtual paths, unknown mounts, longest-prefix mount selection, duplicate mount protection, and failure non-mutation.
- Additional VFS coverage audit tests for root mount reads, mount-point self-read failure, invalid mount points, null backend rejection, partial mount-prefix rejection, native-directory missing files, memory invalid writes, and backend invalid relative reads.
- VirtualFileAssetSource as a new VFS-backed IAssetSource adapter.
- VirtualPathResolverFn for AssetId + AssetKind to virtual path resolution.
- Unit coverage for VirtualFileAssetSource memory VFS reads, native-directory VFS reads, payload metadata, empty resolver path, missing file, missing mount, invalid path, null resolver, null VFS, cancellation before read, and registry-backed failure non-mutation.
```

---

## 3. What Was Changed

```txt
- EditorLab executable now links SagaEditorLib so the development lab can exercise SagaEditor public services.
- ScenarioRunnerPanelViewModel can run scenarios through an injected runtime adapter while preserving the deterministic default adapter.
- EditorHost adapter maps existing EditorLab profile diff scenario aliases to current public editor profile ids.
- EditorShell adapter composes the EditorHost adapter and routes only OpenPanel/ClosePanel through existing EditorShell/IUIMainWindow public APIs.
- ScenarioRunnerPanelViewModel now exposes explicit runtime mode selection; deterministic remains the default even when a connected adapter is injected.
- Saga editor mode now applies externally registered editor-mode panel providers through the existing product-owned editor shell mount.
- EditorLab Scenario Runner panel registration moved from SagaProductLib/SagaEditorModule into the dev-only SagaEditorLabBridge.
- Saga executable links SagaEditorLabBridge only when SAGA_WITH_EDITORLAB_DEV_PANEL is ON.
- EditorLab executable now consumes SagaEditorLabLib instead of compiling the lab sources directly.
- PackageManifestLoader now accepts a single optional assetIdentityManifest path while preserving legacy manifests when the field is absent.
- Runtime asset registry bootstrap can load the package asset identity manifest and construct a StaticAssetIdResolver before registry mutation.
- Runtime asset registry bootstrap now shares package asset planning between identity-backed validation and registration.
- RuntimeStartupGate now routes asset manifest startup validation through identity-backed preflight when PackageManifest.assetIdentityManifest is present.
- AssetStreamingImplementation.md now documents the identity-backed preflight validation path and current extra-mapping behavior.
- AssetStreamingImplementation.md now documents that startup validation uses identity-backed preflight for packages that reference assetIdentityManifest.
- AssetStreamingImplementation.md now documents PackageManifest.assetIdentityManifest, AssetIdentityManifest schema v1, AssetId > 0 validity, and current non-goals.
- AssetStreamingImplementation.md now documents AssetId allocation ownership, ID reuse rules, new ID assignment order, rename behavior, deleted-ID non-reuse, numeric range, and deterministic failure cases.
- AssetStreamingImplementation.md now documents the missing AssetPipeline/Forge boundaries that block generator implementation.
- AssetStreamingImplementation.md now documents that the pure AssetPipeline identity allocation boundary exists while JSON writing, file I/O, package staging, and Forge orchestration remain missing.
- CMake now discovers Tools/AssetPipeline sources and links SagaAssetPipelineLib into SagaUnitTests.
- Architecture tests now enforce selected CMake target boundaries for product/dev-only bridge, AssetPipeline, runtime, and server targets.
- Runtime test coverage now explicitly exercises current FileAssetSource behavior without adding a VFS abstraction or changing streaming behavior.
- Runtime resources now expose a VFS boundary for normalized virtual content paths without changing FileAssetSource, StreamingManager, package manifests, or asset manifests.
- VFS test coverage now pins the current MVP contract more tightly without changing the VFS public API or implementation behavior.
- Runtime resources now include a VFS-backed asset source adapter without changing FileAssetSource, StreamingManager, AssetRegistry.sourcePath semantics, package manifests, or asset manifests.
- AssetStreamingImplementation.md now documents VirtualFileAssetSource status mapping and the deferred AssetRegistry/sourcePath decision.
- AssetRegistry.sourcePath comments, docs, and runtime bootstrap test naming now explicitly describe the current resolved cooked/runtime path behavior without changing behavior.
```

---

## 4. What Was Removed

```txt
- Removed the direct SagaProductLib -> SagaEditorLabLib link dependency.
- Removed EditorLab includes and ScenarioRunnerDockPanel registration from SagaEditorModule.
- No additional files, APIs, or manifest fields were removed for the AssetId allocation policy slice.
- No JSON writer, file I/O, Forge step, package mutation, runtime dependency, editor integration, deterministic hash fallback, or package format field was added for the AssetPipeline identity allocation slice.
- No public API, runtime behavior, package format, VFS, module/plugin system, or saga_apply_compiler_flags include path behavior was changed for the target-boundary test slice.
- No VFS, content mounting, package backend, module/plugin behavior, public API, runtime behavior, or package format changes were added for the FileAssetSource test slice.
- No asset streaming integration, package backend, .pak format, compression, encryption, async streaming, manifest format, Forge behavior, editor behavior, SDE behavior, or module/plugin behavior was added for the IVirtualFileSystem MVP slice.
- Nothing was removed for the VFS coverage audit slice.
- No FileAssetSource refactor, AssetRegistry.sourcePath semantic change, manifest/package format change, RuntimeStartupGate change, package backend, embedded executable backend, async streaming behavior, Forge/AssetPipeline integration, or NativeDirectory hardening was added for the VirtualFileAssetSource slice.
```

---

## 5. Files Changed

```txt
- Apps/EditorLab/include/SagaEditorLab/Scenario/EditorShellScenarioRuntimeAdapter.h
- Apps/EditorLab/src/SagaEditorLab/EditorShellScenarioRuntimeAdapter.cpp
- Apps/EditorLab/include/SagaEditorLab/Scenario/EditorHostScenarioRuntimeAdapter.h
- Apps/EditorLab/src/SagaEditorLab/EditorHostScenarioRuntimeAdapter.cpp
- Apps/EditorLab/include/SagaEditorLab/UI/ScenarioRunnerDockPanel.h
- Apps/EditorLab/src/SagaEditorLab/ScenarioRunnerDockPanel.cpp
- Apps/EditorLab/include/SagaEditorLab/UI/ScenarioRunnerPanelViewModel.h
- Apps/EditorLab/src/SagaEditorLab/ScenarioRunnerPanelViewModel.cpp
- Apps/EditorLab/src/SagaEditorLab/ScenarioRunnerPanel.h
- Apps/EditorLab/src/SagaEditorLab/ScenarioRunnerPanel.cpp
- Apps/Saga/SagaEditorModule.cpp
- Apps/Saga/SagaEditorModule.h
- Apps/Saga/main.cpp
- Apps/SagaDev/SagaEditorLabBridge.h
- Apps/SagaDev/SagaEditorLabBridge.cpp
- Engine/Public/SagaEngine/Resources/AssetIdentityManifest.h
- Engine/Private/SagaEngine/Resources/AssetIdentityManifestLoader.cpp
- Engine/Public/SagaEngine/Packages/PackageManifest.hpp
- Engine/Private/SagaEngine/Packages/PackageManifestLoader.cpp
- Engine/Public/SagaEngine/Resources/RuntimeAssetRegistryBootstrapper.h
- Engine/Private/SagaEngine/Resources/RuntimeAssetRegistryBootstrapper.cpp
- Engine/Private/SagaEngine/Startup/RuntimeStartupGate.cpp
- Engine/Public/SagaEngine/Resources/VirtualFileSystem.h
- Engine/Private/SagaEngine/Resources/VirtualFileSystem.cpp
- Engine/Public/SagaEngine/Resources/VirtualFileAssetSource.h
- Engine/Private/SagaEngine/Resources/VirtualFileAssetSource.cpp
- Engine/Public/SagaEngine/Resources/AssetRegistry.h
- docs/AssetStreamingImplementation.md
- Tools/AssetPipeline/include/SagaAssetPipeline/Identity/AssetIdentityGenerator.hpp
- Tools/AssetPipeline/src/Identity/AssetIdentityGenerator.cpp
- Tests/Unit/AssetPipeline/AssetIdentityGeneratorTests.cpp
- Tests/Unit/Architecture/CMakeTargetBoundaryTests.cpp
- Tests/Unit/Runtime/FileAssetSourceTests.cpp
- Tests/Unit/Runtime/VirtualFileAssetSourceTests.cpp
- Tests/Unit/Runtime/VirtualFileSystemTests.cpp
- Tests/Unit/Editor/EditorLabScenarioRunnerTests.cpp
- Tests/Unit/Runtime/AssetIdentityManifestLoaderTests.cpp
- Tests/Unit/Runtime/PackageManifestLoaderTests.cpp
- Tests/Unit/Runtime/RuntimeAssetRegistryBootstrapperTests.cpp
- Tests/Unit/Runtime/RuntimeStartupGateTests.cpp
- CMakeLists.txt
- cmake/modules/SagaTargets.cmake
- cmake/modules/SagaTests.cmake
- docs/dev/ITERATION_NOTES.md
```

---

## 6. Ownership / Boundary Notes

```txt
Allowed:
- Apps/EditorLab may consume SagaEditor public APIs as a development lab surface.
- EditorHost-backed adapter uses SagaEditor public services only.
- EditorShell-backed adapter uses existing EditorShell::FindPanel and IUIMainWindow::SetPanelVisible public APIs.
- EditorLab Scenario Runner may expose adapter selection as local development-lab UI state.
- Saga product editor mode may apply generic panel providers through Saga-owned editor mode activation.
- EditorLab panel registration belongs to the optional SagaEditorLabBridge target, not SagaProductLib.
- Default product builds keep SAGA_WITH_EDITORLAB_DEV_PANEL OFF and do not mount the EditorLab panel.
- Runtime may consume package asset identity manifests and asset manifests as package artifacts.
- AssetIdentityManifest loading belongs to runtime package consumption; no asset import/cook/generator ownership moved into runtime.
- Identity-backed package asset coverage validation belongs to runtime package consumption and does not stage, cook, generate, or publish assets.
- AssetId allocation policy assigns future generation ownership to AssetPipeline/package generation and keeps runtime as a consumer only.
- RuntimeStartupGate may consume identity-backed package asset validation as startup preflight without mutating runtime asset registry state.
- AssetIdentityManifest generator readiness report keeps Forge as orchestrator and blocks implementation until AssetPipeline/package generation ownership exists.
- AssetPipeline may own pure identity allocation policy under Tools/AssetPipeline without depending on runtime, editor, Forge, Prism, SDE, scripting, or product internals.
- SagaAssetPipelineLib currently uses only standard library primitives and does not include SagaEngine runtime headers.
- AssetIdentityGenerator is an in-memory boundary only; it does not write manifests, mutate packages, invoke Forge, or load files.
- SagaProductLib must not link SagaEditorLabLib or SagaEditorLabBridge directly.
- SagaEditorLabBridge and Saga's SagaEditorLabBridge link must remain behind SAGA_WITH_EDITORLAB_DEV_PANEL.
- SagaAssetPipelineLib must not link SagaRuntimeLib, SagaEditorLib, SagaProductLib, Forge, Prism, or SDE targets.
- Runtime and server role targets must not link editor, dev-only, product, AssetPipeline, Forge, Prism, or SDE targets.
- FileAssetSource tests cover the existing file-backed runtime asset source only; this slice does not introduce VFS ownership, mount tables, package mounting, or alternate storage backends.
- IVirtualFileSystem belongs to SagaEngine::Resources as a runtime content access boundary.
- IVirtualFileSystem owns normalized virtual path validation, mount-point lookup, and backend dispatch.
- NativeDirectoryVirtualFileSystemBackend and MemoryVirtualFileSystemBackend are runtime resource backends only; they do not own package format, package staging, cooking, editor UX, Forge orchestration, or asset identity generation.
- VFS coverage audit is test-only and does not move responsibilities between runtime resources, package loading, Forge, AssetPipeline, editor, SDE, or product code.
- VirtualFileAssetSource belongs to SagaEngine::Resources and is an IAssetSource adapter over IVirtualFileSystem.
- VirtualFileAssetSource consumes virtual paths from an injected resolver; it does not decide whether AssetRegistry.sourcePath is a virtual path.
- VirtualFileAssetSource maps VFS read statuses into existing StreamingStatus values without adding a new streaming status contract.

Forbidden:
- Preserve existing DependencyGraph.md ownership boundaries.
- Do not add new EditorShell public panel-control APIs in this slice.
- Do not add FocusPanel behavior in this slice.
- Do not add project lifecycle, runtime/server, Forge, Prism, SDE, or Qt docking backend behavior in this slice.
- Do not make EditorLab own product lifecycle or production publish behavior.
- Keep SDE standalone.
- Keep Forge as an orchestrator only.
- Keep Prism as an analyzer only.
- Keep runtime/server as manifest/artifact consumers.
- Keep editor as authoring UX only.
- Keep SagaShared implementation-free.
- Do not turn CMakeTargetBoundaryTests into a general-purpose CMake parser.
- Do not change saga_apply_compiler_flags include directory behavior in this slice.
- Do not integrate IVirtualFileSystem into FileAssetSource, StreamingManager, runtime startup, package loading, editor, Forge, SDE, or module/plugin systems in this slice.
- Do not change FileAssetSource, AssetRegistry.sourcePath semantics, asset manifests, package manifests, RuntimeStartupGate, Forge, AssetPipeline, editor, SDE, package backend, embedded executable backend, async streaming, or NativeDirectory hardening in the VirtualFileAssetSource slice.
```

---

## 7. Diagnostics / Manifests / Reports

Diagnostics:

```txt
- No new diagnostic contract added.
- EditorHost-backed adapter can read existing editor diagnostics service counts for scenario assertions.
- Unknown shell panel operations return deterministic scenario failures; no new diagnostics sink was added.
- Runtime mode selection adds no manifests, reports, or diagnostics contracts.
- Build-flag-gated Scenario Runner mounting adds no new diagnostics contract.
- AssetIdentityManifestLoader adds Runtime.AssetIdentityManifest.* diagnostics for package identity manifest parse/validation failures.
- RuntimeAssetRegistryBootstrapper adds Runtime.AssetRegistryBootstrap.MissingAssetIdentityManifest for identity-backed bootstrap calls without the optional field.
- Identity-backed preflight validation reuses existing Runtime.AssetIdentityManifest.*, Runtime.AssetRegistryAdapter.*, and Runtime.AssetRegistryBootstrap.* diagnostics.
- RuntimeStartupGate normalizes identity-backed preflight diagnostics into existing RuntimeStartupGateDiagnostic fields.
- AssetId allocation policy documentation adds no diagnostics contract.
- AssetIdentityManifest generator readiness report adds no diagnostics contract.
- AssetIdentityGenerator adds AssetPipeline.AssetIdentity.* deterministic generation diagnostics inside the AssetPipeline tool boundary.
- CMake target-boundary tests add no runtime diagnostics contract.
- FileAssetSource tests assert existing AssetNotFound behavior and diagnostic text; no new diagnostics contract was added.
- IVirtualFileSystem returns deterministic in-process result statuses and messages, but no new diagnostics sink or report contract was added.
- VFS coverage audit adds no diagnostics contract.
- VirtualFileAssetSource returns deterministic AssetLoadResult status and diagnostic strings, but no new diagnostics sink or report contract was added.
```

Manifests:

```txt
- Added runtime AssetIdentityManifest schema version 1 for explicit package AssetKey to AssetId mappings.
- Added optional PackageManifest assetIdentityManifest field.
- Did not add assetIdentityManifests list or multi-manifest merge semantics.
- Did not add generator, staging, or package format expansion for identity coverage validation.
- Documented AssetId allocation policy without changing the AssetIdentityManifest schema.
- Documented generator readiness gaps without changing manifest schema or package format.
- Added an in-memory AssetPipeline identity allocation API without changing AssetIdentityManifest schema or package format.
- Did not add an AssetIdentityManifest JSON writer, generated file location, or Forge package staging path.
- Target-boundary tests add no manifest or package format changes.
- FileAssetSource tests add no manifest or package format changes.
- IVirtualFileSystem adds no manifest or package format changes.
- VFS coverage audit adds no manifest or package format changes.
- VirtualFileAssetSource adds no manifest or package format changes.
```

Reports:

```txt
- None added.
```

---

## 8. Verification

```txt
Command:
Tools/Forge/bin/forge nix build -j 1

Command:
build/RelWithDebInfo/SagaUnitTests --gtest_filter=AssetIdentityManifestLoaderTests.*:PackageManifestLoaderTests.*:AssetIdResolverTests.*:AssetManifestRegistryAdapterTests.*:RuntimeAssetRegistryBootstrapperTests.*

Command:
build/RelWithDebInfo/SagaUnitTests --gtest_filter=RuntimeStartupGateTests.*:AssetIdentityManifestLoaderTests.*:PackageManifestLoaderTests.*:AssetIdResolverTests.*:AssetManifestRegistryAdapterTests.*:RuntimeAssetRegistryBootstrapperTests.*

Command:
rg "assetIdentityManifest|AssetIdentityManifest|greater than zero|multi-manifest|deterministic hash fallback" docs/AssetStreamingImplementation.md docs/dev/ITERATION_NOTES.md

Command:
rg "AssetId allocation|UINT64_MAX|max\\(existing AssetId\\) \\+ 1|deleted asset IDs are not reused|AssetKey rename|deterministic hash fallback" docs/AssetStreamingImplementation.md docs/dev/ITERATION_NOTES.md

Command:
rg "RuntimeStartupGate|startup validation uses the identity-backed preflight|Runtime startup/package identity focused tests passed: 79 tests" docs/AssetStreamingImplementation.md docs/dev/ITERATION_NOTES.md

Command:
rg "AssetPipelineAdapter|AssetIdentityManifest generator|manifest writer|AssetValidateStep|AssetCookStep|AssetPackageStep" docs/AssetStreamingImplementation.md docs/dev/ITERATION_NOTES.md

Command:
Tools/Forge/bin/forge nix configure -- -DSAGA_WITH_EDITORLAB_DEV_PANEL=ON

Command:
Tools/Forge/bin/forge nix build -j 1

Command:
build/RelWithDebInfo/SagaUnitTests --gtest_filter=EditorLabScenarioRunnerTests.*:SagaProductHostTest.*:EditorModeTest.*

Command:
Tools/Forge/bin/forge nix configure -- -DSAGA_WITH_EDITORLAB_DEV_PANEL=OFF

Command:
Tools/Forge/bin/forge nix build -j 1

Command:
build/RelWithDebInfo/SagaUnitTests --gtest_filter=EditorLabScenarioRunnerTests.*

Command:
build/RelWithDebInfo/SagaUnitTests --gtest_filter=EditorLabScenarioRunnerTests.*:SagaProductHostTest.*:EditorModeTest.*

Command:
Tools/Forge/bin/forge nix build -j 1

Command:
build/RelWithDebInfo/SagaUnitTests --gtest_filter=AssetIdentityGeneratorTests.*

Command:
build/RelWithDebInfo/SagaArchitectureTests

Command:
rg "#include .*SagaEngine|#include .*SagaEditor|#include .*Forge|#include .*Prism|#include .*SDE|#include .*Runtime|#include .*Qt" Tools/AssetPipeline

Command:
rg "SagaAssetPipelineLib|AssetIdentityGenerator|AssetPipeline.AssetIdentity|JSON writer|Forge AssetPipelineAdapter" docs/AssetStreamingImplementation.md docs/dev/ITERATION_NOTES.md

Command:
git diff --check

Command:
Tools/Forge/bin/forge nix build -j 1

Command:
build/RelWithDebInfo/SagaArchitectureTests --gtest_filter=CMakeTargetBoundaryTests.*

Command:
build/RelWithDebInfo/SagaArchitectureTests

Command:
Tools/Forge/bin/forge nix build -j 1

Command:
build/RelWithDebInfo/SagaUnitTests --gtest_filter=FileAssetSourceTests.*:AssetRegistryTests.*

Command:
Tools/Forge/bin/forge nix build -j 1

Command:
build/RelWithDebInfo/SagaUnitTests --gtest_filter=VirtualFileSystemTests.*:FileAssetSourceTests.*

Command:
Tools/Forge/bin/forge nix build -j 1

Command:
build/RelWithDebInfo/SagaUnitTests --gtest_filter=VirtualFileSystemTests.*:FileAssetSourceTests.*

Command:
Tools/Forge/bin/forge nix build -j 1

Command:
build/RelWithDebInfo/SagaUnitTests --gtest_filter=VirtualFileAssetSourceTests.*:VirtualFileSystemTests.*:FileAssetSourceTests.*

Command:
git status --short

Command:
git diff --stat

Command:
git diff --check

Command:
Tools/Forge/bin/forge nix build -j 1

Command:
build/RelWithDebInfo/SagaUnitTests --gtest_filter=VirtualFileAssetSourceTests.*:VirtualFileSystemTests.*:FileAssetSourceTests.*

Command:
build/RelWithDebInfo/SagaUnitTests --gtest_filter=AssetIdentityManifestLoaderTests.*:PackageManifestLoaderTests.*:AssetIdResolverTests.*:AssetManifestRegistryAdapterTests.*:RuntimeAssetRegistryBootstrapperTests.*:RuntimeStartupGateTests.*

Command:
build/RelWithDebInfo/SagaArchitectureTests

Command:
build/RelWithDebInfo/SagaUnitTests --gtest_filter=AssetIdentityGeneratorTests.*:EditorLabScenarioRunnerTests.*

Result:
Passed

Notes:
Full build completed.
Runtime/package AssetIdentityManifest focused tests passed: 60 tests.
Runtime startup/package identity focused tests passed: 79 tests.
AssetIdentityManifest contract documentation static grep passed.
AssetId allocation policy static grep passed.
Runtime startup identity validation static grep passed.
AssetIdentityManifest generator readiness static grep passed.
Dev-panel ON build completed with SagaEditorLabBridge linked into Saga.
Default/OFF build completed with SagaEditorLabBridge excluded from Saga.
Targeted EditorLab scenario tests passed: 29 tests.
Targeted product/editor mount boundary tests passed: 44 tests in both ON and OFF configurations.
AssetPipeline identity allocation build completed.
AssetIdentityGenerator focused tests passed: 9 tests.
Architecture boundary tests passed: 11 tests.
AssetPipeline forbidden include static grep found no matches.
AssetPipeline identity documentation static grep passed.
Git diff whitespace check passed.
CMake target-boundary build completed.
CMakeTargetBoundaryTests focused tests passed: 5 tests.
Architecture boundary tests passed after target-boundary addition: 16 tests.
FileAssetSource build completed.
FileAssetSource and AssetRegistry focused tests passed: 11 tests.
IVirtualFileSystem MVP build completed.
VirtualFileSystem and FileAssetSource focused tests passed: 13 tests.
VFS coverage audit build completed.
VirtualFileSystem and FileAssetSource focused tests passed after coverage audit: 21 tests.
VirtualFileAssetSource build completed.
VirtualFileAssetSource, VirtualFileSystem, and FileAssetSource focused tests passed: 31 tests.
Pre-commit git status and diff stat were reviewed before commit.
Pre-commit git diff whitespace check passed.
Post-sourcePath alignment build completed with Forge/Nix -j 1.
Post-sourcePath alignment VirtualFileAssetSource, VirtualFileSystem, and FileAssetSource focused tests passed: 31 tests.
Post-sourcePath alignment runtime/package identity, registry bootstrap, and startup gate focused tests passed: 79 tests.
Post-sourcePath alignment architecture tests passed: 16 tests.
Post-sourcePath alignment AssetIdentityGenerator and EditorLab scenario focused tests passed: 38 tests.

Not tested:
- Full test suite beyond the focused EditorLab, SagaProductHost, EditorMode, runtime/package manifest, AssetIdentityGenerator, CMake target-boundary, architecture, FileAssetSource, VirtualFileAssetSource, AssetRegistry, and VirtualFileSystem coverage.
- Manual Saga GUI launch/open-project flow.
- AssetIdentityManifest JSON writer or generated file output.
- Forge AssetPipelineAdapter, AssetValidateStep, AssetCookStep, AssetPackageStep, or package staging behavior.
- Negative failure-output mutation test for intentionally broken CMake target links.
- VFS package backend, .pak files, compression, encryption, content package mounting, async streaming, cancellation after VFS read, file size limit failure paths, StreamingManager integration, FileAssetSource VFS integration, AssetRegistry.sourcePath virtual path semantics, native-directory symlink escape behavior, and canonical root containment behavior.
```

---

## 9. Roadmaps To Update

```txt
[x] EDITOR_ROADMAP.md
[x] SHARED_ROADMAP.md
[x] ENGINE_ROADMAP.md
[ ] SDE_ROADMAP.md
[x] FORGE_ROADMAP.md
[ ] PRISM_ROADMAP.md
[ ] COLLABORATION_ROADMAP.md
[x] TOOLS_ROADMAP.md
[x] DependencyGraph.md
[x] DIAGNOSTICS_ROADMAP.md
[x] ASSET_PIPELINE_ROADMAP.md
[ ] SAGA_SCRIPTING_ROADMAP.md
[x] BUILD_PUBLISH_PIPELINE_ROADMAP.md
[x] AssetStreamingImplementation.md
[ ] SCHEMA.md
[x] Other: SAGA_PRODUCT_ROADMAP.md
```

Reason:

```txt
Affected by implementation scope only; roadmap files were not rewritten.
EDITOR_ROADMAP.md is affected because EditorLab now has public SagaEditor-backed scenario adapters, a shell-backed panel visibility adapter, explicit runtime mode selection, and a dockable dev scenario panel.
SAGA_PRODUCT_ROADMAP.md is affected because Saga product editor mode can mount the dev-only Scenario Runner panel only through the SAGA_WITH_EDITORLAB_DEV_PANEL bridge path.
DIAGNOSTICS_ROADMAP.md is affected because the adapter reads existing editor diagnostics service state.
DependencyGraph.md was checked because the change touches an app-to-editor ownership boundary and removes the direct SagaProductLib -> SagaEditorLabLib dependency.
DependencyGraph.md is affected because the new CMake target-boundary tests partially implement its CI enforcement direction for target dependency checks.
ENGINE_ROADMAP.md is affected because runtime package consumption now has an identity-manifest-backed asset registry bootstrap path, preflight coverage validation path, startup gate integration, focused coverage for the existing file-backed asset source, an initial runtime VFS boundary for virtual content paths, expanded VFS boundary test coverage, and a VFS-backed asset source adapter.
ASSET_PIPELINE_ROADMAP.md is affected because AssetId identity is now represented and coverage-validated as a runtime package manifest artifact, and a narrow pure identity allocation boundary now exists under Tools/AssetPipeline.
BUILD_PUBLISH_PIPELINE_ROADMAP.md is affected because package manifests can now reference the asset identity manifest consumed and preflight-validated at runtime startup.
SHARED_ROADMAP.md is affected because AssetId remains a candidate neutral shared contract, but no shared implementation move was made.
TOOLS_ROADMAP.md is affected because the AssetPipeline tool ownership boundary now has an initial SagaAssetPipelineLib target.
FORGE_ROADMAP.md is affected because the generator readiness report identifies missing Forge adapter, step, and manifest writer boundaries; no Forge behavior was added.
AssetStreamingImplementation.md is affected and now documents the package identity manifest contract, the IVirtualFileSystem MVP boundary, and VirtualFileAssetSource status mapping.
SCHEMA.md is not updated because docs/SCHEMA.md does not exist in this repository.
```

---

## 10. Known Problems

```txt
- EditorHost public services still do not expose project load or user override applied state; the adapter returns explicit failures or missing state for those paths instead of inventing behavior.
- FocusPanel is intentionally not part of the first shell-backed adapter slice.
- Connected runtime mode is available only when a caller injects an adapter; standalone EditorLab remains deterministic by default.
- The Scenario Runner panel is available only through the optional SAGA_WITH_EDITORLAB_DEV_PANEL bridge; it is not production publish workflow UX.
- AssetIdentityManifest is currently manually authored in runtime tests; the AssetPipeline identity allocation API does not write JSON files yet.
- AssetIdentityGenerator enforces the in-memory allocation policy, but no generator executable/service entrypoint exists yet.
- AssetIdentityManifest file generation remains blocked on an AssetPipeline JSON writer or tool entrypoint and later Forge orchestration boundaries.
- Identity-backed bootstrap requires callers to opt into RegisterPackageAssetsFromPackageIdentityManifest; existing explicit-resolver bootstrap remains the legacy path.
- Identity-backed coverage validation is automatic through RuntimeStartupGate only when assetIdentityManifest is present and referenced manifest validation is enabled; legacy package manifest loading remains backward compatible.
- docs/SCHEMA.md does not exist; manifest contract documentation currently lives in AssetStreamingImplementation.md until a schema documentation home is explicitly introduced.
- Existing build warnings remain outside this implementation slice.
- saga_apply_compiler_flags still adds broad Engine/Backends/Runtime include directories to every target that uses it; this target-boundary slice intentionally records but does not fix that include path debt.
- IVirtualFileSystem exists as a boundary MVP, but it is not integrated into FileAssetSource, StreamingManager, package startup, package manifests, or asset manifests yet.
- VirtualFileAssetSource exists but is not wired into StreamingManager construction, RuntimeStartupGate, package loading, asset manifests, or product runtime startup yet.
- AssetRegistry.sourcePath is still not defined as a virtual path; the adapter currently relies on caller-provided resolver policy.
- NativeDirectoryVirtualFileSystemBackend rejects relative path escape segments, but canonical root containment and symlink escape behavior are not implemented or tested yet.
```

---

## 11. Next Actions

```txt
[x] Decide the next implementation slice.
[x] If continuing EditorLab, add a narrow EditorHost-backed adapter using only SagaEditor public APIs.
[x] If continuing EditorLab, mount a real EditorShell-backed adapter only after defining the public shell/panel scenario boundary.
[x] If continuing EditorLab, decide whether scenario UI should expose adapter selection/runtime mode or keep deterministic execution as the only default UI path.
[x] If continuing EditorLab, define the product-owned mounting path that can provide a connected adapter without changing standalone defaults.
[x] If continuing runtime package work, decide the production AssetKey to AssetId mapping source before automatic registry bootstrap.
[x] If continuing product/editor work, decide whether dev-only Scenario Runner panel visibility should be profile-gated or build-flag-gated.
[ ] If continuing product/editor work, add focused coverage for editor-mode panel provider registration/clearing if this hook becomes a stable extension point.
[x] If continuing runtime package work, add package asset coverage validation for assetIdentityManifest before generator/staging behavior.
[x] If continuing runtime package work, scaffold the AssetPipeline/package-generation owner boundary for AssetIdentityManifest identity allocation.
[x] If continuing architecture work, add narrow CMake target dependency checks for current high-risk product/dev/tool target boundaries.
[ ] If continuing architecture work, narrow saga_apply_compiler_flags include directory behavior per target ownership without breaking existing builds.
[ ] If continuing runtime package work, add AssetPipeline AssetIdentityManifest JSON writer or generator entrypoint before Forge staging.
[ ] If continuing runtime package work, add Forge generation or staging for assetIdentityManifest only after the AssetPipeline writer/entrypoint exists.
[x] If continuing runtime package work, document the AssetIdentityManifest contract before adding generator/staging behavior.
[x] If continuing runtime package work, document AssetId allocation policy before adding generator/staging behavior.
[x] If continuing runtime package work, integrate identity-backed preflight validation into runtime startup validation.
[x] If continuing runtime package work, document AssetIdentityManifest generator readiness gaps before generator/staging behavior.
[x] If checking current asset-source behavior, add focused FileAssetSource tests before introducing any VFS boundary.
[x] If continuing VFS work, define the IVirtualFileSystem ownership and mounting boundary before implementing package mounting.
[x] If continuing VFS work, audit current VFS test coverage before planning VirtualFileAssetSource.
[x] If continuing VFS work, add a VFS-backed IAssetSource adapter without changing FileAssetSource.
[ ] If continuing VFS work, decide how AssetRegistry.sourcePath or a future asset location field maps to virtual paths before runtime/package integration.
[ ] If continuing VFS work, decide whether FileAssetSource should become a VFS consumer or remain a separate native IAssetSource before integrating streaming.
```

---

## 12. Roadmap Update Request

Use this block when updating roadmaps from this iteration.

```txt
Read docs/dev/ITERATION_NOTES.md.

Update only the roadmap files marked in section 9.

Rules:
- Do not rewrite unrelated sections.
- Do not mark unverified work as shipped.
- If status is Draft or Partially Implemented, keep roadmap items open.
- Preserve DependencyGraph.md ownership boundaries.
- Keep SDE standalone.
- Keep Forge as orchestrator only.
- Keep Prism as analyzer only.
- Keep runtime/server as manifest/artifact consumers.
- Keep editor as authoring UX only.
- Keep SagaShared implementation-free.
```

---

## 13. Next Iteration

```txt
Next iteration: 0.0.8-dev.5

Possible focus:
- Narrow saga_apply_compiler_flags include directory behavior so tool targets do not receive unrelated Engine/Backends/Runtime include paths by default.
- Add an AssetPipeline-owned AssetIdentityManifest JSON writer around the existing identity allocation API.
- Add an AssetPipeline generator executable/service entrypoint after the writer contract is explicit.
- Add Forge manifest writer/package staging boundary only after the AssetPipeline writer or entrypoint exists.
- Product/editor panel-provider test coverage if the generic hook becomes a stable extension point.
- Product-facing runtime/server child diagnostic capture after a narrow child diagnostic contract is defined.
- Decide AssetRegistry.sourcePath virtual path semantics or introduce a separate asset-location contract before wiring VirtualFileAssetSource into runtime/package startup.
```
