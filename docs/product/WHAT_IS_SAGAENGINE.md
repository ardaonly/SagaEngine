# What Is SagaEngine?

> **Owner:** Product direction
> **Audience:** Prospective users, contributors, studios, and maintainers
> **Update trigger:** Product identity, supported profiles, open-source boundary, or maturity claims change
> **Authority:** Product definition; current implementation claims require source and accepted evidence

SagaEngine is an active open-source engine and toolchain architecture codebase.
The engine, base editor, and core authoring tools are intended to remain open
and forkable under the repository's applicable licenses.

Its selected long-term product specialization is **creator-driven persistent
community worlds**: independent, self-hosted online games built around
authoritative simulation, persistent state, modular gameplay, and C# plus
visual authoring.

C# source is intended to be the canonical behavior source. High-level and
low-level Visual Blocks are intended to project and edit that same behavior
model through the same semantic, compile, and runtime path. They are not
separate behavior sources and they do not define separate runtimes.

This is a product direction, not a claim that the current repository is already
a finished MMO engine.

## Product Direction

SagaEngine is intended to provide a general game-engine surface:

- project creation and validation;
- scene and world authoring;
- entity and component editing;
- asset import, processing, and consumption;
- rendering, input, physics, audio, UI, and animation;
- C# source authoring;
- high-level and low-level Visual Blocks;
- build, run, package, profiling, and diagnostics.

Its selected specialization adds emphasis on:

- server authority;
- replication and prediction boundaries;
- persistent player and world state;
- self-hosted dedicated servers;
- community-owned worlds;
- modular gameplay and server-rule packages;
- ownership, permissions, recovery, and long-lived state evolution.

Genre-specific rules such as crafting, quests, classes, guilds, economy, raids,
survival, and combat policy belong in project or package code rather than the
engine core.

See [SAGA_MMO_GENRE_FOCUS.md](SAGA_MMO_GENRE_FOCUS.md) for the complete product
focus and trust boundary.


## Intended Users

SagaEngine is primarily intended for:

- engine developers and contributors who want an open, inspectable codebase;
- C# developers who want visual authoring without giving up source ownership;
- small teams building self-hosted multiplayer sandboxes or persistent worlds;
- studios that need to fork, modify, and operate their own engine and server
  stack;
- teams that value independent distribution over mandatory dependence on a
  centralized creator platform.

This describes the intended audience. It does not claim that the current
repository already provides a production-ready experience for those users.


## Open-Source And Commercial Boundary

The engine, base editor, and core authoring tools are part of the open product
direction.

Optional hosted collaboration, managed services, long-term support, enterprise
integration, or commercial operations offerings may exist separately. Such
services must not become a hidden requirement for building, modifying,
self-hosting, or distributing an independent SagaEngine project under the
applicable licenses.

This is a product policy. It does not claim that hosted or enterprise services
exist today.


The repository's actual license, notice, third-party, and contributor files
remain legally authoritative. Product documentation does not grant additional
license or trademark rights.

## Extension Direction

SagaEngine does not promise a broad, long-lived C++ binary plugin ABI.

The preferred extension path is source modules built with the project or engine.
When a binary boundary is genuinely required, it should be narrow, explicitly
versioned, and C-compatible rather than exposing broad C++ object layouts,
allocators, exceptions, or vendor-native types.

This is an extension policy, not a claim that a complete extension SDK is
already implemented.

## Product Success Signal

SagaEngine's direction is not validated by source-code size or by the existence
of one heavily customized game.

Stronger evidence is that:

- projects reach a first playable result through documented workflows;
- iteration does not require routine engine-core modification;
- packages and runtime roles are reused by a second project;
- another developer can build, run, package, and diagnose the project;
- a community server can be deployed and recovered through documented
  procedures.

These are evaluation principles, not current readiness claims.

## Current Shape

The current repository is still an engine/toolchain development codebase:

- C++ engine, runtime, server, editor, app, and test code exists.
- Project, scripting, packaging, diagnostics, and verification tools exist.
- Bounded render, runtime, server-authority, project, package, and source
  authoring foundations have focused evidence.
- `samples/MultiplayerSandbox` is currently a fixture for project, script,
  package, validation, and headless workflows.
- The repository emphasizes source-truth discipline and explicit evidence
  boundaries.

A new developer should treat the repository as source for engine and toolchain
development, not as an installable end-to-end editor product.

## What It Is Not Yet

SagaEngine does not currently establish a complete:

- game creation workflow;
- playable runtime product loop;
- server gameplay and replication product loop;
- production renderer or asset pipeline;
- persistent community-world product;
- production networking, scale, soak, or anti-cheat boundary;
- visual scripting editor;
- arbitrary C# to blocks round trip;
- untrusted creator-code sandbox;
- cloud collaboration, account, payment, marketplace, or hosting platform;
- stable public SDK or long-term C++ binary ABI;
- release pipeline, installer, updater, or production distribution channel.

See [WHAT_IS_NOT_IMPLEMENTED.md](WHAT_IS_NOT_IMPLEMENTED.md) for the current
non-claim list.

## How To Read The Repository

Start with:

1. [Product documentation index](README.md)
2. [Current capabilities](CURRENT_CAPABILITIES.md)
3. [Current non-claims](WHAT_IS_NOT_IMPLEMENTED.md)
4. [Saga ecosystem map](SAGA_ECOSYSTEM_MAP.md)
5. [Getting started](GETTING_STARTED.md)

Use current architecture summaries for ownership and dependency claims. Treat
milestone, candidate, closure, roadmap, tutorial, and historical strategy files
as background unless a current index explicitly links to them for a bounded
task.
