# SagaEngine Recovery Roadmap

> Last updated: 2026-05-26
> Status: Closed recovery record; Foundation Established; not a
> shipped-feature checklist
> Target: Move SagaEngine from a large multi-file prototype toward a boundary-enforced, testable, production-grade engine foundation with real vertical slices.
> Current truth: SagaEngine is not production-ready.
> Closure note: Recovery roadmap closed as Foundation Established on
> 2026-05-26.
> Current entry point: [docs/README.md](../README.md).

---

## 0. Roadmap Convention

- This roadmap records recovery work. It does not mark planned work as shipped.
- A recovery phase is complete only when code, tests, docs, and build evidence support the claim.
- Roadmap intent, future plans, scaffolding, and partial implementations are not shipped evidence.
- Unknown full-test health must be written as unverified, not implied as passing.
- Heavy tests must not be run by default on this machine; use single-job safe profiles first.

The first implementation slice for this roadmap is Phase 0 only.

Phase 0 allows:

- create or update `docs/roadmaps/ENGINE_RECOVERY_ROADMAP.md`
- create or update `docs/dev/ITERATION_NOTES.md`

Phase 0 explicitly forbids:

- code movement
- CMake behavior changes
- hard-fail gates
- Runtime/App ownership refactors
- server-authoritative movement implementation
- asset pipeline implementation
- editor Qt refactor

---

## 1. Current Recovery Truth

The following items are known incomplete. They are not bugs to hide and they are not shipped capability:

- Runtime has a startup/lifecycle foundation, but it does not yet own real app
  services or complete Runtime/App composition.
- Server gameplay is not yet authoritative enough to support MMO claims.
- Gameplay handlers still include success-returning stub behavior.
- Editor contains substantial scaffold and placeholder surface area.
- Public Editor API still exposes Qt details.
- CMake target boundaries are too loose and can hide dependency violations.
- Tests are not isolated by subsystem strongly enough.
- Asset pipeline ownership is incomplete.
- Publish/package readiness is not yet enforced by a product publish gate.
- Full test health is unverified.

These items must stay visible until repo evidence proves otherwise.

---

## 2. Non-Goals During Recovery

- Do not add broad new features.
- Do not add large batches of new editor panels.
- Do not implement a complete MMO system in one pass.
- Do not expand renderer, scripting, collaboration, or visual scripting scope beyond recovery needs.
- Do not mark roadmap checkboxes as shipped because work is partially nearby.
- Do not add new stubs as placeholders for future work.

---

## 3. Recovery Phases

### Phase 0 - Truth Freeze

Status: active documentation-only slice.

Work:

- Maintain this recovery roadmap.
- Add recovery start notes to `docs/dev/ITERATION_NOTES.md`.
- Keep known incomplete work explicit.
- Preserve the statement that SagaEngine is not production-ready.

Acceptance:

- No code is moved.
- No CMake behavior changes.
- No roadmap checkbox is falsely completed.
- Only truth-freeze documentation changes are made.

### Phase 1 - Build/Test Baseline

Status: closed as Runtime startup/lifecycle foundation established.

Work:

- Add test suites for `unit`, `architecture`, `runtime`, `server`, `networking`, `replication`, `asset`, `editor`, `tools`, `integration`, `stress`, and `slow`.
- Support safe single-job commands such as `forge test --suite architecture --jobs 1`.
- Add `all-safe` as the default local safety profile.
- Keep `stress`, `slow`, load, long-running integration, and network timing-sensitive tests outside `all-safe`.
- Preserve existing `forge test --label <regex>` behavior.

Acceptance:

- Core suites can run separately.
- Failing suites identify the failing area.
- Test health is no longer described as a single monolithic claim.
- Full test health remains unverified unless every relevant suite is actually run and passes.

### Phase 2 - CMake / Target Boundary Hardening

Status: planned.

Phase split:

- Phase 2A is report-only CMake / target boundary inventory.
- Phase 2B is the first behavior-changing dependency hardening slice.
- Phase 2C is boundary enforcement model and Phase 2 closure preparation. Test graph narrowing remains a later narrow follow-up unless it can be done without broad target churn.

Work:

- Reduce excessive `PUBLIC` dependencies, starting with `SagaEditorLib`, `SagaProductLib`, tests, Backends, Runtime, and Server.
- Keep Qt private unless it is truly public ABI.
- Prevent Server, Editor, Apps, Product, and tool internals from leaking into Engine public API.
- Add progressive boundary compile tests.
- Reduce recursive glob risk over time with explicit source ownership for new or risky modules.

Acceptance:

- Converted areas produce compile failures for wrong-layer includes.
- CMake no longer masks dependency violations in those areas.

Phase 2 sufficiently complete means:

- The main CMake boundary inventory exists.
- Test suite visibility exists.
- Several low-risk direct public edges have been narrowed with build evidence.
- Remaining high-risk violations are classified as report-only, future hard-fail candidates, or later-phase ownership work.
- The enforcement strategy is documented.
- No claim is made that all CMake boundaries are fixed.
- Full test health remains unverified unless it is actually proven.

### Phase 3 - Runtime Ownership Realignment

Status: closed as Runtime startup/lifecycle foundation established.

Work:

- Move runtime startup/session/service ownership into Runtime.
- Keep Apps focused on CLI parsing, product mode, process launch, and product bootstrap.
- Keep Engine runtime-neutral.

Acceptance:

- Runtime is more than a symbolic descriptor layer.
- Runtime tests do not need to link Apps.

Phase 3 readiness criteria:

- Runtime/App boundary risks are understood.
- Runtime public API and current CMake dependencies are documented.
- Existing Runtime tests are inventoried.
- The first Phase 3 slice starts with an audit of `Apps/Client` versus Runtime responsibilities.
- Phase 3 must not begin by moving many files at once.

Phase 3C lifecycle checkpoint:

- `docs/recovery/phase-03-runtime/PHASE_3C_LIFECYCLE_OWNERSHIP_CHECKPOINT.md` records Phase 3C as a Runtime lifecycle foundation package.
- Runtime startup/session/diagnostics are `startup-owned`; service lifecycle contracts, registry, and registry diagnostics are `registry-ready`.
- Apps/Runtime has a narrow registry integration shell through Phase 3C-4.
- `ClientHost`, `ClientNetworkSession`, and `SagaRuntime` reuse of `Apps/Client/ClientHost.h/.cpp` remain documented Phase 3 debt.
- Full test health remains unverified unless the relevant suite set is actually run.

Phase 3 closure checkpoint:

- `docs/recovery/phase-03-runtime/PHASE_3_CLOSURE_AND_PHASE_4A_OPENING_CHECKPOINT.md` closes Phase 3 only as Runtime startup/lifecycle foundation established.
- Phase 3 does not close complete Runtime/App ownership, `ClientHost` movement, `ClientNetworkSession` split, real Runtime service ownership, or full test health.

### Phase 4 - Server Authoritative Movement Vertical Slice

Status: closed as Server Authoritative Movement Foundation established.

Work:

- Start with Phase 4A audit only: inspect current server authority, client command flow, replication/prediction boundaries, server/session ownership, dependency edges, related tests, and the minimum authoritative loop needed for a future `MultiplayerSandbox`.
- After Phase 4A and separate approval, implement one real movement slice: command, validation, authoritative mutation, fixed tick, replication output, and deterministic tests.
- After Phase 4A and separate approval, replace success-returning gameplay handler stubs in that path with explicit validation and diagnostics.

Phase 4A non-goals:

- no broad server/network rewrite
- no full MMO gameplay implementation
- no mass server/client file movement
- no Runtime ownership continuation hidden inside server work
- no authoritative movement implementation unless separately approved after the audit

Acceptance:

- A valid movement command mutates server-owned state.
- Invalid or unauthorized movement is rejected.
- Accepted movement emits replication intent.

Phase 4 closure checkpoint:

- `docs/recovery/phase-04-server-authority/PHASE_4_CLOSURE_AND_PHASE_5A_OPENING_CHECKPOINT.md` closes Phase 4 only as Server Authoritative Movement Foundation established.
- Phase 4 delivered a deterministic foundation across audit, movement core, input adapter, command intake, packet normalization, ZoneServer normalized drain, actor ownership, ZoneServer movement authority shell, and movement dirty replication intent bridge.
- Phase 4 does not close `ReplicationManager` movement integration, accepted-state snapshot serialization, client reconciliation proof, `ClientHost` / `ClientNetworkSession` cleanup, or full end-to-end authoritative multiplayer.
- Full test health remains unverified unless the relevant suite set is actually run.

### Phase 5 - Asset Pipeline Ownership V1

Status: closed as Package / Asset / Runtime Readiness Foundation established;
follow-up debt deferred.

Work:

- Make `Tools/AssetPipeline` own source discovery, import, cook, artifact writing, identity allocation, diagnostics, and reports.
- Keep Engine focused on cooked artifact loading and runtime registry contracts.
- Use one UI document asset as the first end-to-end slice.

Acceptance:

- Source asset cooking is not owned by Engine runtime code.
- One cooked UI asset flows through pipeline, staging, manifest, and Runtime loading.

Phase 5A opening audit:

- `docs/recovery/phase-04-server-authority/PHASE_4_CLOSURE_AND_PHASE_5A_OPENING_CHECKPOINT.md` opens Phase 5A as report-only.
- Phase 5A must inspect package manifest path, asset manifest path, runtime asset registry bootstrap, asset identity manifest, content/package example readiness, future `MultiplayerSandbox` asset needs, Runtime/server package consumption, publish gate relationship, and current AssetPipeline/Runtime/Product test and CMake boundaries.
- Phase 5A must not implement a broad asset pipeline rewrite, package format rewrite, Runtime asset service migration, publish gate, broad CMake boundary hardening slice, or UI/editor asset workflow.

