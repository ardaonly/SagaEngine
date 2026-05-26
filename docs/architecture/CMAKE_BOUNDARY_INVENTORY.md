# CMake Boundary Inventory

> Last updated: 2026-05-24
> Status: Phase 2A report-only inventory
> Scope: CMake target/interface risks, include-root risks, recursive glob risks, and obvious public header boundary risks.

This document is not an enforcement gate.

Phase 2A does not change build behavior. It records current risks so later
Phase 2 slices can make narrow, reviewable changes with evidence.

Full test health remains unverified.

## Classification Key

- `report-only legacy violation`: Known existing shape that should remain non-blocking until a later slice owns migration.
- `candidate for PRIVATE conversion`: A current `PUBLIC` link/include/definition may be too broad and should be audited for ABI need.
- `candidate for public API cleanup`: A public header or exported include root exposes implementation/tool/UI details.
- `candidate for test graph narrowing`: A test target can see or link more modules than its subsystem should require.
- `candidate for include-root restriction`: An exported include directory makes another layer visible too broadly.
- `uncertain, needs inspection`: Current evidence is not enough to prescribe the fix safely.

## Boundary Enforcement Model

Phase 2C defines how this inventory should be used after the first narrow
Phase 2B cleanups. It is not a repo-wide hard-fail gate.

### Active Cleanup Already Partially Addressed

These items have had one narrow direct edge corrected, but the surrounding
architecture remains open:

- `SagaEditorLabLib`: direct `Qt6::Core` and `Qt6::Widgets` public edges were narrowed to `PRIVATE`; public Qt exposure remains through `SagaEditorLib`.
- `SagaRuntimeLib`: direct `SagaCollaboration` public edge was narrowed to `PRIVATE`; Runtime ownership and Runtime/App source drift remain open for Phase 3.
- `SagaServerLib`: direct `SagaCollaboration` public edge was narrowed to `PRIVATE`; Server public surface and authoritative behavior remain open for Phase 4.

### Report-Only Legacy Violations

The following stay visible but must not fail builds until their owner slice is
ready:

- recursive project and test globs
- broad test include roots and broad `SagaUnitTests` link graph
- `SagaEditorLib` public Qt ABI
- `SagaProductLib` as a product dependency hub
- `SagaRuntime` reusing `Apps/Client` sources
- broad `SagaBackend` include root with mixed `SagaBackends/...` and generic `Services/...` paths
- `SagaEditorCompositionLib` publishing `SDE::Core` because its public header currently exposes SDE result types

### Future Hard-Fail Candidates

Rules can become hard-fail only after the target area is cleaned and the
expected contract is documented:

- New or cleaned targets must not add new `PUBLIC` dependencies without public-header evidence.
- Cleaned Editor public headers must not include Qt headers or expose Qt types.
- Runtime public headers must not include Apps, Editor, Server private authority, or tool/compiler internals.
- Server public headers must not include Editor, Product shell, tool/compiler internals, or Qt UI.
- Engine public headers must not include Server, Editor, Apps, Product, or tool/compiler internals.
- Architecture tests should fail only for boundaries that have already been migrated or explicitly declared as new-code rules.

### Later Phase Ownership Work

These are not Phase 2 cleanup tasks:

- Phase 3 Runtime Ownership: Runtime/App physical ownership, startup/session/service ownership, and Runtime test isolation.
- Phase 4 Server Authority: authoritative gameplay command flow, validation, fixed tick mutation, and replication output.
- Phase 5 Asset Pipeline: source import/cook ownership and runtime-only cooked artifact consumption.
- Phase 6 Editor Qt De-Exposure: Qt-free public editor panel contracts and private/backend Qt implementation.
- Phase 10 Publish Gate: package, asset, script, server config, cooked asset, and startup gate validation.

### Rejected Or Deferred CMake Cleanup Candidates

- `SagaEditorLib` Qt cannot become `PRIVATE` while public headers expose `QtPanel`/`QWidget`.
- `SagaProductLib` cleanup is too broad because the product target republishes major modules and has Qt-shaped public headers.
- `SagaEditorCompositionLib -> SDE::Core` cannot become `PRIVATE` while `CompositionCompiler.h` exposes SDE types.
- `SagaSandboxLib` is an app/dev composition target and needs a separate ABI audit before any one-edge cleanup.
- Test graph narrowing must be done in Phase 2C follow-up or later as narrow targets; broad test splitting is not part of this closure-preparation slice.

