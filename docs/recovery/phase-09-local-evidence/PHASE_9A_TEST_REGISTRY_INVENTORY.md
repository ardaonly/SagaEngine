# Phase 9A Test Registry Inventory

> Last updated: 2026-05-26
> Status: Phase 9A docs/report-only CTest registry and label inventory
> Phase 9: Local Evidence Gates

Phase 8 closed as Documentation / Code Alignment Evidence established. Phase 9
opens as Local Evidence Gates, starting with a list-only inventory of the local
CTest registry before any safe-suite, CI, relabeling, quarantine, or enforcement
work.

Full CTest remains unverified.

## Scope

Phase 9A records the current CTest registry from
`build/RelWithDebInfo-0.0.9`. It does not:

- run full CTest
- run any safe suite
- change source, CMake, test registration, labels, or Forge behavior
- relabel or quarantine any test
- add enforcement
- claim suite health from registration or label presence

## Registry Inventory

`ctest --test-dir build/RelWithDebInfo-0.0.9 -N` reports 38 registered
entries:

| # | CTest entry | Labels | Phase 9A evidence classification |
|---|---|---|---|
| 1 | `UnitTests` | `unit;runtime;server;networking;replication;asset;editor` | Broad/coarse evidence. |
| 2 | `ActorOwnershipRegistryTests` | `unit;server` | Registered but unbuilt in the current build tree. |
| 3 | `AuthoritativeMovementCoreTests` | `unit;server` | Registered but unbuilt in the current build tree. |
| 4 | `AuthoritativeMovementInputAdapterTests` | `unit;server` | Registered but unbuilt in the current build tree. |
| 5 | `AuthoritativeMovementCommandIntakeTests` | `unit;server` | Registered but unbuilt in the current build tree. |
| 6 | `ServerPacketNormalizationTests` | `unit;networking;server` | Registered but unbuilt in the current build tree. |
| 7 | `ZoneServerPacketDrainTests` | `unit;server;networking` | Registered but unbuilt in the current build tree. |
| 8 | `ZoneServerMovementAuthorityTests` | `unit;server;networking` | Registered but unbuilt in the current build tree. |
| 9 | `MovementDirtyReplicationBridgeTests` | `unit;server;replication` | Registered but unbuilt in the current build tree. |
| 10 | `SagaPipelineTests` | `tools;pipeline;integration` | Focused/direct-intent pipeline evidence. |
| 11 | `SagaPackageStagingTests` | `product;package;asset` | Focused/direct-intent package staging evidence. |
| 12 | `SagaPublishReadinessTests` | `unit;product;package;asset` | Focused/direct-intent publish readiness evidence. |
| 13 | `AssetIdentityRuntimeContractTests` | `unit;asset;tools;runtime;package` | Focused/direct-intent asset/package/runtime contract evidence. |
| 14 | `AssetIdentityManifestWriterTests` | `unit;asset;tools;runtime` | Focused/direct-intent asset identity writer evidence. |
| 15 | `AssetManifestWriterTests` | `unit;asset;tools;runtime;package` | Focused/direct-intent asset manifest writer evidence. |
| 16 | `PackageManifestWriterTests` | `unit;asset;tools;runtime;package` | Focused/direct-intent package manifest writer evidence. |
| 17 | `GeneratedRuntimeSmokeManifestTests` | `unit;asset;tools;runtime;package` | Focused/direct-intent generated manifest evidence. |
| 18 | `GeneratedRuntimeSmokePackageTests` | `unit;asset;tools;runtime;package` | Focused/direct-intent generated package evidence. |
| 19 | `SagaScriptLifecycleTests` | `unit;runtime;scripting` | Focused/direct-intent runtime scripting evidence. |
| 20 | `RuntimeStartupPreflightTests` | `unit;runtime` | Focused/direct-intent runtime startup evidence. |
| 21 | `RuntimeStartupSessionTests` | `unit;runtime` | Focused/direct-intent runtime startup evidence. |
| 22 | `RuntimeStartupDiagnosticsTests` | `unit;runtime` | Registered but unbuilt in the current build tree. |
| 23 | `RuntimeAssetCatalogTests` | `unit;runtime;asset;package` | Focused/direct-intent runtime asset/package evidence. |
| 24 | `RuntimeAssetBootstrapTests` | `unit;runtime;asset;package` | Focused/direct-intent runtime asset bootstrap evidence. |
| 25 | `RuntimeAssetStartupBootstrapTests` | `unit;runtime;asset;package;app` | Focused/direct-intent runtime/app package startup evidence. |
| 26 | `RuntimeAssetBootstrapDiagnosticsTests` | `unit;runtime;asset;package` | Focused/direct-intent runtime asset diagnostics evidence. |
| 27 | `RuntimePackageSmokeTests` | `unit;runtime;asset;package` | Focused/direct-intent runtime package smoke evidence. |
| 28 | `RuntimeServiceLifecycleTests` | `unit;runtime` | Registered but unbuilt in the current build tree. |
| 29 | `RuntimeServiceRegistryTests` | `unit;runtime` | Registered but unbuilt in the current build tree. |
| 30 | `RuntimeServiceRegistryDiagnosticsTests` | `unit;runtime` | Registered but unbuilt in the current build tree. |
| 31 | `ScriptBindingRuntimeTests` | `unit;runtime;scripting` | Focused/direct-intent runtime scripting evidence. |
| 32 | `CSharpScriptHostTests` | `unit;runtime;scripting;csharp` | Focused/direct-intent runtime scripting evidence. |
| 33 | `CSharpGameplayProofTests` | `unit;runtime;scripting;csharp` | Focused/direct-intent runtime scripting evidence. |
| 34 | `ArchitectureTests` | `architecture;unit` | Focused/direct-intent architecture evidence. |
| 35 | `EditorQtPublicAbiBoundaryTests` | `unit;architecture;editor` | Focused/direct-intent editor ABI evidence. |
| 36 | `IntegrationTests` | `integration;runtime;server;networking;replication;timing-sensitive` | Broad/coarse evidence. |
| 37 | `ReplicationTests` | `replication;integration;slow;long-running` | Broad/coarse evidence. |
| 38 | `StressTests` | `stress;load;slow` | Broad/coarse evidence. |

