---
title: Sample executable evidence
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Sample executable evidence

Samples are project-owned examples and evidence fixtures. A sample name alone does not prove that its executable target builds, launches, consumes the intended project, or exercises the claimed runtime path.

## Current evidence mechanism

`cmake/modules/SagaSampleEvidence.cmake` records sample-owned executable declarations and validates their relationship to sample paths and targets. ClaimCheck provides repository-level checks for declared product/evidence claims. These mechanisms should be read against current target ownership, not the names used by pre-cutover aggregate targets.

The current sample-owned operational instructions remain beside their source:

| Sample | Current role | Command authority |
| --- | --- | --- |
| `Samples/StarterArena` | Project validation, headless/visible bounded runtime, synthetic or explicit keyboard input, focused C# lifecycle/gameplay, and graphics evidence. It is not a complete game. | `Samples/StarterArena/README.md` |
| `Samples/MultiplayerSandbox` | Deterministic project, scripting, package, validation, and headless-integration fixture. It is not a finished playable multiplayer sample. | `Samples/MultiplayerSandbox/README.md` |

SagaWiki explains what those results can mean; the sample README owns volatile command lines and fixture-specific limitations. Duplicating every current switch here would create two operational sources of truth.

## Evidence levels

1. **Declared** — source and project inputs exist and are registered.
2. **Build evidence** — the named executable target builds from its intended sources.
3. **Launch evidence** — the executable starts under a controlled scenario.
4. **Behavior evidence** — focused assertions or captured results prove the named interaction.
5. **Manual evidence** — required human/device observation is recorded and validated.

Each level is narrower than a product-readiness claim. A smoke run cannot substitute for interaction evidence, and an automated result cannot silently satisfy a required physical-device observation.

## Maintenance rule

Keep sample identity, target ownership, and evidence descriptors close enough that a rename or removal fails validation. Do not embed absolute local paths or retain one-off evidence bundles as canonical Wiki content.

The full claim vocabulary and suite/report rules are in [Testing, claims, and evidence](../reference/testing-claims-and-evidence.md).
