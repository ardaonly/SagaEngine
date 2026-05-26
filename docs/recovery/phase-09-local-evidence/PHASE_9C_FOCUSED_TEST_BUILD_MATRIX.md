# Phase 9C Focused Test Build Matrix

> Last updated: 2026-05-26
> Status: Phase 9C focused build and test evidence
> Phase 9: Local Evidence Gates

Phase 9C built the twelve Phase 9B registered-but-unbuilt focused targets in
`build/RelWithDebInfo-0.0.9`, then ran only those CTest entries.

Full CTest remains unverified until Phase 9D.

## Scope

This slice verifies on-demand target buildability and focused test execution
for the twelve entries classified in Phase 9B. It does not:

- change source, CMake, test registration, labels, quarantine, or Forge
- claim full CTest health
- claim release, stress, load, performance, or product readiness
- alter suite definitions or CI behavior

## Build Matrix

Each target was built individually with:

`Tools/Forge/bin/forge nix build --target <target> --build=build/RelWithDebInfo-0.0.9 --jobs=1`

| Target | Result | Executable produced |
|---|---|---|
| `ActorOwnershipRegistryTests` | Passed | `build/RelWithDebInfo-0.0.9/bin/ActorOwnershipRegistryTests` |
| `AuthoritativeMovementCoreTests` | Passed | `build/RelWithDebInfo-0.0.9/bin/AuthoritativeMovementCoreTests` |
| `AuthoritativeMovementInputAdapterTests` | Passed | `build/RelWithDebInfo-0.0.9/bin/AuthoritativeMovementInputAdapterTests` |
| `AuthoritativeMovementCommandIntakeTests` | Passed | `build/RelWithDebInfo-0.0.9/bin/AuthoritativeMovementCommandIntakeTests` |
| `ServerPacketNormalizationTests` | Passed | `build/RelWithDebInfo-0.0.9/bin/ServerPacketNormalizationTests` |
| `ZoneServerPacketDrainTests` | Passed | `build/RelWithDebInfo-0.0.9/bin/ZoneServerPacketDrainTests` |
| `ZoneServerMovementAuthorityTests` | Passed | `build/RelWithDebInfo-0.0.9/bin/ZoneServerMovementAuthorityTests` |
| `MovementDirtyReplicationBridgeTests` | Passed | `build/RelWithDebInfo-0.0.9/bin/MovementDirtyReplicationBridgeTests` |
| `RuntimeStartupDiagnosticsTests` | Passed | `build/RelWithDebInfo-0.0.9/bin/RuntimeStartupDiagnosticsTests` |
| `RuntimeServiceLifecycleTests` | Passed | `build/RelWithDebInfo-0.0.9/bin/RuntimeServiceLifecycleTests` |
| `RuntimeServiceRegistryTests` | Passed | `build/RelWithDebInfo-0.0.9/bin/RuntimeServiceRegistryTests` |
| `RuntimeServiceRegistryDiagnosticsTests` | Passed | `build/RelWithDebInfo-0.0.9/bin/RuntimeServiceRegistryDiagnosticsTests` |

## Focused CTest Result

Command:

```sh
ctest --test-dir build/RelWithDebInfo-0.0.9 -R "ActorOwnershipRegistryTests|AuthoritativeMovementCoreTests|AuthoritativeMovementInputAdapterTests|AuthoritativeMovementCommandIntakeTests|ServerPacketNormalizationTests|ZoneServerPacketDrainTests|ZoneServerMovementAuthorityTests|MovementDirtyReplicationBridgeTests|RuntimeStartupDiagnosticsTests|RuntimeServiceLifecycleTests|RuntimeServiceRegistryTests|RuntimeServiceRegistryDiagnosticsTests" --output-on-failure -j 1
```

Result:

- 12 tests passed
- 0 tests failed
- label summary covered `unit`, `server`, `networking`, `replication`, and
  `runtime`

## Inventory After Build

After the focused builds:

- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N` lists all 38 entries
  without missing-executable warnings.
- `runtime` lists 23 entries without missing-executable warnings.
- `server` lists 10 entries without missing-executable warnings.
- `networking` lists 5 entries without missing-executable warnings.
- `replication` lists 4 entries without missing-executable warnings.
- Remaining non-runnable registered entries: none observed in the current build
  tree.

## Evidence Limits

This slice establishes that the twelve focused entries can be built on demand
and pass as a focused CTest set in the current build tree. It does not prove:

- full CTest health
- slow, stress, load, timing-sensitive, or long-running readiness
- release or package/publish readiness
- CI readiness
- broad `UnitTests`, `IntegrationTests`, `ReplicationTests`, or `StressTests`
  behavior

## Phase 9D Readiness

Phase 9D is safe to attempt because the Phase 9B missing executable condition
is resolved in the current build tree and the focused 12-test CTest set passed.

## Verification

- twelve individual `Tools/Forge/bin/forge nix build --target ... --jobs=1`
  commands
- focused 12-entry CTest regex with `--output-on-failure -j 1`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L runtime`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L server`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L networking`
- `ctest --test-dir build/RelWithDebInfo-0.0.9 -N -L replication`
- `find build/RelWithDebInfo-0.0.9/bin -maxdepth 1 -type f -printf '%f\n'`
- `git diff --check`
- touched-doc trailing whitespace scan
- targeted `rg` for Phase 9C wording

Full CTest remains unverified until Phase 9D.
