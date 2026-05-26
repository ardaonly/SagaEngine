# Phase 8C Test Evidence Mapping

> Last updated: 2026-05-26
> Status: Phase 8C docs/report-only test evidence map
> Phase 8: Documentation / Code Alignment Enforcement

Phase 8C maps major recovery claims to focused tests, guards, builds, and
label inventories. It does not add tests, change labels, change CMake, or
claim full CTest health.

Full CTest remains unverified.

## Evidence Classification

- `direct evidence`: focused tests or guards exercise the claim directly.
- `partial evidence`: focused tests cover a foundation slice but not the full
  product claim.
- `coarse evidence`: broad test targets or labels exist but do not isolate the
  ownership boundary.
- `label inventory`: CTest lists matching labels only; this is not pass/fail
  execution evidence.
- `no evidence`: no focused test or guard was found for the claim.

## Claim To Evidence Matrix

| Claim family | Evidence classification | Evidence | Gaps |
|---|---|---|---|
| Runtime startup/lifecycle foundation | partial evidence | Runtime startup/preflight/session/diagnostics and service lifecycle/registry focused test entries exist. | Some registered focused runtime executables are not present in the current build tree during list-only inventory; complete Runtime/App ownership remains unproven. |
| Runtime package asset bootstrap/access | direct evidence for bounded contracts | `RuntimeAssetBootstrapTests`, `RuntimeAssetBootstrapDiagnosticsTests`, `RuntimeAssetStartupBootstrapTests`, `RuntimeAssetCatalogTests`, and `RuntimePackageSmokeTests` are registered. | ClientHost consumption and RuntimeServiceRegistry asset service are not covered. |
| Server authoritative movement foundation | partial evidence | Actor ownership, authoritative movement core/input/command intake, packet normalization, ZoneServer packet/movement, and dirty replication bridge entries are registered. | Several focused server executables are not present in the current build tree during list-only inventory; full snapshot/client reconciliation is not covered. |
| Package / asset / Runtime readiness foundation | partial evidence | Asset identity, asset manifest, package manifest, generated RuntimeSmoke manifest/package, package staging, runtime bootstrap, and publish readiness entries are registered. | Source import/cook, UI/document assets, package distribution, and publish hard gates are not covered. |
| Product publish visibility | partial evidence | `SagaPublishReadinessTests` and `SagaPackageStagingTests` are registered under `product` and package/asset labels. | Publish hard-fail parity with Runtime startup remains unproven. |
| Editor public API de-Qtification checkpoint | direct evidence for guard baseline | `EditorQtPublicAbiBoundaryTests` and `ArchitectureTests` are registered under `architecture`; the focused ABI guard passed in Phase 7E/8B verification. | Full public Qt removal is not covered because `GraphCanvas.h` and `QtPanel.h` remain allowlisted debt. |
| Editor scaffolding quarantine foundation | partial evidence | `ProjectManagerTests.*` passed through the `SagaUnitTests` binary in Phase 7E verification; `editor` label inventory lists `UnitTests` and `EditorQtPublicAbiBoundaryTests`. | Editor workflow readiness, project browser, dashboard rows, visual scripting, and collaboration workflows are not covered. |
| CMake/source/test ownership | coarse evidence | `ArchitectureTests`, `EditorQtPublicAbiBoundaryTests`, and CMake boundary inventory exist. | Broad `SAGA_TEST_INCLUDE_DIRS`, coarse `SagaUnitTests`, recursive globs, and broad target links limit ownership proof. |
| Full test health | no evidence | No full CTest run was performed in Phase 8C. | Full CTest remains unverified. |

## CTest Label Inventory

List-only inventory from `build/RelWithDebInfo-0.0.9`:

| Label/query | Listed tests | Notes |
|---|---:|---|
| all CTest entries | 38 | List-only; not executed. |
| `architecture` | 2 | `ArchitectureTests`, `EditorQtPublicAbiBoundaryTests`. |
| `runtime` | 23 | List-only warnings reported for some registered-but-unbuilt focused executables. |
| `server` | 10 | List-only warnings reported for some registered-but-unbuilt focused executables. |
| `asset` | 14 | Includes package/runtime asset readiness entries. |
| `package` | 12 | Includes staging, publish, generated package, and runtime package entries. |
| `editor` | 2 | Coarse `UnitTests` plus focused Qt ABI guard. |
| `product` | 2 | `SagaPackageStagingTests`, `SagaPublishReadinessTests`. |
| `integration` | 3 | `SagaPipelineTests`, `IntegrationTests`, `ReplicationTests`. |
| `replication` | 4 | Includes coarse and focused replication-related entries. |
| `collaboration` | 0 | No label evidence for collaboration product readiness. |
| `ui` | 0 | No label evidence for UI polish/product readiness. |

The list-only warnings do not fail the inventory command, but they are evidence
that label presence alone is not executable/pass evidence in this build tree.

## Non-Goals

Phase 8C does not:

- add, remove, rename, or relabel tests
- change CMake or build targets
- run full CTest
- claim suite health from label inventory
- harden architecture guards
- implement missing coverage
- change product/runtime/editor behavior

## Verification

Verification completed for this report:

- `git diff --check`
- touched-doc trailing whitespace check
- targeted `rg` for Phase 8C wording, evidence classifications, label
  inventory, no-evidence claims, non-goals, and full CTest caveats
- CTest list-only inventory for all entries and labels: `architecture`,
  `runtime`, `server`, `asset`, `package`, `editor`, `product`,
  `integration`, `replication`, `collaboration`, and `ui`

Full CTest remains unverified.
