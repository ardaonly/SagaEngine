# Iteration Notes

> Status: Active development note
> Current iteration: `0.0.8-1`
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
````

This file may be rewritten for each iteration.

---

## 1. Iteration

```txt
Version: 0.0.8-1
Status: Draft
Date: 2026-05-16
```

Short summary:

```txt
0.0.8-dev.1 runtime artifact, package, and asset manifest validation slice.
```

---

## 2. What Was Added

```txt
- Added runtime artifact manifest value types under Engine/Public/SagaEngine/Artifacts.
- Added ArtifactManifestLoader for minimal JSON manifest loading and validation.
- Added Runtime.Artifact.* structured loader diagnostics.
- Added artifact manifest fixtures and focused unit coverage.
- Added ArtifactManifestLoadOptions for optional referenced artifact file validation.
- Added focused ArtifactManifestLoader coverage for malformed JSON fields and artifact entries.
- Added ArtifactStartupValidator for runtime startup artifact manifest acceptance policy.
- Added focused ArtifactStartupValidator coverage for startup success, loader failures, duplicate ids, path policy, and missing referenced files.
- Added runtime asset manifest value types and AssetManifestLoader under Engine/Public/SagaEngine/Assets.
- Added Runtime.Asset.* structured loader diagnostics.
- Added runtime package manifest value types, PackageManifestLoader, and PackageStartupValidator under Engine/Public/SagaEngine/Packages.
- Added Runtime.Package.* structured loader diagnostics and startup policy diagnostics.
- Added focused unit coverage for runtime asset manifest loading, package manifest loading, and package startup validation.
- Added RuntimeStartupGate as a thin startup validation orchestrator under Engine/Public/SagaEngine/Startup.
- Added optional SagaRuntime and SagaServer --package-manifest startup validation hooks.
- Added focused RuntimeStartupGate unit coverage for client/server domains, compatibility policy, referenced manifest validation, asset file validation policy, relative path resolution, accumulated diagnostics, and resolved diagnostic paths.
- Added SagaAppConfig in-memory --package-manifest parsing for Saga product startup preparation.
- Added Saga product handoff of --package-manifest arguments to prepared runtime/server targets.
- Added focused Saga product tests for package manifest config parsing, runtime/server target requirements, and prepare-only argument visibility.
- Added narrow Saga product startup/preparation diagnostic types for target, phase, diagnosticId, message, and optional path.
- Added SagaTargetPreparationResult for product-level target preparation failures without introducing a shared diagnostics framework.
- Added deterministic Saga product diagnostics for missing --package-manifest on runtime/server targets.
- Added focused Saga product tests for product diagnostics and missing package manifest diagnostic output.
- Added SagaProcessLauncher as a product-local runtime/server process launch boundary.
- Added product launch diagnostics for failed process start and failed child process exit.
- Added focused Saga product tests for runtime/server launch handoff, prepare-only no-launch behavior, launch arguments, launch failure diagnostics, exit code reporting, and editor no-launch behavior.
- Added AssetIdResolver as an explicit AssetKey to AssetId resolution boundary for runtime assets.
- Added StaticAssetIdResolver for focused tests and future explicit package/manifest handoff code.
- Added AssetRegistry assetKey metadata, checked TryInsert result, and FindByKey lookup for canonical manifest keys.
- Added focused AssetIdResolver and AssetRegistry tests for dotted asset keys, explicit id resolution, missing mappings, invalid ids, duplicate resolved ids, duplicate registry ids/keys, and old JSON registry compatibility.
- Added AssetManifestRegistryAdapter as a narrow runtime adapter from already parsed AssetManifest data into AssetRegistry entries.
- Added Runtime.AssetRegistryAdapter.* deterministic diagnostics for invalid keys, duplicate manifest keys, resolver failures, id collisions, registry collisions, and unsupported kinds.
- Added focused AssetManifestRegistryAdapter coverage for valid registration, path resolution, duplicate/collision handling, unsupported kinds, no file-content loading, no package manifest dependency, and disk size compatibility behavior.
- Added RuntimeAssetRegistryBootstrapper as a package-level runtime asset registry bootstrap layer separate from RuntimeStartupGate.
- Added AssetManifestRegistryAdapter planning support so package bootstrap can preflight registry entries without mutating AssetRegistry.
- Added AssetRegistry all-or-nothing batch insertion for package bootstrap and registry tests.
- Added Runtime.AssetRegistryBootstrap.* diagnostics for package-wide duplicate AssetKeys, duplicate resolved AssetIds, and existing registry collisions.
- Added focused RuntimeAssetRegistryBootstrapper tests for client/server packages, multi-manifest atomic registration, package-relative refs, asset path resolution, asset file validation policy, manifest loader failures, duplicate/collision handling, and no partial registry mutation.
```

