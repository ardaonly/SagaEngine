# SagaEngine Test Suites

> Last updated: 2026-05-26
> Status: Phase 9E suite layer with normal local gate and heavy opt-in policy

This document defines the local SagaEngine test suite names exposed through
Forge. These suite names are not the complete raw CTest label inventory;
labels such as `product`, `package`, `ui`, and `collaboration` may appear in
CTest registration outside the stable Forge suite table.

Quick entry: [README.md](README.md) summarizes the current gate policy.

Use single-job execution first on this machine:

```sh
forge test --suite architecture --jobs 1
forge test --suite unit --jobs 1
forge test --suite runtime --jobs 1
forge test --suite server --jobs 1
forge test --suite asset --jobs 1
forge test --suite editor --jobs 1
forge test --suite tools --jobs 1
forge test --suite all-safe --jobs 1
```

Full test health is unverified unless every required suite is actually run and passes.
CTest registration and label presence alone are not pass evidence; focused
entries that are registered but absent from the current build tree must be built
and run before they support a suite claim.

Phase 9E defines the normal local gate as raw CTest with heavy labels excluded:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -LE "stress|slow|load|long-running|benchmark|perf" --output-on-failure --timeout 120 -j 1
```

This normal local gate passed 36/36 on 2026-05-26. It is not raw full CTest and
does not prove `ReplicationTests`, `StressTests`, stress, slow, load,
long-running, benchmark, or perf coverage.

## Suite Names

| Suite | CTest behavior | Notes |
|---|---|---|
| `architecture` | label regex `architecture` | Boundary and architecture checks. |
| `unit` | label regex `unit` | Unit tests. Some coverage is still coarse because `UnitTests` is a broad executable. |
| `runtime` | label regex `runtime` | Runtime and runtime-adjacent tests. |
| `server` | label regex `server` | Server-adjacent tests. Coarse until server tests are split further. |
| `networking` | label regex `networking` | Networking tests and integration coverage. |
| `replication` | label regex `replication` | Replication tests. Heavy replication entries may also be `slow` or `long-running`. |
| `asset` | label regex `asset` | Asset and package/manifest-adjacent tests. |
| `editor` | label regex `editor` | Editor tests and editor tool coverage. |
| `tools` | label regex `tools` | Tooling tests. |
| `integration` | label regex `integration` | Integration tests. Timing-sensitive entries are excluded from `all-safe`. |
| `stress` | label regex `stress` | Opt-in only. |
| `slow` | label regex `slow` | Opt-in only. |
| `all-safe` | label exclude `stress|slow|load|timing-sensitive|long-running` | Forge local safety profile. Phase 9E normal CTest excludes `stress|slow|load|long-running|benchmark|perf`. |

## Existing Label Behavior

Existing Forge label filtering is still supported:

```sh
forge test --label runtime --jobs 1
```

Do not combine `--suite` and `--label`; a suite is the SagaEngine-specific stable profile, while `--label` is the raw CTest label regex escape hatch.

## Focused Architecture Entries

`EditorQtPublicAbiBoundaryTests` is a focused CTest entry for Phase 6B:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure -j 1
```

It runs `SagaArchitectureTests --gtest_filter=EditorQtPublicAbiBoundaryTests.*`
and is labeled `unit;architecture;editor`. The guard scans public Editor
headers and `Apps/Saga/SagaEditorModule.h` for new Qt ABI exposure outside the
current explicit allowlist. It does not link Qt and does not prove Editor public
headers are Qt-free.

## Focused Publish And Package Entries

Phase 10 uses focused package/publish/runtime tests as product packaging reality
evidence:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R "PackageManifestWriterTests|AssetManifestWriterTests|AssetIdentityManifestWriterTests|AssetIdentityRuntimeContractTests|GeneratedRuntimeSmokeManifestTests|GeneratedRuntimeSmokePackageTests|RuntimePackageSmokeTests|RuntimeAssetBootstrapTests|RuntimeAssetCatalogTests|RuntimeAssetStartupBootstrapTests|SagaPackageStagingTests|SagaPublishReadinessTests" --output-on-failure -j 1
```

This focused gate proves current report-first publish readiness, package
staging, generated manifest, RuntimeSmoke package, and runtime asset bootstrap
compatibility. It does not prove release readiness, full AssetPipeline
cook/import, ClientHost package consumption, RuntimeServiceRegistry asset
service, raw full CTest health, stress/load readiness, or heavy replication
readiness.

## Phase 11 Recovery Classification

Phase 11 closes the recovery roadmap as Foundation Established. This
classification accepts the Phase 9 normal local gate and Phase 10 focused
package/publish/runtime compatibility proof as foundation evidence. It does not
upgrade raw full CTest, `StressTests`, or heavy `ReplicationTests` into passing
evidence.

## Known Limitations

- Phase 1 adds suite visibility; it does not prove all suites pass.
- Phase 6B adds an Editor public Qt exposure guard; it does not remove current
  allowed Qt leaks.
- Phase 1 does not refactor CMake target dependencies.
- The current `UnitTests` executable still groups many subsystems together, so several suite labels intentionally select coarse coverage.
- Some focused CTest entries can be registered before their executable has been
  built on demand in the local build tree.
- Under Forge `all-safe`, `stress`, `slow`, load, timing-sensitive, and
  long-running tests remain opt-in. The Phase 9E normal CTest gate uses the
  separate exclusion policy documented above.
- Raw full CTest remains unresolved after Phase 9D terminal/session instability
  and must not be reported as passing without a complete passing run.
- `ReplicationTests` is opt-in heavy because it is labeled
  `slow;long-running`; `StressTests` is opt-in heavy because it is labeled
  `stress;load;slow`.
