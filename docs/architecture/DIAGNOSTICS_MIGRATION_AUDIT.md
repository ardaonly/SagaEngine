# SagaDiagnostics Migration Audit

> Last updated: 2026-05-26
> Status: Historical diagnostics migration audit
> Scope: Current diagnostics inventory, Public/Private migration fit, CMake/test strategy, and first implementation-slice recommendation.

This audit records the current repository reality before expanding
SagaDiagnostics. It is not current architecture truth, product evidence, or an
implementation milestone. Current diagnostics claims should come from the
focused diagnostics boundary/report docs.

## Inspected Evidence

Files and areas inspected for this audit:

- `docs/CodingStandards.md`
- `docs/DependencyGraph.md`
- `docs/architecture/CMAKE_BOUNDARY_INVENTORY.md`
- `docs/architecture/CURRENT_STATUS.md`
- `docs/architecture/TESTING_AND_EVIDENCE.md`
- `docs/testing/TEST_SUITES.md`
- `docs/dev/README.md`
- `docs/product/SAGA_ECOSYSTEM_MAP.md`
- historical product-slice notes under `docs/internal/product-history/`
- `docs/architecture/SOURCE_OF_TRUTH_MAP.md`
- `CMakeLists.txt`
- `cmake/modules/SagaTargets.cmake`
- `cmake/modules/SagaTests.cmake`
- `Engine/Public/SagaEngine/Diagnostics`
- `Engine/Private/SagaEngine/Diagnostics`
- `Shared/include/SagaShared/Diagnostics`
- `Runtime/include/SagaRuntime`
- `Runtime/src/SagaRuntime`
- `Server/include/SagaServer`
- `Server/src/SagaServer`
- `Tools/Forge/include/Forge/Diagnostics`
- `Tests/Unit/Diagnostics/DiagnosticFoundationTests.cpp`
- `Tests/Unit/Shared/DiagnosticContractsTests.cpp`
- `Tests/Unit/Architecture/PublicPrivateBoundaryTests.cpp`
- `Tests/Unit/Architecture/CMakeTargetBoundaryTests.cpp`

The wider repository layout was also inspected for Engine, Runtime, Server,
Net/Networking, Tools, cmake, tests, and existing architecture tests.

## Current Layout Reality

The repository does not currently use a root
`Source/SagaEngine/<Module>/Public/Private` layout.

Current module layout is mixed:

- `Engine` uses `Engine/Public` and `Engine/Private`.
- `Runtime` uses `Runtime/include` and `Runtime/src`.
- `Server` uses `Server/include` and `Server/src`.
- `Shared` uses `Shared/include` and `Shared/src`.
- `Collaboration` uses `Collaboration/include` and `Collaboration/src`.
- `Backends` uses `Backends/include` and `Backends/src`.
- `Tools` are split by tool, with tool-local `include` and `src` roots where needed.
- `Apps` are app-owned executable/product shells and do not use Public/Private module roots.
- Tests live under `Tests/Unit`, `Tests/Integration`, `Tests/Replication`,
  `Tests/Stress`, and `Tests/Tools`.

`Tests/Unit/Architecture/PublicPrivateBoundaryTests.cpp` currently expects
`Engine/Public` and `Engine/Private` to be the Engine split and explicitly
checks that root `Source/SagaEngine` does not exist. Introducing
`Source/SagaEngine/<Module>/Public/Private` immediately would conflict with
current architecture-test expectations and would be a broad migration, not a
safe diagnostics foundation slice.

The compatible transitional direction is:

- keep Engine-owned diagnostics headers in `Engine/Public/SagaEngine/Diagnostics`;
- keep Engine-owned diagnostics implementation in `Engine/Private/SagaEngine/Diagnostics`;
- keep Runtime diagnostics facades in `Runtime/include` and `Runtime/src`;
- keep Server diagnostics integration in `Server/include` and `Server/src`
  when a later milestone needs it;
- keep shared data-only diagnostic contracts in `Shared/include/SagaShared/Diagnostics`;
- defer any root `Source/SagaEngine/...` migration until a dedicated layout
  migration milestone updates build rules, architecture tests, include roots, and
  documentation together.

## Diagnostics Inventory

Existing diagnostics-related code is broader than one subsystem and should not
be collapsed into a single implementation owner.

Current inventory:

- `SagaCoreLog` exists as a static CMake target over
  `Engine/Private/SagaEngine/Core/Log`, exporting `Engine/Public`.
- `SagaDiagnostics` exists as a static CMake target over
  `Engine/Private/SagaEngine/Diagnostics`, exporting `Engine/Public` and
  linking `SagaCoreLog`.
- Engine diagnostics public headers currently cover diagnostic config,
  `DiagnosticSystem`, health metrics/snapshots/monitoring, and lifetime
  handles/records/tracking.
- Engine diagnostics private sources currently implement `DiagnosticSystem`,
  `HealthMonitor`, `HealthSnapshot`, and `LifetimeTracker`.
- Engine Core also has logging, profiling, memory tracking, and a
  `CrashHandler.cpp` under `Engine/Private/SagaEngine/Core/Log`.
- Shared diagnostics contracts already exist under
  `Shared/include/SagaShared/Diagnostics` for severity, category, source, code,
  location, payload, and summary.
