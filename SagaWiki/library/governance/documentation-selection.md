---
title: Documentation selection protocol
status: policy
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Documentation selection protocol

SagaWiki is smaller than the historical `docs/` tree because one current contract replaces repeated status/milestone copies, not because technical detail is meant to disappear. Selection is based on future maintenance value, source traceability, and current 0.0.11 truth rather than file count.

The detailed rewrite selected 82 historical sources containing about 50,556 raw words. Raw parity is not the maintenance target: those words included repeated release-state wrappers, old ownership paths, duplicate non-claims, and time-bound progress language. The retained architecture, data fields, lifecycle, invariants, failure behavior, evidence, and non-claims have been rewritten into the 40-article current library, with 17 long-form technical references. `scripts/wiki old-docs-report` verifies that all 82 selected paths still exist at the historical reference and remain linked by the registry.

## The six-month test

A document or paragraph belongs in the current Wiki only when a maintainer six months from now is likely to need it to answer at least one durable question:

1. What owns this behavior or contract?
2. What is the source of truth?
3. Which dependency or lifecycle direction must remain intact?
4. What evidence is required before making a capability claim?
5. Which explicit non-claim prevents a misleading interpretation?
6. What supported workflow is still expected to work?

It must also agree with current code, tests, paths, and policy. Valuable ideas with stale owners are rewritten; they are not copied verbatim.

## Retain and synthesize

Architecture contracts, source-truth rules, test/evidence taxonomy, diagnostic reliability, authority boundaries, editor transaction semantics, current tool ownership, and licensing authority survive when current evidence supports them. Multiple old files may become one maintained article.

Synthesis must not mean reducing a subsystem to a marketing paragraph. A durable technical reference retains, where present and still true:

- semantic owner and dependency direction;
- public/private and source/derived boundaries;
- data/record fields and status vocabulary;
- lifecycle and state transitions;
- path, freshness, authority, thread, memory, and lifetime invariants;
- failure, cancellation, rollback, retry, and shutdown behavior;
- diagnostics and deterministic reporting;
- test category and exact evidence limit;
- explicit non-claims that prevent adjacent capability inflation.

Version labels, progress trackers, “next milestone” text, one-machine report paths, and old root layouts are removed or translated. Their technical rule remains when it passes the six-month test.

## Keep in Git history

Release plans, phase and milestone records, preview/alpha gates, migration closures, inventories, audit snapshots, evidence bundles, status reports, and old version planning answer “what happened then,” not “how does the repository work now.” Git history is the appropriate archive.

## Private future reference

Product strategy, broad genre positioning, internal organizational material, and proposals for domains that do not yet exist—such as public plugin licensing—do not belong in the public Wiki. They may be useful to Arda later, but SagaWiki must not make them current policy.

## Traceability

`SagaWiki/wiki-library.json` records the historical source documents synthesized into each article. `scripts/wiki old-docs-report` validates those references against the selected 0.0.10 source without restoring `docs/`.

## Detailed migration map

The table below groups every selected source by its main detailed destination. A source may also support a concise entry page and can appear in more than one reference when it defines a cross-cutting boundary. Exact path-level mappings remain machine-readable in `wiki-library.json`.

