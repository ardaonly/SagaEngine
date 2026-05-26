# Phase 9B Registered-But-Unbuilt Classification

> Last updated: 2026-05-26
> Status: Phase 9B docs/report-only classification
> Phase 9: Local Evidence Gates

Phase 9B classifies the twelve CTest entries that Phase 9A found registered
but not runnable in `build/RelWithDebInfo-0.0.9` because their executables were
absent from the build tree.

Full CTest remains unverified.

## Scope

This slice records local classification evidence only. It does not:

- build the missing targets
- run full CTest
- run a focused CTest set
- change source, CMake, test registration, labels, quarantine, or Forge
- claim suite health from registration, labels, source presence, or target
  presence

## Classification Summary

All twelve entries are real focused targets, registered with CTest, backed by
source files, and absent from `build/RelWithDebInfo-0.0.9/bin` at the start of
Phase 9B. The root cause category is:

`CMake target exists but has not been built on demand in this build tree.`

They are not classified as stale registrations because each entry has matching
source, `add_executable`, `add_test`, and label registration in
`cmake/modules/SagaTests.cmake`.

## Entry Matrix

Expected executable paths use
`build/RelWithDebInfo-0.0.9/bin/<CTest name>`.

| CTest name | Expected executable | CMake target | Source file | Subsystem | Labels | Target/source registration | Current executable presence | Stale? | Direct evidence value | Phase 9C action |
|---|---|---|---|---|---|---|---|---|---|---|
| `ActorOwnershipRegistryTests` | `build/RelWithDebInfo-0.0.9/bin/ActorOwnershipRegistryTests` | `ActorOwnershipRegistryTests` | `Tests/Unit/Server/ActorOwnershipRegistryTests.cpp` | Server actor ownership | `unit;server` | Present: source variable, `add_executable`, `add_test`, labels | Absent | No | Focused server evidence only after build and pass | Build target, then run focused CTest |
| `AuthoritativeMovementCoreTests` | `build/RelWithDebInfo-0.0.9/bin/AuthoritativeMovementCoreTests` | `AuthoritativeMovementCoreTests` | `Tests/Unit/Server/AuthoritativeMovementCoreTests.cpp` | Server authoritative movement core | `unit;server` | Present: source variable, `add_executable`, `add_test`, labels | Absent | No | Focused movement core evidence only after build and pass | Build target, then run focused CTest |
| `AuthoritativeMovementInputAdapterTests` | `build/RelWithDebInfo-0.0.9/bin/AuthoritativeMovementInputAdapterTests` | `AuthoritativeMovementInputAdapterTests` | `Tests/Unit/Server/AuthoritativeMovementInputAdapterTests.cpp` | Server authoritative movement input adapter | `unit;server` | Present: source variable, `add_executable`, `add_test`, labels | Absent | No | Focused input adapter evidence only after build and pass | Build target, then run focused CTest |
| `AuthoritativeMovementCommandIntakeTests` | `build/RelWithDebInfo-0.0.9/bin/AuthoritativeMovementCommandIntakeTests` | `AuthoritativeMovementCommandIntakeTests` | `Tests/Unit/Server/AuthoritativeMovementCommandIntakeTests.cpp` | Server authoritative movement command intake | `unit;server` | Present: source variable, `add_executable`, `add_test`, labels | Absent | No | Focused command intake evidence only after build and pass | Build target, then run focused CTest |
| `ServerPacketNormalizationTests` | `build/RelWithDebInfo-0.0.9/bin/ServerPacketNormalizationTests` | `ServerPacketNormalizationTests` | `Tests/Unit/Networking/ServerPacketNormalizationTests.cpp` | Server networking packet normalization | `unit;networking;server` | Present: source variable, `add_executable`, `add_test`, labels | Absent | No | Focused networking evidence only after build and pass | Build target, then run focused CTest |
| `ZoneServerPacketDrainTests` | `build/RelWithDebInfo-0.0.9/bin/ZoneServerPacketDrainTests` | `ZoneServerPacketDrainTests` | `Tests/Unit/Server/ZoneServerPacketDrainTests.cpp` | ZoneServer packet drain | `unit;server;networking` | Present: source variable, `add_executable`, `add_test`, labels | Absent | No | Focused ZoneServer drain evidence only after build and pass | Build target, then run focused CTest |
| `ZoneServerMovementAuthorityTests` | `build/RelWithDebInfo-0.0.9/bin/ZoneServerMovementAuthorityTests` | `ZoneServerMovementAuthorityTests` | `Tests/Unit/Server/ZoneServerMovementAuthorityTests.cpp` | ZoneServer movement authority | `unit;server;networking` | Present: source variable, `add_executable`, `add_test`, labels | Absent | No | Focused ZoneServer movement authority evidence only after build and pass | Build target, then run focused CTest |
| `MovementDirtyReplicationBridgeTests` | `build/RelWithDebInfo-0.0.9/bin/MovementDirtyReplicationBridgeTests` | `MovementDirtyReplicationBridgeTests` | `Tests/Unit/Server/MovementDirtyReplicationBridgeTests.cpp` | Server replication bridge | `unit;server;replication` | Present: source variable, `add_executable`, `add_test`, labels | Absent | No | Focused movement replication evidence only after build and pass | Build target, then run focused CTest |
| `RuntimeStartupDiagnosticsTests` | `build/RelWithDebInfo-0.0.9/bin/RuntimeStartupDiagnosticsTests` | `RuntimeStartupDiagnosticsTests` | `Tests/Unit/Runtime/RuntimeStartupDiagnosticsTests.cpp` | Runtime startup diagnostics | `unit;runtime` | Present: source variable, `add_executable`, `add_test`, labels | Absent | No | Focused runtime startup diagnostics evidence only after build and pass | Build target, then run focused CTest |
| `RuntimeServiceLifecycleTests` | `build/RelWithDebInfo-0.0.9/bin/RuntimeServiceLifecycleTests` | `RuntimeServiceLifecycleTests` | `Tests/Unit/Runtime/RuntimeServiceLifecycleTests.cpp` | Runtime service lifecycle | `unit;runtime` | Present: source variable, `add_executable`, `add_test`, labels | Absent | No | Focused runtime service lifecycle evidence only after build and pass | Build target, then run focused CTest |
| `RuntimeServiceRegistryTests` | `build/RelWithDebInfo-0.0.9/bin/RuntimeServiceRegistryTests` | `RuntimeServiceRegistryTests` | `Tests/Unit/Runtime/RuntimeServiceRegistryTests.cpp` | Runtime service registry | `unit;runtime` | Present: source variable, `add_executable`, `add_test`, labels | Absent | No | Focused runtime service registry evidence only after build and pass | Build target, then run focused CTest |
| `RuntimeServiceRegistryDiagnosticsTests` | `build/RelWithDebInfo-0.0.9/bin/RuntimeServiceRegistryDiagnosticsTests` | `RuntimeServiceRegistryDiagnosticsTests` | `Tests/Unit/Runtime/RuntimeServiceRegistryDiagnosticsTests.cpp` | Runtime service registry diagnostics | `unit;runtime` | Present: source variable, `add_executable`, `add_test`, labels | Absent | No | Focused runtime registry diagnostics evidence only after build and pass | Build target, then run focused CTest |