Use this section for newly added systems, files, modules, commands, manifests, reports, diagnostics, contracts, or tests.

Example:

```txt
- Added initial ArtifactManifestLoader.
- Added Runtime.Artifact.* diagnostics.
- Added Build/Manifests/sde_artifacts.example.json fixture.
```

---

## 3. What Was Changed

```txt
- Adapted the requested artifact manifest loader paths to the repository's enforced Engine/Public and Engine/Private layout.
- Kept nlohmann_json private to the loader implementation instead of exposing it through public headers.
- Changed ArtifactManifestLoader to optionally validate referenced artifact file existence without changing default LoadFromFile behavior.
- Changed ArtifactManifestLoader diagnostics to distinguish missing fields from invalid field types or shapes.
- Changed runtime artifact startup validation to resolve artifact references relative to the manifest parent directory as the MVP artifact root.
- Kept package and asset manifest parsing on JSON v1 with nlohmann_json private to implementation files.
- Kept existing Resources/AssetRegistry behavior unchanged; production asset manifest loading is a separate runtime validation surface.
- Changed AssetManifestError to carry resolvedPath for file-missing diagnostics.
- Changed SagaRuntime to log info and continue when --package-manifest is omitted as a temporary dev compatibility bridge.
- Changed SagaServer to log a warning and continue when --package-manifest is omitted as a temporary dev compatibility bridge.
- Changed startup package validation failures to log every normalized diagnostic and exit before runtime/server host, networking, platform/backend, or execution initialization.
- Changed Saga prepare-only runtime/server target preparation to require an explicit --package-manifest path before preparing the role handoff.
- Changed missing Saga product package manifest errors to include the affected target name.
- Changed Saga prepare-only output to print prepared arguments as individual argument= lines.
- Changed missing Saga product package manifest failures to emit structured product diagnostic lines instead of ad hoc error text.
- Changed prepared runtime/server targets to point at SagaRuntime and SagaServer executables instead of the Saga product executable.
- Changed runtime/server non-prepare Saga product startup to launch the prepared process and report launch.exitCode deterministically.
- Kept --prepare-only as preparation-only output with no process launch.
- Changed the planned AssetManifest to AssetRegistry adapter into an asset identity bridge slice because AssetManifestEntry::id is a string AssetKey while AssetRegistry uses numeric AssetId.
- Kept AssetManifest JSON unchanged and kept dotted string ids as canonical manifest AssetKeys.
- Kept AssetRegistry numeric AssetId lookup unchanged for runtime streaming hot paths.
- Kept AssetRegistry LoadFromJson backward compatible with the existing numeric id JSON format.
- Changed the AssetManifest to AssetRegistry adapter from a deferred item into a small runtime consumption bridge that requires an explicit IAssetIdResolver before registry mutation.
- Interpreted AssetManifestEntry::path/sourcePath semantics as the runtime cooked-loadable path because AssetManifestLoader validates referenced runtime asset files using that path; adapter tests preserve the resolved runtime path in AssetRegistryEntry::sourcePath.
- Kept AssetRegistryEntry::diskSizeBytes at its default 0 during adapter registration; memoryEstimateBytes is intentionally not mapped to diskSizeBytes.
- Changed AssetManifestRegistryAdapter registration to use a planning step plus AssetRegistry batch insertion so registration remains all-or-nothing.
- Kept RuntimeStartupGate validation-only; package asset registry bootstrap is a separate caller-driven runtime resource preparation step.
- Kept package and asset manifest schemas unchanged; RuntimeAssetRegistryBootstrapper requires a caller-supplied IAssetIdResolver and does not define a production identity mapping source.
```

Use this section for modified behavior.

Example:

```txt
- Changed runtime startup to validate package manifests earlier.
- Changed asset diagnostics to include package/artifact context.
```

---

## 4. What Was Removed

```txt
- Nothing removed.
```

Use this section for deleted behavior, files, shortcuts, old assumptions, or deprecated paths.

Example:

```txt
- Removed direct source asset fallback from shipping runtime path.
```

---

## 5. Files Changed

