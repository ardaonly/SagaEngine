# Phase 5H Closure And Phase 5I Runtime Asset Startup Audit

> Last updated: 2026-05-25
> Status: Phase 5H closed as Package Staging Generated Manifest Integration established
> Decision: open Phase 5I as a report-only Runtime Asset Startup Integration Audit

This checkpoint closes Phase 5H narrowly. It does not close source
discovery/import/cook, Apps/Runtime startup AssetRegistry integration, publish
gate implementation, UI/document AssetKind, MultiplayerSandbox package
readiness, or full AssetPipeline completion.

Full CTest remains unverified.

## Phase 5H Closure Summary

Phase 5H can close as:

- accepted as package staging generated manifest integration
- accepted as product staging bridge to `PackageManifestWriter`
- accepted as staged/generated package validation/bootstrap proof
- not accepted as source discovery/import/cook
- not accepted as Apps/Runtime startup AssetRegistry integration
- not accepted as publish gate implementation
- not accepted as UI/document AssetKind
- not accepted as MultiplayerSandbox package readiness
- not accepted as full AssetPipeline completion

## Phase 5H Completion Evidence

Confirmed evidence:

- `Apps/Saga/SagaPackageStaging.cpp` discovers optional `Build/Manifests/asset_identity.json`.
- `Apps/Saga/SagaPackageStaging.cpp` emits client/server package manifests through `SagaAssetPipeline::PackageManifestWriter`.
- `RuntimeStartupGate` supports optional `packageBaseDirectory` while preserving default package-folder-relative behavior.
- `RuntimeAssetRegistryBootstrapper` supports optional `packageBaseDirectory` while preserving default package-folder-relative behavior.
- `SagaPackageStagingTests` proves staged/generated output can validate through `RuntimeStartupGate` and bootstrap one asset through `RuntimeAssetRegistryBootstrapper`.
- Focused verification passed:
  - `git diff --check`
  - `SagaPackageStagingTests` build
  - `PackageManifestWriterTests` build
  - `GeneratedRuntimeSmokePackageTests` build
  - `SagaPackageStagingTests` CTest
  - Phase 5 regression CTest regex: 6/6 passed
- Label inventories were collected after Phase 5H:
  - `asset`: 9 tests
  - `runtime`: 19 tests, with existing registered-but-unbuilt runtime executable warnings
  - `tools`: 7 tests
  - `package`: 7 tests
  - `integration`: 3 tests
- Full CTest remains unverified.

## What Phase 5H Proves

Phase 5H proves:

- Product staging now uses the package writer contract instead of hand-building package manifest JSON.
- Generated asset and identity manifests can be referenced by staged package manifests.
- Staged/generated package output can be accepted by `RuntimeStartupGate`.
- Staged/generated package output can register assets through `RuntimeAssetRegistryBootstrapper`.
- The package writer chain has moved from isolated tool proof into `Apps/Saga` product staging.

## What Phase 5H Does Not Prove

Phase 5H does not prove:

- `Apps/Runtime` bootstraps `AssetRegistry` during startup.
- Runtime owns an asset registry service yet.
- Publish readiness reports generated asset/identity coverage yet.
- Source discovery/import/cook exists.
- UI/document `AssetKind` exists.
- MultiplayerSandbox package exists.
- Full test health.

## Phase 5 Production Path Gap Analysis