Phase 5B runtime package smoke fixture evidence:

- `Tests/Fixtures/Packages/RuntimeSmoke` provides a checked-in client package manifest, asset manifest, asset identity manifest, and one dummy cooked texture artifact.
- `RuntimePackageSmokeTests` proves the fixture passes `RuntimeStartupGate`, registers one identity-backed texture into `AssetRegistry`, and fails without registry mutation when identity coverage or the cooked asset file is missing.
- This does not close asset cook, AssetPipeline identity manifest writing, Runtime app startup registry ownership, server package consumption, publish hard-fail behavior, or UI/document asset kind support.

Phase 5C AssetPipeline identity to Runtime contract evidence:

- `AssetIdentityRuntimeContractTests` proves `AssetIdentityGenerator` output can be serialized in the Runtime identity manifest schema and loaded by `AssetIdentityManifestLoader`.
- The focused test proves previous ID reuse and deterministic new ID allocation survive Runtime resolver construction.
- The focused test overwrites only the RuntimeSmoke temp-copy identity manifest with generated mappings and proves `RuntimeAssetRegistryBootstrapper` can register the fixture asset.
- This still does not close a production AssetPipeline writer, asset manifest generation, package staging generation, asset cook, `Apps/Runtime` asset service ownership, publish gate behavior, or UI/document asset kind support.

Phase 5D production asset identity manifest writer evidence:

- `AssetIdentityManifestWriter` lets `Tools/AssetPipeline` write identity mappings to the Runtime `asset_identity.json` schema.
- `AssetIdentityManifestWriterTests` prove writer output emits schema version `1`, loads through `AssetIdentityManifestLoader`, and preserves generated stable IDs through Runtime resolver construction.
- The writer validates invalid mappings before writing, including empty keys, reserved ID `0`, duplicate keys, and duplicate IDs.
- This still does not close an AssetPipeline CLI, source discovery, asset import/cook, asset manifest generation, package staging generation, `Apps/Runtime` asset service ownership, publish gate behavior, or UI/document asset kind support.

Phase 5E production asset manifest writer evidence:

- `AssetManifestWriter` lets `Tools/AssetPipeline` write runtime-ready assets to the Runtime `assets.json` schema.
- `AssetManifestWriterTests` prove writer output emits schema version `1`, required asset fields, optional Runtime metadata fields, and loads through `AssetManifestLoader`.
- The writer validates invalid entries before writing, including empty ids, unsupported kinds, unsafe paths, duplicate ids, and empty dependencies.
- The focused tests prove generated asset and identity manifests can bootstrap one `AssetRegistry` entry without adding cook, package manifest generation, package staging rewrite, or `Apps/Runtime` startup integration.
- This still does not close an AssetPipeline CLI, source discovery, asset import/cook, package manifest generation, package staging generation, `Apps/Runtime` asset service ownership, publish gate behavior, UI/document asset kind support, or MultiplayerSandbox package readiness.

Phase 5F generated RuntimeSmoke manifest replacement evidence:

- `GeneratedRuntimeSmokeManifestTests` prove the production identity and asset manifest writers can replace the RuntimeSmoke fixture's `asset_identity.json` and `assets.json` in a temp package.
- The temp package keeps `package_manifest.client.json` unchanged and proves those package references can target generated manifests.
- `RuntimeStartupGate` accepts the generated-manifest RuntimeSmoke package, and `RuntimeAssetRegistryBootstrapper` registers the generated identity-backed texture into `AssetRegistry`.
- Missing generated identity or asset manifests fail without mutating the registry.
- This still does not close an AssetPipeline CLI, source discovery, asset import/cook, package manifest generation, package staging generation, `Apps/Runtime` asset service ownership, publish gate behavior, UI/document asset kind support, or MultiplayerSandbox package readiness.

Phase 5G generated RuntimeSmoke package evidence:

- `PackageManifestWriter` lets `Tools/AssetPipeline` write Runtime-compatible package manifests without exposing Runtime package structs in the writer API.
- `PackageManifestWriterTests` prove writer output emits schema version `1`, required package metadata, optional identity and hash fields, required asset/artifact manifest arrays, and loads through `PackageManifestLoader`.
- The writer validates required fields, package kind, unsafe/escaping manifest paths, empty manifest ref ids, and duplicate manifest ref ids before writing.
- `GeneratedRuntimeSmokePackageTests` prove production package, asset, and identity manifest writers can generate a RuntimeSmoke temp package consumed by `RuntimeStartupGate`.
- `RuntimeAssetRegistryBootstrapper` registers the fully generated package's identity-backed texture into `AssetRegistry`, and lookup works by `AssetKey` and `AssetId`.
- Missing generated package, asset, or identity manifest references fail predictably, and invalid generated asset manifest content fails without mutating the registry.
- This still does not close an AssetPipeline CLI, source discovery, asset import/cook, package staging integration, `Apps/Runtime` asset service ownership, publish gate behavior, UI/document asset kind support, or MultiplayerSandbox package readiness.

Phase 5H package staging generated manifest integration evidence:

- `SagaPackageStagingService` now uses `SagaAssetPipeline::PackageManifestWriter` to emit staged client/server package manifests instead of hand-building the package JSON locally.
- Product staging remains the orchestrator: it discovers preexisting generated identity, asset, artifact, and script artifact manifests and passes those references into the writer.
- `Build/Manifests/asset_identity.json` is optional; when present it is written as `assetIdentityManifest`, and when absent staging preserves the previous loader-compatible package shape.
- Focused `SagaPackageStagingTests` generate `asset_identity.json` and `assets.json` with AssetPipeline writers, stage the package, load the staged package through `PackageManifestLoader`, validate it through `RuntimeStartupGate`, and register one texture through `RuntimeAssetRegistryBootstrapper`.
- Runtime startup and asset registry bootstrap options now accept an explicit package base directory while preserving package-manifest-folder-relative behavior by default.
- This still does not close an AssetPipeline CLI, source discovery, asset import/cook, `Apps/Runtime` asset service ownership, publish hard-fail behavior, UI/document asset kind support, or MultiplayerSandbox package readiness.

Phase 5I Runtime Asset Startup Integration Audit evidence:

- `docs/recovery/phase-05-assets-runtime/PHASE_5H_CLOSURE_AND_PHASE_5I_RUNTIME_ASSET_STARTUP_AUDIT.md` closes Phase 5H only as package staging generated manifest integration.
- Phase 5I confirms `Apps/Runtime` already has a local `RuntimeServiceRegistry` shell, but no durable AssetRegistry ownership model.
- Phase 5I confirms `RuntimeStartupPreflight` and `RuntimeStartupSession` validate packages, but do not yet propagate `packageBaseDirectory` or return a loaded package manifest, bootstrap plan, or populated `AssetRegistry`.
- Phase 5I decides the next implementation should be a tested Runtime asset bootstrap facade before Apps/Runtime startup wiring or RuntimeServiceRegistry asset service integration.
- Recommended Phase 5J candidate is a narrow `SagaRuntime::RuntimeAssetBootstrap` facade over `RuntimeStartupGate`, `PackageManifestLoader`, and `RuntimeAssetRegistryBootstrapper`.
- This still does not close an AssetPipeline CLI, source discovery, asset import/cook, Apps/Runtime asset startup integration, publish hard-fail behavior, UI/document asset kind support, or MultiplayerSandbox package readiness.

Phase 5J Runtime Asset Bootstrap facade evidence:

- `SagaRuntime::RuntimeAssetBootstrap` adds a Runtime-owned facade that validates a package through `RuntimeStartupGate` and registers identity-backed package assets through `RuntimeAssetRegistryBootstrapper`.
- The facade accepts a package manifest path, optional package base directory, expected startup domain, referenced-manifest validation flag, cooked asset validation flag, and optional runtime compatibility token.
- The facade registers into a caller-owned `AssetRegistry&`, keeping registry lifetime explicit and avoiding a hidden Runtime global registry.
- Runtime-facing diagnostics include severity, category, diagnostic id, message, manifest path, reference/item context, resource id, asset id, and resolved path when available.
- `RuntimeAssetBootstrapTests` prove the RuntimeSmoke package registers exactly one asset and lookup works by `AssetKey` and `AssetId`.
- `RuntimeAssetBootstrapTests` prove missing package manifest, missing identity manifest, missing cooked asset, invalid package base directory, startup domain mismatch, and registry collision failures do not partially mutate the registry.
- Focused verification passed for `RuntimeAssetBootstrapTests`, and practical Phase 5 regressions passed for `RuntimePackageSmokeTests`, `GeneratedRuntimeSmokePackageTests`, and `SagaPackageStagingTests`.
- This still does not close Apps/Runtime startup asset integration, RuntimeServiceRegistry asset service wiring, an AssetPipeline CLI, source discovery/import/cook, publish hard-fail behavior, UI/document asset kind support, or MultiplayerSandbox package readiness.

Phase 5K Runtime Asset Startup Readiness evidence:

- `RuntimeAssetBootstrapDiagnostics` adds Runtime-owned summaries and diagnostic views for `RuntimeAssetBootstrapResult`.
- `RuntimeAssetBootstrapDiagnosticsTests` prove successful registration with assets summarizes as `Ready`, successful zero-asset bootstrap summarizes as `Empty`, and error diagnostics summarize as `Blocked`.
- Diagnostic views preserve severity, category, diagnostic id, message, manifest path, field/reference/item context, resource id, asset id, and resolved path when available.
- The diagnostics facade preserves package manifest path and package base directory when caller options are supplied.
- The diagnostics facade does not mutate `AssetRegistry` and does not call `RuntimeAssetBootstrap`.
- `docs/recovery/phase-05-assets-runtime/PHASE_5K_RUNTIME_ASSET_STARTUP_READINESS.md` decides that Apps/Runtime wiring should run after `RuntimeStartupSession::Prepare` succeeds and before `ClientHost` construction, but should wait until package base directory propagation and a focused app helper seam are added.
- Phase 5K keeps Apps/Runtime behavior unchanged; no `ClientHost`, render, UI, network, RuntimeServiceRegistry context, publish, or cook behavior changed.
- This still does not close Apps/Runtime startup asset integration, RuntimeServiceRegistry asset service wiring, an AssetPipeline CLI, source discovery/import/cook, publish hard-fail behavior, UI/document asset kind support, or MultiplayerSandbox package readiness.

