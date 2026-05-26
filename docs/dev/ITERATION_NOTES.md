# Iteration Notes

## 2026-05-24 - Recovery Start

This iteration starts the SagaEngine recovery track.

Scope for the first implementation slice is Phase 0 only:

- create or update `docs/roadmaps/ENGINE_RECOVERY_ROADMAP.md`
- create or update this iteration notes file
- no code movement
- no CMake behavior change
- no hard-fail gates
- no Runtime/App ownership refactor
- no server authoritative movement implementation
- no asset pipeline implementation
- no editor Qt refactor

Known incomplete recovery items:

- Runtime is not yet a real ownership layer.
- Server gameplay is not yet authoritative.
- Gameplay handlers still include stub behavior.
- Editor still contains substantial scaffold.
- Public Editor API still leaks Qt details.
- CMake target boundaries are too loose.
- Tests are not yet isolated by subsystem.
- Asset pipeline ownership is incomplete.
- Publish/package readiness gate is missing.
- Full test health is unverified.

These are explicit technical debt items, not shipped capability.

## 2026-05-24 - Phase 1 Build/Test Baseline Slice

Scope for this slice is Build/Test Baseline only:

- add a SagaEngine-specific `forge test --suite <name>` layer
- preserve existing `forge test --label <regex>` behavior
- map required suites to CTest label filters
- add safe local documentation with `--jobs 1` examples
- keep `stress`, `slow`, load, timing-sensitive, and long-running tests opt-in

Out of scope for this slice:

- no CMake target dependency boundary cleanup
- no Runtime/App ownership refactor
- no server-authoritative movement implementation
- no asset pipeline behavior implementation
- no editor Qt boundary refactor
- no hard-fail architecture gates

Full test health remains unverified until the safe suite commands are actually run and pass.

## 2026-05-24 - Phase 2A CMake Boundary Inventory Slice

Scope for this slice is report-only CMake / target boundary inventory:

- add `docs/architecture/CMAKE_BOUNDARY_INVENTORY.md`
- record current PUBLIC dependency, Qt exposure, product/app transitive dependency, test graph, include-root, backend header-root, and recursive glob risks
- classify issues for later Phase 2B / Phase 2C / Phase 3 / Phase 6 work

Out of scope for this slice:

- no target dependency behavior changes
- no `PUBLIC` to `PRIVATE` conversions
- no hard-fail gates
- no test target splitting
- no source movement
- no Runtime/App ownership refactor
- no editor Qt refactor

Full test health remains unverified.

## 2026-05-24 - Phase 3C-2 Runtime Service Registry Slice

Scope for this slice is the next narrow Runtime lifecycle implementation:

- add a Runtime-owned service registry in `SagaRuntimeLib`
- keep service registration, ordering, and state tracking Runtime-owned
- execute start, tick, and stop through test-double services only
- preserve service lifecycle diagnostics in registry reports
- add focused Runtime service registry tests that do not depend on `Apps/Client`

Out of scope for this slice:

- no `ClientHost` movement
- no `ClientNetworkSession` split
- no `Apps/Runtime` integration
- no UI bootstrap ownership migration
- no rendering ownership migration
- no network ownership migration
- no product/app process policy movement into Runtime
- no broad Runtime/App file movement

Full test health remains unverified.

## 2026-05-24 - Phase 3C-3 Runtime Lifecycle Diagnostics Report Slice

Scope for this slice is the next Runtime-only lifecycle diagnostics contract:

- add a Runtime-owned classifier for `RuntimeServiceRegistryReport`
- expose summary state, operation success, service result counts, and diagnostic counts
- expose diagnostic views preserving severity, service id, and message
- add focused Runtime service registry diagnostics tests with `unit;runtime` labels
- keep Apps/Runtime and Apps/Client integration untouched

Out of scope for this slice:

- no `ClientHost` movement
- no `ClientNetworkSession` split
- no network, input, UI, rendering, or asset Runtime service adapters
- no `Apps/Runtime` registry shell
- no product/app logging or exit policy movement into Runtime
- no Runtime dependency on `Apps/Client`, `ClientHost`, or client config types

Full test health remains unverified.

## 2026-05-25 - Phase 3C Lifecycle Ownership Checkpoint

Scope for this slice is report-only Phase 3C closure and Phase 3-to-4 transition planning:

- add `docs/recovery/phase-03-runtime/PHASE_3C_LIFECYCLE_OWNERSHIP_CHECKPOINT.md`
- classify Phase 3C as a Runtime lifecycle foundation package, not a complete Runtime ownership layer
- record Runtime maturity as `startup-owned` / `registry-ready` with Apps/Runtime lifecycle integration still `symbolic`
- recommend Phase 3C-4 Apps/Runtime Registry Integration Shell before Phase 4
- define Phase 3 closure criteria and Phase 4 opening audit scope

Out of scope for this slice:

- no `Apps/Runtime` registry integration implementation
- no `ClientHost` movement
- no `ClientNetworkSession` split
- no real network, input, UI, rendering, asset, or world Runtime service adapters
- no server-authoritative movement implementation
- no CMake behavior changes

Decision gate:

- implement Phase 3C-4 next
- do not close Phase 3 and start Phase 4 until Apps/Runtime has a narrow registry integration shell or a documented deferral

Full test health remains unverified.

## 2026-05-25 - Phase 3C-4 Apps/Runtime Registry Integration Shell

Scope for this slice is the narrow app-facing lifecycle registry integration:

- have `Apps/Runtime` create and consume `RuntimeServiceRegistry`
- register a small app-local bootstrap lifecycle marker service
- summarize registry start/stop reports with `RuntimeServiceRegistryDiagnostics`
- keep logging wording and process exit policy in `Apps/Runtime`
- keep `ClientHost` construction and `Saga::ClientConfig` conversion unchanged

Out of scope for this slice:

- no `ClientHost` movement
- no `ClientNetworkSession` split
- no real network, input, UI, rendering, asset, or world Runtime service adapters
- no Runtime API changes
- no CLI parsing or process exit policy movement into Runtime
- no Runtime dependency on `Apps/Client`, `ClientHost`, or client config types

Full test health remains unverified.

## 2026-05-25 - Phase 3 Closure And Phase 4A Opening Checkpoint

Scope for this slice is documentation-only closure and transition:

- add `docs/recovery/phase-03-runtime/PHASE_3_CLOSURE_AND_PHASE_4A_OPENING_CHECKPOINT.md`
- close Phase 3 only as Runtime startup/lifecycle foundation established
- preserve unresolved Phase 3 debt for `ClientHost`, `ClientNetworkSession`, real Runtime service ownership, and `SagaRuntime` reuse of `Apps/Client/ClientHost.h/.cpp`
- mark Phase 4A open only as a report-only Server Authoritative Movement Audit
- define Phase 4A audit scope, expected outputs, non-goals, and first likely implementation candidate

Out of scope for this slice:

- no Runtime/App ownership refactor
- no `ClientHost` movement
- no `ClientNetworkSession` split
- no server-authoritative movement implementation
- no broad server/network rewrite
- no server/client file movement
- no CMake behavior changes

Decision gate:

- begin Phase 4A audit planning/work next
- do not revise Phase 3 unless new evidence contradicts the Runtime boundary or Phase 3C-4 integration

Full test health remains unverified.

## 2026-05-25 - Phase 4A Server Authoritative Movement Audit

Scope for this slice is report-only server authority audit:

- add `docs/recovery/phase-04-server-authority/PHASE_4A_SERVER_AUTHORITATIVE_MOVEMENT_AUDIT.md`
- classify current movement authority as mixed and not yet proven
- inventory server gameplay command dispatch, server input/simulation handling, session lifecycle, replication, client input send, prediction/reconciliation, shared Engine contracts, and Runtime non-involvement
- map the current client-to-server input flow and identify that `ZoneServer::DrainInputPackets` and `StepSimulation` are not yet authoritative movement paths
- propose three narrow Phase 4B candidates and recommend an authoritative movement core test harness first

Out of scope for this slice:

- no behavior implementation
- no server-authoritative movement implementation
- no broad server/network rewrite
- no full MMO gameplay system
- no `ClientHost` refactor
- no server/client file movement
- no Runtime ownership continuation

Decision gate:

- Phase 4A audit is sufficient to move to a narrow Phase 4B implementation slice
- recommended Phase 4B slice is the authoritative movement core test harness
- production `ZoneServer` input routing, client/server transport alignment, entity ownership mapping, and accepted-state replication remain unresolved risks

Full test health remains unverified.

## 2026-05-25 - Phase 4 Closure And Phase 5A Opening Checkpoint

Scope for this slice is documentation-only closure and transition:

- add `docs/recovery/phase-04-server-authority/PHASE_4_CLOSURE_AND_PHASE_5A_OPENING_CHECKPOINT.md`
- close Phase 4 only as Server Authoritative Movement Foundation established
- preserve unresolved Phase 4 debt for `ReplicationManager` movement dirty integration, accepted-state snapshot serialization, client reconciliation proof, `ClientHost` / `ClientNetworkSession` cleanup, and full end-to-end multiplayer authority
- mark Phase 5A open only as a report-only Asset Pipeline / Package Runtime Readiness Audit
- define Phase 5A audit scope, expected outputs, non-goals, and first likely implementation candidate

Out of scope for this slice:

- no asset pipeline implementation
- no package format rewrite
- no Runtime asset service migration
- no publish gate implementation
- no broad CMake boundary hardening
- no UI/editor asset workflow implementation
- no additional server-authoritative movement implementation

Decision gate:

- Phase 4 is closed only as Server Authoritative Movement Foundation established
- Phase 5A opens as report-only Asset Pipeline / Package Runtime Readiness Audit
- do not revise Phase 4 unless new evidence contradicts the ZoneServer authority shell or movement dirty replication intent bridge

Full test health remains unverified.

## 2026-05-25 - Phase 5B Runtime Package Smoke Fixture

Scope for this slice is the minimal package fixture/runtime smoke proof:

- add `Tests/Fixtures/Packages/RuntimeSmoke` with a client package manifest, asset manifest, asset identity manifest, and one dummy cooked texture artifact
- add focused `RuntimePackageSmokeTests` labelled `unit;runtime;asset;package`
- prove the checked-in fixture passes `RuntimeStartupGate`
- prove package identity plus asset manifest registration inserts exactly one `AssetRegistry` entry by `AssetKey` and `AssetId`
- prove missing identity coverage and missing cooked asset failures do not mutate the registry

Out of scope for this slice:

- no asset cook implementation
- no AssetPipeline manifest writer
- no package format rewrite
- no Runtime app startup asset registry wiring
- no server package consumption
- no publish gate hard-fail
- no UI/document asset kind or editor workflow

Full test health remains unverified.

## 2026-05-25 - Phase 5C Asset Identity Runtime Contract

Scope for this slice is the focused AssetPipeline identity generator to Runtime
loader/bootstrap contract proof:

- add `AssetIdentityRuntimeContractTests` labelled `unit;asset;tools;runtime;package`
- serialize `AssetIdentityGenerator` output in a test-only helper to the Runtime asset identity manifest JSON shape
- prove Runtime `AssetIdentityManifestLoader` accepts generated mappings
- prove previous IDs survive through Runtime resolver construction
- prove new IDs allocate deterministically and load through Runtime
- overwrite only the RuntimeSmoke fixture identity manifest in a temp copy and prove `RuntimeAssetRegistryBootstrapper` registers the fixture asset
- prove duplicate input keys, invalid previous ID `0`, and duplicate previous IDs fail in the generator before Runtime serialization

Out of scope for this slice:

- no production asset identity manifest writer
- no asset manifest generation
- no package manifest generation
- no asset cook implementation
- no `Apps/Runtime` asset registry service wiring
- no publish gate hard-fail
- no UI/document asset kind or editor workflow

Full test health remains unverified.

## 2026-05-25 - Phase 5D Asset Identity Manifest Writer

Scope for this slice is the production AssetPipeline identity manifest writer:

- add `AssetIdentityManifestWriter` under `Tools/AssetPipeline`
- write generated identity mappings to the Runtime asset identity manifest schema
- emit `schemaVersion: 1` and `mappings` entries with `assetKey` and `assetId`
- reject empty keys, reserved ID `0`, duplicate keys, and duplicate IDs before writing
- add focused `AssetIdentityManifestWriterTests` labelled `unit;asset;tools;runtime`
- prove writer output loads through Runtime `AssetIdentityManifestLoader`
- prove generator stable ID reuse survives writer output and Runtime resolver construction

Out of scope for this slice:

- no AssetPipeline CLI
- no source discovery or asset import
- no asset cook implementation
- no asset manifest generation
- no package manifest generation
- no `Apps/Runtime` asset registry service wiring
- no publish gate hard-fail
- no UI/document asset kind or editor workflow

Full test health remains unverified.

## 2026-05-24 - Phase 2B-2 Runtime Collaboration Visibility Slice

Scope for this slice is one narrow Runtime CMake boundary correction:

- keep `SagaRuntimeLib` publicly linked to `SagaEngine` and `SagaShared`
- move the direct `SagaCollaboration` link on `SagaRuntimeLib` from `PUBLIC` to `PRIVATE`
- leave Runtime/App physical ownership drift untouched for Phase 3

Out of scope for this slice:

- no broad dependency cleanup
- no hard-fail gates
- no recursive glob rewrite
- no test executable split
- no Runtime/App ownership refactor
- no server-authoritative movement implementation
- no asset pipeline implementation
- no publish gate implementation
- no source movement

Full test health remains unverified.

## 2026-05-25 - Phase 5E Asset Manifest Writer

Scope for this slice is the production AssetPipeline asset manifest writer:

- add `AssetManifestWriter` under `Tools/AssetPipeline`
- write runtime-ready asset entries to the Runtime asset manifest schema
- emit `schemaVersion: 1` and `assets` entries with `id`, `kind`, and `path`
- preserve optional Runtime metadata fields: `hash`, `dependencies`, `memoryEstimateBytes`, `streamingGroup`, `platform`, and `profile`
- reject empty ids, unsupported kind tokens, unsafe paths, duplicate ids, and empty dependency entries before writing
- add focused `AssetManifestWriterTests` labelled `unit;asset;tools;runtime;package`
- prove writer output loads through Runtime `AssetManifestLoader`
- prove writer output can pass Runtime cooked-file validation without invoking an asset cook
- prove generated asset and identity manifests can bootstrap one `AssetRegistry` entry through `RuntimeAssetRegistryBootstrapper`

Out of scope for this slice:

- no AssetPipeline CLI
- no source discovery or asset import
- no asset cook implementation
- no package manifest writer or package staging rewrite
- no `Apps/Runtime` asset registry service wiring
- no publish gate hard-fail
- no UI/document asset kind or editor workflow
- no MultiplayerSandbox package readiness

Full test health remains unverified.

## 2026-05-25 - Phase 5F Generated RuntimeSmoke Manifest Replacement

Scope for this slice is the focused proof that production AssetPipeline writers
can replace the RuntimeSmoke package fixture manifests:

- add `GeneratedRuntimeSmokeManifestTests` labelled `unit;asset;tools;runtime;package`
- copy `Tests/Fixtures/Packages/RuntimeSmoke` to a temp package root
- overwrite only `Manifests/asset_identity.json` with `AssetIdentityManifestWriter`
- overwrite only `Manifests/assets.json` with `AssetManifestWriter`
- keep `package_manifest.client.json` unchanged so it references the generated manifests
- prove `RuntimeStartupGate` accepts the temp package with generated manifests
- prove `RuntimeAssetRegistryBootstrapper` registers the generated identity-backed texture
- prove missing generated identity or asset manifest files fail without registry mutation

Out of scope for this slice:

- no AssetPipeline CLI
- no source discovery or asset import
- no asset cook implementation
- no package manifest writer or package staging rewrite
- no `Apps/Runtime` asset registry service wiring
- no publish gate hard-fail
- no UI/document asset kind or editor workflow
- no MultiplayerSandbox package readiness

Full test health remains unverified.

## 2026-05-25 - Phase 5G Generated RuntimeSmoke Package

Scope for this slice is the package manifest writer contract and full generated
RuntimeSmoke package proof:

- add `PackageManifestWriter` under `Tools/AssetPipeline`
- write Runtime package manifests with schema version `1`, package metadata,
  optional `assetIdentityManifest`, required `assetManifests` and
  `artifactManifests` arrays, and optional `packageHash`
- keep the writer API tool-side only; it does not depend on Runtime package
  value types
- reject empty required fields, unsupported package kinds, unsafe manifest
  paths, empty manifest ref ids, and duplicate manifest ref ids before writing
