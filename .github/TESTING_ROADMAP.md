SPDX-License-Identifier: MPL-2.0

# SagaEngine Testing Roadmap and CI Redesign

## Status

- This document is an implementation roadmap, not evidence that every listed test already exists.
- Automated GitHub test execution is temporarily disabled while the native test graph is redesigned.
- Existing repository-owned GTest, CTest, Python, managed, Forge, and product tests remain in the repository.
- Tool mirror export automation and Dependabot remain active because they are not test execution workflows.
- New CI workflows must not be treated as trustworthy until they pass from a clean runner with no pre-existing build or generated project state.

## Current Repository Test Inventory

- The repository currently contains 247 C++ test source files.
- The inspected source tree contains approximately 2,112 GTest declarations.
- The configured Linux build examined during this review registers 71 CTest entries.
- Runtime modules own unit coverage for Core, Math, Diagnostics, ECS, Assets, Resources, Input, Audio, UI, Scripting, Networking, Replication, Persistence, Simulation, World, ServerAuthority, and SagaRuntime.
- Editor modules own unit coverage for EditorCore, EditorFramework, EditorQt, EditorQtRender, EditorAuthoring, VisualBlocksEditor, collaboration, experimental features, and EditorLab.
- Repository-level tests cover architecture boundaries, integration, GPU-facing behavior, replication, stress, contracts, product workflows, fixtures, and reusable support code.
- Managed and Python suites cover SagaProjectKit, SagaScript, SagaPackager, licensing, repository audits, environment setup, workflow policy, and tool dispatch.
- Existing strengths include module ownership, architecture constraints, publication recovery, failure injection, networking and replication behavior, managed tool contracts, editor workflows, and headless runtime scenarios.
- Existing test source should be reused and reorganized before equivalent tests are created.

## Why the Previous GitHub Test Workflows Were Retired

- The retired test workflows contained 1,117 lines of YAML and repeated dependency installation, configure, build, reporting, and artifact logic.
- Long explicit CMake target lists drifted from the actual registered CTest inventory.
- Registered tests could be selected even when their executables had not been built.
- Managed test prerequisites such as `SagaScriptRuntimeBridge` were not consistently represented in the native target dependency graph.
- Package-generation state and checked-in package locks could disagree with a newly compiled RuntimeBridge identity.
- Some tests relied on generated sample state left by an earlier command instead of creating isolated fixtures.
- Environment-sensitive requirements such as a display server, GPU, private .NET runtime, ICU availability, SDK version, and runtime library paths were distributed across workflow steps.
- Large test executables caused one prerequisite failure to appear as many unrelated GTest failures.
- The all-safe CTest selection used exclusion labels but did not prove that every remaining selected executable and prerequisite existed.
- Linux, Windows, nightly, licensing, and evidence workflows duplicated setup rules instead of consuming one repository-owned suite definition.
- The workflow checker correctly validated permissions, action pinning, timeouts, concurrency, and failure visibility, but those structural checks did not prove an end-to-end clean-runner build.
- Scheduled GPU preflight intentionally reported a blocked state and did not provide real GPU validation.
- There was no complete sanitizer, leak detection, source coverage, or production fuzzing pipeline.
- CI restoration must solve these ownership and reproducibility problems instead of copying the retired YAML into smaller files.

## Required Test Architecture

- CMake and CTest must remain the source of truth for native test registration, prerequisites, labels, fixtures, resource locks, and timeouts.
- GitHub workflow YAML must remain thin orchestration and must not duplicate native target inventories.
- Suite selection must use stable CTest labels or a generated suite manifest checked against the configured CMake File API graph.
- Every selected CTest entry must have a buildable executable and an explicit dependency path from its suite target.
- Managed assemblies, private runtime closure files, generated packages, samples, shaders, and product executables must be explicit build prerequisites.
- Test runs must start from isolated temporary roots and must not depend on or mutate checked-in sample output directories.
- A shared `SagaTestSupport` library should own deterministic seeds, temporary directories, environment capture, bounded polling, free-port allocation, child-process cleanup, log collection, and common GTest startup behavior.
- Test executables should have one bounded owner and should not combine unrelated product, UI, networking, and recovery responsibilities.
- CTest resource locks should serialize display, GPU, fixed-port, and other exclusive resources.
- All subprocess tests must use explicit timeouts, bounded termination, captured stdout and stderr, and typed exit classifications.
- Every workflow must publish JUnit and failure logs even when its test command fails.
- Sanitizer logs, crash dumps, coverage data, benchmark samples, and environment manifests must be uploaded as separate machine-readable artifacts.
- Tests must record compiler, standard library, CMake, Conan, SDK, runtime, OS image, GPU or display identity, seed, and selected suite.
- Retry must never hide a first failure; repeated execution is evidence for flakiness and must report every attempt.

## Recommended Future File Layout