Phase 5L Apps/Runtime asset bootstrap wiring evidence:

- Runtime startup launch, preflight, and session descriptors now propagate an
  optional `packageBaseDirectory` while preserving empty-path
  package-manifest-folder-relative behavior.
- `Apps/Runtime/RuntimeAssetStartupBootstrap` adds a testable app-local helper
  that accepts a `RuntimeSessionDescriptor` and caller-owned `AssetRegistry&`.
- The helper skips no-package dev launches, calls `RuntimeAssetBootstrap` for
  package launches, and preserves `RuntimeAssetBootstrapDiagnostics` summaries
  and views for app logging.
- `Apps/Runtime/main.cpp` parses `--package-base-directory`, creates a local
  `AssetRegistry` after startup preflight success, and runs asset bootstrap
  before `ClientHost` construction.
- Explicit package asset bootstrap failure now blocks `SagaRuntime` startup;
  no-package dev launches remain skipped.
- `RuntimeAssetStartupBootstrapTests` prove no-package skip, RuntimeSmoke asset
  registration, lookup by `AssetKey` and `AssetId`, bad base directory failure,
  missing cooked asset failure, diagnostics preservation, and no dependency on
  `ClientHost`.
- Focused verification passed for Runtime startup/preflight/session,
  Runtime asset bootstrap diagnostics, the app helper target, `SagaRuntime`,
  and cheap Phase 5 package/asset regressions.
- This still does not close ClientHost asset consumption, RuntimeServiceRegistry
  asset service wiring, an AssetPipeline CLI, source discovery/import/cook,
  publish hard-fail behavior, UI/document asset kind support, or
  MultiplayerSandbox package readiness.

Phase 5M Runtime asset consumption ownership evidence:

- `docs/recovery/phase-05-assets-runtime/PHASE_5M_RUNTIME_ASSET_CONSUMPTION_OWNERSHIP.md` closes
  Phase 5M as an ownership audit and Phase 5 closure direction checkpoint.
- Phase 5M rejects passing raw `AssetRegistry` into `ClientHost` because
  `ClientHost` still mixes network, render, UI, input, and session startup.
- Phase 5M records Apps/Runtime's local `AssetRegistry` as a temporary startup
  proof and lifetime placeholder, not the final consumption ownership model.
- Phase 5M decides Runtime should expose a narrow read-only asset access
  contract before `ClientHost` or RuntimeServiceRegistry asset service wiring.
- Phase 5M defers RuntimeServiceRegistry asset service ownership until startup
  context and registry lifetime are designed.
- This still does not close Runtime asset access facade implementation,
  ClientHost asset consumption, RuntimeServiceRegistry asset service wiring,
  publish readiness reporting, source discovery/import/cook, UI/document asset
  kind support, or MultiplayerSandbox package readiness.

Phase 5N Runtime asset access facade evidence:

- `SagaRuntime::RuntimeAssetCatalog` adds a Runtime-owned read-only facade over
  caller-owned `AssetRegistry`.
- The catalog exposes empty/count checks, containment and lookup by `AssetId`
  and asset key, kind lookup, prefetch candidate view, and total disk size view.
- The catalog has no mutation API, does not own the registry, and does not
  depend on Apps/Runtime, `ClientHost`, RuntimeServiceRegistry, or AssetPipeline.
- `RuntimeAssetCatalogTests` prove empty state, RuntimeSmoke bootstrap lookup by
  key and id, missing lookup behavior, const entry exposure, caller-owned
  lifetime observation, and failed bootstrap committed-state visibility.
- This still does not close ClientHost/runtime asset consumption wiring,
  RuntimeServiceRegistry asset service integration, publish readiness reporting,
  source discovery/import/cook, UI/document asset kind support,
  MultiplayerSandbox package readiness, or full test health.

Phase 5O publish readiness package/asset identity report evidence:

- `SagaPublishReadinessService` emits deterministic top-level
  `packageAssetIdentityCoverage` for the client and server publish package
  slots.
- Coverage reports package manifest existence/load state, package id/kind,
  referenced asset identity manifest existence/load state and mapping count,
  asset manifest existence/load state and asset count, and stable Runtime
  loader diagnostics.
- Existing package manifest validation blockers remain the only package-related
  hard gate expansion point in this slice; coverage-only issues such as missing
  cooked assets are serialized without adding blockers.
- `SagaPublishReadinessTests` prove valid coverage, missing package blockers,
  missing asset/identity manifest reporting, missing cooked asset report-only
  behavior, invalid asset manifest diagnostics, and deterministic JSON output.
- This still does not close ClientHost/runtime asset consumption wiring,
  RuntimeServiceRegistry asset service integration, source discovery/import/cook,
  UI/document asset kind support, MultiplayerSandbox package readiness, publish
  hard-gate parity with Runtime startup, or full test health.

Phase 5 closure checkpoint:

- `docs/recovery/phase-05-assets-runtime/PHASE_5_CLOSURE_AND_PHASE_6_OPENING_CHECKPOINT.md`
  closes Phase 5 as Package / Asset / Runtime Readiness Foundation established.
- Phase 5 is not closed as full AssetPipeline completion, source
  discovery/import/cook completion, full publish gate completion,
  ClientHost/runtime asset consumption completion, MultiplayerSandbox package
  readiness, UI/document asset workflow completion, RuntimeServiceRegistry
  asset service completion, or full test health.
- Remaining Phase 5-related debt is explicitly deferred to later
  AssetPipeline, Editor/UI, Runtime/ClientHost ownership, Product/Publish,
  MultiplayerSandbox/product proof, and verification phases.
- Full CTest remains unverified.

### Phase 6 - Editor Public API De-Qtification

Status: closed as an Editor public API de-Qtification checkpoint with explicit
deferred `GraphCanvas`, `QtPanel`, and `SagaEditorLib` Qt PUBLIC dependency
debt. Phase 6 does not close full public Qt removal.

Work:

- Phase 6A: inventory public Editor Qt exposure and select the first safe
  implementation slice without changing code, CMake, or runtime behavior.
- Phase 6B: enforce the current public Qt exposure baseline with an
  architecture test and focused `EditorQtPublicAbiBoundaryTests` CTest entry.
- Phase 6C: decide the `QtPanel` replacement pattern and safe migration order
  without removing Qt or migrating panels.
- Phase 6D: migrate exactly one low-risk panel from public `QtPanel`
  inheritance to `IPanel` + private Qt pimpl implementation.
- Add Qt-free public editor panel contracts.
- Move Qt implementation details behind private/backend boundaries.
- Start with the Production Dashboard path.

Phase 6A evidence:

- `docs/recovery/phase-06-editor-deqt/PHASE_6A_EDITOR_QT_PUBLIC_ABI_AUDIT.md` records the
  current Qt public ABI inventory, Editor UI ownership boundaries, test/CMake
  inventory, Phase 6B candidate evaluation, and selected Phase 6B slice.
- Public Qt ABI still exists through `QtPanel`, `QWidget`, public panel
  headers, visual scripting panel/view headers, and the product editor
  activation API.
- Qt implementation is allowed in the Editor Qt backend and app/product
  composition implementation files.
- Recommended Phase 6B: Editor Public Header Qt Exposure Report / Boundary
  Guard.

Phase 6B evidence:

- `docs/recovery/phase-06-editor-deqt/PHASE_6B_EDITOR_QT_PUBLIC_ABI_BOUNDARY_GUARD.md` records
  the scanner scope, exact path/token allowlist, failure policy, CTest entry,
  non-goals, and next-slice recommendation.
- `EditorQtPublicAbiBoundaryTests` runs inside `SagaArchitectureTests` without
  linking Qt and scans `Editor/include` plus `Apps/Saga/SagaEditorModule.h`.
- New public Qt exposure outside the legacy allowlist fails the focused
  architecture/editor guard.
- Phase 6B does not claim Qt removal, panel migration, `SagaEditorModule`
  de-Qtification, CMake Qt visibility cleanup, editor test split, or full CTest
  health.
- Recommended Phase 6C: QtPanel Public ABI Replacement Plan.

Phase 6C evidence:

- `docs/recovery/phase-06-editor-deqt/PHASE_6C_QTPANEL_PUBLIC_ABI_REPLACEMENT_PLAN.md` records
  Phase 6B closure, the `QtPanel` root cause, candidate replacement designs,
  allowlisted leak migration inventory, migration order, non-goals, and Phase
  6D recommendation.
- Existing `IPanel` is selected as the backend-neutral public replacement
  contract. Public panels should implement `IPanel` directly and own private Qt
  widgets through pimpl in the Qt backend implementation.
- No optional code was added because the existing `IPanel` contract already
  covers `PanelId`, title, native handle, lifecycle, and focus hooks.
- Phase 6C does not claim Qt removal, `QtPanel` removal, panel migration,
  visual scripting migration, `SagaEditorModule` API change, CMake Qt
  visibility cleanup, editor test split, or full CTest health.
- Recommended Phase 6D: First Low-Risk Panel pimpl Migration.

Phase 6D evidence:

- `docs/recovery/phase-06-editor-deqt/PHASE_6D_FIRST_PANEL_PIMPL_MIGRATION.md` records Phase 6C
  closure, candidate comparison, selected panel, public ABI change, private Qt
  implementation shape, allowlist reduction, non-goals, remaining leaks, and
  Phase 6E recommendation.
- `GraphViewportPanel` no longer exposes `QtPanel` in its public header and now
  follows the `IPanel` + pimpl/private Qt widget pattern.
- `EditorQtPublicAbiBoundaryTests` removes only `GraphViewportPanel.h` from the
  allowlist; all other public Qt leaks remain guarded as explicit debt.
- Phase 6D does not claim Qt removal, `QtPanel` removal, multiple panel
  migration, visual scripting migration, `SagaEditorModule` API change,
  CMake Qt visibility cleanup, editor test split, or full CTest health.
- Recommended Phase 6E: migrate `ProfilerPanel` with the same pattern unless
  verification reveals a missing panel semantic or test-seam blocker.

Phase 6E evidence:

- `docs/recovery/phase-06-editor-deqt/PHASE_6E_PROFILER_PANEL_PIMPL_MIGRATION.md` records Phase
  6D closure, `ProfilerPanel` inspection, public ABI change, private Qt
  implementation shape, allowlist reduction, non-goals, remaining leaks, and
  Phase 6F recommendation.
- `ProfilerPanel` no longer exposes `QtPanel` in its public header and now
  follows the `IPanel` + pimpl/private Qt widget pattern.
- `EditorQtPublicAbiBoundaryTests` removes only `ProfilerPanel.h` from the
  allowlist; all other public Qt leaks remain guarded as explicit debt.
- The boundary guard reports 41 allowed legacy Qt exposure findings and 0
  violations after the `ProfilerPanel` allowlist reduction.
- Phase 6E does not claim broad Qt removal, `QtPanel` removal, additional panel
  migration, visual scripting migration, `SagaEditorModule` API change,
  CMake Qt visibility cleanup, editor test split, or full CTest health.
- Recommended Phase 6F: migrate `ProjectBrowserPanel` with the same pattern if
  Phase 6E verification stays clean.

Phase 6F evidence:

- `docs/recovery/phase-06-editor-deqt/PHASE_6F_PROJECT_BROWSER_PANEL_PIMPL_MIGRATION.md`
  records Phase 6E closure, `ProjectBrowserPanel` inspection, public ABI
  change, private Qt implementation shape, allowlist reduction, non-goals,
  remaining leaks, and Phase 6G recommendation.
- `ProjectBrowserPanel` no longer exposes `QtPanel` in its public header and
  now follows the `IPanel` + pimpl/private Qt widget pattern.
- `EditorQtPublicAbiBoundaryTests` removes only `ProjectBrowserPanel.h` from
  the allowlist; all other public Qt leaks remain guarded as explicit debt.
- The boundary guard reports 38 allowed legacy Qt exposure findings and 0
  violations after the `ProjectBrowserPanel` allowlist reduction.
- Phase 6F does not claim broad Qt removal, `QtPanel` removal, collaboration
  migration, visual scripting migration, `SagaEditorModule` API change,
  CMake Qt visibility cleanup, editor test split, project/workspace ownership
  redesign, or full CTest health.
- Recommended Phase 6G: perform a `CollaborationPanel` migration audit before
  implementation.

Phase 6G evidence:

- `docs/recovery/phase-06-editor-deqt/PHASE_6G_COLLABORATION_PANEL_QT_BOUNDARY.md` records
  Phase 6F closure, `CollaborationPanel` ownership audit, public ABI change,
  private Qt implementation shape, allowlist reduction, non-goals, remaining
  leaks, and Phase 6H recommendation.
- `CollaborationPanel` no longer exposes `QtPanel` in its public header and
  now follows the `IPanel` + pimpl/private Qt widget pattern.
- The collaboration panel remains display-only; this slice does not move or
  redesign collaboration session, networking, document sync, lock, permission,
  presence, or conflict ownership.
- `EditorQtPublicAbiBoundaryTests` removes only `CollaborationPanel.h` from
  the allowlist; visual scripting headers, `QtPanel.h`, and
  `SagaEditorModule.h` remain guarded as explicit debt.
- The boundary guard reports 35 allowed legacy Qt exposure findings and 0
  violations after the `CollaborationPanel` allowlist reduction.
- Phase 6G does not claim broad Qt removal, `QtPanel` removal, visual
  scripting migration, `SagaEditorModule` API change, CMake Qt visibility
  cleanup, editor test split, collaboration ownership redesign, or full CTest
  health.
- Recommended Phase 6H: plan the visual scripting leaf panel cluster.

Phase 6H evidence:

- `docs/recovery/phase-06-editor-deqt/PHASE_6H_VISUAL_SCRIPTING_QTPANEL_CLUSTER.md` records
  Phase 6G closure, visual scripting public `QtPanel` cluster inventory,
  candidate classification, `WatchPanel` migration, allowlist reduction,
  non-goals, remaining leaks, and Phase 6I recommendation.
- `WatchPanel` no longer exposes `QtPanel` in its public header and now
  follows the `IPanel` + pimpl/private Qt widget pattern.
- The visual scripting audit classifies `BreakpointPanel` as the next safe
  leaf candidate, `GraphCanvas` as high-risk graph core view debt, and the
  trace/debugger/node library surfaces as ownership-audit work before
  migration.
- `EditorQtPublicAbiBoundaryTests` removes only `WatchPanel.h` from the
  allowlist; other visual scripting headers, `QtPanel.h`, and
  `SagaEditorModule.h` remain guarded as explicit debt.
- The boundary guard reports 32 allowed legacy Qt exposure findings and 0
  violations after the `WatchPanel` allowlist reduction.
- Phase 6H does not claim broad Qt removal, `QtPanel` removal, broad visual
  scripting migration, `GraphCanvas` migration, `SagaEditorModule` API change,
  CMake Qt visibility cleanup, visual scripting service/model rewrite, or full
  CTest health.
- Recommended Phase 6I: migrate `BreakpointPanel` if Phase 6H verification is
  clean; otherwise perform a debugger/watch model boundary audit.

Phase 6I evidence:

- `docs/recovery/phase-06-editor-deqt/PHASE_6I_BREAKPOINT_PANEL_PIMPL_MIGRATION.md` records
  Phase 6H closure, `BreakpointPanel` migration, allowlist reduction,
  non-goals, remaining visual scripting cluster status, and Phase 6J
  recommendation.
- `BreakpointPanel` no longer exposes `QtPanel` in its public header and now
  follows the `IPanel` + pimpl/private Qt widget pattern.
- `EditorQtPublicAbiBoundaryTests` removes only `BreakpointPanel.h` from the
  allowlist; remaining visual scripting headers, `QtPanel.h`, and
  `SagaEditorModule.h` remain guarded as explicit debt.
- The boundary guard reports 29 allowed legacy Qt exposure findings and 0
  violations after the `BreakpointPanel` allowlist reduction.
- Phase 6I does not claim broad Qt removal, `QtPanel` removal, broad visual
  scripting migration, `ExecutionTraceView` migration, `GraphDebuggerView`
  migration, `NodePalette` migration, `NodeLibraryPanel` migration,
  `GraphCanvas` migration, `SagaEditorModule` API change, CMake Qt visibility
  cleanup, visual scripting service/model rewrite, or full CTest health.
- Recommended Phase 6J: audit `ExecutionTraceView` ownership and migrate it
  only if inspection proves it is stub-level and dependency-free.

Phase 6J evidence:

- `docs/recovery/phase-06-editor-deqt/PHASE_6J_EXECUTION_TRACE_VIEW_QT_BOUNDARY.md` records
  Phase 6I closure, remaining visual scripting public `QtPanel` inventory,
  candidate evaluation, `ExecutionTraceView` migration, allowlist reduction,
  non-goals, remaining leaks, and Phase 6K recommendation.
- `ExecutionTraceView` no longer exposes `QtPanel` in its public header and now
  follows the `IPanel` + pimpl/private Qt widget pattern.
- The migration does not add trace model, debugger session, graph runtime,
  command routing, breakpoint, watch, node library, graph document, or editor
  shell behavior.
- `EditorQtPublicAbiBoundaryTests` removes only `ExecutionTraceView.h` from
  the allowlist; `GraphDebuggerView`, `NodePalette`, `NodeLibraryPanel`,
  `GraphCanvas`, `QtPanel.h`, and `SagaEditorModule.h` remain guarded as
  explicit debt.
- The boundary guard reports 26 allowed legacy Qt exposure findings and 0
  violations after the `ExecutionTraceView` allowlist reduction.
- Phase 6J does not claim broad Qt removal, `QtPanel` removal, broad visual
  scripting migration, `GraphDebuggerView` migration, node UI migration,
  `GraphCanvas` migration, `SagaEditorModule` API change, CMake Qt visibility
  cleanup, visual scripting service/model rewrite, or full CTest health.
- Recommended Phase 6K: audit `GraphDebuggerView` debugger/model ownership
  before attempting another visual scripting public Qt migration.

Phase 6K evidence:

- `docs/recovery/phase-06-editor-deqt/PHASE_6K_GRAPH_DEBUGGER_VIEW_QT_BOUNDARY.md` records
  Phase 6J closure, `GraphDebuggerView` debugger/model ownership audit,
  migration, allowlist reduction, non-goals, remaining visual scripting cluster
  status, and Phase 6L recommendation.
- `GraphDebuggerView` no longer exposes `QtPanel` in its public header and now
  follows the `IPanel` + pimpl/private Qt widget pattern.