### Report-Only Checklist For Future Boundary Checks

- Check public headers before every `PUBLIC` to `PRIVATE` proposal.
- Prefer one direct dependency edge per behavior-changing slice.
- Keep legacy violations report-only until their owner phase has migration evidence.
- Make new/cleaned areas hard-fail before making legacy areas hard-fail.
- Record every rejected candidate with the reason it is unsafe or too broad.
- Keep full test health marked unverified unless the relevant full suite set is actually run.

## CMake Target Inventory

| ID | Classification | Evidence | Why it matters | Later phase/slice |
|---|---|---|---|---|
| CBI-001 | report-only legacy violation | `cmake/modules/SagaTargets.cmake:3-40` defines `saga_collect_sources()` with `file(GLOB_RECURSE)` and applies it to Shared, Collaboration, AssetPipeline, tools, Engine, Backends, Runtime, Server, Apps, Editor, Product, CoreLog, and Diagnostics. | Recursive globbing hides source ownership. New files can silently enter a target without an explicit review of module boundary or target ownership. | Phase 2B source ownership diagnostics, then explicit `target_sources` for new/risky modules. |
| CBI-002 | report-only legacy violation | `cmake/modules/SagaTests.cmake:29-98` uses recursive globs for unit, tools, integration, replication, stress, and architecture tests. | Tests can silently join a broad executable and inherit all broad include/link access. This makes boundary regressions look like ordinary test additions. | Phase 2C test graph narrowing and explicit test manifests for boundary-sensitive suites. |
| CBI-003 | candidate for include-root restriction | `SagaEngine` exports both `Engine/Public` and `Runtime/include` in `cmake/modules/SagaTargets.cmake:229-232`. | The dependency graph says Runtime consumes Engine/Core, but the Engine target currently publishes Runtime headers to Engine consumers. This may blur Engine vs Runtime ownership even when direct includes are not yet observed in `Engine/Public`. | Phase 2B include-root audit, then Phase 3 Runtime ownership realignment. |
| CBI-004 | report-only legacy violation | Phase 2B-2 moved `SagaRuntimeLib`'s direct `SagaCollaboration` link to `PRIVATE`, while `SagaEngine` and `SagaShared` remain `PUBLIC` in `cmake/modules/SagaTargets.cmake:262-269`. | The direct Runtime-to-Collaboration public edge was narrowed, but Runtime target ownership is still incomplete and physical Runtime/App drift remains outside this CMake slice. | Phase 3 Runtime ownership realignment; later Phase 2B slices may audit remaining Runtime public links only with public-header evidence. |
| CBI-005 | partially addressed; report-only legacy violation | Phase 2B-3 moved `SagaServerLib`'s direct `SagaCollaboration` link to `PRIVATE`, while `SagaEngine` and `SagaShared` remain `PUBLIC` in `cmake/modules/SagaTargets.cmake:288-294`. The Phase 2B-3 scan did not find `SagaCollaboration` or `Collaboration` references in `Server/include` or `Server/src`. | The direct Server-to-Collaboration public edge was narrowed, but Server still has a broad public surface and public headers still expose Engine/Shared/server networking contracts. | Phase 4 server authoritative slice; later Phase 2B slices may audit remaining Server public links only with public-header evidence. |
| CBI-006 | candidate for public API cleanup | `SagaEditorLib` exports `Editor/include` and links `Qt6::Core`/`Qt6::Widgets` as `PUBLIC` in `cmake/modules/SagaTargets.cmake:345-354`. Public editor headers include `QtPanel` and `QWidget` through `Editor/include/SagaEditor/UI/Qt/QtPanel.h`. | Qt is part of the public Editor ABI today. This directly conflicts with `docs/DependencyGraph.md:229`, which forbids Editor public headers exposing Qt types. | Phase 6 Editor Public API De-Qtification; Phase 2B can only convert Qt to `PRIVATE` after public headers are cleaned. |
| CBI-007 | report-only legacy violation | Phase 2B-1 moved `SagaEditorLabLib`'s direct `Qt6::Core` and `Qt6::Widgets` links to `PRIVATE`, but `SagaEditorLib` remains `PUBLIC` in `cmake/modules/SagaTargets.cmake:391-399` because public EditorLab headers expose SagaEditor types. | The direct Qt republication was narrowed, but EditorLab still inherits the broader public Editor ABI. This remains partial until Editor public headers stop exposing Qt through `SagaEditorLib`. | Phase 6 Editor Public API De-Qtification before further EditorLab public-interface cleanup. |
| CBI-008 | candidate for PRIVATE conversion | `SagaProductLib` exports `Apps/Saga` and publicly links `SagaEngine`, `SagaShared`, `SagaCollaboration`, `SagaRuntimeLib`, `SagaServerLib`, `Qt6::Core`, and `Qt6::Widgets` in `cmake/modules/SagaTargets.cmake:411-423`. | Product orchestration republishes Runtime, Server, Collaboration, and Qt to every consumer. That makes Apps/Product a transitive dependency hub instead of a thin product wiring layer. | Phase 2B Product target ABI audit; later publish/package work may define narrower product-facing interfaces. |
| CBI-009 | report-only legacy violation | `SagaEditorLabBridge` publicly links `SagaProductLib` and `SagaEditorLabLib` in `cmake/modules/SagaTargets.cmake:443-455`. | This is a dev bridge, but it composes Product + EditorLab as public dependencies. It can easily pull editor/product UI concerns into consumers that only need a bridge facade. | Phase 2B dev-bridge audit after Product and EditorLab public surfaces are narrowed. |
| CBI-010 | candidate for PRIVATE conversion | `SagaSandboxLib` publicly links `SagaEngine`, `SagaRuntimeLib`, `SagaServerLib`, and `SagaBackend` in `cmake/modules/SagaTargets.cmake:316-320`. | Sandbox is an app/dev target, but its public link interface republishes runtime, server, and backend together. That can normalize app-level dependency mixing. | Phase 2B app/dev target audit. |
| CBI-011 | report-only public ABI dependency | `SagaEditorCompositionLib` publicly links `SDE::Core` in `cmake/modules/SagaTargets.cmake:194-199`; Phase 2B-3 inspection found `Tools/SagaEditorComposition/include/SagaEditorComposition/CompositionCompiler.h` includes SDE headers and exposes `SDE::CompileState`, `SDE::Diagnostic`, and `SDE::CompilerHashes`. | SDE is currently public ABI for this tool library, so converting `SDE::Core` to `PRIVATE` would be incorrect until the public result contract is redesigned. | Later tool API cleanup if SagaEditorComposition needs an SDE-free public facade. |
| CBI-012 | report-only legacy violation | `SagaRuntime` reuses `Apps/Client/ClientHost.h` and `.cpp` through `SAGA_RUNTIME_COMMON_SOURCES` in `cmake/modules/SagaTargets.cmake:502-528`. | This is not a CMake visibility leak, but it is physical ownership drift: Runtime executable behavior comes from Apps/Client sources. | Phase 3 Runtime ownership realignment, not Phase 2A. |
| CBI-013 | report-only legacy violation | `SagaDistribution` already has `_saga_assert_no_qt_link()` for `SagaRuntime` and `SagaServer` in `cmake/modules/SagaDistribution.cmake:32-64` and applies it at `:98-99`. | This is useful existing enforcement, but narrow. It protects distribution runtime/server from Qt links, not all target/public-header boundaries. | Keep as-is. Phase 2B should not duplicate it; later boundary reporting can reference it. |
| CBI-014 | uncertain, needs inspection | `RenderClientSmokeTest` is registered in `cmake/modules/SagaTargets.cmake:800-818`, outside `SagaTests.cmake`, and has no Phase 1 suite label. | Test registration outside the central test module can bypass suite inventory conventions. It may be intentional, but it should be classified before Phase 1/2 evidence claims rely on complete suite coverage. | Phase 1 follow-up or Phase 2C test registration inventory. |

