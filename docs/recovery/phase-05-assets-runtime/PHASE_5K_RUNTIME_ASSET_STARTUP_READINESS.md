# Phase 5J Closure And Phase 5K Runtime Asset Startup Readiness

> Last updated: 2026-05-25
> Status: Phase 5K closed as RuntimeAssetBootstrap diagnostics facade plus Apps/Runtime wiring decision
> Decision: open Phase 5L as Apps/Runtime thin asset bootstrap wiring prep and integration

This checkpoint closes Phase 5J narrowly and records Phase 5K as a bounded
Runtime asset startup readiness bundle. Phase 5K adds Runtime-owned reporting
helpers for `RuntimeAssetBootstrapResult` and decides the safe Apps/Runtime
startup wiring shape, but does not change Apps/Runtime behavior yet.

Full CTest remains unverified.

## Phase 5J Closure Summary

Phase 5J is accepted as:

- Runtime-owned package asset bootstrap facade
- Runtime package validation plus `AssetRegistry` bootstrap contract
- caller-owned `AssetRegistry` bootstrap proof

Phase 5J is not accepted as:

- Apps/Runtime startup wiring
- RuntimeServiceRegistry asset service
- ClientHost lifecycle ownership
- source discovery/import/cook
- publish readiness report or hard gate
- UI/document AssetKind
- MultiplayerSandbox package readiness
- full AssetPipeline completion

## Phase 5J Completion Evidence

Confirmed evidence:

- `Runtime/include/SagaRuntime/RuntimeAssetBootstrap.hpp` exists.
- `Runtime/src/SagaRuntime/RuntimeAssetBootstrap.cpp` exists.
- `Tests/Unit/Runtime/RuntimeAssetBootstrapTests.cpp` exists.
- `RuntimeAssetBootstrapTests` is registered as a dedicated CTest target.
- `RuntimeAssetBootstrapTests` is labelled `unit;runtime;asset;package`.
- A valid RuntimeSmoke package registers exactly one asset.
- Lookup works by `AssetKey` and `AssetId`.
- Missing package manifest, missing identity manifest, missing cooked asset,
  bad package base directory, domain mismatch, and registry collision fail
  without partial registry mutation.
- `RuntimeAssetBootstrapOptions` exposes package manifest path and
  `packageBaseDirectory` explicitly.
- Runtime bootstrap diagnostics carry severity, category, message, manifest
  path, and optional path/resource context.
- Focused Phase 5J tests passed.
- Full CTest remains unverified.

## What Phase 5J Proves

Phase 5J proves:

- Runtime owns a package asset bootstrap facade.
- Future Apps/Runtime wiring does not need to know Engine bootstrap policy directly.
- Runtime can bootstrap into a caller-owned `AssetRegistry`.
- `packageBaseDirectory` and diagnostics are part of the Runtime-facing contract.

## What Phase 5J Does Not Prove

Phase 5J does not prove:

- Apps/Runtime calls `RuntimeAssetBootstrap`.
- RuntimeServiceRegistry owns an asset service.
- AssetRegistry lifetime is integrated into the app runtime loop.
- Publish readiness relates to package asset bootstrap.
- Source discovery/import/cook exists.

## Phase 5K Candidate Evaluation

### A. RuntimeAssetBootstrap Diagnostics Report Facade

- Goal: add Runtime-owned summaries/views around `RuntimeAssetBootstrapResult`.
- Bundle contents: Runtime API, focused Runtime tests, docs.
- Files likely touched: `Runtime/include/SagaRuntime`, `Runtime/src/SagaRuntime`,
  `Tests/Unit/Runtime`, `cmake/modules/SagaTests.cmake`, docs.
- Behavior: Runtime API addition only.
- Tests needed: summary state, diagnostic counts, view metadata preservation.
- Risk: low-medium.
- Value: prepares Apps/Runtime logging/reporting without app policy duplication.
- Why now: `RuntimeAssetBootstrapResult` exists but Apps/Runtime has no reporting facade.
- Why later: only if direct app wiring were already isolated and testable.
- Rollback path: remove facade, test target, tests, and docs.

### B. Apps/Runtime Startup Asset Bootstrap Wiring Audit

