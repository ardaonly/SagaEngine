---
title: Testing and evidence
status: policy
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Testing and evidence

Tests answer different questions. A green test is meaningful only when its category, inputs, environment, and asserted boundary match the claim being made.

| Category | What it can establish |
| --- | --- |
| Unit test | A focused type or module behavior under controlled inputs. |
| Architecture test | Repository ownership, dependency, include, naming, or ABI invariants. |
| Installed consumer test | A public installed/exported contract can be consumed without private paths. |
| White-box private test | Internal behavior; never public API stability by itself. |
| Integration test | Interaction among named modules under the tested configuration. |
| GPU test | A graphics/backend path on the executing hardware and driver. |
| Sample evidence | A named sample target builds or runs through the asserted workflow. |
| Stress test | Behavior under a configured load; isolated from ordinary local acceptance. |

## Public and private graphics evidence

Installed graphics consumers must include only public installed headers. `SagaDiligentWhiteboxTests` owns CPU/private RHI and Render evidence colocated under the owning modules and is registered as `DiligentWhiteboxTests` with the `whitebox-private` label. `SagaDiligentGpuIntegrationTests` is a separately buildable native GPU target; its CTest entries are registered only when `SAGA_ENABLE_GPU_TEST_EXECUTION=ON`. Building that target is not a GPU test pass, and GPU execution may be unsuitable for a routine laptop acceptance run.

## Safe local acceptance

Start with the smallest focused target and test regex that covers the change. Use low parallelism where resource pressure is a concern. Do not run StressTests or a broad GPU suite by default. A full CTest result must never be implied when only focused tests ran.

Documentation-only Wiki changes use:

```sh
scripts/wiki build
scripts/wiki verify
git diff --check
```

Source and build changes normally add configure/build, boundary checks, and focused CTest commands appropriate to the owner.

## GitHub evidence tiers

Every pull request runs repository contracts, strict licensing, Linux `all-safe` CTest, and the Windows unit/architecture layer. The Linux safe suite explicitly excludes `stress`, `slow`, `load`, `timing-sensitive`, `long-running`, `gpu`, and `display-required` labels. That exclusion is part of the workflow contract and cannot be silently widened or narrowed. Standalone Forge and SagaTools contract suites run in the repository-contract job rather than being inferred from the engine build.

Configured CMake licensing evidence runs when source, build, test, or licensing inputs change and once every night. It records File API target ownership, install-surface comparison, the installed SDK consumer, and a machine-readable licensing report. Integration and replication run nightly; StressTests run serially each week.

Failures are not hidden with `continue-on-error`. JUnit, JSON reports, and CTest logs upload with `if: always()` and each job writes a summary containing its exercised and excluded boundaries. Pull-request artifacts remain for 14 days, nightly results for 30 days, and licensing/weekly evidence for 90 days.

Visible runtime evidence is also opt-in: `SAGA_RUN_VISIBLE_TESTS=1` must accompany a qualified native display. Otherwise the visible StarterArena cases report an explicit skip while the headless cases in the same executable continue to run.

GPU execution remains a manual, hardware-qualified boundary. With no trusted self-hosted `saga-gpu` runner, the preflight produces an explicit blocked report and intentionally does not claim success or execute GPU binaries. A hosted runner or a successful GPU-target build is not GPU evidence.

## Bounded chaos evidence

SagaChaosLab is an internal developer tool under DistributionCheck. It loads a deterministic, bounded chaos profile and routes it through the stress arena. Invalid probabilities, unsupported fields, unbounded queues, and oversized profiles fail validation. Heavy or manual-only profiles require explicit opt-in. Its smoke result is fault-injection evidence, not a production load or long-soak claim.

## Evidence integrity

Evidence should record producer, command or scenario, input identity, environment where relevant, status, and outputs. Missing manual input is `incomplete`, not success. Screenshots and reports support a stated observation; they do not generalize beyond it.

See [Testing, claims, and evidence](../reference/testing-claims-and-evidence.md) for detailed category, determinism, GPU/private, sample, chaos, artifact, and reporting rules.