```txt
Engine/Public/SagaEngine/Artifacts/ArtifactKind.hpp
Engine/Public/SagaEngine/Artifacts/ArtifactRef.hpp
Engine/Public/SagaEngine/Artifacts/ArtifactManifest.hpp
Engine/Public/SagaEngine/Artifacts/ArtifactManifestLoader.hpp
Engine/Public/SagaEngine/Artifacts/ArtifactStartupValidator.hpp
Engine/Private/SagaEngine/Artifacts/ArtifactManifestLoader.cpp
Engine/Private/SagaEngine/Artifacts/ArtifactStartupValidator.cpp
Engine/tests/fixtures/artifacts/valid_artifact_manifest.json
Engine/tests/fixtures/artifacts/invalid_missing_path_manifest.json
Tests/Unit/Runtime/ArtifactManifestLoaderTests.cpp
Tests/Unit/Runtime/ArtifactStartupValidatorTests.cpp
Engine/Public/SagaEngine/Assets/AssetManifest.hpp
Engine/Public/SagaEngine/Assets/AssetManifestLoader.hpp
Engine/Private/SagaEngine/Assets/AssetManifestLoader.cpp
Engine/Public/SagaEngine/Resources/AssetIdResolver.h
Engine/Private/SagaEngine/Resources/AssetIdResolver.cpp
Engine/Public/SagaEngine/Resources/AssetManifestRegistryAdapter.h
Engine/Private/SagaEngine/Resources/AssetManifestRegistryAdapter.cpp
Engine/Public/SagaEngine/Resources/RuntimeAssetRegistryBootstrapper.h
Engine/Private/SagaEngine/Resources/RuntimeAssetRegistryBootstrapper.cpp
Engine/Public/SagaEngine/Resources/AssetRegistry.h
Engine/Private/SagaEngine/Resources/AssetRegistry.cpp
Engine/Public/SagaEngine/Packages/PackageManifest.hpp
Engine/Public/SagaEngine/Packages/PackageManifestLoader.hpp
Engine/Public/SagaEngine/Packages/PackageStartupValidator.hpp
Engine/Private/SagaEngine/Packages/PackageManifestLoader.cpp
Engine/Private/SagaEngine/Packages/PackageStartupValidator.cpp
Engine/Public/SagaEngine/Startup/RuntimeStartupGate.hpp
Engine/Private/SagaEngine/Startup/RuntimeStartupGate.cpp
Apps/Runtime/main.cpp
Apps/Server/main.cpp
Apps/Saga/SagaAppConfig.h
Apps/Saga/SagaAppConfig.cpp
Apps/Saga/SagaSessionModel.h
Apps/Saga/SagaSessionModel.cpp
Apps/Saga/SagaProcessLauncher.h
Apps/Saga/SagaProcessLauncher.cpp
Apps/Saga/SagaProductHost.h
Apps/Saga/SagaProductHost.cpp
Apps/Saga/SagaApp.cpp
Apps/Saga/SagaApp.h
Tests/Unit/Runtime/AssetManifestLoaderTests.cpp
Tests/Unit/Runtime/AssetManifestRegistryAdapterTests.cpp
Tests/Unit/Runtime/AssetIdResolverTests.cpp
Tests/Unit/Runtime/AssetRegistryTests.cpp
Tests/Unit/Runtime/RuntimeAssetRegistryBootstrapperTests.cpp
Tests/Unit/Runtime/PackageManifestLoaderTests.cpp
Tests/Unit/Runtime/RuntimeStartupGateTests.cpp
Tests/Unit/Saga/SagaProductTests.cpp
docs/dev/ITERATION_NOTES.md
```

---

## 6. Ownership / Boundary Notes