- The migration does not add debugger session, graph model, graph runtime,
  trace model, command routing, breakpoint, watch, node library, graph
  document, or editor shell behavior.
- `EditorQtPublicAbiBoundaryTests` removes only `GraphDebuggerView.h` from the
  allowlist; `NodePalette`, `NodeLibraryPanel`, `GraphCanvas`, `QtPanel.h`,
  and `SagaEditorModule.h` remain guarded as explicit debt.
- Phase 6K does not claim broad Qt removal, `QtPanel` removal, broad visual
  scripting migration, node UI migration, `GraphCanvas` migration,
  `SagaEditorModule` API change, CMake Qt visibility cleanup, visual scripting
  service/model rewrite, or full CTest health.
- Recommended Phase 6L: audit `NodePalette` node/model ownership before
  attempting the next visual scripting public Qt migration.

Phase 6L evidence:

- `docs/recovery/phase-06-editor-deqt/PHASE_6L_NODE_PALETTE_QT_BOUNDARY.md` records Phase 6K
  closure, `NodePalette` node/model ownership audit, migration, allowlist
  reduction, non-goals, remaining visual scripting cluster status, and Phase
  6M recommendation.
- `NodePalette` no longer exposes `QtPanel` in its public header and now
  follows the `IPanel` + pimpl/private Qt widget pattern.
- The migration does not add node catalog, node model, graph document, command
  routing, editor selection, graph canvas, or editor shell behavior.
- `EditorQtPublicAbiBoundaryTests` removes only `NodePalette.h` from the
  allowlist; `NodeLibraryPanel`, `GraphCanvas`, `QtPanel.h`, and
  `SagaEditorModule.h` remain guarded as explicit debt.
- Phase 6L does not claim broad Qt removal, `QtPanel` removal, broad visual
  scripting migration, `NodeLibraryPanel` migration, `GraphCanvas` migration,
  `SagaEditorModule` API change, CMake Qt visibility cleanup, visual scripting
  service/model rewrite, or full CTest health.
- Recommended Phase 6M: audit `NodeLibraryPanel` and `NodeCategoryBrowser`
  before attempting the next visual scripting public Qt migration.

Phase 6M evidence:

- `docs/recovery/phase-06-editor-deqt/PHASE_6M_NODE_LIBRARY_PANEL_QT_BOUNDARY.md` records Phase
  6L closure, `NodeLibraryPanel` / `NodeCategoryBrowser` ownership audit,
  migration, allowlist reduction, non-goals, remaining visual scripting cluster
  status, and Phase 6N recommendation.
- `NodeLibraryPanel` no longer exposes `QtPanel` in its public header and now
  follows the `IPanel` + pimpl/private Qt widget pattern.
- The migration preserves the existing `NodeCategoryBrowser&` constructor and
  does not add node library, import, node category, graph document, graph
  canvas, command routing, editor selection, or editor shell behavior.
- `EditorQtPublicAbiBoundaryTests` removes only `NodeLibraryPanel.h` from the
  allowlist; `GraphCanvas`, `QtPanel.h`, and `SagaEditorModule.h` remain
  guarded as explicit debt.
- Phase 6M does not claim broad Qt removal, `QtPanel` removal, broad visual
  scripting migration, `GraphCanvas` migration, `SagaEditorModule` API change,
  CMake Qt visibility cleanup, visual scripting service/model rewrite, or full
  CTest health.
- Recommended Phase 6N: produce a `GraphCanvas` / `GraphDocument` boundary
  plan before any graph canvas migration.

Phase 6N evidence:

- `docs/recovery/phase-06-editor-deqt/PHASE_6N_GRAPH_CANVAS_BOUNDARY_PLAN.md` records Phase 6M
  closure, `GraphCanvas` / `GraphDocument` boundary audit, migration deferral,
  required future seams, non-goals, and Phase 6O recommendation.
- `GraphCanvas` remains allowlisted because it publicly takes and stores
  `GraphDocument&`; the current API crosses graph view/model ownership.
- The slice makes no source, CMake, or allowlist changes.
- `EditorQtPublicAbiBoundaryTests` continues to guard `GraphCanvas.h`,
  `QtPanel.h`, and `SagaEditorModule.h` as explicit remaining debt.
- Phase 6N does not claim broad Qt removal, `QtPanel` removal, `GraphCanvas`
  migration, graph model redesign, `SagaEditorModule` API change, CMake Qt
  visibility cleanup, or full CTest health.
- Recommended Phase 6O: audit `SagaEditorModule` product Qt ABI and produce a
  bounded product adapter plan unless implementation inspection proves a small
  non-breaking adapter is safe.

Phase 6O evidence:

- `docs/recovery/phase-06-editor-deqt/PHASE_6O_SAGA_EDITOR_MODULE_QT_ABI.md` records Phase 6N
  closure, product editor activation ABI audit, native mount adapter
  implementation, allowlist reduction, non-goals, and Phase 6P recommendation.
- `SagaEditorModule.h` no longer exposes `QMainWindow` or `QStackedWidget` and
  now accepts a backend-neutral `SagaEditorNativeMount`.
- `SagaEditorModule::Shutdown` no longer takes unused native Qt window
  parameters.
- `SagaApp.cpp` remains the product Qt implementation caller and provides the
  native mount handles from its existing widgets.
- `EditorQtPublicAbiBoundaryTests` removes only `SagaEditorModule.h` from the
  allowlist; `GraphCanvas.h` and `QtPanel.h` remain guarded as explicit debt.
- Phase 6O does not claim product shell rewrite, `EditorShell` rewrite,
  `GraphCanvas` migration, `QtPanel` removal, CMake Qt visibility cleanup,
  product readiness, or full CTest health.
- Recommended Phase 6P: audit `QtPanel` privatization readiness. Do not remove
  `QtPanel` while `GraphCanvas.h` remains a public inheritor.

Phase 6P evidence:

- `docs/recovery/phase-06-editor-deqt/PHASE_6P_QTPANEL_PRIVATIZATION_READINESS.md` records Phase
  6O closure, remaining public `QtPanel` dependencies, privatization deferral,
  boundary guard status, non-goals, and Phase 6Q recommendation.
- `QtPanel.h` remains public because `GraphCanvas.h` still includes and
  inherits `SagaEditor::QtPanel`.
- The slice makes no source, CMake, or allowlist changes.
- `EditorQtPublicAbiBoundaryTests` continues to guard `GraphCanvas.h` and
  `QtPanel.h` as explicit remaining debt.
- Phase 6P does not claim `QtPanel` removal, `GraphCanvas` migration, graph
  model redesign, CMake Qt visibility cleanup, or full CTest health.
- Recommended Phase 6Q: audit `SagaEditorLib` Qt PUBLIC dependency cleanup
  readiness. Do not make Qt private while `GraphCanvas.h` and `QtPanel.h`
  remain public Qt ABI.

Phase 6Q evidence:

- `docs/recovery/phase-06-editor-deqt/PHASE_6Q_SAGAEDITORLIB_QT_PUBLIC_READINESS.md` records
  Phase 6P closure, `SagaEditorLib` Qt PUBLIC dependency readiness audit,
  CMake visibility cleanup deferral, boundary guard status, non-goals, and
  Phase 6R recommendation.
- `SagaEditorLib` still publishes `Qt6::Core` and `Qt6::Widgets` because
  `GraphCanvas.h` remains public Qt ABI through `QtPanel`.
- The slice makes no source, CMake, or allowlist changes.
- `EditorQtPublicAbiBoundaryTests` continues to guard `GraphCanvas.h` and
  `QtPanel.h` as explicit remaining debt.
- Phase 6Q does not claim CMake Qt visibility cleanup, `QtPanel` removal,
  `GraphCanvas` migration, graph model redesign, or full CTest health.
- Recommended Phase 6R: close Phase 6 honestly with remaining `GraphCanvas`,
  `QtPanel`, and `SagaEditorLib` Qt PUBLIC dependency debt explicitly
  documented.

Phase 6R evidence:

- `docs/recovery/phase-06-editor-deqt/PHASE_6R_EDITOR_PUBLIC_API_DEQT_CLOSURE.md` records
  Phase 6Q closure, completed public ABI cleanup, remaining explicit debt,
  boundary guard status, non-claims, and Phase 7A recommendation.
- Remaining public Qt exposure is exactly `GraphCanvas.h` and `QtPanel.h`.
- `SagaEditorModule.h` no longer exposes public Qt activation parameters.
- `SagaEditorLib` keeps `Qt6::Core` and `Qt6::Widgets` public because the
  remaining public ABI still requires Qt.
- `EditorQtPublicAbiBoundaryTests` keeps a clean guarded baseline with only
  `GraphCanvas.h` and `QtPanel.h` allowlisted.
- Phase 6R does not claim complete Qt removal, zero public Qt exposure,
  `GraphCanvas` migration, `QtPanel` privatization, `SagaEditorLib` Qt
  PRIVATE conversion, visual scripting product readiness, graph model
  redesign, editor product readiness, or full CTest health.
- Recommended Phase 7A: start Editor Scaffolding Quarantine with a report-only
  scaffold inventory.

Original full de-Qt acceptance, still deferred:

- Public Editor headers do not include Qt headers or expose `QtPanel`.
- Editor public CMake interface does not publish Qt unless the public ABI actually requires it.

Phase 6R closure acceptance:

- Public Qt exposure outside `GraphCanvas.h` and `QtPanel.h` has been removed
  or explicitly documented as not part of Phase 6.
- Product editor activation no longer exposes Qt-shaped public parameters.
- Remaining `GraphCanvas` / `QtPanel` public Qt debt is explicit and guarded.
- `SagaEditorLib` Qt PUBLIC visibility is preserved because public ABI still
  requires it.
- Full test health remains unverified.