- add `PackageManifestWriterTests` labelled `unit;asset;tools;runtime;package`
- add `GeneratedRuntimeSmokePackageTests` labelled
  `unit;asset;tools;runtime;package`
- generate `Manifests/asset_identity.json`, `Manifests/assets.json`, and
  `package_manifest.client.json` in a RuntimeSmoke temp package with production
  AssetPipeline writers
- prove `RuntimeStartupGate` accepts the fully generated package
- prove `RuntimeAssetRegistryBootstrapper` registers one generated
  identity-backed texture and lookup works by `AssetKey` and `AssetId`
- prove missing package, asset, and identity manifest references fail
  predictably, and invalid generated asset manifest content fails without
  registry mutation

Out of scope for this slice:

- no AssetPipeline CLI
- no source discovery or asset import
- no asset cook implementation
- no package staging rewrite
- no `Apps/Runtime` asset registry service wiring
- no publish gate hard-fail
- no UI/document asset kind or editor workflow
- no MultiplayerSandbox package readiness

Full test health remains unverified.

## 2026-05-25 - Phase 5H Package Staging Generated Manifest Integration

Scope for this slice is the product package staging bridge to the generated
AssetPipeline writer chain:

- make `SagaPackageStagingService` emit client/server package manifests through
  `SagaAssetPipeline::PackageManifestWriter`
- keep `Apps/Saga` as the staging orchestrator; it discovers existing generated
  manifest files and asks the AssetPipeline writer to materialize package
  manifests
- discover optional `Build/Manifests/asset_identity.json` and pass it as
  `assetIdentityManifest` when present
- preserve existing asset manifest discovery for `Build/Manifests/assets.json`
  and `Build/Manifests/asset_manifest.json`
- preserve artifact manifest discovery and script artifact manifest staging
- convert package writer diagnostics into existing product
  `PackageStageManifestInvalid` diagnostics
- extend runtime validation/bootstrap options with an explicit package base
  directory so staged project-relative package references can be validated
  without changing current default package-folder-relative behavior
- add focused package staging coverage that generates identity and asset
  manifests with AssetPipeline writers, stages a package, validates it through
  `RuntimeStartupGate`, and bootstraps one texture into `AssetRegistry`

Out of scope for this slice:

- no source discovery or asset import
- no asset cook implementation
- no full package staging rewrite
- no `Apps/Runtime` asset registry service wiring
- no publish gate hard-fail
- no UI/document asset kind or editor workflow
- no MultiplayerSandbox package readiness
- no AssetPipeline CLI

Full test health remains unverified.

## 2026-05-25 - Phase 5H Closure And Phase 5I Runtime Asset Startup Audit

Scope for this slice is docs-only closure and transition:

- add `docs/recovery/phase-05-assets-runtime/PHASE_5H_CLOSURE_AND_PHASE_5I_RUNTIME_ASSET_STARTUP_AUDIT.md`
- close Phase 5H only as package staging generated manifest integration
- preserve Phase 5H claims as product staging bridge to `PackageManifestWriter`
  and staged/generated package validation/bootstrap proof
- mark Phase 5H as not closing source discovery/import/cook, Apps/Runtime
  startup AssetRegistry integration, publish gate implementation, UI/document
  AssetKind, MultiplayerSandbox package readiness, or full AssetPipeline
  completion
- open Phase 5I as a report-only Runtime Asset Startup Integration Audit
- decide that Runtime needs a tested asset bootstrap facade before Apps/Runtime
  startup wiring or RuntimeServiceRegistry service integration
- recommend Phase 5J as a narrow `SagaRuntime::RuntimeAssetBootstrap`
  facade over `RuntimeStartupGate`, `PackageManifestLoader`, and
  `RuntimeAssetRegistryBootstrapper`

Out of scope for this slice:

- no Runtime asset service implementation
- no Apps/Runtime behavior change
- no ClientHost refactor
- no publish hard-fail
- no source discovery/import/cook
- no editor/UI asset workflow
- no MultiplayerSandbox package implementation
- no broad CMake/test restructuring

Full test health remains unverified.

## 2026-05-25 - Phase 5J Runtime Asset Bootstrap Facade

Scope for this slice is the narrow Runtime-owned package asset bootstrap
contract:

- add `SagaRuntime::RuntimeAssetBootstrap`
- accept package manifest path, optional package base directory, expected
  Runtime startup domain, validation flags, and runtime compatibility token
- validate package startup through `RuntimeStartupGate`
- register identity-backed package assets through
  `RuntimeAssetRegistryBootstrapper`
- populate a caller-owned `AssetRegistry&`
- return Runtime-facing diagnostics with severity, category, diagnostic id,
  message, path, reference/item context, resource id, asset id, and resolved
  path where available
- add dedicated `RuntimeAssetBootstrapTests` labelled
  `unit;runtime;asset;package`

Accepted evidence:

- valid RuntimeSmoke package registers exactly one asset
- lookup works by `AssetKey` and `AssetId`
- missing package manifest fails without registry mutation
- missing identity manifest fails without registry mutation
- missing cooked asset fails without registry mutation when file validation is
  enabled
- mismatched package base directory fails without registry mutation
- startup domain mismatch fails through startup validation without registry
  mutation
- registry collision fails through bootstrap diagnostics without partial
  mutation

Verification:

- `git diff --check`
- `Tools/Forge/bin/forge nix build --target RuntimeAssetBootstrapTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R RuntimeAssetBootstrapTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "RuntimePackageSmokeTests|GeneratedRuntimeSmokePackageTests|SagaPackageStagingTests" --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "RuntimeAssetBootstrapTests|RuntimePackageSmokeTests|GeneratedRuntimeSmokePackageTests|SagaPackageStagingTests|PackageManifestWriterTests|AssetManifestWriterTests|AssetIdentityManifestWriterTests" --output-on-failure -j 1`

Label inventory after this slice:

- `asset`: 10 tests
- `runtime`: 20 tests, with existing registered-but-unbuilt runtime executable
  warnings
- `tools`: 7 tests
- `package`: 8 tests
- `integration`: 3 tests

Out of scope for this slice:

- no Apps/Runtime startup wiring
- no RuntimeServiceRegistry asset service
- no ClientHost, UI, rendering, or networking lifecycle movement
- no publish readiness report or hard-fail gate
- no source discovery/import/cook
- no UI/document AssetKind
- no MultiplayerSandbox package implementation

Recommended next slice:

- Apps/Runtime Startup Asset Bootstrap Wiring Audit before app behavior changes

Full test health remains unverified.

## 2026-05-25 - Phase 5K Runtime Asset Startup Readiness Bundle

Scope for this slice is the bounded Runtime diagnostics/report facade plus an
Apps/Runtime startup wiring decision:

- close Phase 5J as Runtime-owned package asset bootstrap facade
- add `RuntimeAssetBootstrapDiagnostics`
- summarize `RuntimeAssetBootstrapResult` as `Ready`, `Blocked`, or `Empty`
- expose diagnostic views for app-owned logging
- preserve package manifest path and package base directory when options are
  supplied to the summary helper
- add dedicated `RuntimeAssetBootstrapDiagnosticsTests` labelled
  `unit;runtime;asset;package`
- add `docs/recovery/phase-05-assets-runtime/PHASE_5K_RUNTIME_ASSET_STARTUP_READINESS.md`
- decide that Apps/Runtime asset bootstrap should eventually run after
  `RuntimeStartupSession::Prepare` succeeds and before `ClientHost`
  construction

Accepted evidence:

- successful bootstrap result with registered assets summarizes as `Ready`
- successful zero-asset result summarizes as `Empty`
- error diagnostics summarize as `Blocked`
- diagnostic views preserve severity, category, diagnostic id, message,
  manifest path, field/reference/item context, resource id, asset id, and
  resolved path where available
- diagnostics helpers do not mutate `AssetRegistry` and do not call
  `RuntimeAssetBootstrap`

Apps/Runtime wiring decision:

- no Apps/Runtime behavior change in Phase 5K
- direct wiring is deferred because `RuntimeLaunchOptions`,
  `RuntimeStartupPreflightOptions`, and `RuntimeSessionDescriptor` do not yet
  propagate `packageBaseDirectory`
- `ClientHost` has no `AssetRegistry` consumption API
- `IRuntimeService::Start()` has no startup context parameter, so
  RuntimeServiceRegistry asset service wiring remains premature
- Phase 5L should add package base directory propagation, an app-local tested
  helper seam, and a thin Apps/Runtime caller before `ClientHost` construction

Verification:

- `git diff --check`
- `Tools/Forge/bin/forge nix build --target RuntimeAssetBootstrapDiagnosticsTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R RuntimeAssetBootstrapDiagnosticsTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R RuntimeAssetBootstrapTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "RuntimePackageSmokeTests|GeneratedRuntimeSmokePackageTests|SagaPackageStagingTests|PackageManifestWriterTests|AssetManifestWriterTests|AssetIdentityManifestWriterTests" --output-on-failure -j 1`

Label inventory after this slice:

- `runtime`: 21 tests, with existing registered-but-unbuilt runtime executable
  warnings
- `asset`: 11 tests
- `package`: 9 tests
- `tools`: 7 tests
- `integration`: 3 tests

Out of scope for this slice:

- no Apps/Runtime startup behavior change
- no ClientHost API change
- no RuntimeServiceRegistry context rewrite or asset service
- no render/UI/network lifecycle movement
- no source discovery/import/cook
- no AssetPipeline writer changes
- no package staging changes
- no publish readiness report or hard-fail gate
- no UI/document AssetKind
- no MultiplayerSandbox package implementation
- no broad CMake/test restructuring

Recommended next slice:

- Phase 5L Apps/Runtime Thin Asset Bootstrap Wiring Prep + Integration

Full test health remains unverified.

## 2026-05-25 - Phase 5L Apps/Runtime Asset Bootstrap Wiring

Scope for this slice is the narrow Runtime app startup asset bridge:

- add `packageBaseDirectory` to Runtime launch, preflight, and session
  descriptors
- pass package base directory through Runtime startup preflight into
  `RuntimeStartupGate`
- add `Apps/Runtime/RuntimeAssetStartupBootstrap` as an app-local helper seam
- have Apps/Runtime call the helper after startup preflight succeeds and before
  `ClientHost` construction
- keep the local `AssetRegistry` caller-owned and do not pass it into
  `ClientHost`
- parse `--package-base-directory <path>` in `SagaRuntime`

Accepted evidence:

- explicit package base directory resolves referenced package manifests outside
  the package manifest folder
- empty package base directory preserves existing manifest-folder-relative
  behavior
- `RuntimeLaunchOptions.packageBaseDirectory` is copied into
  `RuntimeSessionDescriptor` and flows into preflight
- no-package app helper calls skip bootstrap and leave the registry empty
- RuntimeSmoke package registers one asset into the local registry and lookup
  works by `AssetKey` and `AssetId`
- missing cooked asset and bad base directory block startup bootstrap without
  registry mutation
- diagnostics summaries and views are preserved for app logging
- helper tests build without `Apps/Client` or `ClientHost`
- `SagaRuntime` builds with the helper linked into the executable

Verification:

- `git diff --check`
- `Tools/Forge/bin/forge nix build --target RuntimeAssetStartupBootstrapTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target RuntimeStartupSessionTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target RuntimeStartupPreflightTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target RuntimeAssetBootstrapTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target RuntimeAssetBootstrapDiagnosticsTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target SagaRuntime --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "RuntimeStartupSessionTests|RuntimeStartupPreflightTests|RuntimeAssetBootstrapTests|RuntimeAssetBootstrapDiagnosticsTests|RuntimeAssetStartupBootstrapTests" --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "RuntimePackageSmokeTests|GeneratedRuntimeSmokePackageTests|SagaPackageStagingTests|PackageManifestWriterTests|AssetManifestWriterTests|AssetIdentityManifestWriterTests" --output-on-failure -j 1`

Label inventory after this slice:

- `runtime`: 22 tests, with existing registered-but-unbuilt runtime executable
  warnings
- `asset`: 12 tests
- `package`: 10 tests
- `tools`: 7 tests
- `integration`: 3 tests

Out of scope for this slice:

- no `ClientHost` API change
- no passing `AssetRegistry` into `ClientHost`
- no `ClientNetworkSession`, render, UI, or network lifecycle changes
- no RuntimeServiceRegistry asset service or context rewrite
- no source discovery/import/cook
- no AssetPipeline writer or package staging changes
- no publish readiness report or hard gate
- no UI/document `AssetKind`
- no editor workflow
- no MultiplayerSandbox package
- no full CTest

Recommended next slice:

- ClientHost/Runtime Asset Consumption Ownership Audit or Publish Readiness
  Report v0

Full test health remains unverified.

## 2026-05-25 - Phase 5M Runtime Asset Consumption Ownership Audit

Scope for this slice is ownership decision only:

- close Phase 5L as local Apps/Runtime asset bootstrap proof
- reject passing raw `AssetRegistry` into `ClientHost`
- decide Runtime should expose a narrow read-only asset access contract
- keep Apps/Runtime local `AssetRegistry` as a temporary lifetime placeholder
- defer RuntimeServiceRegistry asset service until startup context and lifetime
  are designed

Accepted evidence:

- Apps/Runtime bootstraps explicit package assets into a local `AssetRegistry`
- `ClientHost` does not receive or consume the registry
- `ClientHost` mixes network, render, UI, input, and session startup ownership
- `RuntimeServiceRegistry` has no startup context for package or registry
  ownership
- Phase 5N recommendation is Runtime Asset Access Facade

Out of scope for this slice:

- no Runtime asset access facade implementation
- no `ClientHost` API change
- no RuntimeServiceRegistry asset service
- no publish readiness report or hard gate
- no source discovery/import/cook
- no UI/document `AssetKind`
- no MultiplayerSandbox package
- no full CTest

Full test health remains unverified.

## 2026-05-25 - Phase 5N Runtime Asset Access Facade

Scope for this slice is a bounded Runtime read-only asset access contract:

- add `SagaRuntime::RuntimeAssetCatalog` over caller-owned `AssetRegistry`
- expose read-only count, containment, lookup, kind, prefetch, and disk-size
  views
- add focused `RuntimeAssetCatalogTests`
- keep Apps/Runtime and `ClientHost` behavior unchanged
- keep RuntimeServiceRegistry asset service deferred

Accepted evidence:

- empty catalog reports empty and zero count
- RuntimeSmoke package bootstrap is visible through the catalog
- lookup works by asset key and `AssetId`
- missing asset key and id return false/null
- exposed registry entries are const
- catalog observes caller-owned registry state without taking ownership
- failed bootstrap leaves the catalog on committed registry state

Verification:

- `git diff --check`
- targeted `rg` inventory for `RuntimeAssetCatalog`, `AssetRegistry`,
  `RuntimeAssetBootstrap`, `RuntimeAssetStartupBootstrap`, `ClientHost`,
  `RuntimeServiceRegistry`, publish readiness, `Content/UI`, and `AssetKind`
- `Tools/Forge/bin/forge nix build --target RuntimeAssetCatalogTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target RuntimeAssetBootstrapTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target RuntimeAssetStartupBootstrapTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "RuntimeAssetCatalogTests|RuntimeAssetBootstrapTests|RuntimeAssetStartupBootstrapTests" --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "RuntimePackageSmokeTests|GeneratedRuntimeSmokePackageTests|SagaPackageStagingTests" --output-on-failure -j 1`

Label inventory after this slice:

- `runtime`: 23 tests, with existing registered-but-unbuilt runtime executable
  warnings
- `asset`: 13 tests
- `package`: 11 tests
- `tools`: 7 tests
- `integration`: 3 tests

Out of scope for this slice:

- no `ClientHost` constructor or config changes
- no passing registry/catalog into `ClientHost`
- no Apps/Runtime main behavior changes
- no RuntimeServiceRegistry asset service
- no publish readiness report or hard gate
- no source discovery/import/cook
- no AssetPipeline writer or package staging changes
- no UI/document `AssetKind`
- no MultiplayerSandbox package
- no full CTest

Recommended next slice:

- Publish Readiness Package/Asset Identity Report v0, unless the catalog
  reveals a need for a ClientHost/Runtime consumption integration audit first

Full test health remains unverified.

## 2026-05-25 - Phase 5O Publish Readiness Package Asset Identity Report

Scope for this slice is report-only product publish visibility:

- close Phase 5N as Runtime-owned read-only asset access facade and package
  asset lookup/access contract proof