- Goal: decide call site, registry lifetime, failure behavior, and Phase 5L wiring.
- Bundle contents: docs-only audit and exact implementation plan.
- Files likely touched: docs.
- Behavior: docs-only.
- Tests needed: none beyond inspection and `git diff --check`.
- Risk: low.
- Value: prevents wrong app lifecycle wiring.
- Why now: Runtime facade exists, but app integration is not yet decision-complete.
- Why later: if direct app wiring were safe in this slice.
- Rollback path: revert docs.

### C. Apps/Runtime Thin Startup Asset Bootstrap Wiring

- Goal: call `RuntimeAssetBootstrap` after startup session success and before `ClientHost::Run`.
- Bundle contents: app behavior change, local registry lifetime, logging, tests if possible.
- Files likely touched: `Apps/Runtime/main.cpp`, Runtime/app tests, docs.
- Behavior: app startup behavior changes.
- Tests needed: app helper tests or executable-level seam.
- Risk: medium.
- Value: moves package assets into actual Runtime app startup.
- Why now: Runtime facade exists.
- Why later: no focused test seam currently avoids ClientHost/render/network startup.
- Rollback path: remove app call and tests.

### D. RuntimeServiceRegistry Asset Bootstrap Service Shell

- Goal: add Runtime-owned service wrapping asset bootstrap.
- Bundle contents: service API/context changes and tests.
- Files likely touched: Runtime service lifecycle/registry surfaces, tests, docs.
- Behavior: Runtime lifecycle API changes.
- Tests needed: service context, registry lifetime, bootstrap success/failure.
- Risk: medium-high.
- Value: prepares future service ownership.
- Why now: RuntimeServiceRegistry exists.
- Why later: `IRuntimeService::Start()` has no startup context parameter.
- Rollback path: remove service/context changes and tests.

### E. Publish Readiness Package/Asset Identity Report v0

- Goal: report package/asset/identity coverage without hard-fail.
- Bundle contents: product publish report diagnostics, focused tests, docs.
- Files likely touched: Apps/Saga publish readiness surfaces, publish tests, docs.
- Behavior: report output changes only.
- Tests needed: present/missing refs report diagnostics.
- Risk: medium-low.
- Value: improves product visibility after staging/runtime proof.
- Why now: staging and Runtime facade are proven.
- Why later: Runtime app readiness is the more direct next bridge.
- Rollback path: remove report fields/tests.

### F. Source Discovery / Import / Cook Boundary Audit

- Goal: define source-to-cooked artifact ownership.
- Bundle contents: docs-only.
- Files likely touched: docs.
- Behavior: docs-only.
- Tests needed: none.
- Risk: low.
- Value: prevents cook scope creep.
- Why now: cook remains a major Phase 5 gap.
- Why later: does not advance runtime startup readiness.
- Rollback path: revert docs.

### G. UI/Document AssetKind Audit

- Goal: decide how UI/document assets enter package/runtime schema.
- Bundle contents: docs-only.
- Files likely touched: docs.
- Behavior: docs-only.
- Tests needed: none.
- Risk: low.
- Value: needed for eventual UI runtime package proof.
- Why now: Phase 5 acceptance mentions a UI document asset.
- Why later: Runtime startup bridge comes first.
- Rollback path: revert docs.

## Phase 5K Implementation Evidence

Phase 5K adds:

- `Runtime/include/SagaRuntime/RuntimeAssetBootstrapDiagnostics.hpp`
- `Runtime/src/SagaRuntime/RuntimeAssetBootstrapDiagnostics.cpp`
- `Tests/Unit/Runtime/RuntimeAssetBootstrapDiagnosticsTests.cpp`
- dedicated `RuntimeAssetBootstrapDiagnosticsTests` CTest registration labelled
  `unit;runtime;asset;package`

Confirmed behavior:

- `RuntimeAssetBootstrapDiagnostics::Summarize` classifies successful
  registration with assets as `Ready`.
- Successful bootstrap with zero registered assets is explicitly classified as
  `Empty`.
- Error diagnostics classify the report as `Blocked`.
- Summary output preserves registered asset count and diagnostic/error/warning
  counts.
- The overload that accepts `RuntimeAssetBootstrapOptions` preserves package
  manifest path and package base directory.