```txt
Allowed:
- Runtime/server may read artifact manifests as manifest/artifact consumers.
- Runtime artifact manifest loading may validate manifest shape, schema version, required fields, and artifact kind tokens.
- Runtime artifact manifest loading may optionally validate that referenced artifact files exist.
- Runtime artifact manifest diagnostics may classify malformed existing fields separately from missing fields.
- Runtime startup validation may apply runtime acceptance policy after loader parsing succeeds.
- Runtime startup validation may reject duplicate artifact ids, empty ids or paths, absolute paths, escaping relative paths, and missing referenced files.
- Runtime/server may read package and asset manifests as manifest/artifact consumers.
- Runtime package manifest loading may validate JSON v1 shape, package kind, manifest refs, path policy, duplicate ids, and referenced manifest existence.
- Runtime asset manifest loading may validate JSON v1 shape, asset kind, path policy, duplicate ids, dependency shape, and referenced cooked asset existence.
- Runtime package startup validation may reject wrong package kind and runtime compatibility token mismatches when policy options request those checks.
- RuntimeStartupGate may orchestrate existing package, asset, artifact, and compatibility validators as a thin startup gate.
- RuntimeStartupGate may normalize diagnostics and include resolved paths where the existing loader/validator evaluated a path.
- SagaRuntime and SagaServer may temporarily allow missing --package-manifest for dev compatibility.
- Saga product preparation may require and forward explicit startup package manifest paths for runtime/server targets.
- Saga product may store packageManifestPath only in SagaAppConfig/SagaSessionModel in-memory startup state.
- Saga product may expose a narrow product-local diagnostic/result model for target preparation and startup handoff failures.
- Saga product may report missing runtime/server package manifests as deterministic product diagnostics before target handoff.
- Saga product may launch prepared runtime/server role executables through a narrow product-local process boundary.
- Saga product may resolve relative prepared executable names against the Saga executable directory as implementation behavior.
- Saga product may report deterministic process start and exit diagnostics without owning runtime/server internals.
- Runtime may preserve canonical string AssetKeys in AssetRegistry metadata while keeping numeric AssetId lookup for runtime hot paths.
- Runtime may use an explicit AssetIdResolver boundary to map AssetKey to AssetId before registry mutation.
- Runtime may reject missing, invalid, or colliding AssetKey to AssetId mappings before adapter/registry registration.
- Runtime may convert already parsed and validated AssetManifest entries into AssetRegistry entries through AssetManifestRegistryAdapter after all keys and ids preflight successfully.
- Runtime may preserve resolved runtime cooked-loadable asset paths in AssetRegistryEntry::sourcePath when registering manifest assets.
- Runtime may bootstrap package asset manifest references into AssetRegistry through RuntimeAssetRegistryBootstrapper when the caller supplies an explicit IAssetIdResolver.
- RuntimeAssetRegistryBootstrapper may load package-referenced asset manifests, plan registry entries, validate package-wide identity collisions, and commit the registry batch only after full preflight succeeds.

Forbidden:
- Runtime/server must not include SDE compiler internals.
- Runtime/server must not include Forge package staging internals.
- Runtime/server must not include Prism analysis internals.
- Runtime artifact manifest loading must not compile, cook, stage, or analyze artifacts.
- Runtime artifact manifest loading must not validate hashes yet.
- Runtime startup validation must not add package archive loading, Forge/SDE/Prism integration, editor UI, graph/script loading, registry wiring, resource loading, or hash enforcement.
- Runtime package and asset manifest loading must not compile, cook, stage, import, or analyze assets/artifacts.
- Runtime package and asset manifest loading must not make Engine depend on SDE, Forge, Prism, editor, asset pipeline, or tool internals.
- Runtime package startup validation must not add package archive loading, package staging, or runtime boot orchestration.
- RuntimeStartupGate must not become a broad bootstrap owner or inspect asset/artifact internals beyond calling existing loaders/validators.
- Saga product package manifest handoff must not validate package file contents.
- Saga product package manifest handoff must not add saga.project.json fields, persisted package schema, package discovery defaults, or build output inference.
- Saga product diagnostics must not become a large cross-engine diagnostics framework in this slice.
- Saga product diagnostics must not validate package manifest contents; RuntimeStartupGate inside SagaRuntime/SagaServer remains responsible for validation.
- Saga product process launch must not add supervisor, restart, daemon mode, cluster orchestration, remote launch, deployment, hot reload, or log aggregation.
- Saga product process launch must not parse package manifests or move RuntimeStartupGate validation out of SagaRuntime/SagaServer.
- Asset identity bridge must not silently hash AssetKeys, require numeric-only manifest ids, change asset manifest JSON, enforce hashes, load asset bytes, stream assets, cook/import assets, load package archives, or integrate editor/SDE/Forge/Prism.
- AssetManifestRegistryAdapter must not be wired into RuntimeStartupGate in this slice.
- AssetManifestRegistryAdapter must not load asset bytes, stream assets, enforce hashes, cook/import assets, mount package archives, discover package manifests, or integrate editor/SDE/Forge/Prism.
- RuntimeAssetRegistryBootstrapper must not be called by RuntimeStartupGate in this slice.
- RuntimeAssetRegistryBootstrapper must not define AssetKey to AssetId generation, package identity map schema, CLI identity map flags, asset byte loading, streaming/residency ownership, hash enforcement, archive mounting, import/cook behavior, or SDE/Forge/Prism integration.
```

Example:

```txt
Allowed:
Runtime reads artifact manifest files.

Forbidden:
Runtime includes SDE compiler internals.
Forge owns asset cooker internals.
Prism regenerates stale artifacts.
```

If nothing changed:

```txt
No new ownership boundary.
```

---

## 7. Diagnostics / Manifests / Reports

Diagnostics:

```txt
- Runtime.Artifact.ManifestMissing
- Runtime.Artifact.ParseFailed
- Runtime.Artifact.UnsupportedVersion
- Runtime.Artifact.MissingField
- Runtime.Artifact.InvalidField
- Runtime.Artifact.UnknownKind
- Runtime.Artifact.FileMissing
- Runtime.Artifact.DuplicateId
- Runtime.Asset.ManifestMissing
- Runtime.Asset.ParseFailed
- Runtime.Asset.UnsupportedVersion
- Runtime.Asset.MissingField
- Runtime.Asset.InvalidField
- Runtime.Asset.UnknownKind
- Runtime.Asset.DuplicateId
- Runtime.Asset.InvalidPath
- Runtime.Asset.FileMissing
- Runtime.Package.ManifestMissing
- Runtime.Package.ParseFailed
- Runtime.Package.UnsupportedVersion
- Runtime.Package.MissingField
- Runtime.Package.InvalidField
- Runtime.Package.UnknownKind
- Runtime.Package.DuplicateId
- Runtime.Package.InvalidPath
- Runtime.Package.FileMissing
- Runtime.Package.WrongPackageKind
- Runtime.Package.IncompatibleRuntime
- RuntimeStartupGate adds no new diagnostic ids; it normalizes Runtime.Package.*, Runtime.Asset.*, and Runtime.Artifact.* diagnostics.
- Saga.Target.PackageManifestMissing
- Saga.Target.ProcessStartFailed
- Saga.Target.ProcessExitedWithFailure
- Runtime.AssetIdentity.InvalidAssetKey
- Runtime.AssetIdentity.MissingKey
- Runtime.AssetIdentity.InvalidAssetId
- Runtime.AssetIdentity.DuplicateAssetKey
- Runtime.AssetIdentity.DuplicateAssetId
- Runtime.AssetRegistry.InvalidAssetId
- Runtime.AssetRegistry.DuplicateAssetId
- Runtime.AssetRegistry.DuplicateAssetKey
- Runtime.AssetRegistryAdapter.InvalidAssetKey
- Runtime.AssetRegistryAdapter.DuplicateManifestAssetKey
- Runtime.AssetRegistryAdapter.MissingAssetIdMapping
- Runtime.AssetRegistryAdapter.InvalidResolvedAssetId
- Runtime.AssetRegistryAdapter.DuplicateResolvedAssetId
- Runtime.AssetRegistryAdapter.RegistryAssetKeyCollision
- Runtime.AssetRegistryAdapter.RegistryAssetIdCollision
- Runtime.AssetRegistryAdapter.UnsupportedKind
- Runtime.AssetRegistryBootstrap.DuplicatePackageAssetKey
- Runtime.AssetRegistryBootstrap.DuplicatePackageAssetId
- Runtime.AssetRegistryBootstrap.RegistryAssetKeyCollision
- Runtime.AssetRegistryBootstrap.RegistryAssetIdCollision
```

Manifests:

```txt
- Engine/tests/fixtures/artifacts/valid_artifact_manifest.json
- Engine/tests/fixtures/artifacts/invalid_missing_path_manifest.json
- No new checked-in manifest fixtures; package and asset manifest tests use temporary files.
- RuntimeStartupGate tests use temporary package, asset, and artifact manifest files.
- No persisted project or package manifest schema was changed.
- Asset identity and registry tests use temporary files for old AssetRegistry JSON compatibility.
- AssetManifestRegistryAdapter tests use in-memory AssetManifest values and temporary directories/files only for path and no-read behavior.
- RuntimeAssetRegistryBootstrapper tests use temporary package and asset manifest files only.
```

Reports:

```txt
- No report format added.
- No product diagnostic report format added; Saga product diagnostics are emitted as process output lines for this slice.
- No process log aggregation report added.
```

Example:

```txt
Diagnostics:
- Runtime.Artifact.ManifestMissing
- Runtime.Artifact.UnsupportedVersion

Manifests:
- Build/Manifests/sde_artifacts.json

Reports:
- Build/Reports/runtime_diagnostics.json
```

---

## 8. Verification

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaUnitTests --jobs=1

Result:
Passed

Notes:
Built SagaUnitTests through Forge/Nix with serialized build parallelism for the runtime artifact startup validator continuation.
```

```txt
Command:
Tools/Forge/bin/forge nix run build/RelWithDebInfo/SagaUnitTests --gtest_filter=ArtifactStartupValidatorTests.*:ArtifactManifestLoaderTests.*

Result:
Passed

Notes:
33 runtime artifact tests passed: 23 ArtifactManifestLoaderTests and 10 ArtifactStartupValidatorTests.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaArchitectureTests --jobs=1

