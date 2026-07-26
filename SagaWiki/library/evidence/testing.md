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

`TD-CI-001 — Complete Registered CTest Evidence in GitHub Actions` remains open. The required future CI job must dynamically enumerate the complete registered CTest inventory, run it with bounded parallelism, publish JUnit and failure artifacts, expose disabled and skipped tests, and fail normally without `continue-on-error`. Until that evidence exists, focused local suites are reported as focused evidence and Gate C is not closed by inference.

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

StarterArena visible-runtime cases are registered as focused,
environment-qualified evidence. They require explicit opt-in and a qualified
native display/GPU. Their named frames bind session, canonical launch, input and
UI artifact hashes, prompt semantics, door state, and HUD-region pixels to the
same frame. The region is the floor/ceil physical-pixel conversion of RmlUi's
border bounds, never a fixed-percentage estimate. Each semantic record carries
its own pre-overlay capture and post-overlay capture, frame identity, pixel
ratio, changed-pixel count, RGB absolute aggregate, and anchor measurements.
Missing pairs, cross-frame baselines, invalid DPI conversion, or anchoring beyond
two physical pixels fail closed. They do not replace the complete registered
CTest inventory required by `TD-CI-001`.

## Registration, execution, and gate status

A **registered test** is discoverable in the CTest graph. A **focused
execution** records only the selected tests actually run. **Graphical proof** is
focused execution on a qualified display/backend/GPU with required named
captures and a `passed` result; `executed`, a skipped test, or one arbitrary
screenshot is not equivalent. **Full CTest evidence** dynamically enumerates
and runs the complete registered inventory with skip/disabled accounting. A
**public gate closure** additionally requires the gate's documented
prerequisites; it is never inferred from a focused local pass.

Current implementation and gate record:

| Item | Status |
| --- | --- |
| Accepted foundation | closed |
| Editor/runtime implementation | implemented; bounded viewport lifetime and source non-mutation have focused evidence |
| Repository hygiene | closed |
| Current graphical acceptance | passed in the 2026-07-26 focused local E/F Vulkan run; same-frame RmlUi ROI/RGB/anchoring runner status `passed` |
| Gate C | open under `TD-CI-001` |
| Gate C-UI | open pending prerequisite full evidence |
| Gate D | open |
| Gate E | open |

The earlier graphical execution record remains historical execution only and
does not satisfy the current acceptance contract. The current focused result is
the separately retained E/F run and independently recomputed runner report
under `.saga-private/evidence/closure-acceptance-20260726`. This focused local
pass does not claim full CTest completion, a trusted hardware-GPU run, or public
gate closure; the complete vertical slice is therefore not claimed as publicly
proven.

GPU execution remains a manual, hardware-qualified boundary. With no trusted self-hosted `saga-gpu` runner, the preflight produces an explicit blocked report and intentionally does not claim success or execute GPU binaries. A hosted runner or a successful GPU-target build is not GPU evidence.

## Bounded chaos evidence

SagaChaosLab is an internal developer tool under DistributionCheck. It loads a deterministic, bounded chaos profile and routes it through the stress arena. Invalid probabilities, unsupported fields, unbounded queues, and oversized profiles fail validation. Heavy or manual-only profiles require explicit opt-in. Its smoke result is fault-injection evidence, not a production load or long-soak claim.

## Evidence integrity

Evidence should record producer, command or scenario, input identity, environment where relevant, status, and outputs. Missing manual input is `incomplete`, not success. Screenshots and reports support a stated observation; they do not generalize beyond it.

Source-text scanning is a secondary guardrail, not sufficient behavior evidence
for a critical architecture boundary. Select the appropriate combination of
target-link graph inspection, compile/link tests, executable behavior,
invalid-input rejection, and source scanning. The stronger evidence should fail
when the prohibited dependency or behavior is actually introduced, not merely
when a comment or spelling changes.

See [Testing, claims, and evidence](../reference/testing-claims-and-evidence.md) for detailed category, determinism, GPU/private, sample, chaos, artifact, and reporting rules.