### Phase 7 - Editor Scaffolding Quarantine

Status: closed as Editor Scaffolding Quarantine Foundation established.
Phase 7A is completed as a report-only editor scaffolding inventory. Phase 7B
is completed as a report-only project/workspace workflow ownership map. Phase
7C is completed as the first bounded scaffold behavior replacement. Phase 7D
is completed as an audit-only diagnostics/dashboard reality pass. Phase 7E is
completed as the Phase 7 closure and Phase 8 opening checkpoint.

Work:

- Inventory stubs, scaffolds, placeholders, TODOs, `return true`, and `return success` patterns.
- Classify each as production-path stub, test-only placeholder, future scaffold, or dead code.
- Convert the first real editor workflow: open project, validate readiness, show dashboard, show diagnostics, persist session state.

Acceptance:

- Stub count is measured and reduced.
- At least one editor workflow is real rather than panel scaffold.
- Phase 7 closure claims remain limited to inventory, ownership mapping, one
  bounded `ProjectManager` workflow, diagnostics/dashboard audit, scaffold
  count reduction, and public Qt ABI guard compliance.
- Complete editor product readiness, complete dashboard implementation, full
  project browser workflow, SDE/customization product completion, visual
  scripting readiness, collaboration readiness, UI polish, and full CTest
  health remain unclaimed.

Phase 7A evidence:

- `docs/recovery/phase-07-editor-scaffolding/PHASE_7A_EDITOR_SCAFFOLDING_INVENTORY.md` records Phase
  6 closure, editor scaffold search method, measured scaffold surface,
  classification, candidate workflow, non-goals, and Phase 7B recommendation.
- The inventory found 115 generated or explicit implementation-stub files
  under `Editor/src`.
- Collaboration and visual scripting are the largest scaffold clusters and are
  not safe as broad cleanup targets.
- `ProjectManager` is the strongest candidate production-path stub for Phase
  7B ownership mapping because its API is narrow and it connects to a real
  project/session workflow.
- Phase 7A does not claim stub-count reduction, project workflow
  implementation, editor product readiness, visual scripting readiness,
  collaboration readiness, public Qt cleanup, or full CTest health.
- Recommended Phase 7B: map ownership for one project/workspace session
  workflow before implementation.

Phase 7B evidence:

- `docs/recovery/phase-07-editor-scaffolding/PHASE_7B_EDITOR_WORKFLOW_OWNERSHIP_MAP.md` records Phase
  7A closure, product/editor/lab ownership boundaries, current
  project/workspace flow, editor-only gaps, non-goals, and Phase 7C
  recommendation.
- Product project manifest creation/opening remains owned by
  `Apps/Saga/SagaProjectSystem` and `Apps/Saga/SagaApp`.
- Editor startup consumes prepared workspace state through `EditorHost` and
  `EditorWorkspaceDefinition`.
- `ProjectManager` is an editor-local scaffold with a narrow API and no current
  product dependency, making it the safest first implementation candidate.
- Phase 7B does not claim source behavior changes, project workflow
  implementation, editor menu/file-dialog wiring, product shell changes, SDE
  startup integration, public Qt cleanup, or full CTest health.
- Recommended Phase 7C: implement only `ProjectManager` stateful open/close
  behavior with focused editor unit tests.

Phase 7C evidence:

- `docs/recovery/phase-07-editor-scaffolding/PHASE_7C_PROJECT_MANAGER_WORKFLOW.md` records Phase 7B
  closure, `ProjectManager` implementation result, stub reduction, tests,
  non-goals, and Phase 7D recommendation.
- `ProjectManager` now has real editor-local open/close state, manifest
  metadata projection, directory fallback, current project reporting, and
  project-changed callback behavior.
- Added focused `ProjectManagerTests` for manifest-backed open, directory
  fallback, missing-path rejection, invalid-manifest rejection, state exposure,
  and callback behavior.
- The generated/explicit implementation-stub inventory under `Editor/src` is
  reduced from 115 to 114 files; the `Project` area is reduced from 3 to 2
  stub files.
- Phase 7C does not claim editor menu/file-dialog wiring, `EditorHost` or
  `EditorShell` ownership changes, movement of `SagaProjectSystem` into
  Editor, SDE startup integration, product shell behavior changes, visual
  scripting readiness, collaboration readiness, public Qt cleanup, or full
  CTest health.
- Recommended Phase 7D: audit production dashboard and Problems panel
  diagnostics/readiness reality before implementing any wider editor workflow.

Phase 7D evidence:

- `docs/recovery/phase-07-editor-scaffolding/PHASE_7D_DIAGNOSTICS_DASHBOARD_REALITY.md` records Phase
  7C closure, dashboard/diagnostics reality audit, real/partial/deferred
  classification, non-goals, and stop condition.
- `ProblemsPanel` is wired to the shared `EditorDiagnosticsService` and
  displays service diagnostics.
- `ProductionDashboardPanel` displays real engine/profile/persona/customization
  state, but does not yet display workspace/project readiness or diagnostics
  counts.
- Product SDE compile diagnostics are logged to Console/status by
  `SagaEditorModule`, not bridged into the shared Problems diagnostics service.
- Phase 7D does not claim dashboard implementation changes, product compile
  diagnostics bridging, standalone Open Project wiring, product shell changes,
  SDE startup integration, public Qt cleanup, or full CTest health.
- Stop before Phase 7E or another implementation slice until the next Phase 7
  scope decision is explicit.

Phase 7E closure evidence:

- `docs/recovery/phase-07-editor-scaffolding/PHASE_7_CLOSURE_AND_PHASE_8_OPENING_CHECKPOINT.md`
  closes Phase 7 only as Editor Scaffolding Quarantine Foundation established.
- Accepted claims are limited to scaffold inventory/classification,
  project/workspace ownership mapping, one bounded `ProjectManager` workflow,
  diagnostics/dashboard reality audit, scaffold count reduction from 115 to
  114, and continued public Qt ABI guard compliance.
- Phase 7E does not claim complete editor product readiness, complete
  dashboard implementation, full project browser workflow, SDE/customization
  product completion, visual scripting product readiness, collaboration product
  readiness, full UI polish, or full CTest health.
- Remaining Phase 7 debt is explicitly classified for later phases:
  dashboard workspace/diagnostics rows, remaining generated/stub files,
  project browser real file-tree/workspace behavior, EditorLab/scenario
  integration, SDE customization enforcement, visual scripting workflows,
  collaboration workflows, UI polish, and full test health.
- Recommended Phase 8A: perform a docs/report-only Claim Inventory / Evidence
  Matrix before adding hard-fail guards or broad roadmap corrections.

### Phase 8 - Documentation / Code Alignment Enforcement

Status: closed as Documentation / Code Alignment Evidence established. Phase
8A through Phase 8E are completed as docs/report-only alignment slices. Phase
8F is completed as the Phase 8 closure checkpoint.

Work:

- Audit roadmap claims such as `complete`, `closed`, `implemented`,
  `production-ready`, `shipped`, `Qt-free`, `runtime-owned`,
  `server-authoritative`, and `publish-ready`.
- Classify claims as supported, partial, deferred debt, stale/needs
  correction, or future intent.
- Compare `docs/DependencyGraph.md` against actual CMake and test enforcement.
- Map supported claims to evidence docs, tests, and build commands where
  available.
- Start with docs/report-only inventory before hard-fail guards, source
  changes, test registration changes, publish enforcement, or roadmap-wide
  rewrites.

Acceptance:

- Stale documentation is visible.
- Shipped claims require code, tests, and build evidence.
- Phase 8 closure remains limited to documentation/code alignment evidence.
- Hard-fail docs drift enforcement, non-recovery roadmap normalization, source
  ownership hardening, publish enforcement, and full CTest health remain
  unclaimed unless separately proven.

Phase 8A evidence:

- `docs/recovery/phase-08-doc-alignment/PHASE_8A_CLAIM_INVENTORY_EVIDENCE_MATRIX.md`
  inventories high-risk recovery-document claims for `complete`, `closed`,
  `implemented`, `production-ready`, `shipped`, `Qt-free`, `runtime-owned`,
  `server-authoritative`, and `publish-ready`.
- Supported claims are mapped to existing closure docs, focused tests, and
  verification commands where available.
- Broad product readiness, full Qt-free Editor API, complete
  server-authoritative multiplayer, complete publish readiness, and full CTest
  health remain unclaimed.
- Phase 8A does not add hard-fail guards, source changes, test registration
  changes, publish enforcement, or roadmap-wide rewrites.

Phase 8B evidence:

- `docs/recovery/phase-08-doc-alignment/PHASE_8B_OWNERSHIP_ALIGNMENT_PASS.md` compares recovery
  ownership claims against `docs/DependencyGraph.md`, the CMake boundary
  inventory, source-layout risks, public boundary risks, and accepted
  architecture checkpoints.
- Runtime, server, asset/package, publish, editor Qt, and editor
  project/workspace ownership claims are classified as partial/foundation
  evidence, not complete ownership.
- CMake source/target ownership and broad test graph ownership remain deferred
  debt; no hard-fail guard or target cleanup is introduced.
- Phase 8B does not require immediate recovery-roadmap ownership corrections,
  and it keeps non-recovery roadmap normalization deferred to evidence-specific
  slices.

Phase 8C evidence:

- `docs/recovery/phase-08-doc-alignment/PHASE_8C_TEST_EVIDENCE_MAPPING.md` maps major recovery
  claims to focused tests, guards, build evidence, and CTest label inventory.
- Runtime, server, asset/package, publish, editor Qt, and editor scaffolding
  claims have direct or partial focused evidence only for their bounded
  foundation slices.
