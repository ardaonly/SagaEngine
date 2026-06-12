# Saga MMO Genre Focus

> Status: Current architecture decision
> Scope: Product direction and core/package boundary for MMO genre work

## Decision

Saga's direction is sandbox MMO first, not themepark MMORPG first.

The current product proof remains `samples/MultiplayerSandbox`. It is a
deterministic fixture and bounded headless/server-authoritative proof path, not
a finished game and not production MMO proof.

## Product Shape

Saga is an MMO-first engine and toolchain focused on:

- server-authoritative multiplayer foundations;
- persistent world and project source-truth workflows;
- runtime, server, editor, scripting, packaging, diagnostics, and evidence
  tooling;
- small-team iteration on multiplayer sandbox projects.

Saga should not present the current repository as a finished MMORPG, finished
sandbox MMO, production networking stack, or complete game creation product.

## Core Boundary

Engine/runtime core may own genre-neutral MMO primitives:

- server authority and command validation;
- replication and dirty-state tracking;
- persistence and versioned state primitives;
- ownership, permissions, and access policy primitives;
- world partitioning, zones, shards, and interest management;
- diagnostics, performance budgets, and failure reporting;
- narrow extension seams when there is direct implementation pressure.

Engine/runtime core should not own genre-specific gameplay systems:

- quest systems;
- class, level, raid, dungeon, or loot progression systems;
- survival hunger/thirst/body-temperature rules;
- crafting recipe gameplay;
- building placement gameplay;
- player market or auction systems;
- guild, faction, clan, or territory gameplay rules beyond neutral ownership
  primitives.

## Package Direction

MMORPG, survival sandbox, social sandbox, economy, crafting, building, and other
genre systems may become packages or plugins after the repo has real package
seams that justify them.

Do not add package/plugin abstractions speculatively. Add them when at least one
real gameplay slice needs the seam and the boundary can be tested.

## Current Evidence

Current evidence supports a bounded foundation, not a genre-complete product:

- `MultiplayerSandbox` is the main project/toolchain fixture.
- Server-authoritative movement proof exists at a narrow headless level.
- Shard, zone, replication, interest, runtime, editor, scripting, packaging,
  diagnostics, and collaboration foundations exist in partial form.
- Complete runtime gameplay loop, complete server gameplay loop, production
  networking, production asset pipeline, and finished editor workflow remain
  open.

## Non-Goals

This decision does not:

- replace the product status docs;
- redefine 0.1 as a survival sandbox release;
- require a new top-level `Packages/` directory;
- require package/plugin C++ contracts before implementation pressure exists;
- move current `Server/`, `samples/`, `docs/`, or `Engine/` layout;
- claim MMORPG support is implemented;
- claim production MMO readiness.

## Short Rule

Core knows how an MMO world is operated safely.
Packages may later define how a specific MMO genre behaves.
The editor helps teams author and validate that world.
Collaboration is a product/team workflow layer, not a gameplay genre.