- `.github/workflows/pr-validation.yml` — fast repository, compile, unit, architecture, and headless smoke gates.
- `.github/workflows/nightly-validation.yml` — sanitizers, integration, product, fuzz smoke, determinism, and coverage.
- `.github/workflows/weekly-validation.yml` — ThreadSanitizer, longer fuzzing, replication, stress, benchmarks, and qualified GPU tests.
- `.github/workflows/release-validation.yml` — clean installation, standalone publication, packaging, reproducibility, and installed-consumer evidence.
- `.github/actions/setup-saga/action.yml` — one pinned composite action for toolchain and dependency setup.
- `Tools/Developer/CI/run_suite.py` — one repository-owned entry point that resolves a named CMake/CTest suite and writes an execution manifest.
- `cmake/testing/SagaTestSuites.cmake` — authoritative suite labels, aggregate build targets, fixtures, resource groups, and timeouts.
- `cmake/testing/SagaSanitizers.cmake` — compiler-specific ASan, LSan, UBSan, and TSan configuration with incompatible combinations rejected.
- `Tests/Support` — shared native test infrastructure with no product authority.
- `Tests/Fuzz` — libFuzzer harnesses, seed corpora, dictionaries, and minimized regression inputs.
- Module-local `Tests` directories — fast unit tests for the owning Runtime or Editor module.
- Repository `Tests/Integration` — multi-module and external-process behavior.
- Repository `Tests/Product` or the existing product-test owner — real executable workflows, including Editor and standalone products.
- Repository `Tests/GPU` — tests that require a qualified native graphics runner and declare their backend requirements.

## P0 — Required Before CI Restoration

- Clean configure and dependency installation using repository-pinned Conan profiles and exact tool versions.
- Linux GCC RelWithDebInfo build with warnings visible and no missing registered test executables.
- Windows MSVC RelWithDebInfo build for platform and ABI coverage.
- Fast module unit tests for all Runtime and Editor owners.
- Architecture and public/private dependency boundary tests.
- Headless Runtime startup, shutdown, invalid-input, and crash-diagnostic tests.
- SagaLauncher and Editor product tests under Xvfb/XCB with explicit display qualification.
- Standalone build publication, cancellation, recovery, cleanup, ownership-attack, and no-partial-output tests.
- Editor Play, standalone executable, and headless Runtime canonical gameplay payload and hash parity.
- Private managed runtime closure validation under empty `PATH`, conflicting `DOTNET_ROOT`, unrelated working directory, and missing or corrupt closure members.
- Package lock and generated package identity tests using fixtures created during the test, not stale sample build output.
- ASan and LSan build covering unit, headless product, managed-host boundary, publication, and representative editor tests.
- UBSan build covering math, ECS, serialization, networking, persistence, runtime startup, and product contract paths.
- Strict repository, licensing, DCO, workflow-security, generated-file, and whitespace checks executed outside the deleted workflow architecture until CI is restored.
- JUnit, CTest logs, sanitizer findings, crash output, and environment identity retained for every failing gate.
- A clean-runner acceptance test proving that one documented command builds every test selected by the PR suite before CTest starts.

## P1 — Professional Continuous Validation

- Clang build to complement GCC diagnostics and support sanitizer and coverage instrumentation.
- ThreadSanitizer as a separate build because it is not compatible with ASan and has different runtime requirements.
- TSan coverage for process controllers, asynchronous editor builds, cancellation, workspace services, networking queues, replication, and shared registries.
- LLVM source and branch coverage with per-domain summaries and changed-code visibility.
- Coverage thresholds introduced only after a reviewed baseline; generated, Vendor, fixture, and third-party code must be excluded.
- libFuzzer harnesses for project manifests, package locks, publication journals, gameplay result transport, asset manifests, scene and input documents, and network packet normalization.
- Every fuzz crash minimized and committed as a deterministic regression input before the defect is closed.
- Persistence and recovery fault injection at every journal write, flush, rename, restore, cleanup, cancellation, and re-entry boundary.
- Networking tests for malformed packets, reordering, duplication, loss, latency, disconnects, authority rejection, and deterministic replay.
- Replication convergence tests across different tick partitioning while preserving one semantic seed and endpoint.
- Reproducibility tests that build twice from one immutable snapshot and distinguish canonical differences from inspected publication-only metadata.
- Package visibility and atomic language reconfiguration tests for SagaProjectKit, SagaScript, and SagaPackager.
- Product lifecycle tests for launch, project open, Play, Stop, Play again, standalone publication, and graceful or forced shutdown.
- UI tests for workspace migration, work-mode preservation, controlled Blocks, conversion history, asynchronous build responsiveness, and cancellation.
- Flaky-test detection through fixed-seed repetition, randomized-order scheduled runs, duration tracking, and first-failure preservation.
- Static analysis using the repository `.clang-tidy` policy on changed native ownership areas, with findings reported separately from compiler failures.
- Dependency and supply-chain checks for pinned Actions, Conan lock or graph drift, managed dependencies, licenses, and known vulnerabilities.

## P2 — Scheduled and Release Evidence