| Production path step | State | Current owner | Desired owner | Risk | Likely phase |
|---|---:|---|---|---|---|
| Source content | partial | `Content/`, fixtures | project/content ownership | no real source inventory | 5K+ audit |
| AssetPipeline discovery/import/cook | missing | none | `Tools/AssetPipeline` | largest scope, can balloon | later 5 |
| AssetIdentityGenerator | implemented | `Tools/AssetPipeline` | same | not source/cook driven | done |
| AssetIdentityManifestWriter | implemented | `Tools/AssetPipeline` | same | relies on external orchestration | done |
| AssetManifestWriter | implemented | `Tools/AssetPipeline` | same | relies on external cooked artifact inputs | done |
| PackageManifestWriter | implemented | `Tools/AssetPipeline` | same | schema writer only | done |
| Apps/Saga package staging | implemented bridge | `Apps/Saga` | product staging using writer contracts | still discovery-only, no cook | 5H done |
| RuntimeStartupGate validation | implemented | Engine runtime startup surface | same | Runtime facade does not expose all new base-dir policy | 5I audit |
| RuntimeAssetRegistryBootstrapper | implemented | Engine Resources | same | only tests call registration path | 5I audit |
| AssetRegistry | implemented | Engine Resources | same | no Runtime startup ownership | 5I/5J |
| Apps/Runtime startup consumption | partial | `Apps/Runtime` + `SagaRuntime` | Runtime app shell with explicit asset ownership | lifecycle ownership unclear | 5I audit first |
| Publish readiness | partial | `Apps/Saga` | product publish service | validates package refs, no generated coverage report | 5J/5K |
| MultiplayerSandbox package | missing | none | future fixture/product proof | can expand into gameplay/server work | later 5 or later phase |

## Phase 5I Candidate Bundled Slices

### A. Runtime Asset Startup Integration Audit

- Goal: decide exact Runtime ownership for asset registry population.
- Bundle contents: inspect `RuntimeStartupSession`, `RuntimeStartupPreflight`, `RuntimeServiceRegistry`, `Apps/Runtime`, `RuntimeAssetRegistryBootstrapper`, and `AssetRegistry` lifecycle.
- Files likely touched: docs only.
- Behavior: docs-only.
- Tests needed: none beyond `git diff --check` and targeted inspection.
- Risk: low.
- Value: prevents premature Runtime service/API shape.
- Why now: staging-to-runtime proof exists, but app startup ownership is still unresolved.
- Why later: delays implementation by one slice.
- Rollback plan: revert docs.

### B. Runtime Asset Registry Service Shell / Bootstrap Contract

- Goal: add a Runtime-owned facade/service shell that validates a package and bootstraps `AssetRegistry`.
- Bundle contents: new Runtime facade around package manifest load plus `RuntimeAssetRegistryBootstrapper`, focused Runtime tests, optional Apps/Runtime local adapter.
- Files likely touched: `Runtime/include/SagaRuntime`, `Runtime/src/SagaRuntime`, `Tests/Unit/Runtime`, maybe `Apps/Runtime/main.cpp`, CMake.
- Behavior: Runtime API addition, possibly app startup behavior if wired.
- Tests needed: package validation and registry bootstrap through Runtime-owned facade.
- Risk: medium, because `IRuntimeService::Start()` has no context and `Apps/Runtime` has no durable asset registry owner.
- Value: highest direct Runtime bridge after staging.
- Why now: all required Engine package/bootstrap primitives exist.
- Why later: lifetime and ownership should be decided first.
- Rollback plan: remove facade, tests, CMake entries, and any Apps/Runtime adapter wiring.

### C. Publish Readiness Package/Asset Identity Report v0

- Goal: report generated package/asset/identity readiness without new hard-fail policy.
- Bundle contents: add report metadata/diagnostics for identity refs and asset manifest coverage; preserve current package blocking rules.
- Files likely touched: `Apps/Saga/SagaPublishReadiness.*`, publish tests, docs.
- Behavior: publish report output changes, no hard-fail expansion unless existing package loader fails.
- Tests needed: publish readiness focused tests for present/missing identity and asset refs.
- Risk: medium-low.
- Value: improves product visibility after staging.
- Why now: staging now emits writer-owned package manifests.
- Why later: Runtime startup ownership is the more direct missing bridge.
- Rollback plan: remove report-only fields/tests and restore previous publish report shape.

### D. Source Discovery / Import / Cook Boundary Audit

- Goal: define source-to-cooked-artifact ownership before implementing cook.
- Bundle contents: audit `Content/`, AssetPipeline writers, package staging expectations, and future Forge/Prism/SDE boundaries.
- Files likely touched: docs only.
- Behavior: docs-only.
- Tests needed: none.
- Risk: low.
- Value: prevents cook scope creep.
- Why now: source/cook remains the largest Phase 5 gap.
- Why later: does not move staged packages into Runtime/publish usage.
- Rollback plan: revert docs.