## Phase 2B-3 Candidate Audit

- `SagaServerLib -> SagaCollaboration`: safe to implement now; one direct dependency edge, no observed `SagaCollaboration` references in Server public headers or sources, and rollback is a one-edge CMake revert.
- `SagaEditorLib` Qt exposure: unsafe because dependency is public ABI; public Editor headers expose `QtPanel`/`QWidget` and belong to Phase 6.
- `SagaProductLib` Qt/Product hub exposure: too broad for this slice; product public headers include Qt-shaped activation contracts and the target republishes several major modules.
- `SagaEditorCompositionLib -> SDE::Core`: unsafe because dependency is public ABI; public headers expose SDE compile state, diagnostics, and hashes.
- `SagaSandboxLib`: uncertain and too broad for this slice because it intentionally republishes app/dev runtime, server, engine, and backend concerns.
- Documentation-only clarification: safe and applied here to keep inspected/rejected candidates visible as report-only evidence.

## Test Graph Inventory

| ID | Classification | Evidence | Why it matters | Later phase/slice |
|---|---|---|---|---|
| TBI-001 | candidate for test graph narrowing | `SAGA_TEST_INCLUDE_DIRS` grants tests `Engine/Public`, `Runtime/include`, `Server/include`, `Shared/include`, `Collaboration/include`, `Tools/AssetPipeline/include`, `Backends/include`, `Editor/include`, `Apps/EditorLab/include`, and `Apps/Saga` in `cmake/modules/SagaTests.cmake:110-121`. | A test can include almost any subsystem regardless of its suite label. Boundary mistakes can pass because tests do not model production include visibility. | Phase 2C test graph narrowing. |
| TBI-002 | candidate for test graph narrowing | `SagaUnitTests` links `SagaEngine`, `SagaDiagnostics`, `SagaRuntimeLib`, `SagaServerLib`, `SagaShared`, `SagaCollaboration`, `SagaAssetPipelineLib`, `SagaBackend`, `SagaEditorLib`, and optionally `SagaProductLib` in `cmake/modules/SagaTests.cmake:164-185`. | The `unit` executable is effectively a whole-engine integration target. This masks subsystem ownership and makes unit labels coarse. | Phase 2C split or narrow high-risk unit targets first. |
| TBI-003 | candidate for test graph narrowing | `SagaArchitectureTests` links only `SagaShared` and `SagaCollaboration`, but still inherits `SAGA_TEST_INCLUDE_DIRS` in `cmake/modules/SagaTests.cmake:541-555`. | Architecture tests can see headers from Runtime, Server, Editor, Apps, Backends, and AssetPipeline even when their link graph is narrower. Include-boundary tests are weaker if the test target itself has broad include roots. | Phase 2C architecture test include restriction. |
| TBI-004 | candidate for test graph narrowing | `SagaProductTests` links `SagaProductLib` and uses `SAGA_TEST_INCLUDE_DIRS` in `cmake/modules/SagaTests.cmake:520-534`. | Product tests can see all major include roots, so they cannot prove Product uses only product-level contracts. | Phase 2C product test target narrowing. |
| TBI-005 | report-only legacy violation | `SagaIntegrationTests`, `SagaReplicationTests`, and `SagaStressTests` all link broad engine/runtime/server/backend stacks and use broad include roots in `cmake/modules/SagaTests.cmake:560-633`. | These are integration/stress surfaces and may legitimately need broad access, but they should not be used as proof of clean public boundaries. | Keep report-only until Phase 2C; do not split in Phase 2A. |
| TBI-006 | uncertain, needs inspection | `SagaPackageStagingTests` compiles product sources and `Engine/Private/SagaEngine/Packages/PackageManifestLoader.cpp` directly in `cmake/modules/SagaTests.cmake:275-299`. | This may be a deliberate narrow package-staging slice, but direct private source compilation can bypass target contracts and hide package/public API gaps. | Phase 10 publish gate work, or Phase 2C if target boundary tests start covering package validation. |

