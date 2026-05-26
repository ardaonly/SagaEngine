# Phase 5L Apps Runtime Asset Bootstrap Wiring

> Last updated: 2026-05-25
> Status: Phase 5L closed as thin Apps/Runtime startup asset bootstrap wiring
> Decision: keep `AssetRegistry` app-local until Runtime or ClientHost has a real consumption contract

Phase 5L closes the narrow bridge from Runtime package startup validation to
Apps/Runtime asset bootstrap. It intentionally stops at startup proof and
lifetime placeholder wiring; it does not claim ClientHost or Runtime services
consume the registry yet.

Full CTest remains unverified.

## Phase 5K Closure

Phase 5K is accepted as:

- `RuntimeAssetBootstrap` diagnostics/report facade
- Apps/Runtime startup wiring audit
- Phase 5L thin wiring recommendation

Phase 5K is not accepted as:

- Apps/Runtime startup asset bootstrap wiring
- RuntimeServiceRegistry asset service
- ClientHost `AssetRegistry` consumption
- publish readiness
- source discovery/import/cook
- UI/document `AssetKind`
- MultiplayerSandbox readiness
- full AssetPipeline completion

## Phase 5L Implementation

Runtime launch base directory propagation:

- `RuntimeStartupPreflightOptions` now carries `packageBaseDirectory`.
- `RuntimeLaunchOptions` now carries `packageBaseDirectory`.
- `RuntimeSessionDescriptor` now carries `packageBaseDirectory`.
- `RuntimeStartupSession::Prepare` copies the launch base directory into both
  the descriptor and preflight options.
- `RuntimeStartupPreflight::ValidatePackageForStartup` passes the base
  directory into `RuntimeStartupGateOptions`.
- Empty base directory preserves package-manifest-folder-relative behavior.

Apps/Runtime helper seam:

- `Apps/Runtime/RuntimeAssetStartupBootstrap.hpp`
- `Apps/Runtime/RuntimeAssetStartupBootstrap.cpp`
- The helper accepts `RuntimeSessionDescriptor` and caller-owned
  `AssetRegistry&`.
- No package manifest skips bootstrap and leaves the registry untouched.
- Package manifests call `SagaRuntime::RuntimeAssetBootstrap`.
- Results preserve `RuntimeAssetBootstrapDiagnostics` summary and diagnostic
  views for app logging.
- The helper does not include or construct `ClientHost`.

Apps/Runtime thin wiring:

- `Apps/Runtime/main.cpp` parses `--package-base-directory <path>`.
- A local `AssetRegistry` is created after startup preflight succeeds.
- The helper runs before `ClientHost` construction.
- Explicit package asset bootstrap failure blocks process startup.
- No-package dev launches keep the existing skip behavior.
- The registry is not passed into `ClientHost`.

## Test Evidence

Runtime startup tests:

- `RuntimeStartupPreflightTests` prove explicit `packageBaseDirectory` resolves
  package references outside the package manifest folder.
- `RuntimeStartupPreflightTests` prove empty base directory keeps manifest
  folder behavior.
- `RuntimeStartupSessionTests` prove launch base directory is copied to the
  descriptor and flows into preflight validation.

Apps/Runtime helper tests:

- `RuntimeAssetStartupBootstrapTests` cover no-package skip without registry
  mutation.
- RuntimeSmoke package registers one asset into the local registry.
- Lookup works by `AssetKey` and `AssetId`.
- Missing cooked asset and bad base directory block without registry mutation.
- Diagnostic summary/views are preserved for app logging.
- The test target links the helper without `Apps/Client` or `ClientHost`.

## Verification

Passed:

- `git diff --check`
- `Tools/Forge/bin/forge nix build --target RuntimeAssetStartupBootstrapTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target RuntimeStartupSessionTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target RuntimeStartupPreflightTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target RuntimeAssetBootstrapTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target RuntimeAssetBootstrapDiagnosticsTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target SagaRuntime --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "RuntimeStartupSessionTests|RuntimeStartupPreflightTests|RuntimeAssetBootstrapTests|RuntimeAssetBootstrapDiagnosticsTests|RuntimeAssetStartupBootstrapTests" --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "RuntimePackageSmokeTests|GeneratedRuntimeSmokePackageTests|SagaPackageStagingTests|PackageManifestWriterTests|AssetManifestWriterTests|AssetIdentityManifestWriterTests" --output-on-failure -j 1`

Label inventory after Phase 5L:

- `runtime`: 22 tests, with existing registered-but-unbuilt runtime executable warnings
- `asset`: 12 tests
- `package`: 10 tests
- `tools`: 7 tests
- `integration`: 3 tests

## Non-Goals

Phase 5L does not add:

- `ClientHost` API changes
- passing `AssetRegistry` into `ClientHost`
- `ClientNetworkSession`, render, UI, or network lifecycle changes
- RuntimeServiceRegistry asset service or service context rewrite
- source discovery/import/cook
- AssetPipeline writer or package staging changes
- publish readiness report or hard gate
- UI/document `AssetKind`
- editor workflow
- MultiplayerSandbox package
- broad CMake/test restructuring
- full CTest verification

## Recommended Phase 5M

Choose one of:

- ClientHost/Runtime asset consumption ownership audit
- Publish readiness report v0

The next runtime bridge should decide how package assets become consumable by
Runtime or ClientHost without forcing render/network lifecycle changes.
