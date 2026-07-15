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

Installed graphics consumers must include only public installed headers. Diligent white-box tests may include private RHI/Render helpers through explicit test configuration, but should be labeled as internal evidence. GPU integration is separate from both and may be unsuitable for a routine laptop acceptance run.

## Safe local acceptance

Start with the smallest focused target and test regex that covers the change. Use low parallelism where resource pressure is a concern. Do not run StressTests or a broad GPU suite by default. A full CTest result must never be implied when only focused tests ran.

Documentation-only Wiki changes use:

```sh
scripts/wiki build
scripts/wiki verify
git diff --check
```

Source and build changes normally add configure/build, boundary checks, and focused CTest commands appropriate to the owner.

## Bounded chaos evidence

SagaChaosLab is an internal developer tool under DistributionCheck. It loads a deterministic, bounded chaos profile and routes it through the stress arena. Invalid probabilities, unsupported fields, unbounded queues, and oversized profiles fail validation. Heavy or manual-only profiles require explicit opt-in. Its smoke result is fault-injection evidence, not a production load or long-soak claim.

## Evidence integrity

Evidence should record producer, command or scenario, input identity, environment where relevant, status, and outputs. Missing manual input is `incomplete`, not success. Screenshots and reports support a stated observation; they do not generalize beyond it.

See [Testing, claims, and evidence](../reference/testing-claims-and-evidence.md) for detailed category, determinism, GPU/private, sample, chaos, artifact, and reporting rules.