## Public Header / Include-Root Inventory

| ID | Classification | Evidence | Why it matters | Later phase/slice |
|---|---|---|---|---|
| HBI-001 | candidate for public API cleanup | `Editor/include/SagaEditor/UI/Qt/QtPanel.h` includes `<QWidget>` and defines `QtPanel`. Public headers such as `Editor/include/SagaEditor/Panels/GraphViewportPanel.h`, `ProfilerPanel.h`, `CollaborationPanel.h`, and `ProjectBrowserPanel.h` include `QtPanel`. Visual scripting public headers also include it. | Editor public API exposes Qt directly, so CMake cannot safely make Qt private yet. | Phase 6 Editor Public API De-Qtification. |
| HBI-002 | candidate for public API cleanup | `Apps/Saga/SagaEditorModule.h` forward declares `QMainWindow` and `QStackedWidget`; `Apps/Saga` is exported by `SagaProductLib` in `cmake/modules/SagaTargets.cmake:411-423`. | Product public headers expose Qt-shaped editor activation contracts. Even with forward declarations, the product public ABI is Qt-coupled. | Phase 2B Product target audit, then Phase 6 if editor/product UI split is required. |
| HBI-003 | candidate for public API cleanup | `Backends/include` contains `SagaBackends/UI/RmlUiUiBackend.h` and many headers under `Backends/include/Services/Persistence/...`. `SagaBackend` exports the entire `Backends/include` root in `cmake/modules/SagaTargets.cmake:307-309`. | Backend public headers are not consistently rooted under `SagaBackends`. Consumers can include generic `Services/...` paths that do not identify backend ownership. | Phase 2B Backends include-root cleanup. |
| HBI-004 | candidate for include-root restriction | The quick scan did not find direct `Engine/Public` includes of Qt, Server, Editor, Apps, or Product headers. The observed CMake risk is instead `SagaEngine` publishing `Runtime/include` (`cmake/modules/SagaTargets.cmake:229-232`) and tests exposing all roots (`cmake/modules/SagaTests.cmake:110-121`). | Engine public headers are not currently the direct leak, but Engine's target interface and tests still weaken the intended Engine/Runtime separation. | Phase 2B include-root audit; Phase 3 Runtime ownership. |
| HBI-005 | uncertain, needs inspection | Server public headers include other server networking/simulation internals heavily, for example `Server/include/SagaServer/Networking/Server/ZoneServer.h` includes `ConnectionManager`, `ReplicationManager`, `InterestArea`, and `NetworkTypes`. | This may be acceptable inside the Server public API, but the public/private server boundary is not clearly separated by CMake include roots. | Phase 4 server authoritative slice may need a narrower public server command/simulation contract. |

