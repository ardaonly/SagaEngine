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



### Report-Only Checklist For Future Boundary Checks

- Check public headers before every `PUBLIC` to `PRIVATE` proposal.
- Prefer one direct dependency edge per behavior-changing slice.
- Keep legacy violations report-only until their owner phase has migration evidence.
- Make new/cleaned areas hard-fail before making legacy areas hard-fail.
- Record every rejected candidate with the reason it is unsafe or too broad.
- Keep full test health marked unverified unless the relevant full suite set is actually run.

## CMake Target Inventory



## Phase 2B-3 Candidate Audit



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