- Runtime has startup, startup preflight, startup session, asset bootstrap,
  service lifecycle, service registry, and diagnostic summary/view facades under
  `Runtime/include/SagaRuntime` and `Runtime/src/SagaRuntime`.
- Server has report-like authority and simulation results, plus networking and
  movement validation code that can later emit diagnostics, but it does not yet
  have a SagaDiagnostics integration layer.
- Package and startup validation diagnostics exist under Engine package/startup
  headers and sources.
- Scripting diagnostics exist in shared scripting payloads, Engine scripting
  low-level APIs, managed bridge logging, and `Tools/SagaScript`.
- Forge has diagnostics/report types under `Tools/Forge/include/Forge`.
- Editor and composition code include editor/composition diagnostics concepts,
  but editor diagnostics presentation is a separate ownership concern.
- Existing tests include `DiagnosticFoundationTests`,
  `SharedDiagnosticContractsTests`, and architecture boundary tests for public
  diagnostics headers and target dependency direction.

This inventory means SagaDiagnostics already has foundations, but those
foundations are not a complete runtime reliability and lifetime diagnostics
platform.

## Dependency Direction Rules

The diagnostics dependency model for the next milestones is:

```txt
Core must not depend on Diagnostics.
Diagnostics may depend on Core.
Runtime may depend on Diagnostics where a Runtime-owned boundary needs it.
Server may depend on Diagnostics where a Server-owned boundary needs it.
Net/Networking may depend on Diagnostics through its owning module boundary.
Tools may depend on Diagnostics contracts or reports where appropriate.
Diagnostics must not hard-depend on SDE.
Shared diagnostics contracts must remain data-only and must not own emitters.
```

Current CMake partially matches this:

- `SagaCoreLog` is separate from `SagaDiagnostics`.
- `SagaDiagnostics` links `SagaCoreLog`.
- `SagaEngine` links `SagaCoreLog`.
- `SagaUnitTests` links broad subsystem targets including `SagaDiagnostics`.

The next implementation slices should preserve the current Core-to-Diagnostics
direction by keeping Core logging usable without `SagaDiagnostics`.

## CMake Target Strategy

Do not add a new `SagaDiagnostics` target in the next milestone; it already exists.

Recommended target strategy:

- keep the existing `SagaDiagnostics` target while auditing its public header
  surface;
- avoid adding SDE, Editor, Server, Runtime, Tools, or Apps as hard dependencies
  of `SagaDiagnostics`;
- keep `SagaCoreLog` lower-level than `SagaDiagnostics`;
- add new diagnostics source files only under the current Engine diagnostics
  roots until a dedicated layout migration exists;
- avoid broad `PUBLIC` dependencies from `SagaDiagnostics` unless a public
  header requires the dependency;
- add focused CMake/architecture tests before enforcing any new dependency rule
  as a hard gate.

The current build still uses recursive source collection for Engine
diagnostics. That is a known boundary-inventory risk, but changing it is out of
scope for this milestone.

## Test Strategy

Existing useful tests:

- `Tests/Unit/Diagnostics/DiagnosticFoundationTests.cpp`
- `Tests/Unit/Shared/DiagnosticContractsTests.cpp`
- `Tests/Unit/Architecture/PublicPrivateBoundaryTests.cpp`
- `Tests/Unit/Architecture/CMakeTargetBoundaryTests.cpp`

Recommended next test direction:

- keep diagnostics behavior tests deterministic and independent of wall-clock,
  process-global crash handling, networking, stress, or external services;
- add focused tests for any new diagnostics object before wiring it into
  Runtime, Server, Net, or Tools;
- add architecture tests for dependency direction only after the expected
  boundary is represented in CMake and public headers;
- keep broad `SagaUnitTests` evidence limited because the unit executable still
  links many subsystem targets.

## Recommended First Implementation Slice

The recommended first code slice after this audit is a narrow dependency and
contract cleanup around the existing `SagaDiagnostics` target:

- document the minimal public surface expected from
  `Engine/Public/SagaEngine/Diagnostics`;
- add or refine architecture coverage that proves diagnostics public headers do
  not include Server, Editor, Apps, Tools, SDE, or private paths;
- verify `SagaDiagnostics` does not gain hard dependencies beyond Core logging
  and standard library needs;
- avoid Runtime/Server integration until the diagnostics boundary is made
  explicit and tested.

This keeps the first code milestone small and reviewable.

## Documentation Strategy

Documentation should move in lockstep with implementation slices:

- update current product, architecture, or testing docs when diagnostics
  ownership, test inventory, or non-goals change;
- update `docs/DependencyGraph.md` only when the dependency contract itself
  changes, not for temporary implementation notes;
- update architecture/testing docs when a new boundary test or verification
  command becomes real evidence.

## Explicit Non-Claims

These are non-claims for this milestone:

- This audit does not add SagaDiagnostics code.
- This audit does not add CMake targets.
- This audit does not move existing modules.
- This audit does not rename include roots.
- This audit does not implement logging, health, lifetime, memory, crash,
  stress, chaos, validation, or fault systems.
- The existing `SagaDiagnostics` target is not proof of production readiness.
- Existing crash-related source files are not proof of crash safety.
- Existing stress tests are not proof of stress/load readiness.
- Existing broad unit-test links are not proof of clean subsystem boundaries.
- Raw full CTest health remains unverified unless a complete raw full CTest run
  is executed and passes.