| Current detailed reference | Historical inputs retained | Durable knowledge carried forward |
| --- | --- | --- |
| Product and capability reference | `WHAT_IS_SAGAENGINE`, `CURRENT_CAPABILITIES`, `GETTING_STARTED`, `WHAT_IS_NOT_IMPLEMENTED` | Neutral product definition, repository-first entry, capability evidence levels, public tool direction, project-owned depth, and explicit product/non-product limits. Broad genre positioning was not retained as public identity. |
| Ownership and dependency contracts | `CodingStandards`, `DependencyGraph`, `ARCHITECTURE_OVERVIEW`, `OWNERSHIP` | Module owners, dependency direction, target/include/install visibility, programs versus reusable behavior, vendor/native privacy, test boundaries, retired-root prohibition, and review inventory. |
| Source, artifact, and project contracts | `SOURCE_OF_TRUTH_MAP`, `GENERATED_ARTIFACT_FRESHNESS_CONTRACT`, `SAGA_GENERATED_FILES_POLICY`, `SAGA_ARTIFACT_CONTRACTS_V1`, `SAGAPROJ_SCHEMA_V0`, `SCENE_ENTITY_SOURCE_TRUTH_FOUNDATION` | One authority per concern, project validation phases, artifact envelope/kinds/status, origin, complete-input freshness, stale handling, deterministic reports, and generated-output rules. |
| Runtime lifecycle, assets, and packages | `RUNTIME`, `ASSETS_AND_PACKAGES`, `ASSET_SOURCE_MANIFEST_INVENTORY`, `PUBLISH_POLICY_INTEGRATION`, `SAGAPROJ_SCHEMA_V0` | Runtime composition/startup/shutdown, project input, manifest layers, identity/registration, artifact/package startup validation, authoring/runtime separation, reload, diagnostics, and evidence. |
| Resource streaming and residency | `ASSET_SOURCE_MANIFEST_INVENTORY`, `AssetStreamingImplementation` | Stable identity, immutable registry bootstrap, VFS/source separation, request state machine, priority/backpressure, atomic publication, deduplicated streaming, reference-counted residency, LRU/budgets, typed consumers, thread/lock invariants, reload, diagnostics, and deterministic tests. |
| Rendering and graphics contracts | `ENGINE_RENDER_EXTENSION_BOUNDARY_CONTRACT`, `GRAPHICS_BACKEND_PREFERENCE_ORDER`, `GRAPHICS_CAPABILITY_MATRIX_V0`, `GRAPHICS_COORDINATE_CONVENTION`, `RENDER_CANONICAL_RUNTIME_PATH`, `RENDER_INTEREST_WORLD_STREAMING_CONTRACT`, `RENDER_LIGHTING_STRATEGY`, `RENDER_MATERIAL_GRAPH_AUTHORING_CONTRACT`, `RENDER_PUBLIC_API_CONTRACT` | RHI/Render ownership, config-first extension, target/install boundary, coordinate convention, capability/fallback, singular Diligent lifecycle, frame/snapshot path, relevance/residency separation, material/lighting/shadow, graph, binding, capture, and CPU/GPU evidence levels. |
| World and simulation contracts | `ENGINE_WORLD_PUBLIC_FACADE_CONTRACT`, `SCENE_ENTITY_SOURCE_TRUTH_FOUNDATION` | Narrow facade, authoring/live/network/render/persistence truth layers, scene activation, ECS/simulation, cells, relevance, commands/events, cancellation, diagnostics, and persistent-world non-claim. |
| Replication, networking, and server authority | `AUTHORING_AUTHORITY_MODEL`, `NETWORK_CHAOS_POLICY_FOUNDATION`, `SERVER`, `ClientReplicationFormalSpec` | Packet categories/envelope/defensive decode, client state machine, sequence/baseline/delta, atomic apply, authority, prediction/reconciliation, interpolation/time, thread/memory/rate/fragment bounds, interest, zones/transfers, commands, deterministic chaos, resync, compatibility, disconnect, security limits, and formal invariants. |
| Diagnostics, reliability, and observability | `DIAGNOSTICS_CRASH_CONTEXT_AND_RELIABILITY_RULES`, `DIAGNOSTICS_MEMORY_RESOURCE_SNAPSHOT_FOUNDATION`, `DIAGNOSTICS_OPERATIONAL_REPORTS_AND_SERVER_OBSERVABILITY` | Stable diagnostic shape, fault/crash boundaries, health/lifecycle/retry, lifetime leaks, memory/resource snapshots, bounded operational reports, cardinality, server-authority reliability, tests, and hosted-observability non-claim. |
| Editor shell and authoring contracts | `APPS_SANDBOX_ROLE`, `EDITOR`, `SAGA_EDITOR_CUSTOMIZATION_MODEL`, `SAGA_EDITOR_SHELL_MINIMUM_WORKFLOW`, `SAGA_PRODUCT_SHELL_WORKFLOW_CONTRACT` | Current editor module owners, shell role/minimum workflow, project open/readiness, shared project versus personal view, composition/panels, Qt boundary, semantic operations, inspector/viewport/asset/script integration, customization, report-preserving launch workflows, cancellation, and editor product limits. |
| C# and Visual Blocks contracts | `CSHARP_VISUAL_BLOCKS_COMPATIBILITY_PROFILE`, `SAGA_CSHARP_VISUAL_AUTHORING_BOUNDARY`, `SAGA_GAMEPLAY_NODE_LIBRARY_V1`, `SOURCE_PATCH_APPLY_POLICY`, `VISUAL_BLOCK_PATCH_CONTRACT` | C# authority, projectable/opaque categories, high/low/unsafe authoring, projection record fields, graph/IR limits, node descriptors, operation/rejection vocabulary, non-mutating preview, hash/span/root-preserving apply, staleness, compile/runtime boundary, and explicit no-graph-VM rule. |
| Collaboration transactions and local policy | `DANGEROUS_OPERATION_POLICY_GATE_V1`, `POLICY_DOMAIN_MODEL_V0`, `PROJECT_ROLE_PROFILES_V1`, `REVIEW_APPROVAL_WORKFLOW_V2`, `SAGA_COLLABORATION_CURRENT_BOUNDARY`, `SAGA_COLLABORATION_PRESENCE_LOCK_BOUNDARY`, `SAGA_COLLABORATION_REVIEW_AUDIT_BOUNDARY`, `SAGA_COLLABORATION_ROLE_PERMISSION_BOUNDARY`, `SAGA_LOCAL_APPROVAL_PUBLISH_GATE_BOUNDARY`, `SAGA_LOCAL_WORKSPACE_TRANSACTION_BOUNDARY`, `SAGA_REVIEW_COMMENT_TRANSACTION_MODEL` | Workspace-scoped semantic transactions, local metadata, presence, lock lifecycle, comments/reviews, roles/permission decisions, policy domains, dangerous operations, approval/publish distinction, conflicts, logs/audit, sessions/transport, offline/failure behavior, public-surface debt, and hosted/security non-claims. |
| Scene, entity, and project-slice contracts | `ENTITY_SCHEMA_V1`, `PROJECT_SLICE_RESOLVER_V1`, `PROJECT_SLICE_SCHEMA_V0`, `SAGA_PROJECT_SLICE_VISIBILITY_BOUNDARY`, `SAGA_SCENE_ASSET_AUTHORING_CONTRACT`, `SCENE_ENTITY_SOURCE_TRUTH_FOUNDATION`, `SCENE_SCHEMA_V1`, `SOURCE_VISIBILITY_LEVELS_V1` | Scene/entity schema and asset references, source-preserving semantic transactions, runtime prerequisites, slice purpose/schema/path safety, deterministic resolver precedence, resource targets, visibility as presentation rather than security, inventory views, and failure/evidence. |
| Testing, claims, and evidence | `CLAIM_AND_EVIDENCE_POLICY`, `SAGACHAOSLAB_V1`, `SAMPLE_EXECUTABLE_EVIDENCE`, `TESTING_AND_EVIDENCE`, `TEST_SUITES` | Claim vocabulary/scope, unit/architecture/installed/private/integration/GPU/sample/stress taxonomy, safe local acceptance, labels, determinism, bounded seeded chaos, report/manual evidence, and honest validation reporting. |
| Build, package, and distribution contracts | `NIX_DEVELOPMENT_AND_VALIDATION_POLICY`, `PUBLISH`, `PUBLISH_POLICY_INTEGRATION`, `SAGA_LINUX_ARCHIVE_CHECKSUM`, `SAGA_LINUX_DISTRIBUTION_LAYOUT`, `SAGA_LINUX_DISTRIBUTION_SMOKE`, `SAGA_LINUX_PACKAGE_PREFLIGHT`, `SAGA_LINUX_TOOL_PACKAGING` | Forge/CMake/tool ownership, Nix limits, exact staged layout/whitelist, packaged .NET tool launchers, preflight/stale cleanup, safe archive root, checksum meaning, unpacked smoke, candidate aggregation, native dependency closure, artifact license review, publish hash/approval boundary, and distribution non-claims. |
| Licensing, contribution, and third-party governance | `CONTRIBUTING_DCO_SECTION`, `GENERATED_OUTPUTS`, `LICENSE_FOUNDATION`, `LICENSE_STABILITY`, `THIRD_PARTY_NOTICES`, `LIC-DEC-0001` | Current authority order/domains/MPL direction, stability, DCO and inbound-equals-outbound, copied/generated/AI provenance, third-party/vendor rules, dependency acceptance, Qt/persistence considerations, artifact-specific notice review, generated controls, and failure escalation. |
| Engineering conventions and contract review | `CodingStandards`, `DependencyGraph` | Owner clarity, useful public/function comments, external mappings, invariants, diagnostics, includes, tests, generated-output hygiene, focused commit/refactor practice, and a durable review checklist. |

## Coverage decision

The 82-source set is the positive selection, not a claim that the other historical files were empty. The unselected files were dominated by time-bound plans, release histories, audit/inventory snapshots, closure notes, superseded path analyses, evidence bundles, or product strategy. If a future change reveals a durable rule that was missed, it should be rewritten into the relevant current reference and added to registry traceability; the old tree still must not be restored as a competing documentation root.

The correct maintenance question is therefore not “did every old file get its own page?” It is “can a maintainer find the current owner, contract, data/lifecycle/failure rules, evidence, and limitation without reading the old release history?”
