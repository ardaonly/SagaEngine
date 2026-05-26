# Phase 9E Local Gate Command Matrix

> Last updated: 2026-05-26
> Status: Local gate matrix established from limited Phase 9 evidence
> Phase 9: Local Evidence Gates

This Local Gate Command Matrix records which commands are immediate local
evidence, CI candidate evidence, opt-in only, blocked, deferred, unstable, or
evidence-only after Phase 9D-2 and Phase 9D-4.

Gate classifications are evidence scope, not release claims.

## Baseline Always-Local Checks

| Command | Classification | Use |
|---|---|---|
| `git diff --check` | immediate local | Required after docs or code edits. |
| `rg -n "[ \t]$" <touched-doc-files>` | immediate local | Required for touched docs. |
| targeted `rg` phrase scans | immediate local | Required for phase docs and policy wording. |
| focused architecture guard when boundary claims are touched | immediate local / CI candidate | Use `ArchitectureTests` and `EditorQtPublicAbiBoundaryTests` for boundary claims. |

## Normal Local Gate

| Command | Classification | Status |
|---|---|---|
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -LE "stress|slow|load|long-running|benchmark|perf" --output-on-failure --timeout 120 -j 1` | immediate local | Passed 36/36 in Phase 9D-2. |

The normal local gate excludes heavy labels and is the default local evidence
gate for Phase 9 closure. It is not raw full CTest.

## Editor And Architecture

| Command | Classification | Notes |
|---|---|---|
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -R ArchitectureTests --output-on-failure --timeout 120 -j 1` | immediate local / CI candidate | Use when CMake, ownership, or architecture claims are touched. |
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -R EditorQtPublicAbiBoundaryTests --output-on-failure --timeout 120 -j 1` | immediate local / CI candidate | Use when public Editor API or Qt boundary claims are touched. |
| `build/RelWithDebInfo-0.0.9/bin/SagaUnitTests --gtest_filter=ProjectManagerTests.*` | immediate local / CI candidate when relevant | Focused editor workflow proof; not a separate CTest entry in this matrix. |

## Runtime, Package, Asset, Product, And Tools

| Command | Classification | Notes |
|---|---|---|
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "RuntimeStartupPreflightTests|RuntimeStartupSessionTests|RuntimeStartupDiagnosticsTests|RuntimeServiceLifecycleTests|RuntimeServiceRegistryTests|RuntimeServiceRegistryDiagnosticsTests" --output-on-failure --timeout 120 -j 1` | immediate local / CI candidate | Runtime startup and service lifecycle evidence. |
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "RuntimeAssetCatalogTests|RuntimeAssetBootstrapTests|RuntimeAssetStartupBootstrapTests|RuntimeAssetBootstrapDiagnosticsTests|RuntimePackageSmokeTests" --output-on-failure --timeout 120 -j 1` | immediate local / CI candidate | Runtime asset/package consumption evidence. |
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "SagaPackageStagingTests|SagaPublishReadinessTests" --output-on-failure --timeout 120 -j 1` | immediate local / CI candidate | Product package staging and publish-readiness visibility only; not publish enforcement. |
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "AssetIdentityRuntimeContractTests|AssetIdentityManifestWriterTests|AssetManifestWriterTests|PackageManifestWriterTests|GeneratedRuntimeSmokeManifestTests|GeneratedRuntimeSmokePackageTests" --output-on-failure --timeout 120 -j 1` | immediate local / CI candidate | AssetPipeline/package writer and generated RuntimeSmoke evidence. |
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -R SagaPipelineTests --output-on-failure --timeout 120 -j 1` | immediate local / CI candidate | Tooling pipeline evidence. |

## Server, Networking, And Replication

| Command | Classification | Notes |
|---|---|---|
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "ActorOwnershipRegistryTests|AuthoritativeMovementCoreTests|AuthoritativeMovementInputAdapterTests|AuthoritativeMovementCommandIntakeTests" --output-on-failure --timeout 120 -j 1` | immediate local / CI candidate | Server authoritative movement foundation. |
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -R "ServerPacketNormalizationTests|ZoneServerPacketDrainTests|ZoneServerMovementAuthorityTests|MovementDirtyReplicationBridgeTests" --output-on-failure --timeout 120 -j 1` | immediate local / CI candidate | Server networking and movement replication bridge evidence. |
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -R IntegrationTests --output-on-failure --timeout 120 -j 1` | evidence-only / CI candidate after timing policy | Included in the normal gate and passed, but labeled `timing-sensitive`. |
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -R ReplicationTests --output-on-failure --timeout 180 -j 1` | opt-in only | Heavy replication evidence because the test is `slow;long-running`. |

## C# Scripting

| Command | Classification | Notes |
|---|---|---|
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -R CSharpScriptHostTests --output-on-failure --timeout 120 -j 1` | immediate local / CI candidate with .NET prerequisites | Passed inside the Phase 9D-2 normal gate. |
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -R CSharpGameplayProofTests --output-on-failure --timeout 120 -j 1` | immediate local / CI candidate with .NET prerequisites | Passed inside the Phase 9D-2 normal gate; separate Phase 9D-3 isolation was not required. |

## Raw Full CTest And Heavy Opt-In

| Command | Classification | Status |
|---|---|---|
| `ctest --test-dir build/RelWithDebInfo-0.0.9 --output-on-failure --timeout 180 -j 1` | blocked / unsafe / unresolved | Previous raw full CTest attempts did not complete; no pass evidence. |
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -R StressTests --output-on-failure --timeout 180 -j 1` | opt-in only | Heavy stress/load evidence; excluded from normal local gate. |
| `ctest --test-dir build/RelWithDebInfo-0.0.9 -R ReplicationTests --output-on-failure --timeout 180 -j 1` | opt-in only | Heavy slow/long-running replication evidence; excluded from normal local gate. |

## CI Candidate Policy

CI candidates are deterministic focused tests and the normal local gate after
the CI agent has matching build, runtime, and .NET/hostfxr prerequisites.

Do not include raw full CTest, `StressTests`, or heavy `ReplicationTests` in
default CI until those gates are separately stabilized and intentionally
enabled.

## Verification

Follow-up verification for this docs slice:

- `git diff --check`
- touched-doc trailing whitespace scan
- targeted phrase scan for `Local Gate Command Matrix`, gate classifications,
  `CI candidate`, `opt-in only`, `blocked`, and `raw full CTest`
