---
title: Docs Continuity Map
status: continuity
owner: SagaWiki
generated_html: pages/docs-continuity.html
---

# Docs Continuity Map

This page is a continuity map, not an audit dump. It records how the retired `docs/` tree should inform later evidence-checked SagaWiki expansion without restoring that tree or treating historical claims as current capability.

## Old docs sources

The primary continuity tree comes from commit `177bfde6bc2c4f89ee68aa5a33d1c4d513003b9d`. Shared paths use that later pre-deletion snapshot.

The historical comparison also inspected SagaEngine 0.0.10 commit `904278d3f4ee80c1852a73c2dbb03afbd723798e`; files unique to that snapshot are included without restoring `docs/`.

Current 0.0.11 HEAD remains authoritative. Historical text affects this map only after current source and focused evidence review.

## Classification summary

| Classification | Files |
| --- | ---: |
| `MERGED_CURRENT` | 49 |
| `SHOULD_MERGE` | 10 |
| `DELETE_HISTORY_ONLY` | 81 |
| `PRIVATE_ARCHIVE_CANDIDATE` | 12 |
| `NEEDS_ARDA_DECISION` | 0 |

## Manual review priority

Review these documents first before any later wiki expansion. Classification is a routing decision, not evidence that their historical statements remain true.

1. `docs/architecture/RENDER_MATERIAL_GRAPH_AUTHORING_CONTRACT.md` — `SHOULD_MERGE` → `editor.md`
2. `docs/architecture/SAGA_SCENE_ASSET_AUTHORING_CONTRACT.md` — `SHOULD_MERGE` → `editor.md`
3. `docs/architecture/GRAPHICS_BACKEND_PREFERENCE_ORDER.md` — `SHOULD_MERGE` → `runtime.md`
4. `docs/architecture/RENDER_INTEREST_WORLD_STREAMING_CONTRACT.md` — `SHOULD_MERGE` → `runtime.md`
5. `docs/architecture/RENDER_LIGHTING_STRATEGY.md` — `SHOULD_MERGE` → `runtime.md`
6. `docs/architecture/ENGINE_RENDER_EXTENSION_BOUNDARY_CONTRACT.md` — `SHOULD_MERGE` → `module-boundaries.md`
7. `docs/architecture/ENGINE_WORLD_PUBLIC_FACADE_CONTRACT.md` — `SHOULD_MERGE` → `module-boundaries.md`
8. `docs/architecture/RENDER_PUBLIC_API_CONTRACT.md` — `SHOULD_MERGE` → `module-boundaries.md`
9. `docs/architecture/DIAGNOSTICS_CRASH_CONTEXT_AND_RELIABILITY_RULES.md` — `SHOULD_MERGE` → `testing.md`
10. `docs/architecture/DIAGNOSTICS_OPERATIONAL_REPORTS_AND_SERVER_OBSERVABILITY.md` — `SHOULD_MERGE` → `testing.md`

## Continuity table

