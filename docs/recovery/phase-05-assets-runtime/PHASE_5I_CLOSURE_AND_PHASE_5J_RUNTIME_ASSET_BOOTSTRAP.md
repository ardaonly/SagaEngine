# Phase 5I Closure And Phase 5J Runtime Asset Bootstrap

> Last updated: 2026-05-25
> Status: Phase 5J closed as Runtime asset bootstrap facade established
> Decision: open the next Phase 5 slice toward Apps/Runtime startup wiring or publish readiness only after this facade remains green

This checkpoint closes Phase 5I as the Runtime Asset Startup Integration Audit
and closes Phase 5J as a narrow Runtime-owned package asset bootstrap facade.
It does not close Apps/Runtime startup asset integration, Runtime service
registry asset service wiring, source discovery/import/cook, publish gate
implementation, UI/document AssetKind, MultiplayerSandbox package readiness, or
full AssetPipeline completion.

Full CTest remains unverified.

## Phase 5I Closure Summary

Phase 5I is accepted as:

- Runtime asset startup ownership audit
- Runtime package asset bootstrap ownership decision
- decision to add a tested Runtime facade before Apps/Runtime startup wiring
- decision to keep `AssetRegistry` lifetime caller-owned for the first Runtime bridge

Phase 5I is not accepted as:

- Runtime asset bootstrap implementation
- RuntimeServiceRegistry asset service wiring
- Apps/Runtime startup AssetRegistry integration
- publish readiness package/asset/identity report
- source discovery/import/cook
- UI/document AssetKind
- MultiplayerSandbox package readiness
- full AssetPipeline completion

## Phase 5J Completion Evidence

Phase 5J adds:

- `Runtime/include/SagaRuntime/RuntimeAssetBootstrap.hpp`
- `Runtime/src/SagaRuntime/RuntimeAssetBootstrap.cpp`
- `Tests/Unit/Runtime/RuntimeAssetBootstrapTests.cpp`
- dedicated `RuntimeAssetBootstrapTests` CTest registration labelled `unit;runtime;asset;package`

Confirmed behavior:

- `SagaRuntime::RuntimeAssetBootstrap` accepts a package manifest path.
- The facade accepts optional `packageBaseDirectory` and preserves package-folder-relative behavior when it is empty.
- The facade exposes validation flags for referenced manifests and cooked asset files.
- The facade validates through `RuntimeStartupGate`.
- The facade registers identity-backed package assets through `RuntimeAssetRegistryBootstrapper`.
- The facade registers into a caller-owned `AssetRegistry&`; Runtime does not create a hidden/global registry.
- Runtime-facing diagnostics preserve severity, category, diagnostic id, message, manifest path, reference/item context, resource id, asset id, and resolved path when available.
- Focused tests prove a valid RuntimeSmoke package registers exactly one asset and lookup works by `AssetKey` and `AssetId`.
- Focused tests prove missing package manifest, missing identity manifest, missing cooked asset, bad package base directory, startup domain mismatch, and registry collision failures do not mutate the registry beyond preexisting entries.

Verification passed:

- `git diff --check`
- `Tools/Forge/bin/forge nix build --target RuntimeAssetBootstrapTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R RuntimeAssetBootstrapTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "RuntimePackageSmokeTests|GeneratedRuntimeSmokePackageTests|SagaPackageStagingTests" --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "RuntimeAssetBootstrapTests|RuntimePackageSmokeTests|GeneratedRuntimeSmokePackageTests|SagaPackageStagingTests|PackageManifestWriterTests|AssetManifestWriterTests|AssetIdentityManifestWriterTests" --output-on-failure -j 1`

Label inventories after Phase 5J:

- `asset`: 10 tests
- `runtime`: 20 tests, with existing registered-but-unbuilt runtime executable warnings
- `tools`: 7 tests
- `package`: 8 tests
- `integration`: 3 tests

Full CTest remains unverified.

## What Phase 5J Proves

Phase 5J proves:

- Runtime now has a narrow facade that owns package validation plus asset registry bootstrap orchestration.
- The staged/generated package chain can be consumed through a Runtime API without Apps/Runtime owning package asset policy.
- Runtime can populate a caller-owned `AssetRegistry` from an identity-backed package while preserving no-mutation failure behavior.
- The facade uses existing Engine startup and resource bootstrap contracts instead of inventing a new package schema.

## What Phase 5J Does Not Prove

Phase 5J does not prove:

- Apps/Runtime bootstraps AssetRegistry during startup.
- RuntimeServiceRegistry owns an asset service.
- ClientHost, UI, rendering, networking, or app lifecycle ownership changed.
- Publish readiness reports generated package/asset/identity coverage.
- Source discovery/import/cook exists.
- UI/document AssetKind exists.
- MultiplayerSandbox package exists.
- Full test health.

## Next Phase Candidates

### A. Apps/Runtime Startup Asset Bootstrap Wiring Audit

- Goal: decide the narrow app startup call site and lifetime for a caller-owned `AssetRegistry`.
- Bundle contents: inspect `Apps/Runtime`, `RuntimeStartupSession`, local service registry shell, and app exit/logging policy.
- Files likely touched: docs only.
- Behavior: docs-only.
- Tests needed: none beyond inspection and `git diff --check`.
- Risk: low.
- Value: prevents premature app wiring.
- Why now: Runtime facade exists.
- Why later: delays app integration by one slice.
- Rollback: revert docs.

### B. Apps/Runtime Thin Caller Integration

- Goal: have Apps/Runtime call `RuntimeAssetBootstrap` without owning package asset policy.
- Bundle contents: create app-owned registry lifetime, call facade after startup preflight, summarize diagnostics through existing app logging/exit policy.
- Files likely touched: `Apps/Runtime/main.cpp`, Runtime startup tests or app-focused tests, docs.
- Behavior: app startup behavior changes when a package manifest is supplied.
- Tests needed: focused startup/app tests proving success and failure policy.
- Risk: medium because app registry lifetime and logging/exit behavior need care.
- Value: highest direct product/runtime bridge after Phase 5J.
- Why now: facade exists and is tested.
- Why later: if ownership still needs a docs-only audit first.
- Rollback: remove app call and tests; keep facade.

### C. Publish Readiness Package/Asset Identity Report v0

- Goal: report package/asset/identity coverage without adding a hard-fail gate.
- Bundle contents: report-only diagnostics for identity refs, asset refs, and Runtime bootstrap readiness.
- Files likely touched: `Apps/Saga` publish readiness surfaces, publish tests, docs.
- Behavior: report output changes; hard-fail policy does not expand.
- Tests needed: focused publish readiness tests for present/missing refs.
- Risk: medium-low.
- Value: improves product package visibility.
- Why now: staging and Runtime facade are both proven.
- Why later: Apps/Runtime consumption is still the more direct runtime bridge.
- Rollback: remove report fields/tests.

## Recommended Next Slice

Pick next: Apps/Runtime Startup Asset Bootstrap Wiring Audit.

Reason:

- Phase 5J deliberately avoids app lifecycle changes.
- The next bridge should decide exactly where a caller-owned `AssetRegistry`
  lives in Apps/Runtime and how facade diagnostics map to app logging/exit
  policy.
- A docs-only audit is safer than adding a RuntimeServiceRegistry service
  context or moving ClientHost/render/network lifecycle.

After that audit, the first implementation candidate should be a thin
Apps/Runtime caller integration that uses `RuntimeAssetBootstrap` without
changing ClientHost, UI, rendering, networking, publish readiness, or cook.
