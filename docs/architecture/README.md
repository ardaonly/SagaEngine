# Architecture

This directory contains architecture notes for SagaEngine's current engine,
runtime, server, editor, asset, publish, diagnostics, and testing design.

Start with the short summaries and compact contracts before reading evidence
inventories or audits.

## Current Summaries

| Topic | Start here |
| --- | --- |
| Current status | [CURRENT_STATUS.md](CURRENT_STATUS.md) |
| Architecture overview | [ARCHITECTURE_OVERVIEW.md](ARCHITECTURE_OVERVIEW.md) |
| Ownership and boundaries | [OWNERSHIP.md](OWNERSHIP.md) |
| Claim and evidence policy | [CLAIM_AND_EVIDENCE_POLICY.md](CLAIM_AND_EVIDENCE_POLICY.md) |
| Source of truth map | [SOURCE_OF_TRUTH_MAP.md](SOURCE_OF_TRUTH_MAP.md) |
| Runtime | [RUNTIME.md](RUNTIME.md) |
| Server authority | [SERVER.md](SERVER.md) |
| MMO genre focus | [SAGA_MMO_GENRE_FOCUS.md](SAGA_MMO_GENRE_FOCUS.md) |
| Assets and packages | [ASSETS_AND_PACKAGES.md](ASSETS_AND_PACKAGES.md) |
| Editor | [EDITOR.md](EDITOR.md) |
| Apps/Sandbox role | [APPS_SANDBOX_ROLE.md](APPS_SANDBOX_ROLE.md) |
| Publish | [PUBLISH.md](PUBLISH.md) |
| Testing | [TESTING_AND_EVIDENCE.md](TESTING_AND_EVIDENCE.md) |

## Canonical Boundary Contracts

These documents define compact current boundaries. They are the preferred entry
points before reading longer proposals or audit appendices.

| Topic | Start here |
| --- | --- |
| Authoring authority | [AUTHORING_AUTHORITY_MODEL.md](AUTHORING_AUTHORITY_MODEL.md) |
| C# / Visual authoring | [SAGA_CSHARP_VISUAL_AUTHORING_BOUNDARY.md](SAGA_CSHARP_VISUAL_AUTHORING_BOUNDARY.md) |
| C# compatibility profile | [CSHARP_VISUAL_BLOCKS_COMPATIBILITY_PROFILE.md](CSHARP_VISUAL_BLOCKS_COMPATIBILITY_PROFILE.md) |
| Visual block patching | [VISUAL_BLOCK_PATCH_CONTRACT.md](VISUAL_BLOCK_PATCH_CONTRACT.md) |
| Source patch writes | [SOURCE_PATCH_APPLY_POLICY.md](SOURCE_PATCH_APPLY_POLICY.md) |
| Collaboration | [SAGA_COLLABORATION_CURRENT_BOUNDARY.md](SAGA_COLLABORATION_CURRENT_BOUNDARY.md) |
| Local workspace transactions | [SAGA_LOCAL_WORKSPACE_TRANSACTION_BOUNDARY.md](SAGA_LOCAL_WORKSPACE_TRANSACTION_BOUNDARY.md) |
| Nix development/validation | [NIX_DEVELOPMENT_AND_VALIDATION_POLICY.md](NIX_DEVELOPMENT_AND_VALIDATION_POLICY.md) |
| Render public API | [RENDER_PUBLIC_API_CONTRACT.md](RENDER_PUBLIC_API_CONTRACT.md) |
| Render extension/config | [ENGINE_RENDER_EXTENSION_BOUNDARY_CONTRACT.md](ENGINE_RENDER_EXTENSION_BOUNDARY_CONTRACT.md) |
| World public facade | [ENGINE_WORLD_PUBLIC_FACADE_CONTRACT.md](ENGINE_WORLD_PUBLIC_FACADE_CONTRACT.md) |

## Evidence And Risk Inventories

These documents are useful for cleanup and review, but they are not product
readiness claims and they should not be read as final architecture state.

| Topic | Start here |
| --- | --- |
| Boundary hardening risks | [ARCHITECTURE_BOUNDARY_HARDENING_PLAN.md](ARCHITECTURE_BOUNDARY_HARDENING_PLAN.md) |
| Installed public surface inventory | [ENGINE_PUBLIC_API_AUDIT.md](ENGINE_PUBLIC_API_AUDIT.md) |
| RenderGraph/header audit appendix | [ENGINE_RENDERGRAPH_PUBLIC_HEADER_AUDIT.md](ENGINE_RENDERGRAPH_PUBLIC_HEADER_AUDIT.md) |
| Render residual public surface audit appendix | [ENGINE_RENDER_RESIDUAL_PUBLIC_SURFACE_AUDIT.md](ENGINE_RENDER_RESIDUAL_PUBLIC_SURFACE_AUDIT.md) |
| World public header audit appendix | [ENGINE_WORLD_PUBLIC_HEADER_AUDIT.md](ENGINE_WORLD_PUBLIC_HEADER_AUDIT.md) |
| World node/facade audit appendix | [ENGINE_WORLD_NODE_FACADE_AUDIT.md](ENGINE_WORLD_NODE_FACADE_AUDIT.md) |
| Graphics/render pipeline hardening | [GRAPHICS_RENDER_PIPELINE_HARDENING_PLAN.md](GRAPHICS_RENDER_PIPELINE_HARDENING_PLAN.md) |
| CMake boundary inventory | [CMAKE_BOUNDARY_INVENTORY.md](CMAKE_BOUNDARY_INVENTORY.md) |
| Sample executable evidence | [SAMPLE_EXECUTABLE_EVIDENCE.md](SAMPLE_EXECUTABLE_EVIDENCE.md) |

## Internal Appendices

Larger proposed architecture ideas and implementation-history notes live under
[../internal/architecture-appendices/](../internal/architecture-appendices/).
They preserve design history and rationale, but they are not current
architecture entry points and do not supersede the summaries or boundary
contracts above.

## Documentation Rules

Architecture files describe system shape: ownership, boundaries, contracts,
data flow, invariants, and explicit non-claims.

Architecture files must not act as delivery trackers, progress logs, or product
model documents. Historical progress records belong outside this directory or
should be deleted after their durable architecture decisions are merged into the
stable documents above.

Current implementation decisions should be checked against source code, tests,
and the current summary files above.