Result:
Passed

Notes:
Built architecture boundary tests through Forge/Nix with serialized build parallelism.
```

```txt
Command:
Tools/Forge/bin/forge nix run build/RelWithDebInfo/SagaArchitectureTests

Result:
Passed

Notes:
11 architecture/boundary tests passed.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaUnitTests --jobs=1

Result:
Passed

Notes:
Built SagaUnitTests through Forge/Nix with serialized build parallelism for the runtime package and asset manifest validation slice.
```

```txt
Command:
Tools/Forge/bin/forge nix run build/RelWithDebInfo/SagaUnitTests --gtest_filter=AssetManifestLoaderTests.*:PackageManifestLoaderTests.*:ArtifactStartupValidatorTests.*:ArtifactManifestLoaderTests.*

Result:
Passed

Notes:
50 runtime manifest tests passed: 23 ArtifactManifestLoaderTests, 10 ArtifactStartupValidatorTests, 8 AssetManifestLoaderTests, and 9 PackageManifestLoaderTests.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaUnitTests --jobs=1

Result:
Passed

Notes:
Built SagaUnitTests through Forge/Nix with serialized build parallelism for the RuntimeStartupGate slice.
```

```txt
Command:
Tools/Forge/bin/forge nix run build/RelWithDebInfo/SagaUnitTests --gtest_filter=RuntimeStartupGateTests.*:PackageManifestLoaderTests.*:AssetManifestLoaderTests.*:ArtifactStartupValidatorTests.*

Result:
Passed

Notes:
40 runtime startup and manifest tests passed: 13 RuntimeStartupGateTests, 9 PackageManifestLoaderTests, 8 AssetManifestLoaderTests, and 10 ArtifactStartupValidatorTests.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaRuntime --jobs=1

Result:
Passed

Notes:
Built SagaRuntime with the optional --package-manifest startup validation hook.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaServer --jobs=1

Result:
Passed

Notes:
Built SagaServer with the optional --package-manifest startup validation hook.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaUnitTests --jobs=1

Result:
Passed

Notes:
Built SagaUnitTests through Forge/Nix with serialized build parallelism for the Saga product startup package handoff slice.
```

```txt
Command:
Tools/Forge/bin/forge nix run build/RelWithDebInfo/SagaUnitTests --gtest_filter=SagaAppConfigTest.*:SagaProductHostTest.*

Result:
Passed

Notes:
9 Saga product config/host tests passed for package manifest parsing, runtime/server target requirements, and prepare-only argument output.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaUnitTests --jobs=1

Result:
Passed

Notes:
Built SagaUnitTests through Forge/Nix with serialized build parallelism for the Saga product startup diagnostics slice.
```

```txt
Command:
Tools/Forge/bin/forge nix run build/RelWithDebInfo/SagaUnitTests --gtest_filter=SagaAppConfigTest.*:SagaProductHostTest.*

Result:
Passed

Notes:
10 Saga product config/host tests passed for package manifest parsing, runtime/server target requirements, product diagnostics, and prepare-only argument output.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=Saga --jobs=1

Result:
Passed

Notes:
Built Saga product target with package manifest handoff and product diagnostics support.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaUnitTests --jobs=1

Result:
Passed

Notes:
Built SagaUnitTests through Forge/Nix with serialized build parallelism for the Saga runtime/server process launch slice.
```

```txt
Command:
Tools/Forge/bin/forge nix run build/RelWithDebInfo/SagaUnitTests --gtest_filter=SagaAppConfigTest.*:SagaProductHostTest.*

Result:
Passed

Notes:
16 Saga product config/host tests passed for package manifest parsing, product diagnostics, runtime/server launch handoff, prepare-only no-launch behavior, launch failure diagnostics, exit code reporting, and editor no-launch behavior.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=Saga --jobs=1

Result:
Passed

Notes:
Built Saga product target with the product-local runtime/server process launcher.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaRuntime --jobs=1

Result:
Passed

Notes:
Built SagaRuntime, the runtime executable now referenced by prepared runtime targets.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaServer --jobs=1

Result:
Passed

Notes:
Built SagaServer, the server executable now referenced by prepared server targets.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaUnitTests --jobs=1

Result:
Passed

Notes:
Built SagaUnitTests through Forge/Nix with serialized build parallelism for the AssetKey to AssetId identity bridge slice.
```

```txt
Command:
Tools/Forge/bin/forge nix run build/RelWithDebInfo/SagaUnitTests --gtest_filter=AssetIdResolverTests.*:AssetRegistryTests.*:AssetManifestLoaderTests.*

Result:
Passed