- add product-local publish coverage structs for package/asset identity data
- serialize deterministic top-level `packageAssetIdentityCoverage`
- reuse existing package, asset identity, and asset manifest loaders
- keep current publish blocker and external diagnostics policies unchanged
- add focused `SagaPublishReadinessTests` independent of SDE

Accepted evidence:

- valid package inputs report package id/kind, identity mapping count, asset
  manifest count, asset count, and ready status
- missing package manifests remain existing blockers and appear in coverage
- missing package-referenced asset and identity manifests are reported with
  stable Runtime loader diagnostics
- missing cooked assets are coverage-only and do not add blockers
- invalid asset manifest diagnostics serialize without broad hard-gate expansion
- report JSON stays deterministic and machine-readable

Verification:

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

Label inventory after this slice:

- `runtime`: 23 tests, with existing registered-but-unbuilt runtime executable
  warnings
- `asset`: 14 tests
- `package`: 12 tests
- `tools`: 7 tests
- `integration`: 3 tests

Out of scope for this slice:

- no new publish hard gate
- no ClientHost constructor or config changes
- no Apps/Runtime behavior changes
- no RuntimeServiceRegistry asset service
- no source discovery/import/cook
- no AssetPipeline writer or package staging changes
- no UI/document `AssetKind`
- no MultiplayerSandbox package
- no broad CMake/test restructuring
- no full CTest

Recommended next slice:

- Phase 5 Closure Readiness Checkpoint if targeted regressions stay clean;
  otherwise UI/Document `AssetKind` Audit or ClientHost/Runtime Asset
  Consumption Integration Audit based on the Phase 5O evidence

Full test health remains unverified.

## 2026-05-25 - Phase 5 Closure And Phase 6 Opening Checkpoint

Scope for this slice is docs-only closure and transition:

- close Phase 5O as publish readiness package/asset/identity coverage report v0
- close Phase 5 as Package / Asset / Runtime Readiness Foundation established
- document the full Phase 5 evidence map and remaining debt classification
- open Phase 6 as Editor Public API De-Qtification

Phase 5O closure:

- accepted as publish readiness package/asset/identity coverage report v0
- accepted as report-only product visibility for package readiness
- accepted as deterministic machine-readable `packageAssetIdentityCoverage`
  report output
- not accepted as publish hard gate implementation
- not accepted as source discovery/import/cook
- not accepted as ClientHost/runtime asset consumption wiring
- not accepted as RuntimeServiceRegistry asset service
- not accepted as UI/document `AssetKind`
- not accepted as MultiplayerSandbox package readiness
- not accepted as full AssetPipeline completion

Phase 5 closure:

- Close Phase 5 as Package / Asset / Runtime Readiness Foundation established.
- Do not close Phase 5 as full AssetPipeline completion.
- Do not close Phase 5 as source discovery/import/cook completion.
- Do not close Phase 5 as full publish gate completion.
- Do not close Phase 5 as ClientHost/runtime asset consumption completion.
- Do not close Phase 5 as MultiplayerSandbox package readiness.

Remaining debt:

- later AssetPipeline phase: source discovery, source import, asset cook,
  cooked artifact production
- Editor/UI phase: UI/document `AssetKind`, editor asset workflow,
  `Content/UI` packaging model
- Runtime/ClientHost ownership phase: ClientHost asset consumption,
  RuntimeServiceRegistry asset service, runtime asset service lifetime/context
- Product/Publish phase: publish hard gate, Phase 10 publish enforcement,
  enterprise/report hardening
- MultiplayerSandbox/product proof: MultiplayerSandbox package fixture,
  playable vertical package proof
- verification debt: full CTest, registered-but-unbuilt runtime executable
  warnings, broad test health

Verification:

- `git diff --check`
- targeted `rg` for Phase 5 closure wording, non-claims, remaining debt, and
  Phase 6 opening
- label inventory for `runtime`, `asset`, `package`, `tools`, `product`, and
  `integration`

Phase 6 opening recommendation:

- Editor Public API De-Qtification
- first slice should be an Editor Qt public ABI / Editor UI ownership audit

Full test health remains unverified.

## 2026-05-25 - Phase 6A Editor Qt Public ABI Audit

Scope for this slice is report-only Phase 6 opening evidence:

- confirm Phase 5 is closed as Package / Asset / Runtime Readiness Foundation
  established
- confirm Phase 6 is open as Editor Public API De-Qtification
- inventory public Editor headers that expose Qt types or `QtPanel`
- classify allowed private Qt implementation, public ABI leaks, app-only Qt
  exposure, test-only exposure, legacy/deferred exposure, and false positives
- record Editor UI ownership boundaries for Qt backend implementation,
  backend-neutral Editor contracts, EditorShell, panel registry/layout,
  EditorLab adapters, SDE customization, collaboration UI, and Saga product
  composition
- inventory relevant CMake/test labels for `editor`, `ui`, `integration`,
  `product`, and `collaboration`
- compare bounded Phase 6B implementation candidates and select one

Phase 6A findings:

- `Editor/include/SagaEditor/UI/Qt/QtPanel.h` exposes `<QWidget>`,
  `Q_OBJECT`, `QWidget`, and `QtPanel` through the public Editor include root.
- Public panel headers still inherit `QtPanel`: `GraphViewportPanel`,
  `ProfilerPanel`, `CollaborationPanel`, and `ProjectBrowserPanel`.
- Visual scripting public panel/view headers still inherit `QtPanel`:
  `ExecutionTraceView`, `GraphDebuggerView`, `WatchPanel`, `BreakpointPanel`,
  `GraphCanvas`, `NodePalette`, and `NodeLibraryPanel`.
- `Apps/Saga/SagaEditorModule.h` exposes `QMainWindow` and `QStackedWidget`
  through the product editor activation API.
- Production Dashboard and several newer panel headers already use `IPanel`
  plus pimpl-style public contracts with Qt widget implementation behind
  `Editor/src/SagaEditor/UI/Qt/**`.
- `SagaEditorLib` still publishes `Qt6::Core` and `Qt6::Widgets` because
  public Editor headers still expose Qt.

Ownership decision:

- QtWidgets belongs in the Editor Qt backend implementation and app/product
  composition implementation files.
- Editor public contracts should stay backend-neutral through `IPanel`,
  `PanelId`, `UIDockArea`, `IUIMainWindow`, `IUIApplication`, `IUIFactory`,
  view models, diagnostics, customization models, profile/persona models, and
  collaboration service contracts.
- `EditorShell` owns panel registry, layout, visibility, profile/persona
  application, and command wiring through backend-neutral interfaces.
- SDE customization remains an Editor composition/customization service
  boundary, not a Qt widget or SDE-internals public API.
- Collaboration services remain Qt-free; collaboration UI belongs to a private
  Qt panel implementation or backend adapter.

Recommended Phase 6B slice:

- Editor Public Header Qt Exposure Report / Boundary Guard.
- Add or extend a deterministic architecture check that scans public Editor
  headers for Qt includes/types, records the current legacy allowlist, and
  fails only on new non-allowlisted public Qt exposure.
- Do not start broad Qt removal, file movement, `SagaEditorLib` Qt dependency
  conversion, product editor activation rewrite, SDE rewrite, or collaboration
  rewrite in Phase 6B.

Verification:

