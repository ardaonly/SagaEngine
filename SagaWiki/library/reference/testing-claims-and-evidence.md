---
title: Testing, claims, and evidence
status: policy
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Testing, claims, and evidence

Testing is a set of scoped proofs, not one green/red label for the repository. Every engineering or product statement should name the contract, producer, inputs, environment, and assertion that justify it. This reference preserves the durable evidence policy, suite taxonomy, sample rules, and bounded chaos rules from 0.0.10 in the current 0.0.11 layout.

## Claim vocabulary

Use language that reveals evidence level:

- **implemented** means code for the named behavior exists in the current owner;
- **unit-tested** means focused deterministic tests exercise the named behavior;
- **architecture-guarded** means repository/dependency/include/install structure is checked;
- **installed-consumer-tested** means a downstream project consumes the installed public contract;
- **integration-tested** means named owners operate together in the tested fixture;
- **GPU-verified** means a native graphics path produced the asserted result on the recorded environment;
- **sample-evidenced** means a named sample builds or runs through a stated bounded workflow;
- **manually observed** means a person performed and recorded a scenario;
- **planned**, **experimental**, **transitional**, or **not implemented** states limits explicitly.

Avoid “supported,” “production-ready,” “complete,” “shipped,” or “verified” without the object and evidence. A type, CMake target, CLI flag, test fixture, screenshot, or design document alone is not an end-to-end capability.

## Claim-to-evidence rule

The scope of a claim cannot exceed the intersection of:

```text
asserted behavior
  ∩ tested configuration
  ∩ tested input
  ∩ tested platform/environment
  ∩ observed output
```

A parser test proves accepted/rejected records, not runtime readiness. A headless runtime smoke proves a bounded startup/tick/shutdown path, not interactive gameplay. A loopback client/server test proves that fixture, not Internet deployment or security. A Vulkan pixel test proves that backend/driver/hardware path, not universal backend parity.

Negative tests matter. Rejecting stale hashes, unsafe paths, private includes, invalid sequences, stale handles, missing bindings, and malformed package inputs is part of the contract, not optional edge coverage.

## Test categories

### Unit tests

Unit tests isolate a type/module behavior with deterministic inputs. They prove state transitions, parsers, validation, math, scheduling policy, serialization, failure classes, and ownership invariants that do not require a real external environment.

Good unit tests inject clocks, sources, transports, allocators, or backends instead of sleeping or using global machine state. A unit test that initializes a real GPU/database/network service is misclassified and makes local acceptance less predictable.

### Architecture tests

Architecture tests inspect the repository and build declarations. They protect current module roots, Public/Private include direction, vendor leakage, Qt ownership, server-authority ownership, Diligent lifecycle ownership, target dependency rules, retired path/name absence, and other cutover invariants.

These tests can prove that no public header names a private/vendor type or that a server target does not link editor/UI backends. They cannot prove the implementation behaves correctly at runtime.

Architecture checks should parse real include/type/CMake patterns rather than flag comments or documentation words. Exceptions are narrow and named, such as Qt inside EditorQt or diagnostic capture idle waits.

### Installed consumer tests

An installed consumer configures/builds against the produced install/export surface, not the source tree’s convenient include paths. It includes only installed public headers and links only exported targets. This exposes missing transitive public dependencies, private include leaks, uninstalled headers, and incorrect target exports.

`SagaGraphicsInstalledConsumerTest` is evidence for the public graphics install contract. It does not authorize a private Diligent include and does not prove native GPU rendering.

### White-box private tests

White-box tests include private implementation headers through a deliberate test-only target/include path. They can test caches, timelines, frame slots, resource owners, decoders, or internal state machines that should not be installed.

The name/label and target should make this status visible. A private test include never promotes the header to public API. Conversely, public code must not rely on a white-box target to compile.

Private Diligent CPU tests are colocated in `Runtime/RHI/Tests` and `Runtime/Render/Tests`, built by `SagaDiligentWhiteboxTests`, and registered as `DiligentWhiteboxTests` with the `whitebox-private` label. They are not part of `SagaUnitTests` and must not be mixed into installed-consumer claims.

### Integration tests

Integration tests join named owners: manifest-to-registry-to-streaming, transport-to-replication-apply, editor model-to-tool report, or package-to-unpacked smoke. Their fixture and external dependencies are explicit.

An integration test remains bounded. In-memory transport does not prove sockets; a temporary directory does not prove arbitrary filesystem layout; a mock persistence adapter does not prove PostgreSQL/Redis operation.

### GPU tests

GPU integration tests initialize a native window/device/backend and assert creation, frame lifecycle, resource upload, binding, draw, depth, coordinate, texture, lighting, shadow, overlay, resize, lifetime, or pixel behavior. They can skip when no suitable display/device exists, but a skip is not a pass for GPU capability.

Record backend, adapter/GPU, driver, platform/display environment, validation-layer state, and test selection when these affect interpretation. Pixel assertions prefer robust regions/relationships over exact full-frame equality unless exactness is contractual.