Notes:
17 runtime asset identity, registry, and manifest loader tests passed: 4 AssetIdResolverTests, 5 AssetRegistryTests, and 8 AssetManifestLoaderTests.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaRuntime --jobs=1

Result:
Passed

Notes:
Built SagaRuntime after adding the AssetKey to AssetId resolver boundary and AssetRegistry key metadata.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaServer --jobs=1

Result:
Passed

Notes:
Built SagaServer after adding the AssetKey to AssetId resolver boundary and AssetRegistry key metadata.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaUnitTests --jobs=1

Result:
Passed

Notes:
Built SagaUnitTests through Forge/Nix with serialized build parallelism for the AssetManifest to AssetRegistry adapter slice.
```

```txt
Command:
Tools/Forge/bin/forge nix run build/RelWithDebInfo/SagaUnitTests --gtest_filter=AssetManifestRegistryAdapterTests.*:AssetIdResolverTests.*:AssetRegistryTests.*:AssetManifestLoaderTests.*

Result:
Passed

Notes:
30 runtime asset adapter, identity, registry, and manifest loader tests passed: 13 AssetManifestRegistryAdapterTests, 4 AssetIdResolverTests, 5 AssetRegistryTests, and 8 AssetManifestLoaderTests.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaRuntime --jobs=1

Result:
Passed

Notes:
Built SagaRuntime after adding the AssetManifest to AssetRegistry adapter.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaServer --jobs=1

Result:
Passed

Notes:
Built SagaServer after adding the AssetManifest to AssetRegistry adapter.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaUnitTests --jobs=1

Result:
Passed

Notes:
Built SagaUnitTests through Forge/Nix with serialized build parallelism for the runtime package asset registry bootstrap slice.
```

```txt
Command:
Tools/Forge/bin/forge nix run build/RelWithDebInfo/SagaUnitTests --gtest_filter=RuntimeAssetRegistryBootstrapperTests.*:AssetManifestRegistryAdapterTests.*:AssetRegistryTests.*:AssetManifestLoaderTests.*:PackageManifestLoaderTests.*:RuntimeStartupGateTests.*

Result:
Passed

Notes:
58 runtime package asset bootstrap, adapter, registry, manifest loader, package loader, and startup gate tests passed.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaRuntime --jobs=1

Result:
Passed

Notes:
Built SagaRuntime after adding runtime package asset registry bootstrap support.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaServer --jobs=1

Result:
Passed

Notes:
Built SagaServer after adding runtime package asset registry bootstrap support.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaUnitTests --jobs=1

Result:
Passed

Notes:
Built SagaUnitTests through Forge/Nix with serialized build parallelism for the runtime startup/resource stabilization slice.
```

```txt
Command:
Tools/Forge/bin/forge nix run build/RelWithDebInfo/SagaUnitTests

Result:
Passed

Notes:
Full SagaUnitTests binary passed: 645 tests from 102 test suites.
```

```txt
Command:
ctest --test-dir build/RelWithDebInfo --output-on-failure

Result:
Not run

Notes:
Attempted but aborted because running ctest without explicit single-job execution overloaded the local terminal/session.
```

```txt
Command:
ctest --test-dir build/RelWithDebInfo --output-on-failure -j 1

Result:
Not run

Notes:
Attempted twice with single-job execution, but the terminal/session still aborted. Kept as a verification gap to avoid overloading the machine.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=Saga --jobs=1

Result:
Passed

Notes:
Built Saga product target with serialized build parallelism during stabilization.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaRuntime --jobs=1

Result:
Passed

Notes:
Built SagaRuntime with serialized build parallelism during stabilization.
```

```txt
Command:
Tools/Forge/bin/forge nix build --build=build/RelWithDebInfo --target=SagaServer --jobs=1

Result:
Passed

Notes:
Built SagaServer with serialized build parallelism during stabilization.
```

```txt
Command:
git diff --check

Result:
Passed

Notes:
No whitespace or patch formatting problems found.
```

Example:

```txt
Command:
ctest -R ArtifactManifestLoader

Result:
Passed

Notes:
Valid manifest and missing file cases covered.
```

If not tested:

```txt
Command:
Not run

Result:
Not verified