### E. UI/Document AssetKind Audit

- Goal: decide whether UI/document assets need Runtime `AssetKind`.
- Bundle contents: audit `Content/UI`, RmlUi backend, loader/registry kind tokens, and editor workflow expectations.
- Files likely touched: docs only.
- Behavior: docs-only.
- Tests needed: none.
- Risk: low.
- Value: clarifies first real UI asset path.
- Why now: Phase 5 acceptance mentions a UI document asset.
- Why later: not the immediate staging-to-runtime bridge.
- Rollback plan: revert docs.

### F. MultiplayerSandbox Package Fixture Audit

- Goal: define future MultiplayerSandbox package needs without implementing it.
- Bundle contents: audit runtime/server package needs, content needs, missing cooked assets, and future fixture shape.
- Files likely touched: docs only.
- Behavior: docs-only.
- Tests needed: none.
- Risk: low.
- Value: prepares future vertical proof.
- Why now: package readiness will matter for gameplay proof.
- Why later: no sandbox package should start before Runtime/publish bridge decisions.
- Rollback plan: revert docs.

### G. Package Staging Negative Case Expansion

- Goal: harden staging failure behavior around generated manifest refs.
- Bundle contents: add focused negative tests for invalid identity/asset refs, bad writer inputs, and bootstrap non-mutation.
- Files likely touched: `SagaPackageStagingTests`, maybe no production code.
- Behavior: mostly test-only.
- Tests needed: focused staging tests.
- Risk: low.
- Value: improves confidence.
- Why now: immediately adjacent to Phase 5H.
- Why later: less strategic than Runtime ownership decision.
- Rollback plan: remove tests and any narrow diagnostic adjustments.

## Recommended Phase 5I Slice

Pick: Runtime Asset Startup Integration Audit.

Reason:

- The next useful bridge is Runtime startup usage, but ownership is not decision-complete yet.
- `Apps/Runtime` already has a local `RuntimeServiceRegistry` shell, but the service contract has no shared startup context and no durable asset registry owner.
- `RuntimeStartupPreflight` and `RuntimeStartupSession` validate packages, but do not propagate `packageBaseDirectory` and do not return a loaded package manifest or registry bootstrap plan.
- A direct Runtime asset service shell is plausible, but implementing it now risks baking in the wrong ownership/lifetime model.
- This follows the decision rule: when Runtime ownership/lifetime is unclear, audit first.

## Phase 5I Audit Findings

Current Runtime startup shape:

- `RuntimeLaunchOptions` owns package manifest path, validation flags, and expected runtime compatibility, but not package base directory.
- `RuntimeStartupPreflight` calls `RuntimeStartupGate`, but does not pass package base directory.
- `RuntimeStartupSession::Prepare` returns a session descriptor and preflight result, but not a loaded package manifest, bootstrap plan, or populated `AssetRegistry`.
- `Apps/Runtime` owns CLI parsing, logging, process exit policy, local runtime service registry creation, and `ClientHost` construction.
- `RuntimeServiceRegistry` owns lifecycle ordering and diagnostics, but `IRuntimeService::Start()` has no startup context parameter.
- `RuntimeAssetRegistryBootstrapper` can validate/register package assets when given a loaded package manifest, an `AssetRegistry`, package manifest path, and optional package base directory.

Ownership decision for the next implementation slice:

- Runtime should expose a narrow asset bootstrap facade before wiring app startup.
- The facade should own the package validation/bootstrap orchestration, but not source discovery/import/cook.
- The first implementation should let callers provide an `AssetRegistry&` so registry lifetime remains explicit and no hidden global registry is introduced.
- Apps/Runtime should not create an asset bootstrap service until the facade is tested.
- `RuntimeServiceRegistry` should not receive a generic context parameter in the first slice; that would broaden lifecycle API design beyond Phase 5.
- `packageBaseDirectory` should be added to Runtime launch/preflight/session options so staged project-relative package refs can flow through Runtime facades.

Recommended Phase 5J implementation candidate:

- Add `SagaRuntime::RuntimeAssetBootstrap` facade.
- Input: package manifest path, optional package base directory, expected domain, validation flags, expected runtime compatibility version, and target `AssetRegistry&`.
- Behavior: validate package through `RuntimeStartupGate`, load the package manifest through `PackageManifestLoader`, then register package assets through `RuntimeAssetRegistryBootstrapper::RegisterPackageAssetsFromPackageIdentityManifest`.
- Output: structured Runtime diagnostics and registered asset count.
- Tests: Runtime-only focused tests using RuntimeSmoke/generated fixture copies; prove success, missing identity failure, missing asset file failure, and no registry mutation on failure.
- Out of scope for Phase 5J: Apps/Runtime wiring, ClientHost movement, render/UI/network services, publish hard-fail, source discovery/import/cook.

## Phase 5I Test Strategy

Phase 5I is docs-only. No behavior tests are required.

The audit defines the first safe implementation slice:

- `RuntimeServiceRegistry` should not own the asset bootstrap service yet.
- `Apps/Runtime` should not bootstrap `AssetRegistry` yet.
- Runtime should first expose and test a facade that can populate a caller-owned `AssetRegistry`.
- Runtime launch/preflight/session should later carry `packageBaseDirectory` so staged project-relative manifests can validate through Runtime-owned APIs.

## Phase 5I Verification Strategy

For this audit:

- `git diff --check`
- targeted `rg` over runtime startup/session/preflight/service registry, asset registry/bootstrapper, Apps/Runtime, package staging, publish readiness, CMake, and docs
- label inventories:
  - `asset`
  - `runtime`
  - `tools`
  - `package`
  - `integration`

If Phase 5J implements the Runtime asset bootstrap facade, run:

- focused build with `--jobs=1`
- focused CTests with `-j 1 --output-on-failure`
- practical Phase 5 regressions:
  - `SagaPackageStagingTests`
  - `PackageManifestWriterTests`
  - `GeneratedRuntimeSmokePackageTests`
  - `AssetManifestWriterTests`
  - `AssetIdentityManifestWriterTests`
  - `RuntimePackageSmokeTests`

Full CTest remains unverified unless explicitly run.

## Risks And Non-Goals

Risks:

- Apps/Runtime migration too early
- Runtime service ownership unclear
- publish gate overreach
- source discovery/import/cook scope creep
- UI/editor workflow creep
- MultiplayerSandbox scope creep
- product staging/writer contract drift
- broad CMake/test restructuring
- full test health remaining unverified

Phase 5I non-goals:

- no Runtime asset service implementation
- no Apps/Runtime behavior change
- no `ClientHost` refactor
- no publish hard-fail
- no source discovery/import/cook
- no editor/UI asset workflow
- no MultiplayerSandbox package implementation
- no broad CMake/test restructuring

## Phase 5 Closure Direction

Do not close Phase 5 yet.

Before Phase 5 closure, the project still needs:

- runtime asset startup ownership decision translated into implementation
- publish readiness relationship decided or report-only path started
- source discovery/import/cook explicitly deferred or scoped
- UI/document and MultiplayerSandbox gaps documented or deferred
- generated writer/staging/runtime proof retained

## Verification Summary

Commands run for this closure/audit:

- `git status --short`
- `git diff --check`
- `git diff --name-only`
- `git diff --stat`
- targeted `rg` over package staging, package writer, identity refs, runtime startup/bootstrap, Runtime services, publish readiness, CMake, docs, cook/UI/MultiplayerSandbox terms
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L asset`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L runtime`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L tools`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L package`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L integration`

Results recorded:

- Worktree is broadly dirty with unrelated tracked and untracked changes.
- `git diff --check` passed.
- Phase 5H staging bridge files, Runtime base-directory hooks, and focused tests are present.
- Runtime label inventory still reports existing registered-but-unbuilt runtime executable warnings.
- Full CTest remains unverified.

## Decision Gate

Accept Phase 5H and implement Phase 5I as this Runtime Asset Startup
Integration Audit.

Do not revise Phase 5H before moving forward.

Do not start Runtime asset service/bootstrap implementation until Phase 5J,
using the ownership, lifecycle, API shape, and test boundaries defined above.