## Recursive Glob Risk Locations

| Location | Classification | Risk | Later phase/slice |
|---|---|---|---|
| `cmake/modules/SagaTargets.cmake:3-40` | report-only legacy violation | All major module source ownership is inferred from folders. Misplaced source files can silently enter a target. | Phase 2B diagnostics, then explicit source lists for new/risky modules. |
| `cmake/modules/SagaTests.cmake:29-98` | candidate for test graph narrowing | Tests enter broad executables by directory, not by subsystem ownership contract. | Phase 2C test manifests or narrower targets. |
| `cmake/modules/SagaFindATL.cmake:60`, `:105`, `:140` | uncertain, needs inspection | Toolchain discovery uses globs. This is likely acceptable because it discovers installed MSVC/ATL versions rather than project ownership. | Leave report-only unless reproducibility issues appear. |
| `cmake/modules/SagaDistribution.cmake:163-168` | uncertain, needs inspection | Distribution staging globs Fontconfig data files from generated Conan metadata. This is not source ownership, but it is generated-artifact discovery. | Publish/package phase if deterministic staging evidence requires it. |
| `cmake/modules/SagaThirdparty.cmake:62-68` | uncertain, needs inspection | Dotnet host native directory is selected from a sorted glob of installed SDK host packs. | Scripting/runtime packaging phase if host selection becomes nondeterministic. |

## Existing Report-Only Legacy Violations

- `SagaEditorLib` public Qt exposure is known and must remain report-only until Phase 6 removes Qt from public headers.
- `SagaProductLib` as a product orchestration hub is known and must remain report-only until its public ABI is inspected.
- Broad test include/link graphs are known and must remain report-only until Phase 2C; Phase 2A must not split test targets.
- `SagaRuntime` reuse of `Apps/Client` sources is known and belongs to Phase 3, not Phase 2A.
- Recursive globs are known and must not be removed wholesale in Phase 2A.

## Uncertain Areas For Later Inspection

- Whether any downstream target relies on `SagaServerLib` to transitively publish Collaboration despite Collaboration not appearing in Server public headers.
- Whether `Backends/include/Services/...` should move under `SagaBackends/Services/...` or become a separate neutral service contract root.
- Whether `RenderClientSmokeTest` should be centralized under `SagaTests.cmake` or only receive suite labels in place.
- Whether package-staging tests should compile private package loader sources directly or consume a narrower public package validation target.
- Whether distribution Qt-link checks should emit a report artifact instead of only hard-failing during distribution configuration.