Notes:
Implementation exists but was not tested yet.
```

---

## 9. Roadmaps To Update

```txt
[ ] EDITOR_ROADMAP.md
[ ] SHARED_ROADMAP.md
[ ] ENGINE_ROADMAP.md
[ ] SDE_ROADMAP.md
[ ] FORGE_ROADMAP.md
[ ] PRISM_ROADMAP.md
[ ] COLLABORATION_ROADMAP.md
[ ] TOOLS_ROADMAP.md
[ ] DependencyGraph.md
[ ] DIAGNOSTICS_ROADMAP.md
[ ] ASSET_PIPELINE_ROADMAP.md
[ ] SAGA_SCRIPTING_ROADMAP.md
[ ] BUILD_PUBLISH_PIPELINE_ROADMAP.md
[ ] AssetStreamingImplementation.md
[ ] SCHEMA.md
[ ] Other:
```

Reason:

```txt
No roadmap files were updated in this task by request. Affected roadmaps are ASSET_PIPELINE_ROADMAP.md, ENGINE_ROADMAP.md, BUILD_PUBLISH_PIPELINE_ROADMAP.md, DIAGNOSTICS_ROADMAP.md, and SAGA_PRODUCT_ROADMAP.md.
```

---

## 10. Known Problems

```txt
- Artifact hash validation is not implemented yet.
- The loader does not integrate with a shared diagnostics payload/report system yet.
- Runtime package loading and package-root policy beyond the manifest parent directory are not implemented yet.
- Full ctest suite could not be completed; ctest attempts aborted even with -j 1, so full SagaUnitTests is the broadest completed regression run for this stabilization slice.
- Package hash and referenced manifest hash validation are not implemented yet.
- Asset artifact hash validation is not implemented yet.
- Package archive loading and staging are not implemented.
- AssetManifestLoader is not wired into runtime streaming yet.
- RuntimeAssetRegistryBootstrapper can register package asset manifests only when a caller supplies an explicit IAssetIdResolver.
- Runtime package/startup code still needs a documented production source for AssetKey to AssetId resolver mappings before package asset registry bootstrap can be automatic.
- RuntimeAssetRegistryBootstrapper is intentionally not wired into RuntimeStartupGate, SagaRuntime, or SagaServer CLI startup yet.
- RuntimeStartupGate is intentionally thin and does not load resources, mount archives, wire registries, load graph/script artifacts, enforce hashes, or integrate SDE/Forge/Prism.
- SagaRuntime and SagaServer still allow missing --package-manifest as a temporary dev compatibility bridge; production/server startup should eventually require an explicit startup package manifest.
- Saga product launches prepared runtime/server executables, but does not supervise, restart, daemonize, hot reload, deploy, or aggregate logs.
- Saga product does not discover package manifests from project schema or build outputs yet.
- Saga product does not validate package file contents; runtime/server RuntimeStartupGate remains responsible for content validation.
- Saga product diagnostics are intentionally narrow and do not normalize RuntimeStartupGate diagnostics emitted by child runtime/server processes yet.
```

Example:

```txt
- Manifest version mismatch is not handled yet.
- Runtime.Artifact.* diagnostics exist but are not shown in editor preview yet.
- Tests do not cover invalid JSON manifest input.
```

---

## 11. Next Actions

```txt
[ ] Add artifact hash validation once hash semantics are defined.
[ ] Integrate artifact file validation with package startup once package roots are defined.
[ ] Promote shared artifact/diagnostic contracts only after multiple ownership domains consume them.
[ ] Consider shared diagnostic payloads after runtime/server/tool consumers need common reporting.
[ ] Decide where expectedRuntimeCompatibilityVersion comes from for production role launches.
[ ] Make production/server startup require an explicit startup package manifest after the dev compatibility bridge is retired.
[ ] Add product-facing startup diagnostics presentation for failed runtime/server RuntimeStartupGate validation after a child diagnostic contract is defined.
[ ] Decide long-term package output discovery through build/publish reports before adding persisted package path schema.
[ ] Decide package hash and asset artifact hash semantics before enforcing integrity checks.
[ ] Decide package/build output identity mapping source before wiring package startup to RuntimeAssetRegistryBootstrapper automatically.
[ ] Add production runtime/server registry bootstrap handoff after the resolver mapping source is documented.
[ ] Re-run ctest on a machine/session that can tolerate full CTest execution without terminal aborts.
```

Example:

```txt
[ ] Add unsupported manifest version test.
[ ] Add ArtifactRef shared contract if multiple modules consume it.
[ ] Update ENGINE_ROADMAP.md after verification.
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
Next iteration: 0.0.8-dev.2

Possible focus:
- Decide the package/build output source for AssetKey to AssetId resolver mappings, then wire RuntimeAssetRegistryBootstrapper into runtime/server startup without import/cook/streaming ownership changes.
- Product-facing runtime/server child diagnostic capture once a narrow child diagnostic contract is defined.
- Hash semantics for package, artifact, and asset manifest references after build/publish package semantics are clearer.
```

````
