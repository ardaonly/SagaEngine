---
title: SagaEngine at a glance
status: current
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# SagaEngine at a glance

SagaEngine is an active game-engine and authoring-toolchain codebase. Its current shape combines explicit C++ module ownership, authoritative simulation foundations, project and package contracts, C# authoring, rendering infrastructure, editor foundations, and evidence-oriented verification.

The repository is not evidence of a finished public SDK or a complete editor product. A class, executable target, or test fixture proves only the boundary exercised by that artifact. Start with [non-claims](../governance/non-claims.md) whenever a capability name could be mistaken for a shipped product promise.

## How to use this wiki

SagaWiki is a curated knowledge base, not a mirror of every design note ever written. Its Markdown files are the reviewable source. The generated reader adds navigation, search, status labels, and source inspection without changing that source of truth.

- **Current** pages describe stable repository shape or an implemented contract.
- **Bounded** pages join implemented facts to explicit evidence limits.
- **Guide** pages explain a supported maintenance or development workflow.
- **Policy** pages govern how claims, documentation, and licensing are interpreted.

## Product shape

SagaEngine provides a general-purpose engine surface whose practical depth comes from project-owned content, scripts, packages, and samples. The durable direction is creator-driven rather than genre-specific: runtime and toolchain contracts should support distinct projects without baking one public product identity into the engine.

The small public tool surface is `sagaproject`, `sagascript`, and `sagapack`, with SagaTools as a stable dispatcher. Developer checks under `Tools/Developer` remain internal unless promoted deliberately.

## Read next

1. [Getting started](getting-started.md)
2. [Repository map](repository-map.md)
3. [Programs and local workflows](programs-and-workflows.md)
4. [Ownership and dependencies](../architecture/ownership-and-dependencies.md)
5. [Source of truth](../architecture/source-of-truth.md)
6. [Testing and evidence](../evidence/testing.md)
7. [Detailed technical references](../reference/product-and-capability-boundaries.md)