- Broad test include roots, coarse `SagaUnitTests`, recursive source/test
  globs, and registered-but-unbuilt focused executables keep test evidence
  from proving full ownership or full suite health.
- `collaboration` and `ui` label inventories are empty in the current build
  tree, so collaboration readiness and UI polish remain unproved.
- Phase 8C does not add tests, change labels, change CMake, run full CTest, or
  claim global test health.

Phase 8D evidence:

- `docs/recovery/phase-08-doc-alignment/PHASE_8D_DOCS_DRIFT_GUARD_PROTOTYPE.md` defines a
  report-only drift guard prototype for high-risk stale or unsupported docs
  phrases.
- The prototype focuses on risky uses of `production-ready`, broad `Qt-free`,
  full `server-authoritative`, unqualified `publish-ready`, broad `shipped`,
  unqualified `complete`, and unqualified `implemented`.
- Explicit negative claims, non-claims, foundation closures, future roadmap
  intent, bounded local evidence, and quoted historical references are recorded
  as allowed contexts before enforcement.
- Phase 8D does not add a scanner, CMake target, CI step, hard-fail guard,
  source change, test change, or publish enforcement.

Phase 8E evidence:

- `docs/recovery/phase-08-doc-alignment/PHASE_8E_ROADMAP_NORMALIZATION.md` normalizes the recovery
  roadmap's Phase 8 status, evidence, non-claims, deferred debt, and closure
  direction.
- Phase 8A through Phase 8D remain docs/report-only evidence slices; Phase 8E
  does not rewrite non-recovery roadmaps.
- Phase 8 can claim claim-term inventory, ownership alignment, test evidence
  mapping, report-only drift-guard scoping, and explicit recovery-roadmap
  non-claims.
- Phase 8 cannot claim complete docs correctness, hard-fail drift enforcement,
  full dependency graph enforcement, product/runtime/editor readiness, publish
  readiness enforcement, non-recovery roadmap normalization, or full CTest
  health.

Phase 8F closure evidence:

- `docs/recovery/phase-08-doc-alignment/PHASE_8_CLOSURE_CHECKPOINT.md` closes Phase 8 only as
  Documentation / Code Alignment Evidence established.
- Accepted claims are limited to claim inventory, ownership alignment, test
  evidence mapping, report-only docs drift guard scoping, and recovery roadmap
  Phase 8 normalization.
- Phase 8F does not claim complete documentation correctness, non-recovery
  roadmap normalization, hard-fail docs drift enforcement, complete dependency
  graph enforcement, CMake/source/test ownership hardening,
  product/runtime/editor behavior readiness, publish readiness enforcement, or
  full CTest health.
- Recommended next phase: Phase 9 Local Evidence Gates, starting with safe
  single-job evidence gates and no full-health claim unless the required suite
  set is actually run and passes.

### Phase 9 - Local Evidence Gates

Status: limited closure. Phase 9A through Phase 9F are documented; the normal
local gate passed with heavy labels excluded, and raw full CTest remains
unresolved and non-required for the normal local gate.

Work:

- Phase 9A: inventory the current CTest registry, labels, missing executables,
  and evidence quality without running full CTest or changing build behavior.
- Phase 9B: classify registered-but-unbuilt entries before changing tests,
  labels, CMake, quarantine, or CI behavior.
- Phase 9C: build the registered-but-unbuilt focused targets on demand and run
  their focused CTest set before any full CTest dry run.
- Phase 9D: classify the failed raw full CTest attempts, run the normal gate
  with heavy labels excluded, and document raw full CTest policy.
- Phase 9E: establish the Local Gate Command Matrix.
- Phase 9F: close Phase 9 with explicit accepted claims and non-claims.
- Establish local single-job evidence commands first.
- Run safe suites before any CI expansion.
- Keep `stress`, `slow`, `load`, `long-running`, `benchmark`, and `perf`
  opt-in or excluded from the normal gate.

Acceptance:

- Phase evidence names the exact safe suites and gates run.
- Normal local gate health is accepted only from the heavy-label-excluded CTest
  command.
- Raw full CTest health remains unverified unless it is intentionally run and
  completed with a passing summary.

Phase 9A evidence:

- `docs/recovery/phase-09-local-evidence/PHASE_9A_TEST_REGISTRY_INVENTORY.md` records the
  `build/RelWithDebInfo-0.0.9` CTest list-only registry as 38 registered
  entries.
- Current label inventory records `architecture` 2, `editor` 2, `ui` 0,
  `product` 2, `collaboration` 0, `runtime` 23, `server` 10, `networking` 5,
  `replication` 4, `asset` 14, `package` 12, `tools` 7, and `integration` 3.
- `ui` and `collaboration` are zero-test labels in the current build tree.
- Twelve entries are registered but unbuilt in the current build tree:
  `ActorOwnershipRegistryTests`, `AuthoritativeMovementCoreTests`,
  `AuthoritativeMovementInputAdapterTests`,
  `AuthoritativeMovementCommandIntakeTests`,
  `ServerPacketNormalizationTests`, `ZoneServerPacketDrainTests`,
  `ZoneServerMovementAuthorityTests`, `MovementDirtyReplicationBridgeTests`,
  `RuntimeStartupDiagnosticsTests`, `RuntimeServiceLifecycleTests`,
  `RuntimeServiceRegistryTests`, and
  `RuntimeServiceRegistryDiagnosticsTests`.
- Phase 9A does not claim suite health. Label presence, test registration, and
  missing executables are not pass evidence.
- Full CTest remains unverified.
- Recommended Phase 9B: Label Cleanup / Registered-But-Unbuilt Test
  Classification.

Phase 9B evidence:

- `docs/recovery/phase-09-local-evidence/PHASE_9B_REGISTERED_BUT_UNBUILT_CLASSIFICATION.md`
  classifies all twelve registered-but-unbuilt entries as real focused CMake
  targets with matching source files, `add_executable`, `add_test`, and label
  registration.
- The current root cause category is target exists but has not been built on
  demand in `build/RelWithDebInfo-0.0.9`.
- None of the twelve entries are classified as stale registrations.
- The twelve entries still are not pass evidence until their targets are built
  and the focused CTest set runs successfully.
- Phase 9B does not change source, CMake, labels, registration, quarantine, or
  tests.
- Full CTest remains unverified.
- Recommended Phase 9C: Focused Test Build Matrix / On-Demand Target
  Verification.

Phase 9C evidence:

- `docs/recovery/phase-09-local-evidence/PHASE_9C_FOCUSED_TEST_BUILD_MATRIX.md` records twelve
  individual `Tools/Forge/bin/forge nix build --target ... --jobs=1` commands
  for the Phase 9B entries.
- All twelve focused targets built successfully and produced executables under
  `build/RelWithDebInfo-0.0.9/bin`.
- The focused CTest regex covering only those twelve entries passed: 12 tests
  passed, 0 failed.
- Follow-up `ctest -N` and label inventories for `runtime`, `server`,
  `networking`, and `replication` no longer report missing-executable
  warnings in the current build tree.
- Remaining non-runnable registered entries: none observed.
- Phase 9C does not claim full CTest health, release readiness, stress/load
  readiness, performance readiness, package/publish readiness, or CI readiness.
- Recommended Phase 9D: Full CTest Dry Run And Failure Triage.

Phase 9D attempted evidence:

- `docs/recovery/phase-09-local-evidence/PHASE_9D_FULL_CTEST_DRY_RUN_TRIAGE.md` records two full
  normal CTest attempts with
  `ctest --test-dir build/RelWithDebInfo-0.0.9 --output-on-failure -j 1`.
- Both attempts reached `CSharpGameplayProofTests` after tests #1 through #32
  had passed, then the terminal/session ended before CTest returned a complete
  summary.
- No deterministic CTest assertion failure output was captured.
- Full CTest status is unknown, not passing.
- At this point in the evidence chain, Phase 9E and Phase 9F were blocked to
  avoid overclaiming local gate readiness or Phase 9 closure.

Phase 9D stability blocker follow-up:

- `docs/recovery/phase-09-local-evidence/PHASE_9D_FULL_CTEST_STABILITY_BLOCKER.md` records the
  terminal/session instability as a Phase 9D environment/test-stability
  blocker, not a normal test failure and not a pass.
- List-only CTest inspection shows raw full CTest includes opt-in heavy entries:
  `StressTests` is registered under `stress` and `load`, while both
  `ReplicationTests` and `StressTests` are registered under `slow`.
- `benchmark` and `perf` labels currently list zero entries.
- CMake label inspection shows `ReplicationTests` is
  `replication;integration;slow;long-running` and `StressTests` is
  `stress;load;slow`.
- Normal local evidence gates must exclude `stress`, `slow`, `load`,
  `benchmark`, and `perf` by default.
- No safer normal-gate dry run was attempted in that follow-up because the
  list-only inspection found registered stress/slow/load entries.
- `CSharpGameplayProofTests` remains a later isolated stability follow-up if it
  still appears near the crash point after heavy labels are excluded.
- This follow-up kept Phase 9 open until a heavy-label-excluded normal gate
  could be run.

Phase 9D-2 normal gate evidence:

- `docs/recovery/phase-09-local-evidence/PHASE_9D_NORMAL_GATE_DRY_RUN.md` records the normal gate
  list-only check and execution with heavy label exclusion.
- The normal local gate command passed 36/36:
  `ctest --test-dir build/RelWithDebInfo-0.0.9 -LE "stress|slow|load|long-running|benchmark|perf" --output-on-failure --timeout 120 -j 1`.
- `CSharpGameplayProofTests` was included in that normal gate and passed, so
  separate Phase 9D-3 isolation was not required by the continuation policy.
- `ReplicationTests` and `StressTests` were excluded. Their stress, slow, load,
  and long-running coverage remains unproven.
- Raw full CTest still is not a pass.

