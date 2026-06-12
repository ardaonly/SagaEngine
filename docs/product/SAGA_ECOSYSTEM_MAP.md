# Saga Ecosystem Map

SagaEngine is a source-built engine and toolchain ecosystem for MMO-first game
development. It has source-truth discipline, deterministic local
reports/evidence, and bounded engine/toolchain subsystem foundations. It is not
currently a finished public engine product, beta, candidate release, production
SDK, production game engine, or complete editor workflow.

This page gives a compact map of what the Saga ecosystem is expected to
produce, which layer owns each responsibility, and which claims remain out of
scope.

## Product Shape

Saga is designed as a coordinated product ecosystem:

```txt
Saga
  primary product shell and workflow router

SagaEditor
  authoring module mounted by Saga

Runtime
  client/game execution over validated project and package artifacts

Server
  authoritative multiplayer simulation and session policy

Engine Core
  reusable runtime-neutral contracts and primitives

Tools
  validation, compilation, packaging, analysis, reports, and diagnostics

Samples
  deterministic fixtures and bounded proof paths

Reports
  evidence artifacts that describe what was checked and what remains open
```

Saga coordinates these systems. It must not absorb their implementation
ownership.

## Current Distribution Output

The current Linux technical-preview candidate path is expected to stage a
bounded distribution tree, archive, checksum, and local reports for selected
paths. The staged layout is not a final release, release candidate, installer,
updater, marketplace, or production package.

These outputs are evidence and preview artifacts. They do not prove production
readiness, enterprise readiness, full distribution certification, full editor
workflow, full runtime gameplay, full server gameplay, or MMO-scale behavior.

## Component Map

| Component | Role | Current maturity | Must not claim |
| --- | --- | --- | --- |
| `Saga` | Product shell, project workflow router, mode orchestration, diagnostics routing | Report-first foundation and workflow smoke paths | Finished dashboard, editor, runtime, server, compiler, package builder, or publisher |
| `SagaEditor` | Authoring module for scenes, assets, scripts, graphs, diagnostics, and preview UX | Scaffolding and boundary cleanup exist | Complete editor workflow, complete Visual Blocks UI, or product lifecycle ownership |
| `SagaRuntime` | Client/game runtime over validated packages and manifests | Partial startup, lifecycle, package validation, and smoke paths | Complete playable runtime loop or production client |
| `SagaServer` / server targets | Authoritative multiplayer simulation and server-side policy | Narrow server-authority and movement evidence exists | Production networking, complete replication loop, scale proof, or MMO readiness |
| Engine Core | Runtime-neutral primitives, diagnostics vocabulary, graphics/render public contracts, resource and package vocabulary | Focused foundations across multiple subsystems | Product shell ownership, editor internals, backend-native public SDK, or genre-specific gameplay systems |
| `sagaproject` | Project manifest validation and doctor-style reporting | Supported validation paths exist | Public stable SDK or complete project authoring product |
| `sagascript` | Selected C# / SagaScript analysis and compilation workflows | Useful source-authoring infrastructure exists | Finished visual authoring product or unbounded C# to blocks round trip |
| `sagapack` / package scripts | Package/profile inspection, staging, archive, checksum, and candidate reports | Limited Linux technical-preview candidate path exists | Full publish pipeline, installer, updater, marketplace, signing, or production distribution |
| `sde` | Deterministic data compilation boundary | Toolchain artifact role exists | Runtime, editor, product shell, or gameplay system ownership |
| `StarterArena` | Distribution sample and workflow fixture | Limited packaged validation and script analysis path exists | Finished sample game or complete packaged runtime/editor workflow |
| `MultiplayerSandbox` | Deterministic fixture for project, script, package, validation, and headless proof paths | Fixture/projection/headless evidence exists | Finished or playable multiplayer sample |
| Reports and diagnostics | Machine-readable evidence for checks, blockers, and limitations | Focused local reports and tests exist | Full raw CTest health, stress/load coverage, production readiness, or enforced CI gates unless explicitly stated |

## Ownership Boundaries

Saga product architecture depends on strict ownership:

- `Saga` owns product lifecycle and orchestration.
- `SagaEditor` owns authoring UX.
- Runtime owns client/game execution.
- Server owns authoritative multiplayer state and command validation.
- Engine Core owns reusable runtime foundations and public contracts.
- Tools own validation, compilation, analysis, packaging, and reports through
  explicit command or service boundaries.
- Reports describe evidence. They do not silently convert limitations into
  success.

Forbidden shortcuts include:

- Product code directly owning editor panel implementation.
- Editor code owning project lifecycle truth.
- Runtime code owning editor import or cook workflows.
- Server code trusting client authority for MMO truth.
- Engine Core depending upward into product, editor, server, app, or tool
  implementation folders.
- Public render or graphics contracts exposing backend-native objects.

## MMO-First Boundary

Saga is sandbox-MMO-first, but the engine core should stay genre-neutral.

Engine/runtime core may own MMO operation primitives:

- server authority and command validation;
- replication and dirty-state tracking;
- persistence and versioned state primitives;
- ownership, permissions, and access policy primitives;
- zones, shards, world partitioning, and interest management;
- diagnostics, performance budgets, and failure reporting.

Engine/runtime core should not own genre-specific gameplay systems such as
quests, classes, raids, loot progression, survival meters, crafting rules,
building placement gameplay, markets, guilds, factions, or territory rules.
Those belong in future packages, plugins, or project code once real seams exist.

## Evidence Model

Saga documentation uses claim discipline:

- Product claims start from current product docs.
- Architecture claims start from current architecture summaries.
- Proposed architecture notes and product slice documents describe direction,
  not shipped capability.
- Historical documents provide context, not current product
  promises.
- `Implemented-Unverified` means work exists but accepted verification is
  missing.
- Report-only evidence is useful for review, but it is not enforcement.
- Foundation work must name what is covered and what remains open.

The safest current claim is:

```txt
SagaEngine is an MMO-first engine/toolchain architecture codebase with
source-controlled project truth, deterministic local evidence reports, and
bounded runtime, server, editor, rendering, packaging, and diagnostics
foundations.
```

Forbidden claim example:

```txt
SagaEngine is ready for production as a complete editor-driven playable MMO engine
with a complete runtime, complete server, complete asset pipeline, and complete
publish pipeline.
```

## What This Page Is Not

This page is not a backlog, release note, SDK promise, or verification report.
It is an ecosystem orientation map. Check the current product docs,
architecture summaries, focused tests, and generated reports before making a
capability claim.