## Label Inventory

List-only CTest label inventory from `build/RelWithDebInfo-0.0.9`:

| Label or registry group | Count | Notes |
|---|---:|---|
| all registered entries | 38 | From `ctest -N`; `all` is not a raw CTest label in this build tree. |
| `architecture` | 2 | `ArchitectureTests`, `EditorQtPublicAbiBoundaryTests`. |
| `editor` | 2 | `UnitTests`, `EditorQtPublicAbiBoundaryTests`. |
| `ui` | 0 | Zero-test label. |
| `product` | 2 | `SagaPackageStagingTests`, `SagaPublishReadinessTests`. |
| `collaboration` | 0 | Zero-test label. |
| `runtime` | 23 | Includes broad `UnitTests`, broad `IntegrationTests`, and four registered-but-unbuilt runtime entries. |
| `server` | 10 | Includes broad `UnitTests`, broad `IntegrationTests`, and eight registered-but-unbuilt server entries. |
| `networking` | 5 | Includes broad entries and three registered-but-unbuilt server/networking entries. |
| `replication` | 4 | Includes broad entries and one registered-but-unbuilt bridge entry. |
| `asset` | 14 | Includes broad `UnitTests` plus focused asset/package/runtime entries. |
| `package` | 12 | Focused package/asset/runtime entries. |
| `tools` | 7 | Pipeline and asset/package tool entries. |
| `integration` | 3 | `SagaPipelineTests`, `IntegrationTests`, `ReplicationTests`. |

Zero-test labels observed in Phase 9A:

- `ui`
- `collaboration`

## Registered But Unbuilt

The list-only CTest commands warn that these registered entries cannot find
their executable in the current `build/RelWithDebInfo-0.0.9` tree:

- `ActorOwnershipRegistryTests`
- `AuthoritativeMovementCoreTests`
- `AuthoritativeMovementInputAdapterTests`
- `AuthoritativeMovementCommandIntakeTests`
- `ServerPacketNormalizationTests`
- `ZoneServerPacketDrainTests`
- `ZoneServerMovementAuthorityTests`
- `MovementDirtyReplicationBridgeTests`
- `RuntimeStartupDiagnosticsTests`
- `RuntimeServiceLifecycleTests`
- `RuntimeServiceRegistryTests`
- `RuntimeServiceRegistryDiagnosticsTests`

These entries are not runnable evidence in the current build tree. Their labels
still appear in list-only inventories, so Phase 9B should classify whether each
entry is expected to build, stale registration, build-target drift, or a
temporary generated-tree mismatch.

## Evidence Quality

Broad/coarse evidence:

- `UnitTests` aggregates many subsystem tests behind one broad executable and
  carries several labels.
- `IntegrationTests` carries runtime/server/networking/replication labels but
  is timing-sensitive and not isolated to one claim.
- `ReplicationTests` is an integration-style slow/long-running entry.
- `StressTests` is opt-in stress/load evidence, not local safe-suite evidence.

Focused/direct-intent evidence:

- `ArchitectureTests` and `EditorQtPublicAbiBoundaryTests` directly target
  architecture and editor ABI guard intent.
- `SagaPackageStagingTests` and `SagaPublishReadinessTests` directly target
  package staging and publish-readiness visibility.
- Asset/package/runtime manifest, identity, bootstrap, package smoke, startup,
  and scripting entries directly target their named slice when their executable
  is present.
- `SagaPipelineTests` directly targets pipeline-focused behavior.

Label presence and CTest registration are not pass evidence. Executable
presence is also not pass evidence. Missing executables are not runnable
evidence in this build tree.

## Verification

List/report checks only:

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

Full CTest remains unverified.

## Recommended Phase 9B

Recommended next slice: Label Cleanup / Registered-But-Unbuilt Test
Classification.

Phase 9B should classify each registered-but-unbuilt entry and decide whether
the correct remediation is build-target repair, registration cleanup, label
cleanup, quarantine, or explicit docs-only debt. It should still avoid claiming
suite health unless the relevant tests are actually run and pass.
