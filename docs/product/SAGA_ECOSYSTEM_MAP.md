# Saga Ecosystem Map

> **Owner:** Product and architecture
> **Audience:** Contributors deciding ownership, dependency, and product boundaries
> **Update trigger:** A component owner, dependency direction, product boundary, or artifact flow changes
> **Authority:** Current product map; source, build graph, and accepted evidence govern implemented behavior

> Status: Current product and ownership map
> Scope: Product shape, component roles, ownership boundaries, and evidence limits

## Product Shape

SagaEngine is an open-source, C#-first game engine and toolchain codebase.

Its selected long-term specialization is **creator-driven persistent community
worlds**, while the engine core remains genre-neutral. The product direction
combines:

- a general game-engine surface;
- C# as the intended canonical behavior source;
- high-level and low-level Visual Blocks as projections over the same semantic,
  compile, and runtime model;
- standalone, client, listen-server, dedicated-server, and headless roles;
- authoritative multiplayer and persistent-world foundations;
- modular gameplay packages;
- self-hosting and source ownership;
- an open and forkable engine, base editor, and core authoring path under the
  applicable repository licenses.

The current repository does not yet establish a finished engine, editor,
runtime product, MMO stack, creator platform, or public SDK.

## Current Distribution Output

Current outputs are development targets, tools, reports, fixtures, and bounded
package candidates. They should not be presented as a complete installable
product distribution.

Generated reports and staged artifacts are evidence or intermediate outputs.
They do not become product truth merely because they exist.

See [CURRENT_DISTRIBUTION_STATUS.md](CURRENT_DISTRIBUTION_STATUS.md) for the
current bounded distribution claim.

## Component Map

| Area | Intended ownership | Current bounded evidence | Explicit non-claim |
| --- | --- | --- | --- |
| `Saga` product layer | Product lifecycle and orchestration across project, authoring, preview, build, package, run, publish, and diagnostics entry points | Product and composition foundations exist | Finished product shell or end-to-end workflow |
| `SagaEditor` | Scene, asset, script, graph, diagnostics, and preview authoring UX | Editor scaffolding and boundary work exist | Complete editor or Visual Blocks product |
| `SagaRuntime` | Client and game execution over validated project/package artifacts | Startup, lifecycle, asset, package, and smoke foundations exist | Finished playable runtime |
| `SagaServer` and server targets | Authoritative simulation, validation, state mutation, sessions, and replication output | Narrow authority, movement, ownership, and diagnostics evidence exists | Production networking, scale proof, or MMO readiness |
| Engine Core | Runtime-neutral primitives, ECS/simulation, math, diagnostics, graphics contracts, platform boundaries, and package vocabulary | Focused foundations exist across several subsystems | Product/editor ownership, genre policy, or backend-native public SDK |
| Forge | Build and repository orchestration through explicit commands and tool boundaries | Build/toolchain workflows exist | Universal product shell or owner of every tool implementation |
| `sagaproject` | Project-manifest validation, resolution, and doctor-style reporting | Supported validation paths exist | Stable public project SDK |
| `sagascript` | Selected C# analysis, compilation, source discovery, artifact, and authoring workflows | Useful source-authoring infrastructure exists | Complete managed runtime or arbitrary C# round trip |
| `sagapack` and package tooling | Package/profile inspection, staging, archive, checksum, preflight, and smoke reporting | Bounded local paths exist | Full publishing, installer, updater, signing, or marketplace system |
| Prism | Standalone retained repository-analysis/history surface outside the first-playable Saga product path | Standalone tool sources remain in repo | Active Saga product, scripting, build, runtime, or canonical artifact ownership |
| `StarterArena` | Sample or workflow fixture according to verified behavior | Limited workflow evidence may exist | Automatically a finished sample game |
| `MultiplayerSandbox` | Deterministic integration fixture for project, script, package, validation, and headless workflows | Fixture and report paths exist | Finished or playable multiplayer sample |
| Diagnostics and tests | Focused, reproducible evidence about bounded behavior | Unit, integration, architecture, GPU, CLI, and report foundations exist | Product readiness or permanent full-suite health |

## Ownership Boundaries

Saga product architecture depends on explicit ownership:

- The product layer coordinates public module and tool boundaries.
- The editor owns authoring presentation and editor-side commands.
- Runtime owns client and game execution.
- Server owns authoritative multiplayer decisions and state.
- Engine Core owns reusable runtime-neutral contracts and foundations.
- Tools own offline validation, compilation, analysis, packaging, and reports.
- Diagnostics and tests describe bounded evidence.