- Diagnostic views preserve severity, category, diagnostic id, message,
  manifest path, field/reference/item context, resource id, asset id, and
  resolved path when available.
- The diagnostics facade does not mutate `AssetRegistry` and does not call
  `RuntimeAssetBootstrap`.

## Apps/Runtime Startup Wiring Audit Decision

Current Apps/Runtime facts:

- `Apps/Runtime/main.cpp` prepares startup through `RuntimeStartupSession`.
- It logs startup diagnostics through `RuntimeStartupDiagnostics`.
- It starts a local `RuntimeServiceRegistry` shell.
- It constructs `ClientHost` after runtime service startup and then calls
  `ClientHost::Run`.
- `ClientHost` has no `AssetRegistry` constructor parameter or runtime asset
  consumption API.
- `RuntimeLaunchOptions`, `RuntimeStartupPreflightOptions`, and
  `RuntimeSessionDescriptor` do not yet propagate `packageBaseDirectory`.
- `IRuntimeService::Start()` has no startup context parameter.

Decision:

- Do not wire Apps/Runtime asset bootstrap in Phase 5K.
- The eventual call site should be after `RuntimeStartupSession::Prepare`
  succeeds and before `ClientHost` construction.
- Apps/Runtime may own a local `AssetRegistry` for the first integration, but
  it must not pretend `ClientHost` consumes it until a real API exists.
- When a package manifest is explicitly supplied, asset bootstrap failure
  should block process startup.
- When no package manifest is supplied, existing dev preflight skip behavior
  should remain unchanged.
- Bootstrap diagnostics should be summarized by
  `RuntimeAssetBootstrapDiagnostics`; Apps/Runtime should own log wording and
  process exit policy.
- `packageBaseDirectory` should flow through Runtime launch/preflight/session
  options before or during the app wiring slice.
- RuntimeServiceRegistry asset service should wait until the registry has a
  startup context/lifetime contract.

## Recommended Phase 5L Slice

Pick Phase 5L: Apps/Runtime Thin Asset Bootstrap Wiring Prep + Integration.

Required Phase 5L bundle:

- add `packageBaseDirectory` to `RuntimeLaunchOptions`,
  `RuntimeStartupPreflightOptions`, and `RuntimeSessionDescriptor`
- pass package base directory through `RuntimeStartupPreflight` to
  `RuntimeStartupGate`
- add an app-local helper seam in `Apps/Runtime` that can be tested without
  running `ClientHost`
- have Apps/Runtime call `RuntimeAssetBootstrap` after startup session success
  and before `ClientHost` construction
- keep `AssetRegistry` app-local until `ClientHost` has a real consumption API
- use `RuntimeAssetBootstrapDiagnostics` for reporting

Out of scope for Phase 5L:

- no ClientHost constructor/API change unless a separate tested consumption
  contract is introduced
- no RuntimeServiceRegistry context rewrite
- no render/UI/network lifecycle movement
- no source discovery/import/cook
- no publish hard-fail
- no UI/document AssetKind
- no MultiplayerSandbox package

## Phase 5K Verification Strategy

Verification passed:

- `git diff --check`
- targeted `rg` for `RuntimeAssetBootstrapDiagnostics`,
  `RuntimeAssetBootstrap`, `Apps/Runtime`, `RuntimeStartupSession`,
  `RuntimeServiceRegistry`, `ClientHost`, `AssetRegistry`,
  `packageBaseDirectory`, and Phase 5K docs
- `Tools/Forge/bin/forge nix build --target RuntimeAssetBootstrapDiagnosticsTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R RuntimeAssetBootstrapDiagnosticsTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R RuntimeAssetBootstrapTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "RuntimePackageSmokeTests|GeneratedRuntimeSmokePackageTests|SagaPackageStagingTests|PackageManifestWriterTests|AssetManifestWriterTests|AssetIdentityManifestWriterTests" --output-on-failure -j 1`

Label inventories after Phase 5K:

- `runtime`: 21 tests, with existing registered-but-unbuilt runtime executable warnings
- `asset`: 11 tests
- `package`: 9 tests
- `tools`: 7 tests
- `integration`: 3 tests

Full CTest remains unverified unless explicitly run.
