# Phase 5O Publish Readiness Package Asset Identity Report

> Last updated: 2026-05-25
> Status: Phase 5O closed as report-only publish package/asset identity visibility
> Decision: add product-local publish report coverage without expanding publish hard gates

Phase 5O bridges the Runtime package/catalog proof into product-facing publish
visibility. It reports whether the client and server package manifests load,
which asset identity manifest they reference, which asset manifests they
reference, and whether cooked asset file validation succeeds through the
existing Runtime loaders.

Full CTest remains unverified.

## Phase 5N Closure

Phase 5N is accepted as:

- Runtime-owned read-only asset access facade
- `RuntimeAssetCatalog` over caller-owned `AssetRegistry`
- package asset lookup/access contract proof

Phase 5N is not accepted as:

- ClientHost asset consumption
- Apps/Runtime-to-ClientHost asset wiring
- RuntimeServiceRegistry asset service
- publish readiness report
- source discovery/import/cook
- UI/document `AssetKind`
- MultiplayerSandbox package readiness
- full AssetPipeline completion

## Phase 5O Implementation

`SagaPublishReadinessService` now collects product-local
`packageAssetIdentityCoverage` for the fixed publish package slots:

- `client`: `Build/Manifests/package_manifest.client.json`
- `server`: `Build/Manifests/package_manifest.server.json`

Coverage records include:

- package slot, package manifest path, existence, load status, package id, and
  package kind
- referenced asset identity manifest path, existence, load status, and mapping
  count
- referenced asset manifest id/path, existence, load status, parsed asset
  count, and cooked asset validation diagnostics
- deterministic loader diagnostic codes, messages, paths, fields, and reference
  or entry indexes where available

The implementation reuses:

- `PackageManifestLoader` for package manifest parsing and the existing publish
  blocker path
- `AssetIdentityManifestLoader` for identity mapping visibility
- `AssetManifestLoader` with `validateAssetFiles = true` for asset manifest and
  cooked-file coverage

Package-relative manifest references resolve against the project root, matching
the existing package staging and publish validation base. Asset file references
resolve against the asset manifest directory, matching Runtime startup and
registry bootstrap behavior.

## Status Policy

Phase 5O is report-only.

Existing package manifest validation blockers still block publish. For example,
missing package manifests, missing package-referenced asset manifests, and
missing package-referenced identity manifests remain publish blockers through
the existing `PackageManifestLoader` validation path.

Coverage-only issues do not add blockers or change readiness status. For
example, a missing cooked asset referenced by an otherwise present asset
manifest is serialized under `packageAssetIdentityCoverage` with the stable
`Runtime.Asset.FileMissing` diagnostic, but it does not expand the current
publish hard gate.

External diagnostics warning/blocking behavior is unchanged.

## Test Evidence

`SagaPublishReadinessTests` prove:

- valid generated package inputs report package, identity, mapping, asset
  manifest, and asset counts with ready status
- a missing package manifest remains a blocker and appears in coverage
- a missing package-referenced asset manifest appears deterministically
- a missing package-referenced identity manifest appears deterministically
- a missing cooked asset is coverage-only and does not add a blocker
- invalid asset manifest loader diagnostics serialize without hard-gate
  expansion
- JSON report output is deterministic and machine-readable

## Verification

Passed:

- `git diff --check`
- targeted `rg` inventory for publish readiness, package manifest, asset
  manifest, identity manifest, `RuntimeAssetCatalog`, `RuntimeAssetBootstrap`,
  and loader use
- `Tools/Forge/bin/forge nix build --target SagaPublishReadinessTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target SagaPackageStagingTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target RuntimeAssetCatalogTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target RuntimeAssetStartupBootstrapTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target RuntimePackageSmokeTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target GeneratedRuntimeSmokePackageTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R SagaPublishReadinessTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "SagaPublishReadinessTests|SagaPackageStagingTests|RuntimeAssetCatalogTests|RuntimeAssetStartupBootstrapTests|RuntimePackageSmokeTests|GeneratedRuntimeSmokePackageTests" --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L runtime`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L asset`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L package`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L tools`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L integration`

Label inventory after Phase 5O:

- `runtime`: 23 tests, with existing registered-but-unbuilt runtime executable
  warnings
- `asset`: 14 tests
- `package`: 12 tests
- `tools`: 7 tests
- `integration`: 3 tests

Full CTest remains unverified.

## Non-Goals

Phase 5O does not add:

- new publish hard gates
- ClientHost constructor/config changes
- Apps/Runtime behavior changes
- RuntimeServiceRegistry asset service
- source discovery/import/cook
- AssetPipeline writer changes
- package staging changes
- UI/document `AssetKind`
- MultiplayerSandbox package fixture
- broad CMake/test restructuring
- full CTest verification

## Recommended Phase 5P

Choose Phase 5 Closure Readiness Checkpoint if Phase 5O remains clean and
report-only after targeted regressions.

If Phase 5O evidence exposes product-facing gaps that need a narrower design
slice first, choose UI/Document `AssetKind` Audit or ClientHost/Runtime Asset
Consumption Integration Audit.