- `git diff --check`
- targeted `rg` for Phase 6A wording, Qt exposure classifications, selected
  Phase 6B slice, non-goals, and full CTest unverified statement
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L editor`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L ui`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L integration`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L product`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L collaboration`

Out of scope for this slice:

- no broad Qt removal
- no editor architecture rewrite
- no SDE customization rewrite
- no collaboration rewrite
- no UI/document `AssetKind` implementation
- no runtime/client asset work
- no publish gate work
- no MultiplayerSandbox work
- no full CTest

Full test health remains unverified.

## 2026-05-25 - Phase 6B Editor Qt Public ABI Boundary Guard

Scope for this slice is bounded enforcement/report only:

- close Phase 6A as an accepted Editor Qt public ABI / UI ownership audit,
  public Qt exposure inventory, Qt ownership boundary decision, and Phase 6B
  boundary guard recommendation
- add `EditorQtPublicAbiBoundaryTests` to the existing Qt-free
  `SagaArchitectureTests` target
- scan `Editor/include` public headers and `Apps/Saga/SagaEditorModule.h`
- block new public Qt exposure outside the exact current allowlist
- register a focused CTest entry named `EditorQtPublicAbiBoundaryTests` with
  `unit;architecture;editor` labels
- document Phase 6B guard behavior and the recommended Phase 6C follow-up

Current allowed legacy leaks:

- `Editor/include/SagaEditor/UI/Qt/QtPanel.h` for `<QWidget>`, `Q_OBJECT`,
  `QWidget`, and `QtPanel`
- public panel and visual scripting headers that currently include and inherit
  `QtPanel`
- `Apps/Saga/SagaEditorModule.h` for its current `QMainWindow` and
  `QStackedWidget` forward declarations

The guard is path-exact and token-specific. Adding `QString` to a
`QtPanel`-only header is a violation even though the file is already
allowlisted for `QtPanel`.

Out of scope for this slice:

- no broad Qt removal
- no `QtPanel` removal
- no panel migration
- no `SagaEditorModule` API change
- no `EditorShell` change
- no `SagaEditorLib` Qt visibility cleanup
- no SDE customization rewrite
- no collaboration rewrite
- no UI/document `AssetKind` implementation
- no runtime/client asset work
- no publish gate work
- no MultiplayerSandbox work
- no full CTest

Recommended next slice:

- Phase 6C: QtPanel Public ABI Replacement Plan

Full test health remains unverified.

## 2026-05-25 - Phase 6C QtPanel Public ABI Replacement Plan

Scope for this slice is a docs-only decision checkpoint:

- close Phase 6B as Editor public header Qt exposure boundary guard,
  deterministic public Qt exposure scanner, allowlisted baseline for known
  public Qt leaks, and architecture/editor CTest enforcement
- record that Phase 6B is not Qt removal, `QtPanel` public ABI removal, panel
  migration, visual scripting migration, `SagaEditorModule` API
  de-Qtification, CMake Qt `PUBLIC` cleanup, editor test split, or full CTest
  health
- choose the public replacement pattern for `QtPanel` inheritance
- define migration order for all allowlisted public Qt leaks
- recommend exactly one Phase 6D direction

Replacement decision:

- use existing `IPanel` as the backend-neutral public panel contract
- migrate public panel headers to inherit `IPanel` directly and own private Qt
  widget implementation through pimpl
- keep `QWidget`, QObject/moc, layouts, casts, and concrete widget ownership in
  `Editor/src/SagaEditor/UI/Qt/Panels/**`
- do not add a new `EditorPanelBase`, descriptor, or adapter in Phase 6C
  because `IPanel` already covers `PanelId`, title, native handle, lifecycle,
  and focus hooks

Migration order:

- first low-risk panel pimpl migration: prefer `GraphViewportPanel`, with
  `ProfilerPanel` as fallback if implementation inspection proves it simpler
- remaining simple panel stubs: `ProfilerPanel` and `ProjectBrowserPanel`
- `CollaborationPanel` after the collaboration service/UI boundary is checked
- visual scripting leaf panels/views next
- `GraphCanvas` after graph view/model ownership is explicit
- remove or privatize `QtPanel` only after no public header inherits/includes it
- handle `Apps/Saga/SagaEditorModule.h` as separate product ABI debt
- audit `SagaEditorLib` Qt `PUBLIC` visibility only after public headers are
  clean

Recommended next slice:

- Phase 6D: First Low-Risk Panel pimpl Migration

Out of scope for this slice:

- no Qt removal
- no `QtPanel` removal
- no panel migration
- no visual scripting UI migration
- no `SagaEditorModule` API change
- no `EditorShell` change
- no `SagaEditorLib` Qt visibility cleanup
- no SDE customization rewrite
- no collaboration rewrite
- no UI/document `AssetKind` implementation
- no runtime/client asset work
- no publish gate work
- no MultiplayerSandbox work
- no full CTest

Full test health remains unverified.

## 2026-05-25 - Phase 6D First Panel pimpl Migration

Scope for this slice is a bounded first panel migration:

- close Phase 6C as QtPanel public ABI replacement plan, `IPanel` +
  pimpl/private Qt implementation migration strategy, and migration order
  checkpoint
- migrate exactly one public panel header away from `QtPanel`
- update the architecture guard allowlist for only that panel
- preserve `QtPanel` as temporary allowlisted debt
- keep broad Qt removal, visual scripting migration, product ABI cleanup, and
  CMake Qt visibility cleanup deferred

Candidate decision:

- `GraphViewportPanel` and `ProfilerPanel` were both current stub-level
  `QtPanel` inheritors with no real implementation body.
- `GraphViewportPanel` was selected because it was the Phase 6C default and no
  rendering/view-model coupling was present in the current source.
- `ProfilerPanel` remains the next simple migration candidate.

Implementation:

- `GraphViewportPanel.h` now includes `IPanel`, inherits `IPanel`, and exposes
  no Qt header, `QWidget`, `QObject`, `Q_OBJECT`, or `QtPanel`.
- The private Qt widget implementation lives in
  `Editor/src/SagaEditor/UI/Qt/Panels/GraphViewportPanel.cpp`.
- `GetPanelId()` returns `saga.panel.graph`, matching existing persona/profile
  references.
- `EditorQtPublicAbiBoundaryTests` removed only the
  `GraphViewportPanel.h` allowlist entry.

Out of scope for this slice:

- no broad Qt removal
- no `QtPanel` removal
- no multiple panel migration
- no visual scripting migration
- no `SagaEditorModule` API change
- no `EditorShell` rewrite
- no `SagaEditorLib` Qt visibility cleanup
- no SDE customization rewrite
- no collaboration rewrite
- no UI/document `AssetKind` implementation
- no runtime/client asset work
- no publish gate work
- no MultiplayerSandbox work
- no full CTest

Recommended next slice:

- Phase 6E: migrate `ProfilerPanel` with the same `IPanel` + pimpl/private Qt
  widget pattern, unless Phase 6D verification exposes missing panel semantics
  or a weak test seam that should be addressed first.

Full test health remains unverified.

## 2026-05-25 - Phase 6E ProfilerPanel pimpl Migration

Scope for this slice is the second low-risk panel migration:

- close Phase 6D as first low-risk panel pimpl migration, `GraphViewportPanel`
  public Qt exposure removal, `IPanel` + pimpl/private Qt widget pattern proof,
  and Phase 6B allowlist reduction
- migrate exactly `ProfilerPanel` away from public `QtPanel` inheritance
- update the architecture guard allowlist for only `ProfilerPanel`
- preserve `QtPanel` as temporary allowlisted debt
- keep broad Qt removal, visual scripting migration, product ABI cleanup, and
  CMake Qt visibility cleanup deferred

Inspection:

- `ProfilerPanel.h` was still a simple `QtPanel` include/inheritance leak.
- `Editor/src/SagaEditor/Panels/ProfilerPanel.cpp` was a stub with no profiler
  data/model dependency.
- Persona/profile references use the stable id `saga.panel.profiler`.
- No QWidget/QObject behavior beyond the existing native widget handle was
  required.

Implementation:

- `ProfilerPanel.h` now includes `IPanel`, inherits `IPanel`, and exposes no Qt
  header, `QWidget`, `QObject`, `Q_OBJECT`, or `QtPanel`.
- The private Qt widget implementation lives in
  `Editor/src/SagaEditor/UI/Qt/Panels/ProfilerPanel.cpp`.
- `GetPanelId()` returns `saga.panel.profiler`, matching existing
  persona/profile references.
- `EditorQtPublicAbiBoundaryTests` removed only the `ProfilerPanel.h`
  allowlist entry.
- `EditorQtPublicAbiBoundaryTests` reports 288 scanned public headers, 41
  allowed legacy Qt exposure findings, and 0 violations.

Out of scope for this slice:

- no broad Qt removal
- no `QtPanel` removal
- no second additional panel migration
- no `ProjectBrowserPanel` migration
- no `CollaborationPanel` migration
- no visual scripting migration
- no `SagaEditorModule` API change
- no `EditorShell` rewrite
- no `SagaEditorLib` Qt visibility cleanup
- no SDE customization rewrite
- no collaboration rewrite
- no UI/document `AssetKind` implementation
- no runtime/client asset work
- no publish gate work
- no MultiplayerSandbox work
- no full CTest

Recommended next slice:

- Phase 6F: migrate `ProjectBrowserPanel` with the same `IPanel` +
  pimpl/private Qt widget pattern if Phase 6E verification stays clean.

Full test health remains unverified.

## 2026-05-25 - Phase 6F ProjectBrowserPanel pimpl Migration

Scope for this slice is the third low-risk panel migration:

- close Phase 6E as second low-risk panel pimpl migration, `ProfilerPanel`
  public Qt exposure removal, repeated `IPanel` + pimpl/private Qt widget
  pattern proof, and Phase 6B allowlist reduction
- migrate exactly `ProjectBrowserPanel` away from public `QtPanel` inheritance
- update the architecture guard allowlist for only `ProjectBrowserPanel`
- preserve `QtPanel` as temporary allowlisted debt
- keep broad Qt removal, collaboration migration, visual scripting migration,
  product ABI cleanup, and CMake Qt visibility cleanup deferred

Inspection:

- `ProjectBrowserPanel.h` was still a simple `QtPanel` include/inheritance
  leak.
- `Editor/src/SagaEditor/Panels/ProjectBrowserPanel.cpp` was a stub with no
  project/workspace/file-tree/model dependency.
- No QWidget/QObject behavior beyond the existing native widget handle was
  required.
- No project/workspace ownership behavior was added or redesigned.

Implementation:

- `ProjectBrowserPanel.h` now includes `IPanel`, inherits `IPanel`, and exposes
  no Qt header, `QWidget`, `QObject`, `Q_OBJECT`, or `QtPanel`.
- The private Qt widget implementation lives in
  `Editor/src/SagaEditor/UI/Qt/Panels/ProjectBrowserPanel.cpp`.
- `GetPanelId()` returns `saga.panel.project`; `GetTitle()` returns
  `Project Browser`.
- `EditorQtPublicAbiBoundaryTests` removed only the `ProjectBrowserPanel.h`
  allowlist entry.
- `EditorQtPublicAbiBoundaryTests` reports 288 scanned public headers, 38
  allowed legacy Qt exposure findings, and 0 violations.

Out of scope for this slice:

- no broad Qt removal
- no `QtPanel` removal
- no second additional panel migration
- no `CollaborationPanel` migration
- no visual scripting migration
- no `SagaEditorModule` API change
- no `EditorShell` rewrite
- no project/workspace ownership redesign
- no `SagaEditorLib` Qt visibility cleanup
- no SDE customization rewrite
- no collaboration rewrite
- no UI/document `AssetKind` implementation
- no runtime/client asset work
- no publish gate work
- no MultiplayerSandbox work
- no full CTest

Recommended next slice:

- Phase 6G: perform a `CollaborationPanel` migration audit before
  implementation because it may carry collaboration service ownership risk.

Full test health remains unverified.

## 2026-05-25 - Phase 6G CollaborationPanel Qt Boundary

Scope for this slice is the fourth simple panel migration:

- close Phase 6F as third simple panel pimpl migration,
  `ProjectBrowserPanel` public Qt exposure removal, repeated `IPanel` +
  pimpl/private Qt widget pattern proof, and Phase 6B allowlist reduction
- audit `CollaborationPanel` ownership before implementation
- migrate exactly `CollaborationPanel` away from public `QtPanel` inheritance
- update the architecture guard allowlist for only `CollaborationPanel`
- preserve `QtPanel` as temporary allowlisted debt
- keep broad Qt removal, visual scripting migration, product ABI cleanup,
  collaboration service ownership, and CMake Qt visibility cleanup deferred

Inspection:

- `CollaborationPanel.h` was still a simple `QtPanel` include/inheritance
  leak.
- `Editor/src/SagaEditor/Panels/CollaborationPanel.cpp` was a stub with no
  collaboration session, networking, document sync, lock, permission,
  presence, or conflict dependency.
- The wider collaboration ownership constraints argue for keeping this panel
  display-only, not for blocking the public Qt ABI cleanup.
- No QWidget/QObject behavior beyond the existing native widget handle was
  required.

Implementation:

- `CollaborationPanel.h` now includes `IPanel`, inherits `IPanel`, and exposes
  no Qt header, `QWidget`, `QObject`, `Q_OBJECT`, or `QtPanel`.
- The private Qt widget implementation lives in
  `Editor/src/SagaEditor/UI/Qt/Panels/CollaborationPanel.cpp`.
- `GetPanelId()` returns `saga.panel.collaboration`; `GetTitle()` returns
  `Collaboration`.
- `EditorQtPublicAbiBoundaryTests` removed only the `CollaborationPanel.h`
  allowlist entry.
- `EditorQtPublicAbiBoundaryTests` reports 288 scanned public headers, 35
  allowed legacy Qt exposure findings, and 0 violations.

Out of scope for this slice:

- no broad Qt removal
- no `QtPanel` removal
- no visual scripting migration
- no `SagaEditorModule` API change
- no `EditorShell` rewrite
- no collaboration session/networking/document sync/lock/permission/presence
  or conflict ownership redesign
- no `SagaEditorLib` Qt visibility cleanup
- no SDE customization rewrite
- no UI/document `AssetKind` implementation
- no runtime/client asset work
- no publish gate work
- no MultiplayerSandbox work
- no full CTest

Recommended next slice:

- Phase 6H: plan the visual scripting leaf panel cluster because the simple
  editor-panel cluster is cleared and the remaining public Qt leaks are visual
  scripting headers, `QtPanel.h`, and `SagaEditorModule.h`.

Full test health remains unverified.

## 2026-05-25 - Phase 6H Visual Scripting QtPanel Cluster

Scope for this slice is the first visual scripting Qt boundary slice:

- close Phase 6G as `CollaborationPanel` Qt boundary audit and pimpl
  migration, simple editor-panel cluster public Qt exposure cleanup, repeated
  `IPanel` + pimpl/private Qt widget pattern proof, and Phase 6B allowlist
  reduction
- inventory the remaining visual scripting `QtPanel` cluster
- migrate exactly `WatchPanel` away from public `QtPanel` inheritance
- update the architecture guard allowlist for only `WatchPanel`
- preserve `QtPanel` as temporary allowlisted debt
- keep broad Qt removal, GraphCanvas migration, visual scripting architecture
  rewrite, product ABI cleanup, and CMake Qt visibility cleanup deferred

Inventory result:

- `WatchPanel` and `BreakpointPanel` are the safe leaf candidates: default
  constructors, stub implementations, and no public graph/debugger/node model
  references.
- `ExecutionTraceView` and `GraphDebuggerView` are stub-level but carry
  debugger/trace ownership semantics by role, so they need an explicit
  debugger boundary audit before migration.
- `NodePalette` is stub-level but sits on the node catalog boundary, so it
  should follow a node palette/model ownership audit.
- `NodeLibraryPanel` publicly stores `NodeCategoryBrowser&`, so it needs a
  node browser ownership audit before migration.
- `GraphCanvas` publicly stores `GraphDocument&` and remains the high-risk
  graph core view; it is explicitly deferred.

Implementation:

- `WatchPanel.h` now includes `IPanel`, inherits `IPanel`, and exposes no Qt
  header, `QWidget`, `QObject`, `Q_OBJECT`, or `QtPanel`.
- The private Qt widget implementation lives in
  `Editor/src/SagaEditor/UI/Qt/VisualScripting/Editor/WatchPanel.cpp`.
- `GetPanelId()` returns `saga.panel.visual_scripting.watch`; `GetTitle()`
  returns `Watch`.
- `EditorQtPublicAbiBoundaryTests` removed only the `WatchPanel.h` allowlist
  entry.
- `EditorQtPublicAbiBoundaryTests` reports 288 scanned public headers, 32
  allowed legacy Qt exposure findings, and 0 violations.

Out of scope for this slice:

- no broad Qt removal
- no `QtPanel` removal
- no broad visual scripting migration
- no `GraphCanvas` migration
- no `SagaEditorModule` API change
- no `EditorShell` rewrite
- no collaboration rewrite
- no visual scripting service/model/debugger/node ownership rewrite
- no `SagaEditorLib` Qt visibility cleanup
- no UI/document `AssetKind` implementation
- no runtime/client asset work
- no publish gate work
- no MultiplayerSandbox work
- no full CTest

Recommended next slice:

- Phase 6I: migrate `BreakpointPanel` with the same `IPanel` +
  pimpl/private Qt widget pattern if Phase 6H verification stays clean;
  otherwise stop for a debugger/watch model boundary audit.

Full test health remains unverified.

## 2026-05-25 - Phase 6I BreakpointPanel pimpl Migration

Scope for this slice is the second visual scripting Qt boundary migration:

- close Phase 6H as first visual scripting leaf pimpl migration, `WatchPanel`
  public Qt exposure removal, visual scripting cluster inventory, and Phase 6B
  allowlist reduction
- migrate exactly `BreakpointPanel` away from public `QtPanel` inheritance
- update the architecture guard allowlist for only `BreakpointPanel`
- preserve `QtPanel` as temporary allowlisted debt
- keep broad Qt removal, higher-risk visual scripting view migration, product
  ABI cleanup, and CMake Qt visibility cleanup deferred

Implementation:

- `BreakpointPanel.h` now includes `IPanel`, inherits `IPanel`, and exposes no
  Qt header, `QWidget`, `QObject`, `Q_OBJECT`, or `QtPanel`.
- The private Qt widget implementation lives in
  `Editor/src/SagaEditor/UI/Qt/VisualScripting/Editor/BreakpointPanel.cpp`.
- `GetPanelId()` returns `saga.panel.visual_scripting.breakpoints`;
  `GetTitle()` returns `Breakpoints`.
- The old visual scripting source file is now a non-defining moved stub.
- `EditorQtPublicAbiBoundaryTests` removed only the `BreakpointPanel.h`
  allowlist entry.
- `EditorQtPublicAbiBoundaryTests` reports 288 scanned public headers, 29
  allowed legacy Qt exposure findings, and 0 violations.

Out of scope for this slice:

- no broad Qt removal
- no `QtPanel` removal
- no broad visual scripting migration
- no `ExecutionTraceView`, `GraphDebuggerView`, `NodePalette`,
  `NodeLibraryPanel`, or `GraphCanvas` migration
- no `SagaEditorModule` API change
- no `EditorShell` rewrite
- no debugger/session/model/node ownership rewrite
- no `BreakpointController` wiring
- no `SagaEditorLib` Qt visibility cleanup
- no runtime/client asset work
- no publish gate work
- no collaboration or MultiplayerSandbox work
- no full CTest

Recommended next slice:

- Phase 6J: audit `ExecutionTraceView` debugger/trace ownership before
  attempting another visual scripting public Qt migration.

Full test health remains unverified.

## 2026-05-25 - Phase 6J ExecutionTraceView Qt Boundary

Scope for this slice is the first visual scripting debugger/trace Qt boundary
migration:

- close Phase 6I as second visual scripting leaf pimpl migration,
  `BreakpointPanel` public Qt exposure removal, repeated visual scripting
  `IPanel` + pimpl/private Qt widget pattern proof, and Phase 6B allowlist
  reduction
- inventory the remaining visual scripting public `QtPanel` cluster
- migrate exactly `ExecutionTraceView` away from public `QtPanel` inheritance
- update the architecture guard allowlist for only `ExecutionTraceView`
- preserve `QtPanel` as temporary allowlisted debt
- keep broad Qt removal, graph/debugger view migration, node UI migration,
  product ABI cleanup, and CMake Qt visibility cleanup deferred

Decision evidence:

- `ExecutionTraceView` is default-constructed, has no public non-Qt
  dependencies, and its current implementation is a stub.
- No current source path wires it to a debug session, trace model, graph
  runtime, command router, breakpoint store, or watch controller.
- `GraphDebuggerView` still needs a graph/debugger ownership audit.
- `NodePalette`, `NodeLibraryPanel`, and `GraphCanvas` remain deferred because
  their roles or public references cross node/graph model boundaries.

Implementation:

- `ExecutionTraceView.h` now includes `IPanel`, inherits `IPanel`, and exposes
  no Qt header, `QWidget`, `QObject`, `Q_OBJECT`, or `QtPanel`.
- The private Qt widget implementation lives in
  `Editor/src/SagaEditor/UI/Qt/VisualScripting/Debugger/ExecutionTraceView.cpp`.
- `GetPanelId()` returns `saga.panel.visual_scripting.execution_trace`;
  `GetTitle()` returns `Execution Trace`.
- The old visual scripting source file is now a non-defining moved stub.
- `EditorQtPublicAbiBoundaryTests` removed only the
  `ExecutionTraceView.h` allowlist entry.
- `EditorQtPublicAbiBoundaryTests` reports 288 scanned public headers, 26
  allowed legacy Qt exposure findings, and 0 violations.

Out of scope for this slice:

- no broad Qt removal
- no `QtPanel` removal
- no broad visual scripting migration
- no `GraphDebuggerView`, `NodePalette`, `NodeLibraryPanel`, or `GraphCanvas`
  migration
- no `SagaEditorModule` API change
- no `EditorShell` rewrite
- no debugger/session/model/node ownership rewrite
- no trace/debugger behavior implementation
- no `SagaEditorLib` Qt visibility cleanup
- no runtime/client asset work
- no publish gate work
- no collaboration or MultiplayerSandbox work
- no full CTest

Recommended next slice:

- Phase 6K: audit `GraphDebuggerView` debugger/model ownership before
  attempting another visual scripting public Qt migration.

Full test health remains unverified.

## 2026-05-26 - Phase 6K GraphDebuggerView Qt Boundary

Scope for this slice is the second visual scripting debugger Qt boundary
migration:

- close Phase 6J as `ExecutionTraceView` debugger/trace ownership audit,
  public Qt exposure removal, repeated visual scripting `IPanel` +
  pimpl/private Qt widget pattern proof, and Phase 6B allowlist reduction
- audit `GraphDebuggerView` debugger/model ownership before implementation
- migrate exactly `GraphDebuggerView` away from public `QtPanel` inheritance
- update the architecture guard allowlist for only `GraphDebuggerView`
- preserve `QtPanel` as temporary allowlisted debt
- keep broad Qt removal, node UI migration, product ABI cleanup, and CMake Qt
  visibility cleanup deferred

Decision evidence:

- `GraphDebuggerView` is default-constructed, has no public non-Qt
  dependencies, and its current implementation is a stub.
- No current source path wires it to a debug session, graph model, graph
  runtime, trace model, command router, breakpoint store, watch controller,
  node library, or graph document.
- `NodePalette`, `NodeLibraryPanel`, and `GraphCanvas` remain deferred because
  their roles or public references cross node/graph model boundaries.

Implementation:

- `GraphDebuggerView.h` now includes `IPanel`, inherits `IPanel`, and exposes
  no Qt header, `QWidget`, `QObject`, `Q_OBJECT`, or `QtPanel`.
- The private Qt widget implementation lives in
  `Editor/src/SagaEditor/UI/Qt/VisualScripting/Debugger/GraphDebuggerView.cpp`.
- `GetPanelId()` returns `saga.panel.visual_scripting.graph_debugger`;
  `GetTitle()` returns `Graph Debugger`.
- The old visual scripting source file is now a non-defining moved stub.
- `EditorQtPublicAbiBoundaryTests` removed only the
  `GraphDebuggerView.h` allowlist entry.

Out of scope for this slice:

- no broad Qt removal
- no `QtPanel` removal
- no broad visual scripting migration
- no `NodePalette`, `NodeLibraryPanel`, or `GraphCanvas` migration
- no `SagaEditorModule` API change
- no `EditorShell` rewrite
- no debugger/session/model/node ownership rewrite
- no graph debugger behavior implementation
- no `SagaEditorLib` Qt visibility cleanup
- no runtime/client asset work
- no publish gate work
- no collaboration or MultiplayerSandbox work
- no full CTest

Recommended next slice:

- Phase 6L: audit `NodePalette` node/model ownership before attempting the
  next visual scripting public Qt migration.

Full test health remains unverified.

## 2026-05-26 - Phase 6L NodePalette Qt Boundary

Scope for this slice is the node palette/model boundary migration:

- close Phase 6K as `GraphDebuggerView` debugger/model ownership audit, public
  Qt exposure removal, repeated visual scripting `IPanel` + pimpl/private Qt
  widget pattern proof, and Phase 6B allowlist reduction
- audit `NodePalette` node/model ownership before implementation
- migrate exactly `NodePalette` away from public `QtPanel` inheritance
- update the architecture guard allowlist for only `NodePalette`
- preserve `QtPanel` as temporary allowlisted debt
- keep broad Qt removal, `NodeLibraryPanel`, `GraphCanvas`, product ABI
  cleanup, and CMake Qt visibility cleanup deferred

Decision evidence:

- `NodePalette` is default-constructed, has no public non-Qt dependencies, and
  its current implementation is a stub.
- No current source path wires it to a node catalog, node model, graph
  document, command router, editor selection, or graph canvas.
- `NodeLibraryPanel` and `GraphCanvas` remain deferred because their public
  references cross node/graph model boundaries.

Implementation:

- `NodePalette.h` now includes `IPanel`, inherits `IPanel`, and exposes no Qt
  header, `QWidget`, `QObject`, `Q_OBJECT`, or `QtPanel`.
- The private Qt widget implementation lives in
  `Editor/src/SagaEditor/UI/Qt/VisualScripting/Editor/NodePalette.cpp`.
- `GetPanelId()` returns `saga.panel.visual_scripting.node_palette`;
  `GetTitle()` returns `Node Palette`.
- The old visual scripting source file is now a non-defining moved stub.
- `EditorQtPublicAbiBoundaryTests` removed only the `NodePalette.h` allowlist
  entry.

Out of scope for this slice:

- no broad Qt removal
- no `QtPanel` removal
- no broad visual scripting migration
- no `NodeLibraryPanel` or `GraphCanvas` migration
- no `SagaEditorModule` API change
- no `EditorShell` rewrite
- no node/model ownership rewrite
- no node palette or node catalog behavior implementation
- no `SagaEditorLib` Qt visibility cleanup
- no runtime/client asset work
- no publish gate work
- no full CTest

Recommended next slice:

- Phase 6M: audit `NodeLibraryPanel` and `NodeCategoryBrowser` before
  attempting the next visual scripting public Qt migration.

Full test health remains unverified.

## 2026-05-26 - Phase 6M NodeLibraryPanel Qt Boundary

Scope for this slice is the node library/browser boundary migration:

- close Phase 6L as `NodePalette` node/model ownership audit, public Qt
  exposure removal, repeated visual scripting `IPanel` + pimpl/private Qt
  widget pattern proof, and Phase 6B allowlist reduction
- audit `NodeLibraryPanel` and `NodeCategoryBrowser` before implementation
- migrate exactly `NodeLibraryPanel` away from public `QtPanel` inheritance
- update the architecture guard allowlist for only `NodeLibraryPanel`
- preserve `QtPanel` as temporary allowlisted debt
- keep broad Qt removal, `GraphCanvas`, product ABI cleanup, and CMake Qt
  visibility cleanup deferred

Decision evidence:

- `NodeLibraryPanel` publicly depends on `NodeCategoryBrowser&`.
- `NodeCategoryBrowser` is Qt-free and currently exposes only registration,
  list, and search data APIs.
- The previous `NodeLibraryPanel` implementation is a stub.
- The migration preserves the existing `NodeCategoryBrowser&` constructor and
  keeps the non-owning reference behind the pimpl without adding node library
  behavior.

Implementation:

- `NodeLibraryPanel.h` now includes `IPanel`, inherits `IPanel`, and exposes no
  Qt header, `QWidget`, `QObject`, `Q_OBJECT`, or `QtPanel`.
- The private Qt widget implementation lives in
  `Editor/src/SagaEditor/UI/Qt/VisualScripting/Nodes/NodeLibraryPanel.cpp`.
- `GetPanelId()` returns `saga.panel.visual_scripting.node_library`;
  `GetTitle()` returns `Node Library`.
- The old visual scripting source file is now a non-defining moved stub.
- `EditorQtPublicAbiBoundaryTests` removed only the
  `NodeLibraryPanel.h` allowlist entry.

Out of scope for this slice:

- no broad Qt removal
- no `QtPanel` removal
- no broad visual scripting migration
- no `GraphCanvas` migration
- no `SagaEditorModule` API change
- no `EditorShell` rewrite
- no node/model ownership rewrite
- no node library, import, or node category behavior implementation
- no `SagaEditorLib` Qt visibility cleanup
- no runtime/client asset work
- no publish gate work
- no full CTest

Recommended next slice:

- Phase 6N: produce a `GraphCanvas` / `GraphDocument` boundary plan. Do not
  migrate `GraphCanvas` until graph view/model ownership is explicit.

Full test health remains unverified.

## 2026-05-26 - Phase 6N GraphCanvas Boundary Plan

Scope for this slice is a docs-only graph view/model boundary plan:

- close Phase 6M as `NodeLibraryPanel` / `NodeCategoryBrowser` ownership audit,
  public Qt exposure removal, repeated visual scripting `IPanel` +
  pimpl/private Qt widget pattern proof, and Phase 6B allowlist reduction
- audit `GraphCanvas` and `GraphDocument`
- keep `GraphCanvas` allowlisted because its public `GraphDocument&` contract
  crosses graph model ownership
- define prerequisites for a future graph canvas migration
- keep `QtPanel`, `SagaEditorModule`, and CMake Qt visibility cleanup deferred

Decision evidence:

- `GraphCanvas` still exposes `QtPanel` and publicly takes/stores
  `GraphDocument&`.
- `GraphDocument` is shared by graph mutation, serialization, validation,
  imports, preview, compiler adapters, pin rendering, overlays, and graph editor
  helpers.
- Even though the current canvas implementation is a stub, the public API is a
  graph view/model boundary and should not be pimpl-migrated blindly.

Implementation:

- Added `docs/recovery/phase-06-editor-deqt/PHASE_6N_GRAPH_CANVAS_BOUNDARY_PLAN.md`.
- No source, CMake, or allowlist changes were made for `GraphCanvas`.
- `EditorQtPublicAbiBoundaryTests` continues to allowlist only the remaining
  public Qt exposures: `GraphCanvas.h`, `QtPanel.h`, and
  `SagaEditorModule.h`.

Out of scope for this slice:

- no `GraphCanvas` migration
- no broad Qt removal
- no `QtPanel` removal
- no `GraphDocument` behavior change
- no graph model, graph runtime, serializer, importer, preview, compiler,
  validation, pin, overlay, or editor shell ownership rewrite
- no `SagaEditorModule` API change
- no `SagaEditorLib` Qt visibility cleanup
- no full CTest

Recommended next slice:

- Phase 6O: audit `SagaEditorModule` product Qt ABI and produce a bounded
  product adapter plan unless implementation inspection proves a small
  non-breaking adapter is safe.

Full test health remains unverified.

## 2026-05-26 - Phase 6O SagaEditorModule Qt ABI

Scope for this slice is a bounded product editor activation ABI cleanup:

- close Phase 6N as `GraphCanvas` / `GraphDocument` boundary audit and explicit
  migration deferral
- audit `SagaEditorModule` product Qt ABI
- replace public `QMainWindow&` / `QStackedWidget&` activation parameters with
  a backend-neutral native mount adapter
- update the architecture guard allowlist for only `SagaEditorModule.h`
- preserve product/editor behavior and keep broader product shell rewrite
  deferred

Decision evidence:

- `SagaEditorModule.h` forward-declared `QMainWindow` and `QStackedWidget`.
- `SagaEditorModule::Shutdown` accepted those references but did not use them.
- The only production caller is `Apps/Saga/SagaApp.cpp`.
- `SagaEditorModule.cpp` already mounts via
  `SagaEditor::QtUIMainWindow(void*, void*)`.

Implementation:

- `SagaEditorModule.h` now defines `SagaEditorNativeMount` with
  `void* mainWindow` and `void* centralStack`.
- `Activate` accepts `SagaEditorNativeMount`; `Shutdown` no longer takes unused
  native window parameters.
- `SagaEditorModule.cpp` validates the native mount handles and passes them to
  `QtUIMainWindow`.
- `SagaApp.cpp` builds the native mount from its existing Qt objects.
- `EditorQtPublicAbiBoundaryTests` removed only the `SagaEditorModule.h`
  allowlist entry.

Out of scope for this slice:

- no product shell rewrite
- no `EditorShell` rewrite
- no `GraphCanvas` migration
- no `QtPanel` removal
- no `SagaEditorLib` Qt visibility cleanup
- no product readiness claim
- no full CTest

Recommended next slice:

- Phase 6P: audit `QtPanel` privatization readiness. Do not remove `QtPanel`
  while `GraphCanvas.h` remains a public inheritor.

Full test health remains unverified.

## 2026-05-26 - Phase 6P QtPanel Privatization Readiness

Scope for this slice is a readiness audit only:

- close Phase 6O as `SagaEditorModule` product Qt ABI cleanup and allowlist
  reduction
- audit remaining public `QtPanel` dependencies
- keep `QtPanel` public and allowlisted because `GraphCanvas.h` still inherits
  it
- keep CMake Qt visibility cleanup deferred

Decision evidence:

- `GraphCanvas.h` still includes `SagaEditor/UI/Qt/QtPanel.h` and inherits
  `SagaEditor::QtPanel`.
- `QtPanel.h` still exposes `<QWidget>`, `Q_OBJECT`, `QWidget`, and `QtPanel`
  through the public Editor include root.
- Phase 6N explicitly deferred `GraphCanvas` migration until the graph
  view/model seam is defined.

Implementation:

- Added `docs/recovery/phase-06-editor-deqt/PHASE_6P_QTPANEL_PRIVATIZATION_READINESS.md`.
- No source, CMake, or allowlist changes were made.

Out of scope for this slice:

- no `QtPanel` removal or privatization
- no `GraphCanvas` migration
- no `GraphDocument` behavior change
- no graph model, graph runtime, serializer, importer, preview, compiler,
  validation, pin, overlay, or editor shell ownership rewrite
- no `SagaEditorLib` Qt visibility cleanup
- no full CTest

Recommended next slice:

- Phase 6Q: audit `SagaEditorLib` Qt PUBLIC dependency cleanup readiness. Do
  not make Qt private while `GraphCanvas.h` and `QtPanel.h` remain public Qt
  ABI.

Full test health remains unverified.

## 2026-05-26 - Phase 6Q SagaEditorLib Qt PUBLIC Readiness

Scope for this slice is a readiness audit only:

- close Phase 6P as `QtPanel` privatization readiness with explicit deferral
- audit whether `SagaEditorLib` can stop publishing Qt
- keep `SagaEditorLib` linked publicly to `Qt6::Core` and `Qt6::Widgets`
  because public Editor headers still expose `GraphCanvas` / `QtPanel`
- keep `GraphCanvas.h` and `QtPanel.h` allowlisted as explicit public Qt debt

Decision evidence:

- `SagaEditorLib` publishes `Editor/include` as public include API.
- `SagaEditorLib` currently links `Qt6::Core` and `Qt6::Widgets` as `PUBLIC`.
- `GraphCanvas.h` still includes `SagaEditor/UI/Qt/QtPanel.h` and inherits
  `SagaEditor::QtPanel`.
- `QtPanel.h` still exposes `<QWidget>`, `Q_OBJECT`, `QWidget`, and `QtPanel`
  through the public Editor include root.

Implementation:

- Added `docs/recovery/phase-06-editor-deqt/PHASE_6Q_SAGAEDITORLIB_QT_PUBLIC_READINESS.md`.
- No source, CMake, or allowlist changes were made.

Out of scope for this slice:

- no `SagaEditorLib` Qt visibility cleanup
- no `QtPanel` removal or privatization
- no `GraphCanvas` migration
- no `GraphDocument` behavior change
- no graph model, graph runtime, serializer, importer, preview, compiler,
  validation, pin, overlay, or editor shell ownership rewrite
- no full CTest

Recommended next slice:

- Phase 6R: close Phase 6 honestly as a checkpoint with remaining
  `GraphCanvas`, `QtPanel`, and `SagaEditorLib` Qt PUBLIC dependency debt
  explicitly documented.

Full test health remains unverified.

## 2026-05-26 - Phase 6R Editor Public API De-Qtification Closure

Scope for this slice is a Phase 6 closure checkpoint:

- close Phase 6Q as `SagaEditorLib` Qt PUBLIC dependency cleanup readiness
  with explicit deferral
- record completed public ABI cleanup across bounded panels, debugger views,
  node panels, and `SagaEditorModule`
- record remaining public Qt debt as exactly `GraphCanvas.h`, `QtPanel.h`, and
  `SagaEditorLib` Qt PUBLIC visibility
- keep the boundary guard as the source of truth for unexpected public Qt
  exposure

Decision evidence:

- `GraphCanvas.h` remains public Qt ABI because it includes `QtPanel.h` and
  inherits `SagaEditor::QtPanel`.
- `QtPanel.h` remains public while `GraphCanvas.h` inherits it.
- `SagaEditorLib` still publishes Qt because its public headers still require
  Qt.
- `SagaEditorModule.h` no longer exposes public `QMainWindow` or
  `QStackedWidget` activation parameters.
- `EditorQtPublicAbiBoundaryTests` allowlists only `GraphCanvas.h` and
  `QtPanel.h`.

Implementation:

- Added `docs/recovery/phase-06-editor-deqt/PHASE_6R_EDITOR_PUBLIC_API_DEQT_CLOSURE.md`.
- Updated the roadmap Phase 6 status and acceptance wording to distinguish
  closure-with-deferred-debt from the still-deferred full public Qt-free
  target.
- No source, CMake, or allowlist changes were made.

Out of scope for this slice:

- no complete Qt removal claim
- no zero public Qt exposure claim
- no `GraphCanvas` migration
- no `QtPanel` removal or privatization
- no `SagaEditorLib` Qt visibility cleanup
- no graph model redesign
- no visual scripting product readiness claim
- no editor product readiness claim
- no full CTest

Recommended next slice:

- Phase 7A: start Editor Scaffolding Quarantine with a report-only scaffold
  inventory.

Full test health remains unverified.

## 2026-05-26 - Phase 7A Editor Scaffolding Inventory

Scope for this slice is report-only scaffold inventory:

- close Phase 6 as a de-Qtification checkpoint with explicit deferred debt
- scan editor-facing code for stubs, scaffolds, placeholders, TODOs, mock/demo
  flows, and success-return patterns
- classify scaffold surfaces as production-path stubs, high-risk future
  scaffolds, test-only placeholders, lab/test scaffolding, or real/partial
  editor behavior
- recommend a bounded Phase 7B workflow ownership map

Decision evidence:

- `Editor/src` contains 115 generated or explicit implementation-stub files.
- The largest clusters are Collaboration at 41 files and VisualScripting at
  32 files.
- Placeholder/TODO/not-wired search found 25 editor source/header/test files.
- `ProjectManager` is a production-path stub with a narrow public API:
  `OpenProject`, `CloseProject`, `IsProjectOpen`, `GetCurrentProject`, and
  `SetOnProjectChanged`.
- Production dashboard, diagnostics, persona/profile/customization,
  composition, command, and notification surfaces have more concrete behavior
  and focused tests than the generated stub clusters.

Implementation:

- Added `docs/recovery/phase-07-editor-scaffolding/PHASE_7A_EDITOR_SCAFFOLDING_INVENTORY.md`.
- Updated the roadmap with Phase 7A evidence and Phase 7B recommendation.
- No source, CMake, allowlist, or test behavior changes were made.

Out of scope for this slice:

- no stub-count reduction
- no project workflow implementation
- no editor shell rewrite
- no visual scripting or graph model behavior
- no collaboration product behavior
- no SDE or SagaPipeline invocation from editor startup
- no public Qt boundary change
- no full CTest

Recommended next slice:

- Phase 7B: map ownership for one project/workspace session workflow before
  implementation.

Full test health remains unverified.

## 2026-05-26 - Phase 7B Editor Workflow Ownership Map

Scope for this slice is a report-only workflow ownership map:

- close Phase 7A as editor scaffold inventory
- map the product/editor/lab ownership boundaries for project and workspace
  session flow
- identify editor-only gaps that can be implemented without product shell,
  SDE startup, visual scripting, or collaboration rewrites
- select one bounded Phase 7C implementation candidate

Decision evidence:

- `Apps/Saga/SagaProjectSystem` owns product project manifest create/open and
  recent-project persistence.
- `Apps/Saga/SagaApp` owns product-shell file dialogs and same-process editor
  view switching.
- `Apps/Saga/SagaEditorModule` converts product project sessions into
  `EditorWorkspaceDefinition` for `EditorHost`.
- `EditorHost` consumes prepared workspace state and initializes editor
  services.
- `EditorShell` owns shell commands and panels, but its standalone
  `saga.command.file.open_project` command remains a stub.
- `ProjectManager` is editor-local, currently unused, and has a narrow API
  suitable for a deterministic stateful open/close slice.

Implementation:

- Added `docs/recovery/phase-07-editor-scaffolding/PHASE_7B_EDITOR_WORKFLOW_OWNERSHIP_MAP.md`.
- Updated the roadmap with Phase 7B evidence and Phase 7C recommendation.
- No source, CMake, allowlist, or test behavior changes were made.

Out of scope for this slice:

- no project workflow implementation
- no editor menu or file-dialog wiring
- no movement of `SagaProjectSystem` into Editor
- no Editor dependency on Apps/Saga
- no SDE or SagaPipeline invocation from editor startup
- no product shell behavior change
- no visual scripting, graph model, collaboration, runtime, server, or package
  behavior
- no public Qt boundary change
- no full CTest

Recommended next slice:

- Phase 7C: implement only `ProjectManager` stateful open/close behavior with
  focused editor unit tests.

Full test health remains unverified.

## 2026-05-26 - Phase 7C ProjectManager Stateful Open/Close

Scope for this slice is one bounded editor workflow implementation:

- close Phase 7B as the project/workspace workflow ownership map
- replace the editor-local `ProjectManager` generated scaffold with real
  stateful open/close behavior
- add focused editor unit tests for manifest-backed open, directory fallback,
  missing-path rejection, invalid-manifest rejection, state exposure, and
  project-changed callback behavior

Implementation:

- `ProjectManager::OpenProject` now validates a non-empty existing directory or
  manifest file parent, reads `saga.project.json` metadata when available, and
  stores a current `ProjectInfo`.
- `ProjectManager::CloseProject` clears open state and current project data.
- `ProjectManager::IsProjectOpen`, `GetCurrentProject`, and
  `SetOnProjectChanged` now reflect real state.
- Added `Tests/Unit/Editor/ProjectManagerTests.cpp`.

Out of scope for this slice:

- no editor menu or file-dialog wiring
- no `EditorHost` or `EditorShell` ownership change
- no movement of `SagaProjectSystem` into Editor
- no Editor dependency on Apps/Saga
- no SDE or SagaPipeline invocation from editor startup
- no product shell behavior change
- no visual scripting, graph model, collaboration, runtime, server, or package
  behavior
- no public Qt boundary change
- no full CTest

Verification:

- `git diff --check`
- targeted `rg` for `ProjectManager`, `ProjectManagerTests`, Phase 7C,
  project/session ownership, non-goals, and remaining scaffold counts
- `Tools/Forge/bin/forge nix build --target SagaEditorLib --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `Tools/Forge/bin/forge nix build --target SagaUnitTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `build/RelWithDebInfo-0.0.9/bin/SagaUnitTests --gtest_filter=ProjectManagerTests.*`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`

Recommended next slice:

- Phase 7D: audit production dashboard and Problems panel diagnostics/readiness
  reality before implementing any wider editor workflow.

Full test health remains unverified.

## 2026-05-26 - Phase 7D Diagnostics Dashboard Reality

Scope for this slice is an audit-only diagnostics/dashboard reality pass:

- close Phase 7C as the first bounded scaffold behavior replacement
- inspect the production dashboard, Problems panel, shared diagnostics service,
  editor host diagnostics bridges, and product compile diagnostics flow
- classify real, partial, and deferred dashboard/diagnostics behavior
- stop before choosing a second Phase 7 implementation or Phase 7 closure path

Decision evidence:

- `EditorDiagnosticsService` is real and supports mutation, source replacement,
  filtering, and subscriptions.
- `ProblemsPanel` subscribes to the shared diagnostics service and displays
  severity, source, code, message, location, and publish-blocking status.
- `EditorShell` wires `ProblemsPanel` to `EditorHost` diagnostics.
- `ProductionDashboardPanel` displays real engine bridge, profile, persona, and
  customization status.
- `ProductionDashboardPanel` does not display workspace/project readiness or
  diagnostics counts.
- `SagaEditorModule::ValidateAndCompile` logs product SDE compile diagnostics
  to Console/status rather than the shared Problems diagnostics service.

Implementation:

- Added `docs/recovery/phase-07-editor-scaffolding/PHASE_7D_DIAGNOSTICS_DASHBOARD_REALITY.md`.
- Updated the roadmap with Phase 7D evidence and stop condition.
- No source, CMake, allowlist, or test behavior changes were made.

Out of scope for this slice:

- no dashboard row implementation
- no product compile diagnostics bridge into Problems
- no standalone Open Project command wiring
- no file dialogs
- no product shell behavior change
- no movement of `SagaProjectSystem` into Editor
- no SDE or SagaPipeline invocation from editor startup
- no visual scripting, graph model, collaboration, runtime, server, or package
  behavior
- no public Qt boundary change
- no full CTest

Stop condition:

- The next Phase 7 step would choose between a second bounded UI
  implementation and a Phase 7 closure checkpoint. That is a scope decision
  after the accepted one-workflow implementation.

Full test health remains unverified.

## 2026-05-26 - Phase 7E Closure And Phase 8 Opening

Scope for this slice is a docs/checkpoint closure:

- close Phase 7 as Editor Scaffolding Quarantine Foundation established
- record the accepted claims and exact non-claims
- keep remaining editor scaffolding, dashboard, SDE/customization, visual
  scripting, collaboration, UI polish, and full test health debt visible
- open Phase 8 with Phase 8A Claim Inventory / Evidence Matrix as the
  recommended first docs/report-only slice

Accepted Phase 7 claims:

- scaffold inventory/classification completed
- project/workspace ownership map completed
- one bounded workflow implemented through `ProjectManager`
- diagnostics/dashboard reality audit completed
- scaffold count reduced from 115 to 114
- public Qt ABI guard remains compliant

Exact non-claims:

- not complete editor product readiness
- not complete dashboard implementation
- not full project browser workflow
- not SDE/customization product completion
- not visual scripting or collaboration product readiness
- not full UI polish
- not full CTest health

Implementation:

- Added
  `docs/recovery/phase-07-editor-scaffolding/PHASE_7_CLOSURE_AND_PHASE_8_OPENING_CHECKPOINT.md`.
- Updated the roadmap Phase 7 status, Phase 7E evidence, remaining debt, and
  Phase 8 opening.
- No source, CMake, allowlist, test registration, or UI behavior changes were
  made.

Verification:

- `git diff --check`
- targeted `rg` for Phase 7 closure wording, accepted claims, non-claims,
  scaffold count, `ProjectManager`, diagnostics/dashboard, and Phase 8 opening
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R ArchitectureTests --output-on-failure -j 1`
- `build/RelWithDebInfo-0.0.9/bin/SagaUnitTests --gtest_filter=ProjectManagerTests.*`
- label inventory only for `architecture`, `editor`, `ui`, `product`,
  `collaboration`, and `integration`

Recommended next slice:

- Phase 8A: docs/report-only claim inventory and evidence matrix.

Full test health remains unverified.

## 2026-05-26 - Phase 8A Claim Inventory Evidence Matrix

Scope for this slice is docs/report-only claim inventory:

- inventory high-risk recovery-document claims for `complete`, `closed`,
  `implemented`, `production-ready`, `shipped`, `Qt-free`, `runtime-owned`,
  `server-authoritative`, and `publish-ready`
- classify claims as supported, partial, deferred debt, stale/needs
  correction, or future intent
- map supported claims to existing evidence docs, tests, and verification
  commands where available
- avoid hard-fail guards, source changes, test registration changes, publish
  enforcement, and roadmap-wide rewrites

Decision evidence:

- Recovery closure docs already phrase Phase 3, 4, 5, 6, and 7 as foundation
  or checkpoint closures with explicit non-claims.
- `production-ready` is supported only as a negative recovery truth:
  SagaEngine is not production-ready.
- `Qt-free` is supported only for bounded migrated surfaces; full public
  Editor Qt-free status remains deferred.
- `server-authoritative` is supported only as a Phase 4 movement foundation;
  full multiplayer authority remains deferred.
- `publish-ready` is future/deferred outside current publish coverage
  reporting.

Implementation:

- Added `docs/recovery/phase-08-doc-alignment/PHASE_8A_CLAIM_INVENTORY_EVIDENCE_MATRIX.md`.
- Updated the roadmap with Phase 8A evidence.
- No source, CMake, allowlist, test registration, publish enforcement, or
  hard-fail guard changes were made.

Verification:

- `git diff --check`
- targeted `rg` over recovery docs for high-risk claim terms
- targeted `rg` for Phase 8A wording, classification names, evidence matrix
  entries, and full CTest caveats

Full test health remains unverified.

## 2026-05-26 - Phase 8B Ownership Alignment Pass

Scope for this slice is docs/report-only ownership alignment:

- compare high-risk ownership claims against `docs/DependencyGraph.md`
- cross-check known CMake/source/test boundary risks from the boundary
  inventory
- classify ownership claims as supported, partial, deferred debt,
  stale/needs correction, or future intent
- avoid source, CMake, test registration, guard, publish enforcement, and
  roadmap-wide rewrite changes

Findings:

- Runtime startup/lifecycle, server authoritative movement, package/runtime
  asset readiness, product publish visibility, editor public Qt cleanup, and
  editor project/workspace ownership are all partial/foundation claims.
- CMake source/target ownership and test ownership evidence remain deferred
  debt because recursive globs, broad public target links, broad include roots,
  and coarse test executables remain visible.
- No recovery-roadmap ownership claim required immediate correction in this
  slice.
- Non-recovery roadmap wording with broad ownership/product-readiness language
  remains deferred to evidence-specific correction slices.

Implementation:

- Added `docs/recovery/phase-08-doc-alignment/PHASE_8B_OWNERSHIP_ALIGNMENT_PASS.md`.
- Updated the roadmap with Phase 8B evidence.
- No source, CMake, allowlist, test registration, publish enforcement, or
  hard-fail guard changes were made.

Verification:

- `git diff --check`
- touched-doc trailing whitespace check
- targeted `rg` for Phase 8B wording, ownership claims, classifications,
  stale claims, non-goals, and full CTest caveats
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`

Remaining Phase 8 debt:

- test evidence mapping
- report-only docs drift guard prototype
- roadmap normalization
- Phase 8 closure checkpoint

Full test health remains unverified.

## 2026-05-26 - Phase 8C Test Evidence Mapping

Scope for this slice is docs/report-only test evidence mapping:

- map major recovery claims to focused tests, guards, builds, and label
  inventories
- classify evidence as direct, partial, coarse, label inventory, or no evidence
- record gaps without adding tests, changing labels, changing CMake, or running
  full CTest

Findings:

- Editor public Qt boundary has direct focused guard evidence through
  `EditorQtPublicAbiBoundaryTests`, but full public Qt removal remains
  unproved.
- `ProjectManagerTests.*` directly supports only the bounded editor-local
  project manager workflow, not broader editor readiness.
- Runtime, server, package/asset, and publish claims have focused foundation
  evidence, but their broader ownership/product claims remain partial.
- Current CTest list-only inventory reports 38 entries and shows
  registered-but-unbuilt focused executables for some runtime/server entries.
- `collaboration` and `ui` label inventories list zero tests, so collaboration
  readiness and UI polish remain unproved.

Implementation:

- Added `docs/recovery/phase-08-doc-alignment/PHASE_8C_TEST_EVIDENCE_MAPPING.md`.
- Updated the roadmap with Phase 8C evidence.
- No source, CMake, allowlist, test registration, publish enforcement, or
  hard-fail guard changes were made.

Verification:

- `git diff --check`
- touched-doc trailing whitespace check
- targeted `rg` for Phase 8C wording, evidence classifications, label
  inventory, no-evidence claims, non-goals, and full CTest caveats
- CTest list-only inventory for all entries and labels: `architecture`,
  `runtime`, `server`, `asset`, `package`, `editor`, `product`,
  `integration`, `replication`, `collaboration`, and `ui`

Remaining Phase 8 debt:

- report-only docs drift guard prototype
- roadmap normalization
- Phase 8 closure checkpoint

Full test health remains unverified.

## 2026-05-26 - Phase 8D Docs Drift Guard Prototype

Scope for this slice is a report-only guard prototype:

- define high-risk stale or unsupported docs phrases
- record allowed contexts before enforcement
- document prerequisites for any future hard-fail guard
- avoid adding scripts, CMake targets, CI steps, source changes, test changes,
  publish enforcement, or roadmap rewrites

Findings:

- A hard-fail docs drift guard is premature because negative claims,
  non-claims, foundation closures, future intent, and bounded local evidence
  all intentionally use high-risk words.
- The safe next step is a report-only scanner concept that emits file, line,
  phrase, classification candidate, context, and suggested evidence owner.
- Recovery docs should be the first guard scope; broad non-recovery roadmap
  coverage should wait until false positives are understood.

Implementation:

- Added `docs/recovery/phase-08-doc-alignment/PHASE_8D_DOCS_DRIFT_GUARD_PROTOTYPE.md`.
- Updated the roadmap with Phase 8D evidence.
- No scanner, CMake target, CI step, source change, test change, publish
  enforcement, or hard-fail guard was added.

Verification:

- `git diff --check`
- touched-doc trailing whitespace check
- targeted `rg` for Phase 8D wording, high-risk phrases, false-positive
  controls, report-only guard status, non-goals, and full CTest caveats

Remaining Phase 8 debt:

- roadmap normalization
- Phase 8 closure checkpoint

Full test health remains unverified.

## 2026-05-26 - Phase 8E Roadmap Normalization

Scope for this slice is docs/report-only recovery roadmap normalization:

- normalize the Phase 8 status around completed 8A-8E evidence
- make Phase 8 non-claims and deferred debt explicit
- leave non-recovery roadmaps for later evidence-specific correction slices
- avoid source, CMake, test, label, guard, publish enforcement, and broad
  roadmap rewrite changes

Findings:

- Phase 8 has enough report-only evidence to prepare a narrow closure
  checkpoint.
- Hard-fail docs drift enforcement remains premature.
- Non-recovery roadmap stale-phrase correction remains deferred.
- Full dependency graph enforcement, source ownership hardening, and full test
  health remain unproved.

Implementation:

- Added `docs/recovery/phase-08-doc-alignment/PHASE_8E_ROADMAP_NORMALIZATION.md`.
- Updated the recovery roadmap Phase 8 status, acceptance caveats, Phase 8E
  evidence, and closure direction.
- No non-recovery roadmap, source, CMake, test registration, publish
  enforcement, or hard-fail guard changes were made.

Verification:

- `git diff --check`
- touched-doc trailing whitespace check
- targeted `rg` for Phase 8E wording, roadmap normalization, normalized
  claims, deferred debt, non-goals, and full CTest caveats

Remaining Phase 8 debt:

- Phase 8 closure checkpoint

Full test health remains unverified.

## 2026-05-26 - Phase 8F Closure Checkpoint

Scope for this slice is a docs/checkpoint closure:

- close Phase 8 as Documentation / Code Alignment Evidence established
- record accepted claims and exact non-claims
- keep docs drift enforcement, non-recovery roadmap normalization,
  CMake/source/test ownership hardening, publish enforcement, and full CTest
  health deferred
- recommend Phase 9 Local Evidence Gates as the next recovery phase

Accepted Phase 8 claims:

- claim inventory / evidence matrix completed
- ownership alignment pass completed
- test evidence mapping completed
- docs drift guard prototype scoped as report-only
- recovery roadmap Phase 8 normalized with explicit non-claims

Exact non-claims:

- not complete documentation correctness
- not non-recovery roadmap normalization
- not hard-fail docs drift enforcement
- not complete dependency graph enforcement
- not CMake/source/test ownership hardening
- not product/runtime/editor behavior readiness
- not publish readiness enforcement
- not full CTest health

Implementation:

- Added `docs/recovery/phase-08-doc-alignment/PHASE_8_CLOSURE_CHECKPOINT.md`.
- Updated the recovery roadmap Phase 8 status and Phase 8F evidence.
- No source, CMake, allowlist, test registration, publish enforcement, or
  hard-fail guard changes were made.

Verification:

- `git diff --check`
- touched-doc trailing whitespace check
- targeted `rg` for Phase 8 closure wording, accepted claims, non-claims,
  evidence links, remaining debt, Phase 9 recommendation, and full CTest
  caveats
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1`

Remaining Phase 8 debt is deferred to later owner phases:

- report-only docs drift scanner implementation before enforcement
- false-positive review across recovery and non-recovery docs
- stale non-recovery roadmap phrase correction
- CMake/source/test ownership hardening
- safe-suite or full CTest evidence only when explicitly scoped
- publish enforcement in product/publish phases

Execution stopped after Phase 8F because the requested Phase 8 closure point
was reached.

Full test health remains unverified.

## 2026-05-26 - Phase 9A Test Registry Inventory

Scope for this slice is a docs/report-only opening of Phase 9 Local Evidence
Gates:

- inventory the current `build/RelWithDebInfo-0.0.9` CTest registry
- record label counts, zero-test labels, registered-but-unbuilt entries, and
  evidence quality
- avoid source, CMake, test registration, label, quarantine, enforcement, safe
  suite, and full CTest changes

Findings:

- `ctest -N` lists 38 registered entries.
- `all` is not a raw CTest label in this build tree; the Phase 9A `all` count
  means all registered entries.
- Label inventory: `architecture` 2, `editor` 2, `ui` 0, `product` 2,
  `collaboration` 0, `runtime` 23, `server` 10, `networking` 5,
  `replication` 4, `asset` 14, `package` 12, `tools` 7, and `integration` 3.
- Zero-test labels: `ui`, `collaboration`.
- Twelve entries are registered but unbuilt in the current build tree:
  `ActorOwnershipRegistryTests`, `AuthoritativeMovementCoreTests`,
  `AuthoritativeMovementInputAdapterTests`,
  `AuthoritativeMovementCommandIntakeTests`,
  `ServerPacketNormalizationTests`, `ZoneServerPacketDrainTests`,
  `ZoneServerMovementAuthorityTests`, `MovementDirtyReplicationBridgeTests`,
  `RuntimeStartupDiagnosticsTests`, `RuntimeServiceLifecycleTests`,
  `RuntimeServiceRegistryTests`, and
  `RuntimeServiceRegistryDiagnosticsTests`.
- `UnitTests`, `IntegrationTests`, `ReplicationTests`, and `StressTests` remain
  broad/coarse evidence.
- Architecture, editor ABI, asset/package/runtime/package staging, publish
  readiness, and pipeline-focused entries are direct-intent evidence only when
  their executable is present and the test is actually run.

Implementation:

- Added `docs/recovery/phase-09-local-evidence/PHASE_9A_TEST_REGISTRY_INVENTORY.md`.
- Updated the recovery roadmap to open Phase 9 as Local Evidence Gates and
  record Phase 9A findings without claiming suite health.
- Clarified `docs/testing/TEST_SUITES.md` so Forge suite names are not treated
  as the complete raw CTest label inventory.

Verification:

- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L all`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L architecture`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L editor`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L ui`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L product`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L collaboration`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L runtime`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L server`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L networking`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L replication`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L asset`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L package`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L tools`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L integration`
- `rg -n "add_test|set_tests_properties|LABELS|unit;|architecture|editor|runtime|server|networking|replication|asset|package|tools|product|integration|collaboration|ui" cmake Tests docs`

Recommended Phase 9B: Label Cleanup / Registered-But-Unbuilt Test
Classification.

Full CTest remains unverified.

## 2026-05-26 - Phase 9B Registered-But-Unbuilt Classification

Scope for this slice is docs/report-only classification of the twelve Phase 9A
registered-but-unbuilt CTest entries:

- confirm CTest name, expected executable path, CMake target, source file,
  subsystem, labels, target existence, executable absence, stale/not-stale
  status, direct evidence value, and Phase 9C action
- avoid source, CMake, test registration, label, quarantine, focused test, and
  full CTest changes

Findings:

- All twelve entries have matching source files and CMake registration through
  `cmake/modules/SagaTests.cmake`.
- Each entry has a matching `add_executable`, `add_test`, and
  `set_tests_properties(... LABELS ...)` block.
- Each expected executable is absent from
  `build/RelWithDebInfo-0.0.9/bin` at the Phase 9B classification point.
- Root cause category: target exists but has not been built on demand in this
  build tree.
- None of the twelve entries are classified as stale registrations.
- The entries are focused/direct-intent evidence only after Phase 9C builds
  them and the focused CTest regex passes.

Implementation:

- Added `docs/recovery/phase-09-local-evidence/PHASE_9B_REGISTERED_BUT_UNBUILT_CLASSIFICATION.md`.
- Updated the recovery roadmap with Phase 9B evidence and the Phase 9C
  recommendation.
- Clarified `docs/testing/TEST_SUITES.md` that registered entries absent from
  the build tree are not pass evidence.

Verification:

- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L runtime`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L server`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L networking`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L replication`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L architecture`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L editor`
- targeted `rg` for the twelve CMake registrations
- targeted `rg --files` for the twelve source and subsystem files
- `git diff --check`
- touched-doc trailing whitespace scan
- targeted `rg` for Phase 9B wording

Phase 9C may proceed to focused on-demand target builds.

Full CTest remains unverified.

## 2026-05-26 - Phase 9C Focused Test Build Matrix

Scope for this slice is focused on-demand build verification for the twelve
Phase 9B entries:

- build each focused target individually with Forge/Nix and `--jobs=1`
- run only the focused twelve-entry CTest regex
- refresh `runtime`, `server`, `networking`, and `replication` list
  inventories after the builds
- avoid source, CMake, label, registration, quarantine, and broad test changes

Findings:

- All twelve targets built successfully:
  `ActorOwnershipRegistryTests`, `AuthoritativeMovementCoreTests`,
  `AuthoritativeMovementInputAdapterTests`,
  `AuthoritativeMovementCommandIntakeTests`,
  `ServerPacketNormalizationTests`, `ZoneServerPacketDrainTests`,
  `ZoneServerMovementAuthorityTests`, `MovementDirtyReplicationBridgeTests`,
  `RuntimeStartupDiagnosticsTests`, `RuntimeServiceLifecycleTests`,
  `RuntimeServiceRegistryTests`, and
  `RuntimeServiceRegistryDiagnosticsTests`.
- The focused twelve-entry CTest regex passed: 12 passed, 0 failed.
- Follow-up `ctest -N` lists all 38 entries without missing-executable
  warnings.
- Follow-up label inventories for `runtime`, `server`, `networking`, and
  `replication` show no missing-executable warnings.
- Remaining non-runnable registered entries: none observed in this build tree.

Implementation:

- Added `docs/recovery/phase-09-local-evidence/PHASE_9C_FOCUSED_TEST_BUILD_MATRIX.md`.
- Updated the recovery roadmap with Phase 9C focused build and test evidence.

Verification:

- twelve individual `Tools/Forge/bin/forge nix build --target ... --jobs=1`
  commands
- focused twelve-entry CTest regex with `--output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L runtime`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L server`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L networking`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L replication`
- `find build/RelWithDebInfo-0.0.9/bin -maxdepth 1 -type f -printf '%f\n'`
- `git diff --check`
- touched-doc trailing whitespace scan
- targeted `rg` for Phase 9C wording

Phase 9D may proceed to full normal CTest dry run.

Full CTest remains unverified until Phase 9D.

## 2026-05-26 - Phase 9D Full CTest Dry Run Triage

Scope for this slice is full normal CTest dry-run evidence after Phase 9C:

- attempt `ctest --test-dir build/RelWithDebInfo-0.0.9 --output-on-failure -j 1`
- record pass evidence only if CTest returns a complete passing summary
- classify failures or stop conditions without broad fixes
- avoid Phase 9E/9F if full CTest status remains unknown

Findings:

- Full CTest was attempted twice.
- In both attempts, tests #1 through #32 had passed before the terminal/session
  ended.
- Both attempts had reached `CSharpGameplayProofTests`.
- No deterministic CTest assertion failure output was captured.
- Because CTest did not return a complete summary, full CTest status is
  unknown and cannot be counted as passing.

Implementation:

- Added `docs/recovery/phase-09-local-evidence/PHASE_9D_FULL_CTEST_DRY_RUN_TRIAGE.md`.
- Updated the recovery roadmap to mark Phase 9 as open with partial evidence
  and Phase 9D as attempted but incomplete.

Stop decision:

- Phase 9E and Phase 9F are not created.
- Phase 10 remains blocked by unknown full CTest status.
- Next recommended slice is a controlled isolated run of
  `CSharpGameplayProofTests`, followed by full CTest only after the local
  session crash condition is understood.

Verification:

- Phase 9C focused build and focused CTest verification completed before this
  slice.
- Phase 9D full CTest did not complete.
- Final `git diff --check`, touched-doc whitespace scan, and targeted `rg`
  checks are deferred because repeated terminal/process execution crashed the
  local session during Phase 9D.

Full CTest remains unverified.

## 2026-05-26 - Phase 9D Full CTest Stability Blocker Follow-Up

Scope for this follow-up is list-only investigation of whether raw full CTest
includes opt-in heavy entries:

- do not run full CTest
- run only `ctest -N` and label list checks for `stress`, `slow`, `load`,
  `benchmark`, and `perf`
- inspect CMake/test/doc references for stress/slow/load/perf labels and
  `CSharpGameplayProofTests`
- document the blocker without relabeling, quarantining, or changing CMake

Findings:

- Full CTest did not complete in Phase 9D and must not be counted as passing.
- The terminal/session instability is a Phase 9D environment/test-stability
  blocker, not yet a classified test failure.
- `ctest -N` lists 38 registered entries.
- `ctest -N -L stress` lists `StressTests`.
- `ctest -N -L slow` lists `ReplicationTests` and `StressTests`.
- `ctest -N -L load` lists `StressTests`.
- `ctest -N -L benchmark` and `ctest -N -L perf` list zero entries.
- CMake registers `ReplicationTests` as
  `replication;integration;slow;long-running`.
- CMake registers `StressTests` as `stress;load;slow`.
- CMake registers `CSharpGameplayProofTests` as
  `unit;runtime;scripting;csharp`.

Implementation:

- Added `docs/recovery/phase-09-local-evidence/PHASE_9D_FULL_CTEST_STABILITY_BLOCKER.md`.
- Updated the recovery roadmap with the Phase 9D stability-blocker finding.

Stop decision:

- No safer normal-gate dry run was attempted because list-only inspection found
  registered stress/slow/load entries.
- Phase 9E and Phase 9F were not created.
- Phase 9 remains open.
- Phase 10 publish gate work remains blocked until Phase 9 full-test/evidence
  status is known.
- `CSharpGameplayProofTests` should be isolated later if it still appears near
  the crash point after heavy labels are excluded.

Verification:

- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L stress`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L slow`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L load`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L benchmark`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L perf`
- `rg -n "StressTests|stress|slow|load|benchmark|perf|CSharpGameplayProofTests|set_tests_properties|LABELS" cmake Tests docs`
- targeted CMake-only `rg` for the exact `CSharpGameplayProofTests`,
  `ReplicationTests`, and `StressTests` labels

Pending if safe:

- `git diff --check`
- touched-doc trailing whitespace scan
- targeted `rg` for the Phase 9D stability blocker wording

Full CTest remains unverified.

## 2026-05-26 - Phase 9D-2 Normal Gate Dry Run

Scope for this slice is the controlled normal local gate after the raw full
CTest stability blocker:

- run list-only normal gate selection with heavy label exclusion
- run the normal gate serially with `--timeout 120 -j 1`
- check whether `CSharpGameplayProofTests` is included and passes
- avoid source, CMake, labels, quarantine, tests, CI, and publish enforcement
  changes

Findings:

- The normal gate selected 36 entries.
- The heavy label exclusion removed `ReplicationTests` and `StressTests`.
- The normal gate passed 36/36.
- `CSharpGameplayProofTests` was included as test #33 and passed.
- Raw full CTest still is not a pass.
- Stress, slow, load, long-running, benchmark, and perf coverage remains
  excluded and unproven.

Implementation:

- Added `docs/recovery/phase-09-local-evidence/PHASE_9D_NORMAL_GATE_DRY_RUN.md`.
- Updated the recovery roadmap and test-suite documentation with normal/heavy
  gate wording.

Verification:

- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -LE "stress|slow|load|long-running|benchmark|perf"`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -LE "stress|slow|load|long-running|benchmark|perf" --output-on-failure --timeout 120 -j 1`

Full CTest remains unverified.

## 2026-05-26 - Phase 9D-4 Raw Full CTest Policy Decision

Scope for this slice is the raw full CTest policy after the normal gate passed:

- choose whether to retry raw full CTest once or block it as unsafe
- define the normal local gate
- classify `ReplicationTests` and `StressTests` as opt-in heavy
- avoid rerunning raw full CTest unless the policy explicitly chooses the retry

Decision:

- Selected Decision B/C: raw full CTest retry is unsafe for this local evidence
  path and blocked by heavy-test policy.
- The normal local gate is the heavy-label-excluded CTest command.
- `ReplicationTests` is opt-in heavy because it is `slow;long-running`.
- `StressTests` is opt-in heavy because it is `stress;load;slow`.
- Raw full CTest remains unresolved and cannot be used as pass evidence.

Implementation:

- Added `docs/recovery/phase-09-local-evidence/PHASE_9D_FULL_CTEST_POLICY_DECISION.md`.
- Updated the recovery roadmap and `docs/testing/TEST_SUITES.md`.

Full CTest remains unverified.

## 2026-05-26 - Phase 9E Local Gate Command Matrix

Scope for this slice is an honest local gate command matrix:

- classify always-local checks, focused test commands, the normal local gate,
  raw full CTest, and heavy opt-in gates
- use classifications `immediate local`, `CI candidate`, `opt-in only`,
  `blocked`, `deferred`, `unstable`, and `evidence-only`
- avoid CI enforcement or test registration changes

Findings:

- `git diff --check`, touched-doc whitespace scans, and targeted `rg` checks
  are always-local documentation checks.
- Architecture, editor ABI, project manager, runtime/package, asset/tool,
  product, server/networking, and C# focused tests are immediate local evidence
  when relevant.
- Deterministic focused tests and the normal local gate are CI candidates after
  matching prerequisites are available.
- `IntegrationTests` is evidence-only for timing-sensitive behavior even though
  it passed inside the normal gate.
- Raw full CTest is blocked/unsafe/unresolved.
- `StressTests` and `ReplicationTests` are opt-in only.

Implementation:

- Added `docs/recovery/phase-09-local-evidence/PHASE_9E_LOCAL_GATE_COMMAND_MATRIX.md`.
- Updated the recovery roadmap and test-suite documentation.

## 2026-05-26 - Phase 9F Closure Checkpoint

Scope for this slice is Phase 9 closure after the normal gate and policy matrix:

- choose the closure variant
- state accepted claims and non-claims
- record normal gate, raw full CTest, CSharp, heavy labels, CI readiness, and
  Phase 10 status
- avoid opening Phase 10 unless the closure document explicitly supports it

Decision:

- Closure variant is Variant B limited closure.
- Accepted claims are limited to Phase 9A inventory, Phase 9B classification,
  Phase 9C focused build/run evidence, Phase 9D-2 normal gate pass,
  `CSharpGameplayProofTests` pass inside that normal gate, heavy gate
  separation, and Phase 9E matrix readiness.
- Non-claims include raw full CTest pass, stress/load readiness, heavy
  replication readiness, release readiness, and publish hard-fail enforcement.
- Phase 10 may open as a publish gate / product packaging reality check using
  the Phase 9E matrix and normal local gate.

Implementation:

- Added `docs/recovery/phase-09-local-evidence/PHASE_9_CLOSURE_CHECKPOINT.md`.
- Updated the recovery roadmap with limited Phase 9 closure.

Full CTest remains unverified.

## 2026-05-26 - Phase 10A Publish Gate Reality Audit

Scope for this slice is a docs/report-first audit of current publish readiness:

- inspect publish-check CLI, publish readiness service, package staging,
  runtime package smoke, generated package, Phase 5 package/runtime evidence,
  and Phase 9 gate evidence
- classify current blockers, report-only checks, warnings, and deferred checks
- avoid behavior changes and CI enforcement

Findings:

- `Saga --publish-check` exists with `--publish-profile`, `--publish-report`,
  and `--publish-diagnostics`.
- Publish reports currently include schema, status, readiness, blockers,
  diagnostic summary, metadata, and `packageAssetIdentityCoverage`.
- Current blockers are deterministic project/package/explicit diagnostics
  failures.
- Package/asset/identity coverage diagnostics are report-only unless current
  package manifest validation already blocks.
- Phase 9 normal gate evidence is accepted; raw full CTest remains unresolved;
  `StressTests` and `ReplicationTests` remain heavy opt-in debt.

Implementation:

- Added `docs/recovery/phase-10-publish-gate/PHASE_10A_PUBLISH_GATE_REALITY_AUDIT.md`.

Verification:

- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "SagaPublishReadinessTests|SagaPackageStagingTests|RuntimePackageSmokeTests|GeneratedRuntimeSmokePackageTests" --output-on-failure -j 1`

Result: 4 passed, 0 failed.

## 2026-05-26 - Phase 10B Publish Report Schema Freeze

Scope for this slice is freezing the current publish report schema contract:

- document schema version 1
- classify stable top-level report fields and coverage fields
- add deterministic parsed JSON schema assertions to
  `SagaPublishReadinessTests`
- avoid a broad report rewrite or new hard gate

Implementation:

- Added `docs/recovery/phase-10-publish-gate/PHASE_10B_PUBLISH_REPORT_SCHEMA_FREEZE.md`.
- Extended `SagaPublishReadinessTests` with schema version, stable top-level
  field, readiness, blocker array, and package coverage assertions.

Verification:

- `Tools/Forge/bin/forge nix build --target SagaPublishReadinessTests --build=build/RelWithDebInfo-0.0.9 --jobs=1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R SagaPublishReadinessTests --output-on-failure -j 1`

Result: `SagaPublishReadinessTests` passed.

## 2026-05-26 - Phase 10C Report-Only Validation Expansion

Scope for this slice is bounded report-only validation expansion:

- keep new checks out of the hard blocker path
- add focused coverage tests for existing diagnostics seams
- document implemented, already implemented, docs-only planned, and deferred
  checks

Implementation:

- Added `docs/recovery/phase-10-publish-gate/PHASE_10C_REPORT_ONLY_VALIDATION_EXPANSION.md`.
- Added `SagaPublishReadinessTests` coverage for unknown asset kind and
  duplicate asset identity mapping as report-only diagnostics.
- No new blockers were introduced.

Verification:

- `SagaPublishReadinessTests` passed after rebuilding the focused target.

## 2026-05-26 - Phase 10D Publish Profile Policy

Scope for this slice is policy-first hard-gate eligibility:

- define `dev-local`, `package-smoke`, `shipping-report-only`,
  `shipping-full`, `CI-candidate`, and `future-enterprise`
- preserve current deterministic blockers
- keep raw full CTest unresolved, heavy gates, cook/import, ClientHost asset
  consumption, RuntimeServiceRegistry asset service, and full product proof as
  warning-only or deferred

Implementation:

- Added `docs/recovery/phase-10-publish-gate/PHASE_10D_PUBLISH_PROFILE_POLICY.md`.
- No broad profile implementation or CI hard enforcement was added.

## 2026-05-26 - Phase 10E Product Packaging Compatibility Proof

Scope for this slice is focused verification of package/publish/runtime
compatibility:

- run package manifest writer, asset manifest writer, identity writer,
  generated RuntimeSmoke, runtime package smoke, runtime asset, package
  staging, and publish readiness tests
- document the compatibility matrix
- avoid raw full CTest and heavy gates

Implementation:

- Added `docs/recovery/phase-10-publish-gate/PHASE_10E_PRODUCT_PACKAGING_COMPATIBILITY_PROOF.md`.

Verification:

- `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "PackageManifestWriterTests|AssetManifestWriterTests|AssetIdentityManifestWriterTests|AssetIdentityRuntimeContractTests|GeneratedRuntimeSmokeManifestTests|GeneratedRuntimeSmokePackageTests|RuntimePackageSmokeTests|RuntimeAssetBootstrapTests|RuntimeAssetCatalogTests|RuntimeAssetStartupBootstrapTests|SagaPackageStagingTests|SagaPublishReadinessTests" --output-on-failure -j 1`

Expected/recorded result for Phase 10E: 12 passed, 0 failed.

## 2026-05-26 - Phase 10F Closure Checkpoint

Scope for this slice is honest Phase 10 closure:

- summarize Phase 10A through Phase 10E
- choose closure variant
- state accepted claims and exact non-claims
- document schema, report-only validation, hard-gate, compatibility, Phase 9,
  and Phase 11 status

Decision:

- Closure variant is Variant A strong report/gate closure.
- Accepted claim: report-first publish readiness and selected existing
  deterministic blockers are established as a product packaging reality check.
- Non-claims include release readiness, product beta readiness, full
  AssetPipeline cook/import, full ClientHost/runtime asset consumption, raw
  full CTest pass, stress/load/performance readiness, heavy replication
  readiness, and default CI hard enforcement.
- Phase 11 may open only as a scoring re-audit that preserves Phase 10
  non-claims.

Implementation:

- Added `docs/recovery/phase-10-publish-gate/PHASE_10_CLOSURE_CHECKPOINT.md`.
- Updated the recovery roadmap and test-suite documentation.

Full CTest remains unverified.

## 2026-05-26 - Phase 11A Scoring Rubric Refresh

Scope for this slice is docs/audit-only scoring rubric definition:

- define Phase 11 scoring categories before assigning scores
- define 0/10 through 10/10 scoring levels
- define evidence quality levels, classification levels, and score caps
- avoid source, CMake, test behavior, raw full CTest, heavy gates, and feature
  work

Implementation:

- Added `docs/recovery/phase-11-scoring/PHASE_11A_SCORING_RUBRIC.md`.
- Updated the recovery roadmap with Phase 11A evidence.

Result:

- Score caps explicitly cover raw full CTest unresolved, heavy gate debt,
  missing product vertical proof, incomplete AssetPipeline source import/cook,
  incomplete runtime/client asset consumption, incomplete RuntimeServiceRegistry
  asset service, and incomplete editor product readiness.

## 2026-05-26 - Phase 11B Evidence Collection Matrix

Scope for this slice is docs/audit-only evidence collection:

- map each scoring category to accepted evidence, proof quality, missing
  evidence, deferred debt, confidence, and score caps
- avoid scoring before evidence collection
- preserve Phase 9 and Phase 10 non-claims

Implementation:

- Added `docs/recovery/phase-11-scoring/PHASE_11B_EVIDENCE_COLLECTION_MATRIX.md`.
- Updated the recovery roadmap with Phase 11B evidence.

Result:

- Evidence matrix classifies direct proof, focused proof, partial proof, coarse
  proof, docs-only proof, report-only proof, explicitly deferred debt, no proof,
  and contradicted/unsafe claim.

## 2026-05-26 - Phase 11C Hard Blocker / Soft Blocker Review

Scope for this slice is blocker classification only:

- classify hard blockers, soft blockers, deferred debt, evidence blockers, and
  environment/test blockers
- do not fix blockers or start post-recovery work

Implementation:

- Added `docs/recovery/phase-11-scoring/PHASE_11C_BLOCKER_REVIEW.md`.
- Updated the recovery roadmap with Phase 11C evidence.

Result:

- Product beta and release candidate remain blocked by raw full CTest
  unresolved, heavy gates unresolved, missing stress/load/performance evidence,
  incomplete AssetPipeline source import/cook, incomplete runtime/client asset
  consumption, missing RuntimeServiceRegistry asset service, missing playable
  product proof, incomplete editor product readiness, incomplete replication
  product loop, absent CI hard enforcement, and release packaging not proven.

## 2026-05-26 - Phase 11D Final Score Re-Audit

Scope for this slice is evidence-backed scoring:

- score each category from accepted evidence and caps
- choose final classification
- avoid score inflation based on roadmap intent

Implementation:

- Added `docs/recovery/phase-11-scoring/PHASE_11D_FINAL_SCORE_REAUDIT.md`.
- Updated the recovery roadmap with Phase 11D evidence.

Result:

- Overall foundation score: 6/10.
- Unweighted subsystem average: 5.4/10.
- Final classification: Foundation Established.
- Foundation Complete Candidate, Product Beta Candidate, and Release Candidate
  are rejected under current evidence.

## 2026-05-26 - Phase 11E Post-Recovery Roadmap Recommendation

Scope for this slice is roadmap recommendation only:

- recommend post-recovery tracks and priorities
- do not implement post-recovery roadmap items

Implementation:

- Added `docs/recovery/phase-11-scoring/PHASE_11E_POST_RECOVERY_ROADMAP.md`.
- Updated the recovery roadmap with Phase 11E evidence.

Result:

- Priority order is Product Vertical Proof, Runtime/ClientHost Asset
  Consumption, AssetPipeline Source Import/Cook, Server/Replication Product
  Loop, Test/CI Hardening, Editor Productization, and Release/Packaging
  Hardening.
- First recommended post-recovery slice is Product Vertical Proof 1A:
  MultiplayerSandbox package/playable proof audit.

## 2026-05-26 - Phase 11F Closure Checkpoint

Scope for this slice is Phase 11 and recovery roadmap closure:

- summarize Phase 11A through Phase 11E
- record final score and classification
- state accepted claims, non-claims, hard blockers, soft blockers, deferred
  debt, and post-recovery roadmap

Implementation:

- Added `docs/recovery/phase-11-scoring/PHASE_11_CLOSURE_CHECKPOINT.md`.
- Updated the recovery roadmap to close Phase 11 and the recovery roadmap as
  Foundation Established.

Decision:

- Closure outcome is Outcome A: Recovery closed as Foundation Established.
- Product beta, release candidate, raw full CTest pass, stress/load/performance
  readiness, heavy replication readiness, full AssetPipeline cook/import, full
  runtime/client asset consumption, full product proof, and CI hard enforcement
  remain non-claims.

Verification:

- Docs-only verification; no raw full CTest and no heavy gates.

## 2026-05-26 - Docs Indexing Entry Layer Slice

Scope for this slice is documentation navigation only:

- add short README entry points without deleting, moving, or shortening Phase
  files
- classify the recovery roadmap as a closed recovery record
- preserve Foundation Established, Product Beta, Release Candidate, raw full
  CTest, and heavy gate non-claims
- avoid source, CMake, test behavior, roadmap claim, or closure-content changes

Implementation:

- Added `docs/README.md`.
- Added `docs/architecture/README.md`.
- Added `docs/roadmaps/README.md`.
- Added `docs/testing/README.md`.
- Added `docs/dev/README.md`.
- Added a small closed-record note to
  `docs/roadmaps/ENGINE_RECOVERY_ROADMAP.md`.
- Added a quick-entry pointer to `docs/testing/TEST_SUITES.md`.

Result:

- `docs/README.md` is the current first-read entry point.
- Existing Phase files remain in place as historical evidence.
- No raw full CTest and no heavy gates were run.

## 2026-05-26 - Recovery Archive Reorganization Slice

Scope for this slice is docs-only archive organization:

- move closed recovery `PHASE_*` files from `docs/architecture/` into
  `docs/recovery/` phase folders
- add clean architecture summary entry points
- preserve Foundation Established, Product Beta, Release Candidate, raw full
  CTest, heavy gate, score, closure, and non-claim wording
- avoid source, CMake, test behavior, raw full CTest, and heavy gates

Implementation:

- Moved Phase 3 through Phase 11 evidence files into `docs/recovery/`.
- Added `docs/recovery/README.md`.
- Reworked `docs/architecture/README.md` as a clean architecture entry point.
- Added current architecture summary pages for status, ownership, runtime,
  server, assets/packages, editor, publish, and testing/evidence.
- Updated stale architecture Phase references to recovery paths.

Result:

- `docs/architecture/` no longer contains top-level `PHASE_*` files.
- Recovery evidence remains available as historical archive material.
- No raw full CTest and no heavy gates were run.

## 2026-05-24 - Phase 2B-1 First Narrow Boundary Hardening Slice

Scope for this slice is the first narrow CMake boundary hardening candidate only:

- keep `SagaEditorLabLib` publicly linked to `SagaEditorLib` because public EditorLab headers expose SagaEditor types
- move the direct `Qt6::Core` and `Qt6::Widgets` links on `SagaEditorLabLib` from `PUBLIC` to `PRIVATE`
- leave `SagaEditorLib` Qt exposure untouched for the later Editor public API de-Qtification phase

Out of scope for this slice:

- no broad dependency cleanup
- no hard-fail gates
- no recursive glob rewrite
- no test executable split
- no Runtime/App ownership refactor
- no editor public API refactor
- no source movement

Full test health remains unverified.

## 2026-05-24 - Phase 2B-3 Server Collaboration Visibility Slice

Scope for this slice is one bundled low-risk Phase 2B continuation:

- checkpoint current recovery state and dirty worktree risk
- inspect and rank narrow CMake boundary candidates
- keep `SagaServerLib` publicly linked to `SagaEngine` and `SagaShared`
- move the direct `SagaCollaboration` link on `SagaServerLib` from `PUBLIC` to `PRIVATE`
- document rejected candidates without starting their later-phase work

Candidate audit result:

- `SagaServerLib -> SagaCollaboration`: selected; `Server/include` and `Server/src` did not reference `SagaCollaboration` or `Collaboration`, so Collaboration is not currently observed as Server public ABI.
- `SagaEditorLib` Qt exposure: rejected for this slice because public Editor headers expose Qt through `QtPanel`/`QWidget`.
- `SagaProductLib` Qt/Product hub exposure: rejected as too broad; `Apps/Saga/SagaEditorModule.h` exposes Qt-shaped public activation contracts.
- `SagaEditorCompositionLib -> SDE::Core`: rejected because `CompositionCompiler.h` exposes SDE result and diagnostic types.
- `SagaSandboxLib`: deferred as too broad/uncertain for a one-edge Phase 2B slice.

Out of scope for this slice:

- no broad dependency cleanup
- no hard-fail gates
- no recursive glob rewrite
- no test executable split
- no Runtime/App ownership refactor
- no server-authoritative movement implementation
- no asset pipeline implementation
- no publish gate implementation
- no editor public API refactor
- no source movement

Full test health remains unverified.

## 2026-05-24 - Phase 2C Boundary Enforcement Model Slice

Scope for this slice is documentation-only Phase 2 closure preparation:

- define how the CMake boundary inventory should be enforced over time
- separate report-only legacy violations from future hard-fail candidates
- record active cleanup that is only partially addressed
- identify issues that belong to Phase 3, Phase 4, Phase 5, Phase 6, and Phase 10 instead of Phase 2
- add Phase 2 closure criteria and Phase 3 readiness criteria

Enforcement model:

- legacy violations remain report-only until their owner slice has cleanup evidence
- cleaned or new boundaries can become hard-fail candidates after their public API and target graph are proven
- later-phase ownership problems must not be hidden inside broad CMake cleanup
- rejected candidates stay documented with the reason they are unsafe or too broad

Out of scope for this slice:

- no CMake behavior changes
- no target dependency changes
- no hard-fail gates
- no recursive glob rewrite
- no test target split
- no Runtime/App ownership refactor
- no server-authoritative movement implementation
- no asset pipeline implementation
- no publish gate implementation
- no editor public API refactor
- no source movement

Full test health remains unverified.

## 2026-05-24 - Phase 3B-1 Runtime Startup Preflight Ownership Slice

Scope for this slice is the first narrow Runtime ownership implementation:

- add a Runtime-owned startup package preflight facade in `SagaRuntimeLib`
- keep the process entrypoint and CLI parsing in `Apps/Runtime`
- move only the startup package validation handoff behind the Runtime facade
- add focused Runtime preflight tests that do not depend on `Apps/Client`

Out of scope for this slice:

- no `ClientHost` movement
- no `ClientNetworkSession` split
- no UI bootstrap ownership migration
- no rendering ownership migration
- no network ownership migration
- no asset pipeline implementation
- no server-authoritative movement implementation
- no publish gate implementation
- no broad Runtime/App file movement

Full test health remains unverified.

## 2026-05-24 - Phase 3B-2 Runtime Startup Session Facade Slice

Scope for this slice is the next narrow Runtime ownership implementation:

- add a Runtime-owned startup/session preparation facade in `SagaRuntimeLib`
- keep `RuntimeLaunchOptions` and `RuntimeSessionDescriptor` runtime-facing only
- keep `Saga::ClientConfig` conversion local to `Apps/Runtime`
- reuse `RuntimeStartupPreflight` inside the Runtime startup session facade
- add focused Runtime startup session tests that do not depend on `Apps/Client`

Out of scope for this slice:

- no `ClientHost` movement
- no `ClientNetworkSession` split
- no UI bootstrap ownership migration
- no rendering ownership migration
- no network ownership migration
- no asset pipeline implementation
- no server-authoritative movement implementation
- no publish gate implementation
- no broad Runtime/App file movement

Full test health remains unverified.

## 2026-05-24 - Phase 3B-3 Runtime Startup Diagnostics Facade Slice

Scope for this slice is the next narrow Runtime ownership implementation:

- add Runtime-owned startup report diagnostic classification
- keep app logging text and process exit policy in `Apps/Runtime`
- classify startup reports as ready, preflight-skipped, or blocked
- provide structured diagnostic views for app-owned logging
- add focused Runtime startup diagnostics tests that do not depend on `Apps/Client`

Out of scope for this slice:

- no `ClientHost` movement
- no `ClientNetworkSession` split
- no UI bootstrap ownership migration
- no rendering ownership migration
- no network ownership migration
- no asset pipeline implementation
- no server-authoritative movement implementation
- no publish gate implementation
- no broad Runtime/App file movement

Full test health remains unverified.