Forbidden shortcuts include:

- product code owning editor-panel implementation;
- editor code becoming project, runtime, server, compiler, or package truth;
- runtime code owning asset import/cook or build-tool internals;
- server code accepting client state as authoritative gameplay truth;
- Engine Core depending upward into product, editor, server, app, or tool
  implementation;
- public contracts exposing backend-native implementation types;
- reports silently converting limitations into success.

## Open Product And Service Boundary

The engine, base editor, and core authoring tools belong to the open product
direction.

Optional hosted collaboration, managed operations, long-term support, or
enterprise integration may be separate services. They must not become an
undocumented requirement for independent source builds, project ownership,
self-hosting, or distribution under the applicable licenses.

This boundary is a product policy, not evidence that commercial services
currently exist.

## Extension Boundary

SagaEngine prefers source modules over a broad stable C++ binary plugin ABI.

A required binary integration boundary should be narrow, versioned, and
C-compatible. Public extension contracts should not depend on C++ object
layout, cross-module allocation ownership, exceptions, compiler-specific ABI,
or backend-native implementation types.

A complete stable extension SDK is not currently claimed.

## Source Provenance Boundary

Open-source reuse is allowed only through sources whose license and provenance
are understood and recorded.

The product direction does not authorize:

- leaked, stolen, proprietary, or license-unclear source code;
- renamed or mechanically transformed copies of such code;
- AI-generated rewrites derived from tainted source material;
- third-party code whose required license, copyright, notice, patent, or
  attribution obligations cannot be satisfied.

When external code is reused, prefer the original upstream project over an
engine-specific wrapper when practical. Keep dependency ownership, selected
license, source revision, local modifications, notices, and removal or upgrade
paths discoverable.

Detailed acceptance and review rules belong in contribution and governance
documentation.

## Persistent Community World Specialization

The selected online-world product focus is:

> **Creator-Driven Persistent Community Sandbox RPG**

The public category is:

> **Persistent Community Worlds**

Engine and runtime layers may own genre-neutral mechanisms:

- authority and command validation;
- replication and state-change tracking;
- prediction and reconciliation contracts;
- persistence and schema migration;
- ownership, permissions, and access primitives;
- zones, partitioning, and interest management;
- runtime roles, package identity, recovery, and diagnostics.

They should not own fixed genre policies:

- crafting, building, or survival rules;
- quests, classes, raids, and loot progression;
- markets, guilds, factions, or territory rules;
- project-specific combat, economy, or social design.

Those belong in project code or gameplay packages.

The specialization does not claim that the complete persistent-world stack is
implemented today. See
[SAGA_MMO_GENRE_FOCUS.md](SAGA_MMO_GENRE_FOCUS.md) for the full direction and
trust boundary.

## Creator And Trust Boundary

Creator workflows initially target trusted developers, project teams, reviewed
package authors, and community-server operators.

The ecosystem map does not claim arbitrary untrusted code execution, a secure
public script sandbox, or a complete Roblox-style platform. Data-driven content
and validated packages are separate from unrestricted user code.

## Artifact Flow

The intended high-level flow is:

```text
Source-controlled project inputs
  -> validation, compile, import, and analysis tools
  -> generated artifacts, manifests, and reports
  -> package and build outputs
  -> Runtime and Server consumption
  -> diagnostics and focused test evidence
```

Authored source, generated artifacts, reports, packages, runtime state, and
persistent state must retain separate ownership and lifecycle rules.

## Evidence Model

A focused build, test, report, package candidate, or runtime smoke proves only
its declared slice. Positive current claims must remain traceable to the
specific accepted evidence that supports them.

It does not automatically prove:

- complete game creation;
- complete editor or runtime workflow;
- production networking or MMO scale;
- complete asset pipeline;
- stable SDK or ABI;
- release, beta, enterprise, or production readiness.

Use [CURRENT_CAPABILITIES.md](CURRENT_CAPABILITIES.md) for current capability
claims and [WHAT_IS_NOT_IMPLEMENTED.md](WHAT_IS_NOT_IMPLEMENTED.md) for current
non-claims.

## What This Page Is Not

This page is not:

- a roadmap;
- a release plan;
- a completed-feature matrix;
- a target dependency graph;
- a stable API or ABI specification;
- proof that every stated ownership boundary is already enforced.

Current architecture summaries, source, build targets, and accepted test evidence
remain the authority for implemented behavior.