- Weekly stress and soak tests for ECS churn, memory growth, networking, replication, persistence, editor open/close cycles, and repeated standalone publication.
- Valgrind or an equivalent independent Linux memory check on a small representative headless suite to complement ASan and LSan.
- Performance benchmarks for collectible processing, ECS iteration, serialization, package resolution, runtime startup, editor project open, and standalone build stages.
- Benchmark reports must retain raw samples, warmup policy, machine identity, variance, and comparison baseline.
- Qualified GPU tests for Vulkan and OpenGL on trusted runners with driver, device, backend, shader, and framebuffer evidence.
- Rendering tests should combine semantic state assertions with bounded image comparisons; golden image changes require explicit review.
- Release-mode LTO smoke tests and installed SDK consumer tests.
- Clean standalone packaging tests with empty user configuration and no developer/global runtime fallback.
- Release reproducibility and artifact inventory verification across two independent clean staging roots.
- Long fuzz campaigns for the P1 harnesses with retained corpora and deduplicated crash reports.

## C++ Memory Safety Requirements

- ASan must fail the job on use-after-free, heap or stack buffer overflow, invalid free, double free, and lifetime errors.
- LSan must fail on confirmed leaks in Saga-owned code after documented one-time runtime caches are suppressed narrowly.
- Suppressions must include an owner, justification, upstream reference when applicable, and removal condition.
- Broad module, library, path, or wildcard suppressions are forbidden.
- Tests must explicitly drain worker threads, event loops, process controllers, render resources, and service registries before leak evaluation.
- Qt and graphics tests must distinguish Saga leaks from process-global framework allocations through minimal reproductions and reviewed suppressions.
- Allocation failure tests should cover publication, document persistence, package materialization, diagnostics, and critical runtime startup paths where injection is feasible.
- Memory-growth soak tests must report sampled resident memory and owned allocation counters; a single final snapshot is insufficient.
- Windows should add MSVC AddressSanitizer coverage for representative native tests after the Linux sanitizer gate is stable.
- MemorySanitizer is deferred because it requires a fully instrumented dependency stack and is not a near-term CI restoration requirement.

## Product and Gameplay Requirements

- Product tests must launch real executables rather than substituting in-process helpers for every layer.
- Headless tests remain the primary deterministic product gate and must not require a display or GPU.
- Editor UI tests must use a real Qt event loop and a qualified virtual display on Linux.
- Standalone tests must verify executable hash, canonical manifest, complete inventory, cancellation behavior, recovery state, and previous-output preservation.
- Gameplay parity must compare the complete canonical payload and full SHA-256 across Editor Play, standalone, and headless execution.
- Gameplay coverage must include low and high score paths, active and inactive collectibles, repeated collection, reset, respawn, objectives, terminal results, package generation, and active implementation identity.
- Conversion history tests must cover Apply, Reverse, detached review, divergent custom source, recovery, and isolation from global Editor Undo/Redo.
- Workspace migration tests must cover exact opaque bytes, read-back verification, marker ordering, crash re-entry, invalid legacy values, and real XCB state restoration.

## CI Execution Tiers

- Pull request target: deterministic repository checks, compilation, unit, architecture, sanitizer smoke, and headless product smoke within approximately 20 minutes.
- Main branch target: complete supported-platform unit and product validation within approximately 60 minutes.
- Nightly target: ASan/LSan, UBSan, integration, product UI, fuzz smoke, coverage, reproducibility, and deterministic replay.
- Weekly target: TSan, stress, soak, longer fuzzing, benchmarks, replication, and trusted GPU evidence.
- Release target: clean package, install, standalone, private-runtime, licensing, inventory, and reproducibility validation.
- Linux GCC, Linux Clang, and Windows MSVC are the near-term supported CI compiler set.
- macOS and additional platforms are deferred until the near-term graph is stable and must not block initial CI restoration.

## Acceptance Criteria for New Workflows

- The workflow succeeds from a clean checkout with empty build and generated-output directories.
- One suite command builds exactly the prerequisites for the tests it selects.
- No workflow contains a hand-maintained list that can silently diverge from CMake test registration.
- Every test has an owner, labels, timeout, isolation policy, and failure artifact path.
- Display, GPU, network port, and other resource requirements are explicit and enforceable.
- No test reads a developer-global tool or runtime when a captured or private runtime is required.
- A missing prerequisite fails once with a direct diagnostic instead of producing a cascade of unrelated failures.
- Cancellation and timeout paths terminate child processes and publish diagnostics.
- Security-sensitive Actions remain pinned to full commit SHAs with read-only repository permissions by default.
- Workflow validation includes both static policy checks and an end-to-end clean-runner proof.
- CI is restored only after the P0 suite passes twice from independent clean build roots.

## Explicit Non-Goals for the First Redesign

- Do not add console, mobile, or cloud-platform matrices during initial restoration.
- Do not require MemorySanitizer before the dependency stack can be instrumented consistently.
- Do not make GPU rendering a prerequisite for deterministic headless product correctness.
- Do not create a second test registry in YAML or Python.
- Do not report skipped, blocked, or missing tests as passing evidence.
- Do not introduce a global coverage percentage without domain ownership and a reviewed baseline.
- Do not delete existing tests merely because their previous GitHub workflow was unreliable.
