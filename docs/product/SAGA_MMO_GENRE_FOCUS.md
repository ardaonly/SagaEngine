# Saga MMO Genre Focus

> **Owner:** Product direction
> **Audience:** Product, architecture, runtime, editor, and server contributors
> **Update trigger:** The accepted online-world direction is invalidated by the documented evidence triggers
> **Authority:** Product prioritization only; not an implementation or readiness claim

> Status: Current product direction
> Scope: Product specialization and prioritization, not a current capability claim

## Decision

SagaEngine's primary online-world reference profile is:

> **Creator-Driven Persistent Community Sandbox RPG**

The shorter public-facing category is:

> **Persistent Community Worlds**

SagaEngine remains a general-purpose game engine. This decision does not limit
the engine to one game or one fixed set of sandbox mechanics. It defines the
online-world use case that should guide product priorities, reference projects,
package boundaries, server validation, and long-term differentiation.

## Product Definition

SagaEngine is an open-source, C#-first game engine intended to combine source
authoring and visual authoring under one behavior model. Its selected
specialization is independent, self-hosted, persistent online worlds built
around authoritative simulation, persistent state, modular gameplay packages,
and community ownership.

The long-term product direction emphasizes:

- server-authoritative simulation;
- persistent player and world state;
- dedicated and community-hosted servers;
- reconnect, restart recovery, and schema migration;
- ownership, permissions, and moderation hooks;
- zones, world partitioning, and interest management;
- C# and Visual Blocks as complementary authoring surfaces;
- modular gameplay and server-rule packages;
- creator and community control without mandatory dependence on a centralized
  platform or managed commercial service;
- an open and forkable engine, base editor, and core authoring path under the
  repository's applicable licenses.

This is a target direction. The current repository may claim only the
capabilities supported by current source, build, test, package, and runtime
evidence.

## Why This Focus

This focus aligns the engine's intended differentiators instead of treating
them as unrelated features:

- C# source and Visual Blocks support both rapid authoring and deeper source
  ownership;
- authoritative simulation and persistence naturally serve long-lived worlds;
- gameplay packages allow communities and studios to define different rules
  without moving genre policy into the engine core;
- self-hosting and forkability distinguish SagaEngine from centralized creator
  platforms;
- community-created activity can reduce dependence on a permanent
  developer-authored theme-park content treadmill.

The focus is deliberately narrower than “all MMOs” and broader than one
survival-crafting game. It gives networking, persistence, package, editor, and
server work a common product filter while preserving the general engine
surface.

## Engine Core Boundary

The engine core should own genre-neutral online-world mechanisms:

- authority and command validation;
- replication and state-change tracking;
- prediction, reconciliation, and interpolation contracts;
- persistence primitives and schema migration;
- ownership, permission, and access-control primitives;
- zones, world partitioning, and interest management;
- runtime roles, package identity, diagnostics, and recovery contracts.

The engine core should not own project-specific gameplay policies:

- crafting and building rules;
- survival meters;
- quests and dialogue content;
- classes, skills, and progression rules;
- guild, faction, market, and territory rules;
- raids, loot tables, and encounter design;
- a fixed combat, economy, or social model.

Those systems belong in project code or gameplay packages once real consumers
and stable boundaries exist.

## Supported Product Profiles

The selected specialization does not remove support for:

- standalone and client-only games;
- single-player 2D and 3D games;
- local or listen-server co-op;
- competitive session games;
- community-hosted RPGs;
- modifiable sandbox games;
- persistent social or role-playing worlds.

These profiles are not all equal product priorities. Online-world investment
should be evaluated first against the Persistent Community Worlds reference
profile.

## Explicit Non-Focus

SagaEngine is not currently positioning itself as:

- a theme-park MMORPG framework first;
- a hardcore full-loot PvP MMO first;
- a generic survival-crafting clone;
- a centralized Roblox-style platform operator;
- a cloud hosting, account, payment, or marketplace company;
- a session-multiplayer-only engine;
- a finished production MMO stack.

PvP, survival, crafting, economy, and progression may exist as optional project
or package policies. They are not mandatory engine identity.

## Creator And Trust Boundary

In this product direction, `creator` initially means:

- a game developer;
- a trusted project team member;
- a reviewed or explicitly trusted package author;
- a community-server owner or administrator.

The direction does not imply arbitrary untrusted code execution.

The current product promise does not include:

- players uploading arbitrary native or managed code;
- unreviewed C# assemblies running inside authoritative server processes;
- a complete untrusted user-script platform;
- user code execution without an explicit sandbox, capability model, resource
  budgets, moderation controls, and security evidence.

Early community-created content may use data-driven world content, reviewed
packages, constrained rules, event definitions, or validated authoring
artifacts. Arbitrary code execution requires a separate security model and
independent evidence.

## Reference Evidence

A future reference project should eventually prove a bounded path containing:

- a dedicated headless server;
- authoritative player movement and interaction;
- persistent player identity or profile state;
- selected persistent world state;
- ownership and permission primitives;
- reconnect behavior;
- bounded recovery after server restart;
- at least one package-defined gameplay rule;
- basic interest or relevance filtering;
- a documented community-hosted deployment path.

This list is a product validation target, not a claim that the repository
already provides the complete path.

## Direction Validation Gates

The direction becomes credible only through working product evidence. Useful
validation gates include:

1. a fresh checkout can build the required targets through documented commands;
2. a developer can create or open a project, edit content, run it, and produce
   a bounded package without undocumented engine surgery;
3. a dedicated headless server can run authoritative movement and interaction;
4. selected player and world state survives reconnect and bounded server
   restart recovery;
5. at least one gameplay package changes server rules without modifying the
   engine core;
6. a second project reuses the same runtime and package boundaries;
7. a developer other than the primary author can follow the documented path;
8. a community-server operator can deploy and diagnose the reference world
   through documented procedures.

A single game existing is not sufficient evidence by itself. Review must ask
whether the game was produced **because of** the engine workflow or only
**despite** it through repeated core edits, manual workarounds, and
author-specific knowledge.

These are product validation gates, not current capability claims or a release
schedule.

## Re-evaluation Rule

This direction is considered frozen unless new evidence shows that it should be
changed.

Valid re-evaluation triggers include:

- a working reference project showing that the profile is wrong or not
  producible;
- meaningful external-user concentration around a different online use case;
- evidence that the specialization damages the general engine surface or the
  genre-neutral core boundary;
- measured product, performance, security, or operations requirements that
  invalidate the direction.

The following are not sufficient triggers:

- a new MMO trend;
- one competitor or game becoming popular;
- a short-term change in personal interest;
- a feature found in another engine that does not block the reference product;
- another theoretical genre comparison without a working project or user
  evidence.

## Legal And Naming Boundary

This document defines product direction only. It does not modify or replace the
repository's license files, notices, third-party obligations, contributor
terms, or trademark rights.

The ability to fork code under an applicable license does not automatically
grant the right to present a fork as an official SagaEngine distribution or to
use protected names, logos, or marks. Any future trademark policy must be
defined in the appropriate legal or governance document.

## Documentation Boundary

Use this document for product direction.

Use [CURRENT_CAPABILITIES.md](CURRENT_CAPABILITIES.md) for current repository
capabilities and [WHAT_IS_NOT_IMPLEMENTED.md](WHAT_IS_NOT_IMPLEMENTED.md) for
current non-claims. Product direction must never be rewritten as present-tense
implementation without accepted evidence.