## Evidence Notes

- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N` still reports 38
  registered entries and warns for the twelve absent executables above.
- Label list commands include these entries because registration exists, but
  label presence is not pass evidence.
- The source and CMake checks support on-demand build verification as the next
  step; they do not prove test pass health.
- `ui` and `collaboration` remain zero-test labels in the current build tree.
- `all` remains a Phase 9 inventory term for all registered entries, not a raw
  CTest label in this build tree.

## Phase 9C Readiness

Phase 9C is safe to attempt if the build tree remains valid. The next action is
to build each of the twelve focused targets individually, then run the focused
CTest regex covering only those entries.

## Verification

List/report checks only:

- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L runtime`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L server`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L networking`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L replication`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L architecture`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L editor`
- `rg -n "ActorOwnershipRegistryTests|AuthoritativeMovementCoreTests|AuthoritativeMovementInputAdapterTests|AuthoritativeMovementCommandIntakeTests|ServerPacketNormalizationTests|ZoneServerPacketDrainTests|ZoneServerMovementAuthorityTests|MovementDirtyReplicationBridgeTests|RuntimeStartupDiagnosticsTests|RuntimeServiceLifecycleTests|RuntimeServiceRegistryTests|RuntimeServiceRegistryDiagnosticsTests|add_executable|add_test|set_tests_properties" cmake/modules/SagaTests.cmake`
- `rg --files Tests/Unit Server Runtime | rg "ActorOwnershipRegistryTests|AuthoritativeMovementCoreTests|AuthoritativeMovementInputAdapterTests|AuthoritativeMovementCommandIntakeTests|ServerPacketNormalizationTests|ZoneServerPacketDrainTests|ZoneServerMovementAuthorityTests|MovementDirtyReplicationBridgeTests|RuntimeStartupDiagnosticsTests|RuntimeServiceLifecycleTests|RuntimeServiceRegistryTests|RuntimeServiceRegistryDiagnosticsTests|ActorOwnershipRegistry|AuthoritativeMovement|ServerPacketNormalizer|MovementDirtyReplicationBridge|RuntimeStartupDiagnostics|RuntimeServiceLifecycle|RuntimeServiceRegistry"`
- `git diff --check`
- touched-doc trailing whitespace scan
- targeted `rg` for Phase 9B wording

Full CTest remains unverified.