| Old docs path | Classification | Suggested SagaWiki destination | Reason |
| --- | --- | --- | --- |
| `docs/CodingStandards.md` | `MERGED_CURRENT` | `toolchain.md` | Its durable boundary is represented in current `toolchain.md`; the old file remains historical evidence only. |
| `docs/DependencyGraph.md` | `MERGED_CURRENT` | `architecture.md` | Its durable boundary is represented in current `architecture.md`; the old file remains historical evidence only. |
| `docs/LICENSING.md` | `MERGED_CURRENT` | `licensing.md` | Its durable boundary is represented in current `licensing.md`; the old file remains historical evidence only. |
| `docs/README.md` | `DELETE_HISTORY_ONLY` | `architecture.md` | This retired docs-tree index only navigated removed material; Git history is sufficient. |
| `docs/architecture/APPS_SANDBOX_ROLE.md` | `DELETE_HISTORY_ONLY` | `architecture.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/ARCHITECTURE_BOUNDARY_HARDENING_PLAN.md` | `DELETE_HISTORY_ONLY` | `module-boundaries.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/ARCHITECTURE_OVERVIEW.md` | `MERGED_CURRENT` | `architecture.md` | Its durable boundary is represented in current `architecture.md`; the old file remains historical evidence only. |
| `docs/architecture/ASSETS_AND_PACKAGES.md` | `MERGED_CURRENT` | `runtime.md` | Its durable boundary is represented in current `runtime.md`; the old file remains historical evidence only. |
| `docs/architecture/ASSET_SOURCE_MANIFEST_INVENTORY.md` | `DELETE_HISTORY_ONLY` | `source-of-truth.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/AUTHORING_AUTHORITY_MODEL.md` | `MERGED_CURRENT` | `server-authority.md` | Its durable boundary is represented in current `server-authority.md`; the old file remains historical evidence only. |
| `docs/architecture/CLAIM_AND_EVIDENCE_POLICY.md` | `MERGED_CURRENT` | `testing.md` | Its durable boundary is represented in current `testing.md`; the old file remains historical evidence only. |
| `docs/architecture/CLIENT_PREVIEW_AND_RUNTIME_UI_STRATEGY.md` | `DELETE_HISTORY_ONLY` | `runtime.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/CMAKE_BOUNDARY_INVENTORY.md` | `DELETE_HISTORY_ONLY` | `toolchain.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/CSHARP_VISUAL_BLOCKS_COMPATIBILITY_PROFILE.md` | `MERGED_CURRENT` | `scripting.md` | Its durable boundary is represented in current `scripting.md`; the old file remains historical evidence only. |
| `docs/architecture/CURRENT_STATUS.md` | `DELETE_HISTORY_ONLY` | `architecture.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/DANGEROUS_OPERATION_POLICY_GATE_V1.md` | `DELETE_HISTORY_ONLY` | `architecture.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/DIAGNOSTICS_CRASH_CONTEXT_AND_RELIABILITY_RULES.md` | `SHOULD_MERGE` | `testing.md` | Crash-context ownership and failure capture need a current, code-checked diagnostics summary before merging. |
| `docs/architecture/DIAGNOSTICS_MEMORY_RESOURCE_SNAPSHOT_FOUNDATION.md` | `DELETE_HISTORY_ONLY` | `testing.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/DIAGNOSTICS_MIGRATION_AUDIT.md` | `DELETE_HISTORY_ONLY` | `testing.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/DIAGNOSTICS_OPERATIONAL_REPORTS_AND_SERVER_OBSERVABILITY.md` | `SHOULD_MERGE` | `testing.md` | Operational report ownership remains useful, but current diagnostics producers and non-claims must be revalidated first. |
| `docs/architecture/DIAGNOSTICS_STRESS_ARENA_V1.md` | `DELETE_HISTORY_ONLY` | `testing.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/EDITOR.md` | `MERGED_CURRENT` | `editor.md` | Its durable boundary is represented in current `editor.md`; the old file remains historical evidence only. |
| `docs/architecture/EDITOR_UI_IMPLEMENTATION_STRATEGY.md` | `DELETE_HISTORY_ONLY` | `editor.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/ENGINE_PUBLIC_API_AUDIT.md` | `DELETE_HISTORY_ONLY` | `module-boundaries.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/ENGINE_RENDERGRAPH_PUBLIC_HEADER_AUDIT.md` | `DELETE_HISTORY_ONLY` | `module-boundaries.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/ENGINE_RENDER_EXTENSION_BOUNDARY_CONTRACT.md` | `SHOULD_MERGE` | `module-boundaries.md` | Current render extension points need public-consumer verification before their supported boundary is documented. |
| `docs/architecture/ENGINE_RENDER_RESIDUAL_PUBLIC_SURFACE_AUDIT.md` | `DELETE_HISTORY_ONLY` | `runtime.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/ENGINE_WORLD_NODE_FACADE_AUDIT.md` | `DELETE_HISTORY_ONLY` | `architecture.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/ENGINE_WORLD_PUBLIC_FACADE_CONTRACT.md` | `SHOULD_MERGE` | `module-boundaries.md` | The current World public facade needs header and consumer review before a durable contract is stated. |
| `docs/architecture/ENGINE_WORLD_PUBLIC_HEADER_AUDIT.md` | `DELETE_HISTORY_ONLY` | `architecture.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/ENTERPRISE_SECURITY_LIMITATIONS.md` | `MERGED_CURRENT` | `server-authority.md` | Its durable boundary is represented in current `server-authority.md`; the old file remains historical evidence only. |
| `docs/architecture/ENTITY_SCHEMA_V1.md` | `DELETE_HISTORY_ONLY` | `architecture.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/GENERATED_ARTIFACT_FRESHNESS_CONTRACT.md` | `MERGED_CURRENT` | `source-of-truth.md` | Its durable boundary is represented in current `source-of-truth.md`; the old file remains historical evidence only. |
| `docs/architecture/GRAPHICS_BACKEND_PREFERENCE_ORDER.md` | `SHOULD_MERGE` | `runtime.md` | Backend selection and fallback order remain useful but must be checked against the current private RHI bootstrap. |
| `docs/architecture/GRAPHICS_CAPABILITY_MATRIX_V0.md` | `DELETE_HISTORY_ONLY` | `runtime.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/GRAPHICS_COORDINATE_CONVENTION.md` | `MERGED_CURRENT` | `runtime.md` | Its durable boundary is represented in current `runtime.md`; the old file remains historical evidence only. |
| `docs/architecture/GRAPHICS_RENDER_PIPELINE_HARDENING_PLAN.md` | `DELETE_HISTORY_ONLY` | `runtime.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/GRAPHICS_TARGET_BOUNDARY_INVENTORY.md` | `DELETE_HISTORY_ONLY` | `module-boundaries.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/IMMUTABLE_AUDIT_LOG_V1.md` | `DELETE_HISTORY_ONLY` | `architecture.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/NETWORK_CHAOS_POLICY_FOUNDATION.md` | `DELETE_HISTORY_ONLY` | `testing.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/NIX_DEVELOPMENT_AND_VALIDATION_POLICY.md` | `MERGED_CURRENT` | `toolchain.md` | Its durable boundary is represented in current `toolchain.md`; the old file remains historical evidence only. |
| `docs/architecture/OWNERSHIP.md` | `MERGED_CURRENT` | `repository-layout.md` | Its durable boundary is represented in current `repository-layout.md`; the old file remains historical evidence only. |
| `docs/architecture/POLICY_DOMAIN_MODEL_V0.md` | `DELETE_HISTORY_ONLY` | `architecture.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/PROJECT_ROLE_PROFILES_V1.md` | `DELETE_HISTORY_ONLY` | `architecture.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/PROJECT_SLICE_RESOLVER_V1.md` | `DELETE_HISTORY_ONLY` | `architecture.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/PROJECT_SLICE_SCHEMA_V0.md` | `DELETE_HISTORY_ONLY` | `source-of-truth.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/PUBLISH.md` | `MERGED_CURRENT` | `toolchain.md` | Its durable boundary is represented in current `toolchain.md`; the old file remains historical evidence only. |
| `docs/architecture/PUBLISH_POLICY_INTEGRATION.md` | `MERGED_CURRENT` | `toolchain.md` | Its durable boundary is represented in current `toolchain.md`; the old file remains historical evidence only. |
| `docs/architecture/README.md` | `DELETE_HISTORY_ONLY` | `source-of-truth.md` | This retired docs-tree index only navigated removed material; Git history is sufficient. |
| `docs/architecture/RENDER_CANONICAL_RUNTIME_PATH.md` | `MERGED_CURRENT` | `runtime.md` | Its durable boundary is represented in current `runtime.md`; the old file remains historical evidence only. |
| `docs/architecture/RENDER_INTEREST_WORLD_STREAMING_CONTRACT.md` | `SHOULD_MERGE` | `runtime.md` | The interest-to-render streaming boundary needs current World, Replication, and Render ownership evidence. |
| `docs/architecture/RENDER_LIGHTING_STRATEGY.md` | `SHOULD_MERGE` | `runtime.md` | A concise lighting boundary would be useful after current renderer capability and evidence review. |
| `docs/architecture/RENDER_MATERIAL_GRAPH_AUTHORING_CONTRACT.md` | `SHOULD_MERGE` | `editor.md` | Material authoring, generated data, and runtime consumption ownership need current Editor/Render validation. |
| `docs/architecture/RENDER_OVERLAY_API_MIGRATION.md` | `DELETE_HISTORY_ONLY` | `runtime.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/RENDER_PUBLIC_API_CONTRACT.md` | `SHOULD_MERGE` | `module-boundaries.md` | The vendor-neutral render API needs an installed-consumer review before expanding its public documentation. |
| `docs/architecture/RESTRICTED_ARTIFACT_EXPORT_GATE.md` | `DELETE_HISTORY_ONLY` | `architecture.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/REVIEW_APPROVAL_WORKFLOW_V2.md` | `DELETE_HISTORY_ONLY` | `editor.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/RUNTIME.md` | `MERGED_CURRENT` | `runtime.md` | Its durable boundary is represented in current `runtime.md`; the old file remains historical evidence only. |
| `docs/architecture/SAGACHAOSLAB_V1.md` | `DELETE_HISTORY_ONLY` | `testing.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SAGA_ARTIFACT_CONTRACTS_V1.md` | `DELETE_HISTORY_ONLY` | `source-of-truth.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SAGA_COLLABORATION_CURRENT_BOUNDARY.md` | `MERGED_CURRENT` | `editor.md` | Its durable boundary is represented in current `editor.md`; the old file remains historical evidence only. |
| `docs/architecture/SAGA_COLLABORATION_PRESENCE_LOCK_BOUNDARY.md` | `MERGED_CURRENT` | `editor.md` | Its durable boundary is represented in current `editor.md`; the old file remains historical evidence only. |
| `docs/architecture/SAGA_COLLABORATION_REVIEW_AUDIT_BOUNDARY.md` | `DELETE_HISTORY_ONLY` | `editor.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SAGA_COLLABORATION_ROLE_PERMISSION_BOUNDARY.md` | `MERGED_CURRENT` | `editor.md` | Its durable boundary is represented in current `editor.md`; the old file remains historical evidence only. |
| `docs/architecture/SAGA_CSHARP_VISUAL_AUTHORING_BOUNDARY.md` | `MERGED_CURRENT` | `scripting.md` | Its durable boundary is represented in current `scripting.md`; the old file remains historical evidence only. |
| `docs/architecture/SAGA_DISTRIBUTION_STARTER_ARENA_WORKFLOW.md` | `DELETE_HISTORY_ONLY` | `toolchain.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SAGA_EDITOR_CUSTOMIZATION_MODEL.md` | `MERGED_CURRENT` | `editor.md` | Its durable boundary is represented in current `editor.md`; the old file remains historical evidence only. |
| `docs/architecture/SAGA_EDITOR_SHELL_MINIMUM_WORKFLOW.md` | `DELETE_HISTORY_ONLY` | `editor.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SAGA_ENTERPRISE_THREAT_MODEL_V0.md` | `DELETE_HISTORY_ONLY` | `server-authority.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SAGA_GAMEPLAY_NODE_LIBRARY_V1.md` | `DELETE_HISTORY_ONLY` | `scripting.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SAGA_GENERATED_FILES_POLICY.md` | `MERGED_CURRENT` | `source-of-truth.md` | Its durable boundary is represented in current `source-of-truth.md`; the old file remains historical evidence only. |
| `docs/architecture/SAGA_LINUX_ARCHIVE_CHECKSUM.md` | `DELETE_HISTORY_ONLY` | `architecture.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SAGA_LINUX_DISTRIBUTABLE_CANDIDATE.md` | `DELETE_HISTORY_ONLY` | `licensing.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SAGA_LINUX_DISTRIBUTION_LAYOUT.md` | `DELETE_HISTORY_ONLY` | `toolchain.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SAGA_LINUX_DISTRIBUTION_SMOKE.md` | `DELETE_HISTORY_ONLY` | `toolchain.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SAGA_LINUX_PACKAGE_PREFLIGHT.md` | `DELETE_HISTORY_ONLY` | `toolchain.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SAGA_LINUX_TOOL_PACKAGING.md` | `DELETE_HISTORY_ONLY` | `architecture.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SAGA_LOCAL_APPROVAL_PUBLISH_GATE_BOUNDARY.md` | `DELETE_HISTORY_ONLY` | `editor.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SAGA_LOCAL_WORKSPACE_TRANSACTION_BOUNDARY.md` | `MERGED_CURRENT` | `editor.md` | Its durable boundary is represented in current `editor.md`; the old file remains historical evidence only. |
| `docs/architecture/SAGA_MMO_GENRE_FOCUS.md` | `DELETE_HISTORY_ONLY` | `product.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SAGA_PERFORMANCE_BUDGETS.md` | `DELETE_HISTORY_ONLY` | `testing.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SAGA_PRODUCT_SHELL_WORKFLOW_CONTRACT.md` | `MERGED_CURRENT` | `product.md` | Its durable boundary is represented in current `product.md`; the old file remains historical evidence only. |
| `docs/architecture/SAGA_PROJECT_SLICE_VISIBILITY_BOUNDARY.md` | `DELETE_HISTORY_ONLY` | `module-boundaries.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SAGA_REVIEW_COMMENT_TRANSACTION_MODEL.md` | `MERGED_CURRENT` | `editor.md` | Its durable boundary is represented in current `editor.md`; the old file remains historical evidence only. |
| `docs/architecture/SAGA_SCENE_ASSET_AUTHORING_CONTRACT.md` | `SHOULD_MERGE` | `editor.md` | Scene and asset authoring ownership needs current EditorAuthoring and source-manifest evidence before merging. |
| `docs/architecture/SAGA_SCHEMA_PACKAGE_BOUNDARY.md` | `DELETE_HISTORY_ONLY` | `module-boundaries.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SAGA_WORKSPACEHUB_SERVER_ARCHITECTURE.md` | `PRIVATE_ARCHIVE_CANDIDATE` | `server-authority.md` | This speculative hosted-workspace design names a retired tool and would overstate current collaboration; keep it out of public SagaWiki. |
| `docs/architecture/SAMPLE_EXECUTABLE_EVIDENCE.md` | `DELETE_HISTORY_ONLY` | `testing.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SCENE_ENTITY_SOURCE_TRUTH_FOUNDATION.md` | `MERGED_CURRENT` | `source-of-truth.md` | Its durable boundary is represented in current `source-of-truth.md`; the old file remains historical evidence only. |
| `docs/architecture/SCENE_SCHEMA_V1.md` | `DELETE_HISTORY_ONLY` | `editor.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SDE_ARTIFACT_MANIFEST_CONTRACT.md` | `DELETE_HISTORY_ONLY` | `architecture.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SDE_CURRENT_CONTRACT.md` | `DELETE_HISTORY_ONLY` | `architecture.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/SERVER.md` | `MERGED_CURRENT` | `server-authority.md` | Its durable boundary is represented in current `server-authority.md`; the old file remains historical evidence only. |
| `docs/architecture/SOURCE_OF_TRUTH_MAP.md` | `MERGED_CURRENT` | `source-of-truth.md` | Its durable boundary is represented in current `source-of-truth.md`; the old file remains historical evidence only. |
| `docs/architecture/SOURCE_PATCH_APPLY_POLICY.md` | `MERGED_CURRENT` | `scripting.md` | Its durable boundary is represented in current `scripting.md`; the old file remains historical evidence only. |
| `docs/architecture/SOURCE_VISIBILITY_LEVELS_V1.md` | `DELETE_HISTORY_ONLY` | `architecture.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/architecture/TESTING_AND_EVIDENCE.md` | `MERGED_CURRENT` | `testing.md` | Its durable boundary is represented in current `testing.md`; the old file remains historical evidence only. |
| `docs/architecture/VISUAL_BLOCK_PATCH_CONTRACT.md` | `MERGED_CURRENT` | `scripting.md` | Its durable boundary is represented in current `scripting.md`; the old file remains historical evidence only. |
| `docs/dev/README.md` | `DELETE_HISTORY_ONLY` | `not-implemented.md` | This retired docs-tree index only navigated removed material; Git history is sufficient. |
| `docs/dev/SAGA_ENTERPRISE_CLAIM_LEVELS.md` | `DELETE_HISTORY_ONLY` | `not-implemented.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/dev/SCRATCH.md` | `DELETE_HISTORY_ONLY` | `not-implemented.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/internal/README.md` | `DELETE_HISTORY_ONLY` | `architecture.md` | This retired docs-tree index only navigated removed material; Git history is sufficient. |
| `docs/internal/architecture-appendices/AssetStreamingImplementation.md` | `PRIVATE_ARCHIVE_CANDIDATE` | `runtime.md` | This large internal proposal or formal note mixes durable ideas with unverified detail and should not become public SagaWiki text. |
| `docs/internal/architecture-appendices/ClientReplicationFormalSpec.md` | `PRIVATE_ARCHIVE_CANDIDATE` | `server-authority.md` | This large internal proposal or formal note mixes durable ideas with unverified detail and should not become public SagaWiki text. |
| `docs/internal/architecture-appendices/README.md` | `PRIVATE_ARCHIVE_CANDIDATE` | `architecture.md` | This large internal proposal or formal note mixes durable ideas with unverified detail and should not become public SagaWiki text. |
| `docs/internal/architecture-appendices/SAGAWEAVER_SOURCE_BRIDGE_ARCHITECTURE.md` | `PRIVATE_ARCHIVE_CANDIDATE` | `scripting.md` | This large internal proposal or formal note mixes durable ideas with unverified detail and should not become public SagaWiki text. |
| `docs/internal/architecture-appendices/SAGA_AUXILIARY_TOOLCHAIN_ARCHITECTURE.md` | `PRIVATE_ARCHIVE_CANDIDATE` | `toolchain.md` | This large internal proposal or formal note mixes durable ideas with unverified detail and should not become public SagaWiki text. |
| `docs/internal/architecture-appendices/SAGA_CAPABILITY_PACKAGE_SYSTEM_ARCHITECTURE.md` | `PRIVATE_ARCHIVE_CANDIDATE` | `module-boundaries.md` | This large internal proposal or formal note mixes durable ideas with unverified detail and should not become public SagaWiki text. |
| `docs/internal/architecture-appendices/SAGA_COLLABORATIVE_WORKSPACE_ARCHITECTURE.md` | `PRIVATE_ARCHIVE_CANDIDATE` | `editor.md` | This large internal proposal or formal note mixes durable ideas with unverified detail and should not become public SagaWiki text. |
| `docs/internal/architecture-appendices/SAGA_CSHARP_VISUAL_BLOCKS_ARCHITECTURE.md` | `PRIVATE_ARCHIVE_CANDIDATE` | `scripting.md` | This large internal proposal or formal note mixes durable ideas with unverified detail and should not become public SagaWiki text. |
| `docs/internal/product-history/DIAGNOSTICS_VISIBILITY_PRODUCT_LAYER_MILESTONE26_28.md` | `DELETE_HISTORY_ONLY` | `product.md` | This dated product-history artifact has no current authority; Git history is sufficient. |
| `docs/internal/product-history/MULTIPLAYER_SANDBOX_PLAYABLE_VERTICAL_SLICE_MILESTONE20_25.md` | `DELETE_HISTORY_ONLY` | `product.md` | This dated product-history artifact has no current authority; Git history is sufficient. |
| `docs/internal/product-history/MULTIPLAYER_SANDBOX_SMALL_TEAM_ALPHA_TUTORIAL.md` | `DELETE_HISTORY_ONLY` | `product.md` | This dated product-history artifact has no current authority; Git history is sufficient. |
| `docs/internal/product-history/NEXT_PRODUCT_SLICE.md` | `DELETE_HISTORY_ONLY` | `product.md` | This dated product-history artifact has no current authority; Git history is sufficient. |
| `docs/internal/product-history/PACKAGING_PUBLISH_PROOF_MILESTONE29_33.md` | `DELETE_HISTORY_ONLY` | `product.md` | This dated product-history artifact has no current authority; Git history is sufficient. |
| `docs/internal/product-history/README.md` | `DELETE_HISTORY_ONLY` | `product.md` | This dated product-history artifact has no current authority; Git history is sufficient. |
| `docs/internal/product-history/SAGAENGINE_0_1_TECHNICAL_PREVIEW_CLOSURE_MILESTONE60_65.md` | `DELETE_HISTORY_ONLY` | `product.md` | This dated product-history artifact has no current authority; Git history is sufficient. |
| `docs/internal/product-history/SAGAENGINE_0_1_TECHNICAL_PREVIEW_QUICKSTART.md` | `DELETE_HISTORY_ONLY` | `getting-started.md` | This dated product-history artifact has no current authority; Git history is sufficient. |
| `docs/internal/product-history/SAGAENGINE_SMALL_TEAM_ALPHA_QUICKSTART.md` | `DELETE_HISTORY_ONLY` | `getting-started.md` | This dated product-history artifact has no current authority; Git history is sufficient. |
| `docs/internal/product-history/SAGAWEAVER_SAGASCRIPT_MVP_MILESTONE34_43.md` | `DELETE_HISTORY_ONLY` | `scripting.md` | This dated product-history artifact has no current authority; Git history is sufficient. |
| `docs/internal/product-history/SAGA_MMO_GENRE_STRATEGY_DRAFT.md` | `PRIVATE_ARCHIVE_CANDIDATE` | `product.md` | This broad genre strategy is private product-direction history and must not become public SagaWiki positioning. |
| `docs/internal/product-history/SAGA_PRODUCT_DEFINITION.md` | `PRIVATE_ARCHIVE_CANDIDATE` | `product.md` | The durable neutral product boundary was merged, but the full historical strategy document should remain private context. |
| `docs/internal/product-history/SAGA_PROJECT_MODEL_INVENTORY_MILESTONE14.md` | `DELETE_HISTORY_ONLY` | `product.md` | This dated product-history artifact has no current authority; Git history is sufficient. |
| `docs/internal/product-history/SAGA_RELEASE_VOCABULARY_AND_CLAIM_LEVELS.md` | `DELETE_HISTORY_ONLY` | `product.md` | This dated product-history artifact has no current authority; Git history is sufficient. |
| `docs/internal/product-history/SMALL_TEAM_ALPHA_COLLABORATION_GUIDE.md` | `DELETE_HISTORY_ONLY` | `editor.md` | This dated product-history artifact has no current authority; Git history is sufficient. |
| `docs/internal/product-history/SMALL_TEAM_ALPHA_CSHARP_BLOCKS_AUTHORING_GUIDE.md` | `DELETE_HISTORY_ONLY` | `scripting.md` | This dated product-history artifact has no current authority; Git history is sufficient. |
| `docs/internal/product-history/SMALL_TEAM_ALPHA_KNOWN_LIMITATIONS.md` | `DELETE_HISTORY_ONLY` | `not-implemented.md` | This dated product-history artifact has no current authority; Git history is sufficient. |
| `docs/internal/product-history/SMALL_TEAM_ALPHA_TROUBLESHOOTING.md` | `DELETE_HISTORY_ONLY` | `getting-started.md` | This dated product-history artifact has no current authority; Git history is sufficient. |
| `docs/internal/product-history/TECHNICAL_PREVIEW_CANDIDATE.md` | `DELETE_HISTORY_ONLY` | `product.md` | This dated product-history artifact has no current authority; Git history is sufficient. |
| `docs/licensing/CONTRIBUTING_DCO_SECTION.md` | `DELETE_HISTORY_ONLY` | `licensing.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/licensing/GENERATED_OUTPUTS.md` | `MERGED_CURRENT` | `licensing.md` | Its durable boundary is represented in current `licensing.md`; the old file remains historical evidence only. |
| `docs/licensing/LICENSE_FOUNDATION.md` | `MERGED_CURRENT` | `licensing.md` | Its durable boundary is represented in current `licensing.md`; the old file remains historical evidence only. |
| `docs/licensing/LICENSE_STABILITY.md` | `MERGED_CURRENT` | `licensing.md` | Its durable boundary is represented in current `licensing.md`; the old file remains historical evidence only. |
| `docs/licensing/PLUGIN_LICENSING.md` | `PRIVATE_ARCHIVE_CANDIDATE` | `licensing.md` | This is useful future reference for Arda if stable plugin contracts become real, but it is not valid current public SagaWiki policy. |
| `docs/licensing/README.md` | `MERGED_CURRENT` | `licensing.md` | Its durable boundary is represented in current `licensing.md`; the old file remains historical evidence only. |
| `docs/licensing/THIRD_PARTY_NOTICES.md` | `MERGED_CURRENT` | `licensing.md` | Its durable boundary is represented in current `licensing.md`; the old file remains historical evidence only. |
| `docs/licensing/TRANSITION_CHECKLIST.md` | `DELETE_HISTORY_ONLY` | `licensing.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/licensing/decisions/LIC-DEC-0001.md` | `MERGED_CURRENT` | `licensing.md` | Its durable boundary is represented in current `licensing.md`; the old file remains historical evidence only. |
| `docs/media/sagasandbox/render-01.png` | `MERGED_CURRENT` | `index.md` | The bounded render evidence is already present under SagaWiki/assets. |
| `docs/media/sagasandbox/render-02.png` | `MERGED_CURRENT` | `index.md` | The bounded render evidence is already present under SagaWiki/assets. |
| `docs/product/CURRENT_CAPABILITIES.md` | `MERGED_CURRENT` | `product.md` | Its durable boundary is represented in current `product.md`; the old file remains historical evidence only. |
| `docs/product/CURRENT_DISTRIBUTION_STATUS.md` | `DELETE_HISTORY_ONLY` | `product.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/product/GETTING_STARTED.md` | `MERGED_CURRENT` | `getting-started.md` | Its durable boundary is represented in current `getting-started.md`; the old file remains historical evidence only. |
| `docs/product/README.md` | `MERGED_CURRENT` | `product.md` | Its durable boundary is represented in current `product.md`; the old file remains historical evidence only. |
| `docs/product/SAGAPROJ_SCHEMA_V0.md` | `MERGED_CURRENT` | `source-of-truth.md` | Its durable boundary is represented in current `source-of-truth.md`; the old file remains historical evidence only. |
| `docs/product/SAGA_ECOSYSTEM_MAP.md` | `DELETE_HISTORY_ONLY` | `product.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/product/SAGA_MMO_GENRE_FOCUS.md` | `DELETE_HISTORY_ONLY` | `product.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/product/WHAT_IS_NOT_IMPLEMENTED.md` | `MERGED_CURRENT` | `not-implemented.md` | Its durable boundary is represented in current `not-implemented.md`; the old file remains historical evidence only. |
| `docs/product/WHAT_IS_SAGAENGINE.md` | `MERGED_CURRENT` | `product.md` | Its durable boundary is represented in current `product.md`; the old file remains historical evidence only. |
| `docs/testing/README.md` | `MERGED_CURRENT` | `testing.md` | Its durable boundary is represented in current `testing.md`; the old file remains historical evidence only. |
| `docs/testing/SAGA_SMALL_TEAM_ALPHA_ACCEPTANCE_TEST_PLAN.md` | `DELETE_HISTORY_ONLY` | `testing.md` | This dated plan, snapshot, retired-tool contract, or strategy is not current authority; Git history is sufficient. |
| `docs/testing/TEST_SUITES.md` | `MERGED_CURRENT` | `testing.md` | Its durable boundary is represented in current `testing.md`; the old file remains historical evidence only. |
