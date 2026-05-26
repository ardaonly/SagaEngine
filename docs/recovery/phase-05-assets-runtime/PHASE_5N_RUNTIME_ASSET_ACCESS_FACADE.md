# Phase 5N Runtime Asset Access Facade

> Last updated: 2026-05-25
> Status: Phase 5N closed as Runtime read-only asset access facade
> Decision: add `RuntimeAssetCatalog` as the Runtime-owned read-only contract over caller-owned `AssetRegistry`

Phase 5N turns the Apps/Runtime local registry startup proof into a narrow
Runtime consumption contract. It does not wire that contract into `ClientHost`
or Runtime services yet.

Full CTest remains unverified.

## Phase 5M Closure

Phase 5M is accepted as:

- Runtime asset consumption ownership audit
- ClientHost raw `AssetRegistry` rejection
- Runtime read-only asset access contract recommendation
- Phase 5 closure direction checkpoint

Phase 5M is not accepted as:

- Runtime asset access facade implementation
- ClientHost asset consumption
- RuntimeServiceRegistry asset service
- publish readiness report/hard gate
- source discovery/import/cook
- UI/document `AssetKind`
- MultiplayerSandbox package readiness
- full AssetPipeline completion

## Phase 5N Implementation

`RuntimeAssetCatalog` is a Runtime-owned facade over a caller-owned
`AssetRegistry`.

It provides read-only inspection:

- empty state and asset count
- lookup by `AssetId`
- lookup by asset key
- containment checks by `AssetId` and asset key
- lookup by `AssetKind`
- prefetch candidate view
- total disk size view

The facade does not own the registry, mutate registry entries, expose a global
singleton, depend on Apps/Runtime, depend on `ClientHost`, depend on
RuntimeServiceRegistry, or depend on AssetPipeline.

No Engine-side const query was needed because `AssetRegistry` already exposes
the required read APIs.

## Test Evidence

`RuntimeAssetCatalogTests` prove:

- an empty registry reports empty and zero count
- RuntimeSmoke package bootstrap is visible through the catalog
- lookup by asset key succeeds
- lookup by `AssetId` succeeds
- missing asset key and id return false/null
- catalog entry pointers are const
- the catalog observes caller-owned registry state without owning lifetime
- failed bootstrap leaves the catalog on the committed registry state
- no `ClientHost` construction or Apps/Runtime process execution is required

## Verification

Passed:

- `git diff --check`
- targeted `rg` inventory for `RuntimeAssetCatalog`, `AssetRegistry`,
  `RuntimeAssetBootstrap`, `RuntimeAssetStartupBootstrap`, `ClientHost`,
  `RuntimeServiceRegistry`, publish readiness, `Content/UI`, and `AssetKind`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L runtime`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L asset`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L package`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L tools`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L integration`
- `Tools/Forge/bin/forge nix build --target RuntimeAssetCatalogTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target RuntimeAssetBootstrapTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target RuntimeAssetStartupBootstrapTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "RuntimeAssetCatalogTests|RuntimeAssetBootstrapTests|RuntimeAssetStartupBootstrapTests" --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "RuntimePackageSmokeTests|GeneratedRuntimeSmokePackageTests|SagaPackageStagingTests" --output-on-failure -j 1`

Label inventory after Phase 5N:

- `runtime`: 23 tests, with existing registered-but-unbuilt runtime executable
  warnings
- `asset`: 13 tests
- `package`: 11 tests
- `tools`: 7 tests
- `integration`: 3 tests

Full CTest remains unverified.

## Non-Goals

Phase 5N does not add:

- `ClientHost` constructor/config changes
- passing `AssetRegistry` or `RuntimeAssetCatalog` into `ClientHost`
- Apps/Runtime main behavior changes
- RuntimeServiceRegistry asset service
- publish readiness report or hard gate
- source discovery/import/cook
- AssetPipeline writer or package staging changes
- UI/document `AssetKind`
- editor workflow
- MultiplayerSandbox package
- broad CMake/test restructuring
- full CTest verification

## Recommended Phase 5O

Prefer Publish Readiness Package/Asset Identity Report v0 if
`RuntimeAssetCatalog` remains clean and tested. Choose ClientHost/Runtime Asset
Consumption Integration Audit only if the next bridge needs to decide how a
Runtime catalog reaches runtime/client systems without changing render, UI, or
network lifecycle.

Phase 5 closure is nearer after Phase 5N, but still blocked on publish
readiness relationship, ClientHost/runtime consumption wiring, service
ownership, UI/document asset kind, MultiplayerSandbox package fixture, and full
test health.