Phase 9D-4 raw full CTest policy:

- `docs/recovery/phase-09-local-evidence/PHASE_9D_FULL_CTEST_POLICY_DECISION.md` selects Decision
  B/C: raw full CTest retry is unsafe for this local evidence path and remains
  blocked by heavy-test policy.
- The normal local gate is the heavy-label-excluded command above.
- `ReplicationTests` and `StressTests` are opt-in heavy evidence.
- Raw full CTest remains unresolved and cannot be represented as passing
  without a complete passing run.

Phase 9E gate matrix:

- `docs/recovery/phase-09-local-evidence/PHASE_9E_LOCAL_GATE_COMMAND_MATRIX.md` classifies local
  commands as immediate local, CI candidate, opt-in only, blocked, deferred,
  unstable, or evidence-only.
- Deterministic focused tests and the normal local gate are CI candidates after
  matching build/runtime prerequisites are available.
- Raw full CTest, `StressTests`, and heavy `ReplicationTests` are not default
  CI candidates.

Phase 9F closure:

- `docs/recovery/phase-09-local-evidence/PHASE_9_CLOSURE_CHECKPOINT.md` closes Phase 9 as Variant B
  limited closure.
- Accepted claims are limited to registry inventory, focused target build/run
  evidence, the normal local gate pass, known C# gameplay proof status, heavy
  gate separation, and the local gate matrix.
- Non-claims include raw full CTest pass, stress/load readiness, heavy
  replication readiness, release readiness, and publish hard-fail enforcement.
- Phase 10 may open as a publish gate / product packaging reality check using
  the Phase 9E matrix and normal local gate; it must not claim raw full CTest
  health or heavy-test readiness without new evidence.

### Phase 10 - Publish Gate / Product Packaging Reality Check

Status: closed as Variant A strong report/gate closure.

Work:

- Audit current publish readiness behavior before changing enforcement.
- Freeze the current publish report schema.
- Expand report-only validation diagnostics before any new hard blockers.
- Define publish profile policy and hard-gate eligibility.
- Prove focused package/publish/runtime compatibility.
- Close Phase 10 with accepted claims and explicit non-claims.

Acceptance:

- Publish report schema version 1 is stable for current fields.
- Existing deterministic blockers are classified and preserved.
- Warnings/report-only checks are separated from blockers.
- Focused package/publish/runtime tests pass.
- Raw full CTest and heavy opt-in debt remain explicit non-claims.
- No release readiness, product beta readiness, full AssetPipeline cook/import,
  ClientHost asset consumption, RuntimeServiceRegistry asset service, or full
  product proof is claimed.

Phase 10A evidence:

- `docs/recovery/phase-10-publish-gate/PHASE_10A_PUBLISH_GATE_REALITY_AUDIT.md` audits current
  publish readiness entry points, report behavior, schema fields, package asset
  identity coverage, current blockers, report-only checks, warnings, deferred
  checks, and relationships to Phase 5 and Phase 9.
- Current entry points include `Saga --publish-check`, `--publish-profile`,
  `--publish-report`, `--publish-diagnostics`, and
  `SagaPublishReadinessService::Check`.
- Current blockers are deterministic project/package/explicit diagnostics
  failures. Current package/asset/identity coverage is reported through
  `packageAssetIdentityCoverage`.
- The Phase 10A smoke set passed: `SagaPublishReadinessTests`,
  `SagaPackageStagingTests`, `RuntimePackageSmokeTests`, and
  `GeneratedRuntimeSmokePackageTests`.

Phase 10B evidence:

- `docs/recovery/phase-10-publish-gate/PHASE_10B_PUBLISH_REPORT_SCHEMA_FREEZE.md` freezes the
  current publish report as `schemaVersion` 1.
- Stable top-level fields include `reportId`, `buildId`, `packageId`,
  `status`, `diagnosticSummary`, `metadata`, `blockers`, `readiness`, and
  `packageAssetIdentityCoverage`.
- `SagaPublishReadinessTests` now assert parsed JSON schema shape and stable
  coverage fields.

Phase 10C evidence:

- `docs/recovery/phase-10-publish-gate/PHASE_10C_REPORT_ONLY_VALIDATION_EXPANSION.md` documents
  report-only validation expansion.
- `SagaPublishReadinessTests` now cover unknown asset kind and duplicate asset
  identity mapping as coverage diagnostics that do not add publish blockers.
- No new hard blockers were introduced.

Phase 10D evidence:

- `docs/recovery/phase-10-publish-gate/PHASE_10D_PUBLISH_PROFILE_POLICY.md` defines profile
  policy for `dev-local`, `package-smoke`, `shipping-report-only`,
  `shipping-full`, `CI-candidate`, and `future-enterprise`.
- Existing deterministic blockers remain enabled; new hard gates are deferred.
- Raw full CTest unresolved status, stress/load debt, heavy replication debt,
  source cook/import, ClientHost asset consumption, RuntimeServiceRegistry
  asset service, and full product proof remain warning-only or deferred.

Phase 10E evidence:

- `docs/recovery/phase-10-publish-gate/PHASE_10E_PRODUCT_PACKAGING_COMPATIBILITY_PROOF.md`
  records the focused compatibility matrix.
- The 12-test package/publish/runtime focused CTest set passed: package
  manifest writer, asset manifest writer, identity manifest writer, identity
  runtime contract, generated RuntimeSmoke manifest/package, runtime package
  smoke, runtime asset catalog/bootstrap/startup bootstrap, package staging,
  and publish readiness.

Phase 10F closure:

- `docs/recovery/phase-10-publish-gate/PHASE_10_CLOSURE_CHECKPOINT.md` closes Phase 10 as
  Variant A strong report/gate closure for report-first publish readiness and
  selected existing deterministic blockers.
- Phase 11 may open only as a scoring re-audit that preserves Phase 10
  non-claims.

### Phase 11 - Scoring Re-Audit

Status: closed as Foundation Established. Recovery roadmap closed.

Work:

- Define scoring rubric before scoring.
- Collect evidence by category from Phase 3 through Phase 10 closure docs.
- Classify hard blockers, soft blockers, deferred debt, evidence blockers, and
  environment/test blockers.
- Re-score each category from evidence and blockers.
- Recommend the post-recovery roadmap.
- Close the recovery roadmap honestly.

Acceptance:

- Every score change cites files, tests, docs, or build output.
- Production-grade claims require boundary enforcement, vertical slices, publish gate evidence, and safe-suite evidence.
- Product beta and release candidate claims remain blocked unless evidence
  proves them.
- Recovery closure preserves raw full CTest, heavy gate, full AssetPipeline,
  ClientHost asset consumption, RuntimeServiceRegistry asset service, editor
  product readiness, and product proof non-claims.

Phase 11A evidence:

- `docs/recovery/phase-11-scoring/PHASE_11A_SCORING_RUBRIC.md` defines the scoring
  categories, 0/10 through 10/10 scale, evidence quality levels,
  classification levels, and score caps.
- Score caps include raw full CTest unresolved, heavy gates unresolved, missing
  product vertical proof, incomplete source import/cook, incomplete
  runtime/client asset consumption, incomplete RuntimeServiceRegistry asset
  service, and incomplete editor product readiness.

Phase 11B evidence:

- `docs/recovery/phase-11-scoring/PHASE_11B_EVIDENCE_COLLECTION_MATRIX.md` maps accepted
  evidence, proof quality, missing evidence, deferred debt, confidence, and
  score caps for all scoring categories.
- Evidence quality uses direct proof, focused proof, partial proof, coarse
  proof, docs-only proof, report-only proof, explicitly deferred debt, no
  proof, and contradicted/unsafe claim.

Phase 11C evidence:

- `docs/recovery/phase-11-scoring/PHASE_11C_BLOCKER_REVIEW.md` classifies hard blockers,
  soft blockers, deferred debt, evidence blockers, and environment/test
  blockers.
- Hard blockers include raw full CTest unresolved, heavy gates unresolved,
  missing stress/load/performance evidence, incomplete AssetPipeline
  source/import/cook, incomplete ClientHost/runtime asset consumption,
  incomplete RuntimeServiceRegistry asset service, missing product/playable
  proof, incomplete editor product readiness, incomplete replication product
  loop, no CI hard enforcement, and release packaging not proven.

Phase 11D evidence:

- `docs/recovery/phase-11-scoring/PHASE_11D_FINAL_SCORE_REAUDIT.md` assigns final category
  scores from evidence and blocker caps.
- Overall foundation score is 6/10; unweighted subsystem average is 5.4/10.
- Final classification is Foundation Established.
- Foundation Complete Candidate, Product Beta Candidate, and Release Candidate
  are rejected under current evidence.

Phase 11E evidence:

- `docs/recovery/phase-11-scoring/PHASE_11E_POST_RECOVERY_ROADMAP.md` recommends the
  post-recovery roadmap.
- Priority order is Product Vertical Proof, Runtime/ClientHost Asset
  Consumption, AssetPipeline Source Import/Cook, Server/Replication Product
  Loop, Test/CI Hardening, Editor Productization, and Release/Packaging
  Hardening.
- First recommended post-recovery slice is Product Vertical Proof 1A:
  MultiplayerSandbox package/playable proof audit.

Phase 11F closure:

- `docs/recovery/phase-11-scoring/PHASE_11_CLOSURE_CHECKPOINT.md` closes Phase 11 as
  Outcome A: Recovery closed as Foundation Established.
- Accepted claims are foundation-level only. Product beta, release candidate,
  raw full CTest health, stress/load/performance readiness, full AssetPipeline
  cook/import, full runtime/client asset consumption, full product proof, and
  CI hard enforcement remain non-claims.
- Future work moves into the post-recovery roadmap.
