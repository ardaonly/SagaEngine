---
title: Product and capability reference
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Product and capability reference

This reference explains how to interpret SagaEngine as it exists in 0.0.11. It deliberately separates repository capability, usable workflow, evidence, and product completion. Names in code are not release claims, and historical positioning does not override present implementation.

## Product definition

SagaEngine is an open-source game-engine and authoring-toolchain codebase. Its durable product idea is a general-purpose engine surface with project-owned depth: projects provide manifests, scenes, assets, packages, scripts, profiles, and samples; the engine provides contracts and runtime/editor infrastructure that can consume them.

The current repository is useful to engine contributors, toolchain developers, and project authors willing to work with explicit contracts and incomplete product surfaces. It is not presented as a turnkey engine distribution for users who expect a stable SDK, complete editor, asset marketplace, installer, hosted service, or long-term API compatibility guarantee.

Genre-specific historical language is not current public positioning. Server authority, replication, persistence, and world partitioning remain valuable technical domains, but they are described neutrally as foundations for authoritative or persistent community-world workloads rather than as a shipped genre product.

## Current implemented areas

The repository contains implemented code and focused evidence in these areas:

- runtime foundations: Core, Math, Diagnostics, ECS, Assets, Resources, Input, Audio, Render, RHI, UI, Scripting, Networking, Replication, Persistence, Simulation, World, ServerAuthority, and SagaRuntime;
- editor foundations: EditorCore, EditorFramework, EditorQt, EditorAuthoring, VisualBlocksEditor, EditorScripting, EditorCollaboration, and EditorExperimental;
- programs: runtime, editor, editor lab, launcher, sandbox, and a reserved server owner;
- project and packaging tools: project validation, script analysis/building, package inspection/staging, Forge dependency setup, licensing checks, and internal boundary checks;
- tests and evidence: focused unit, architecture, installed-consumer, integration, GPU, sample, and stress-oriented areas.

“Implemented” means code exists and can be linked to relevant tests or executable paths. It does not mean every configuration is complete, public, supported, polished, or suitable for production deployment.

## Capability reading rules

Use four evidence levels when describing a feature:

1. **Contract present** — types, schemas, commands, or interfaces define a boundary.
2. **Implementation present** — an owning source path performs the behavior.
3. **Focused evidence present** — tests or reports exercise named behavior under controlled inputs.
4. **Product workflow demonstrated** — a user-facing entry point performs the complete workflow with appropriate failure handling and evidence.

Do not jump from the first or second level to the fourth. A public header can be transitional. A report-only tool can validate metadata without enforcing a policy. A sample can prove a narrow path without proving general content authoring. A command name can exist while its intended product workflow remains incomplete.

### Capability status table

| Area | Current defensible statement | Statement that would be too broad |
| --- | --- | --- |
| Runtime | Modules and focused runtime/simulation/render/network/resource foundations exist | “Complete general game runtime” |
| Editor | Shell, Qt, authoring, Visual Blocks, collaboration, and experimental code/models exist | Not claimed: finished editor workflow |
| Graphics | Vendor-neutral contracts, private Diligent paths, CPU and focused GPU evidence exist | “Stable cross-backend graphics SDK” |
| Scripting | C# analysis/compile/host and bounded source-preserving projection/patch evidence exist | “Complete visual scripting runtime” |
| Networking/authority | Transport, replication, server-authority libraries and focused tests exist | “Production online service/dedicated server” |
| Persistence | Interfaces and PostgreSQL/Redis-shaped implementations exist | “Shipped persistent-world operations” |
| Collaboration | Semantic transaction, presence, lock, review, policy and concrete surfaces exist | “Hosted secure real-time collaboration” |
| Packaging | Project/package tools and Linux staging/archive/smoke contracts exist | “Final portable release distribution” |

The right column is not forbidden forever. It requires the missing product workflow, operational behavior, compatibility/security/performance work, and evidence appropriate to that statement.

## Public tool surface

The intentionally small public CLI direction is:

- `sagaproject` for `.sagaproj`, project-relative paths, scene/entity source truth, asset metadata, project slices, and readiness-style reports;
- `sagascript` for C# analysis, compilation, node metadata, source-patch evidence, and script diagnostics;
- `sagapack` for package/profile inspection, staging, and policy-evidence consumption;
- SagaTools as a dispatcher where the repository exposes a stable aggregate tool entry.

Tools under `Tools/Developer` are internal verification or distribution-evidence tools unless explicitly promoted. Their output can be valuable evidence without becoming a supported public CLI.

## Project-owned depth

A project is not merely a sample directory. Its `.sagaproj` manifest identifies the project and resolves owned inputs. Project-owned scene and asset sources remain canonical within their declared formats. Package manifests and generated artifacts are derived inputs used for runtime startup, staging, or evidence. C# remains behavior source of truth even when Visual Blocks, node metadata, compiled assemblies, or source maps provide additional views.

Samples serve three roles:

- understandable examples of valid project structure;
- fixtures for focused tests and packaging/evidence paths;
- bounded demonstrations of a named runtime or editor scenario.

They are not schema authorities, hidden global configuration, production content promises, or proof of all intended workflows.

## What is intentionally not claimed

SagaEngine 0.0.11 keeps these limits explicit:

- it does not claim a finished public SDK or stable ABI/API compatibility window;
- it does not claim a complete editor, content browser, scene workflow, or Visual Blocks product;
- it does not claim a production networking service, dedicated-server product, persistent-world operation, or database operations model;
- it does not claim a hosted collaborative editor, tenant security, identity service, remote permission enforcement, or real-time multi-user product;
- it does not claim equal quality or availability across all graphics backends and platforms;
- it does not claim complete import/cook coverage for arbitrary content formats;
- it does not claim a stable third-party plugin domain, plugin license, marketplace, or extension compatibility promise;
- it does not claim a final Linux distribution, installer, updater, publishing service, or release certification process.

These statements are not a roadmap. They prevent repository foundations from being presented as finished capability.

## Getting started boundary

The supported development entry is repository-first:

```sh
git status --short
scripts/wiki verify
nix-shell --run "Tools/Forge/bin/forge install --profile linux-gcc"
nix-shell --run "cmake --preset linux-gcc"
nix-shell --run "cmake --build --preset linux-gcc"
```

Focused validation should follow the touched owner. A full CTest, stress suite, or broad GPU run is not automatically safe or necessary on every local machine. Reports must identify exactly which targets and tests ran.

## Maintaining honest capability language

When implementation changes, update the owning topic and its detailed reference together. Prefer statements such as “the focused test validates baseline mismatch rejection” over “replication is complete.” Prefer “the launcher surfaces a tool report” over “the workflow is finished.” Prefer “the editor models presence and locks” over “collaboration is shipped.”

The best current product description is the intersection of code, contract, evidence, and a working entry path—not the union of all future-looking names found in the tree.

## Capability change protocol

Promoting a capability statement requires all of the following:

1. identify the owning module/tool/program and source-of-truth input;
2. define public/private and lifecycle/failure boundaries;
3. implement the complete behavior named by the statement;
4. add focused rejection/failure evidence, not only a happy path;
5. demonstrate the user/program workflow if the statement is product-facing;
6. record platform/environment limits, dependencies, and operational assumptions;
7. update the concise topic, detailed contract, and non-claim page together;
8. remove only the exact non-claim that evidence has closed.

Adjacent plans remain non-claims. For example, proving a dedicated server entry point would not automatically prove persistence, hosted operation, deployment security, or scale.
