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

Use the repository-owned quick suite first:

```sh
nix-shell --run "python3 Tools/Developer/CI/run_suite.py quick"
```

CMake/CTest owns test membership, labels, timeouts, and aggregate build
prerequisites. The Python runner reads the configured catalog, builds the
aggregate target with two jobs, runs the selected CTest label, and preserves a
JSON execution manifest plus JUnit output. It is orchestration, not a second
test registry.

Full local CTest is not the normal developer validation path. Do not run stress,
broad GPU, or environment-sensitive UI work by default. A focused result must
never be reported as complete-repository evidence.

`TD-CI-001 — Complete Registered CTest Evidence in GitHub Actions` remains open. The required future CI job must dynamically enumerate the complete registered CTest inventory, run it with bounded parallelism, publish JUnit and failure artifacts, expose disabled and skipped tests, and fail normally without `continue-on-error`. Until that evidence exists, focused local suites are reported as focused evidence and Gate C is not closed by inference.

Documentation-only Wiki changes use:

```sh
scripts/wiki build
scripts/wiki verify
git diff --check
```

Source and build changes normally add configure/build, boundary checks, and focused CTest commands appropriate to the owner.

## Current local suite map

| Suite | What it proves | Display | GPU | Private runtime | Normal cost |
| --- | --- | --- | --- | --- | --- |
| `quick` | Policy, licensing, architecture, selected native modules, and managed CLI contracts | No | No | Contract checks; developer SDK needed | Bounded; not yet runner-timed as an SLA |
| `native-fast` | Display-independent Runtime and non-Qt Editor module behavior | No | No | No | Focused native build/test |
| `managed-tools` | Freshly staged SagaProjectKit, SagaScript, and SagaPackager startup plus private-runtime boundary rules | No | No | Staged closure; developer SDK host used for smoke | Focused managed validation |
| `architecture` | Repository ownership and dependency boundaries | No | No | No | Fast/focused |
| `headless` | Deterministic Runtime and headless sample paths | No | No | No | Focused product contract |
| `product-xcb` | Qt/XCB product workflow under an explicit display | Yes | No | Yes | Environment-sensitive |
| `standalone` | Snapshot, staging, verification, recoverable publication contracts | No | No | Yes | Heavier and serial |

List the configured catalog with:

```sh
python3 Tools/Developer/CI/run_suite.py --list
```

Benchmarks, performance, stress, soak, and current failure-injection cases
remain separately labeled evidence; they are not silently folded into quick.
GPU validation remains opt-in and is not represented by an available local
suite until a qualified environment contract is implemented.

The complete managed CLI drivers remain registered diagnostic tests, but are
not members of a safe suite while they depend on mutable checked-in sample
generation state. They must be converted to isolated generated fixtures before
promotion; their absence from `quick` is not reported as a pass.

## GitHub CI status

Automated GitHub test execution is currently disabled. Existing tests remain in
the repository, and tool-mirror export automation is not test CI. Local focused
validation is useful before professional CI returns, but it is not future
GitHub CI and does not provide clean-runner proof.

Future workflows must be thin setup/invocation/upload orchestration over these
repository-owned suites. PR/main/nightly timing figures in the testing roadmap
are design targets until measured on qualified runners. Sanitizer, coverage,
fuzzing, stress, benchmark, and qualified GPU workflows remain future work.

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
