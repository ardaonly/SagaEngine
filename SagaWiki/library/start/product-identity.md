---
title: Product identity and direction
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Product identity and direction

Saga is an open-source game engine with a creator-native, multiplayer-first product focus. It is intended to support general-purpose game development, including standalone games, without making every project depend on an online service or a hosted platform.

This page describes Saga's product identity and intended distinction. It does not claim that every part of that direction is already available as a complete user workflow. Current implementation and evidence are documented in the [product and capability reference](../reference/product-and-capability-boundaries.md).

## What Saga is being designed for

Saga gives multiplayer and creator-authored experiences a central place in its product direction. The engine is intended to make authority, client/server roles, and self-hosted operation part of the design from the beginning, rather than treating them only as later integrations. Standalone games remain within the intended scope; they should not have to adopt multiplayer concepts they do not need.

The product direction also emphasizes a gradual path from accessible authoring toward more explicit control. Creators should be able to work at a suitable level while developers retain a path to source-level and lower-level systems. The goal is to connect those workflows to the same project and runtime, rather than create disconnected authoring and execution models.

## Intended distinction

Saga's intended distinction is the combination of these priorities:

- creator-oriented authoring with room to move toward deeper technical control;
- multiplayer authority and client/server operation treated as engine concerns;
- creator control over project source and self-hosted operation as a design goal;
- a general-purpose engine scope that still includes standalone games.

These are product priorities, not claims that a complete creator-multiplayer pipeline, production server, or finished editor is available today.

## What the repository demonstrates today

The current codebase contains C++ runtime and editor foundations, C# gameplay analysis and compilation, project and package tooling, and networking, replication, and authority libraries with focused evidence. StarterArena demonstrates a bounded project-to-runtime interaction. These examples support specific implementation claims; they do not establish the complete product direction described above.

The repository does not currently claim a production multiplayer service or dedicated-server product, a complete end-to-end editor, or a finished self-hosted distribution. C# remains the current canonical high-level gameplay source. Product direction must not be read as evidence that future scripting languages or workflows are already implemented.

For the exact evidence limits, see [capability boundaries and non-claims](../governance/non-claims.md) and the [product and capability reference](../reference/product-and-capability-boundaries.md).