GPU tests may be heavy or unstable on a local machine. `SagaDiligentGpuIntegrationTests` is buildable for compile/link evidence, while its CTest entries are opt-in through `SAGA_ENABLE_GPU_TEST_EXECUTION=ON`. It is separate from the general `SagaIntegrationTests` target. Do not infer that a GPU test ran because its executable built.

### Sample evidence

A sample is project-owned executable/content evidence. It demonstrates that a named project can traverse a bounded workflow using current public/declared surfaces. Its manifest, source, expected output, and test target remain under `Samples` plus the appropriate cross-cutting tests.

Sample evidence does not become architecture authority. Engine behavior is not implemented under a sample solely to make the sample pass. A procedural fixture proves the fixture path; an asset-driven project needs separate evidence.

### Stress and soak tests

Stress tests exercise configured load, concurrency, duration, memory pressure, packet faults, or repeated lifecycle. Soak tests add longer duration and leak/stability observation. They are isolated from routine local gates and require explicit opt-in, resource limits, timeout, and report output.

A passing short stress run does not prove production scale. A failed run must leave enough context to reproduce profile, seed, workload, and limits.

## Safe local acceptance

Start with the smallest checks that cover the change. Documentation-only changes use:

```sh
scripts/wiki build
scripts/wiki verify
git diff --check
```

Source/build changes normally add `cmake --preset linux-gcc`, a `-j2` focused build, the relevant BoundaryCheck commands, and named CTest regexes with `--output-on-failure` and low parallelism. Select the build directory generated by the preset rather than copying a stale versioned path from history.

Do not run `StressTests` by default. Do not run a broad GPU or full CTest suite when it can crash/freeze the machine or when focused evidence answers the question. If only focused tests ran, say so explicitly. “All tests pass” is forbidden unless the declared complete set ran and its skips/failures are reported.

## Test naming and labels

A test name identifies the contract and behavior, not a historical release gate. Labels separate architecture, installed consumer, white-box, integration, GPU, sample, stress, and manual/heavy concerns. Retired phase/preview/alpha identities must not return as suite names.

Tests with required external hardware/services declare that requirement. A disabled or skipped test reports why. Hidden environment detection that converts a real failure into a pass weakens evidence.

## Determinism

Deterministic tests control time, randomness, input order, filesystem roots, locale, and environment. Randomized/property tests record the seed on failure. Chaos tests require an explicit seed. Serialization tests define ordering and newline/encoding policy.

Avoid arbitrary sleeps. Use latches, test clocks, explicit pumps, or observable terminal states. If timing itself is the contract, allow bounded tolerance and record the machine-sensitive assumption.

## SagaChaosLab boundary

SagaChaosLab lives under `Tools/Developer/DistributionCheck/ChaosLab`. It validates a bounded profile and executes controlled fault scenarios. A profile can define seed, duration/steps, loss, delay/jitter, duplication/reorder, bounded corruption, resource pressure, or other supported actions.

Validation rejects probabilities outside range, unsupported fields/actions, excessive duration/count/queue, missing seed where determinism is required, and heavy/manual profiles without opt-in. The runner records effective actions, counts, diagnostics, terminal state, and non-claims.

Chaos uses normal validation/failure paths. It does not disable packet bounds or resource invariants to create a dramatic test. Its result proves behavior under the modeled profile and seed, not production resilience.

## Evidence artifacts

A machine-readable report should include:

- schema version, producer, command/scenario/test id;
- repository/build/project/artifact identity as relevant;
- inputs, selected profile, seed, and important environment facts;
- start/end/exit and pass/fail/blocked/incomplete/skipped state;
- assertions/checks and stable diagnostics;
- output paths/hashes where useful;
- known limitations and non-claims.

Evidence is derived output. It is regenerated and normally untracked unless a deliberate fixture/golden contract stores it. Local audit folders, crash reports, package staging trees, and repomix output are not source files.

## Manual evidence

Manual evidence records exact setup, steps, expected observation, actual observation, environment, and date/actor when relevant. A checklist with unchecked steps is incomplete. A screenshot supports only what is visible and can be stale; pair it with source/build identity.

Manual evidence is valuable for interactive editor/render workflows not fully automated. It should not be rewritten as an automated pass.

## Reporting a validation run

A final report lists every command actually run, result, relevant skipped tests, and reasons for omitted heavy suites. It distinguishes build success from test success, focused from full, CPU from GPU, and repository-cleanliness from runtime behavior.

If a command failed due to environment rather than product behavior, report both the failure and the evidence still obtained. Do not erase failed output and rerun a smaller command while presenting only the success.

## Current non-claims

No suite inventory by itself proves a finished public SDK, finished editor, production networking/collaboration, shipped persistent worlds, clean-machine distribution portability, security, scalability, performance, or production readiness. Those need explicit contracts and evidence sets.

## Change checklist

- Name the exact claim before choosing tests.
- Choose the smallest correct evidence category.
- Test rejection and failure paths as first-class behavior.
- Keep installed consumers public-only and white-box tests clearly private.
- Record environment for GPU/external-service evidence.
- Isolate stress/heavy/manual work and use bounded profiles.
- Preserve reports/seeds/diagnostics needed to reproduce failure.
- Say what did not run and never inflate a focused result.
