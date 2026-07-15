---
title: Scene, entity, and project-slice contracts
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Scene, entity, and project-slice contracts

Scene/entity source and project-slice views solve different problems. Scene/entity schemas define canonical authored content. Project slices and source-visibility levels define bounded views over project resources for a workflow or role. Neither replaces the `.sagaproj` manifest or creates an alternate source tree.

## Source truth

The `.sagaproj` manifest identifies the project and its declared roots/entry references. Scene source identifies scene identity and entity composition. Entity source or embedded entity records identify entity/component data. Asset manifests connect canonical asset keys to validated runtime/cooked paths.

Runtime ECS, render snapshots, project-browser inventories, project slices, and generated scene artifacts are derived representations. They do not write back silently.

## Scene schema

A scene record carries schema version, stable scene id/name as defined, entity declarations/references, and any supported scene-level settings. Required fields and types are validated. Unknown fields follow the schema compatibility rule: preserve or reject deliberately, never discard merely because the current editor view does not display them.

Scene-relative references are resolved under the project root. Absolute paths and traversal are rejected. Duplicate scene/entity identifiers, missing required entity source, invalid component data, or malformed asset references produce structured diagnostics.

A generated scene artifact includes source fingerprint, tool/schema contract, produced hash, and dependencies. Runtime accepts it only when freshness and compatibility checks pass.

## Entity schema

An entity record has stable identity within its scene/world domain, display/name metadata where supported, component declarations, and asset references. Component type ids/names are registered and schema-versioned according to the owning runtime/editor contract.

Validation checks duplicate ids/components, required fields, supported component schema, typed values/ranges, finite numeric data, and reference shape. A missing required asset reference blocks readiness. An optional missing reference can produce a declared fallback/diagnostic.

Editor selection and hierarchy row ids map to entity identity but are not stored as behavior. Reordering a hierarchy view does not change source unless hierarchy order is a declared scene semantic and a transaction is applied.

## Asset reference ownership

Scene/entity source stores canonical asset keys or artifact references according to schema, not machine-specific native paths. The asset manifest and resolver map those keys to runtime identity/path. The editor can show resolved status without rewriting the source key to an absolute path.

References distinguish source assets, generated artifacts, and packages. A thumbnail or cache path is not a valid replacement for the canonical asset reference.

## Scene authoring transaction

Authoring changes target scene/entity/component/property identity plus expected source hash/version. Preview validates type, range, reference, policy/lock state where applicable, and shows the exact semantic result. Apply writes through the scene-authoring owner and revalidates the complete affected record.

Create/delete/reparent operations carry stronger structural checks. Delete reports inbound references and dangerous-operation policy. Reparent prevents cycles and preserves transform semantics according to explicit local/world policy. An editor drag is not committed until the transaction succeeds.

Unsupported fields/components remain preserved. A limited editor must not rewrite an entire scene serialization and erase data it does not understand.

## Runtime read prerequisites

Before runtime activation:

- project and scene schema validate;
- entity/component registrations are known or compatible under policy;
- required asset/artifact references resolve and are fresh;
- paths remain inside allowed project/package roots;
- generated artifacts identify their source/tool contract;
- startup/activation can fail without half-committing live world state.

Runtime does not search old repository roots or import arbitrary authoring source as a fallback.

## Project-slice purpose

A project slice is a named, versioned view over project resources for navigation, review, role focus, or a bounded workflow. It can include/exclude paths/resource targets and carry visibility hints. It does not change which files exist, which source is canonical, or what the build consumes unless a separate build profile contract explicitly uses it.

Examples include a scene-authoring slice, scripting slice, review slice, or diagnostic focus. These are neutral workflow terms, not historical release stages.

## Slice storage and schema

Slice definitions live under a project-declared or tool-owned location according to current SagaProjectKit contract. A slice includes schema version, id/name, optional description, include/exclude/resource target rules, and visibility level assignments where supported.

Validation checks:

- supported schema version;
- unique slice id/name under the project;
- normalized project-relative paths;
- no absolute/traversal escape;
- known resource target kind;
- deterministic include/exclude precedence;
- known visibility level;
- no reference to another project root;
- bounded rule count and diagnostic output.

An invalid slice does not invalidate canonical project source by itself unless the selected workflow requires that slice. It blocks or degrades that view explicitly.

## Slice resolver

The resolver receives validated project inventory and one validated slice. It applies rules deterministically and produces copied resource records with included/excluded reason, effective visibility, source/artifact status, and diagnostics.

The resolver does not read file contents unnecessarily, mutate files, generate artifacts, or change source-control state. It does not infer resources from arbitrary glob matches outside the project inventory.

Include/exclude precedence is explicit. For example, an exact exclusion can override a broad include according to schema; the rule order is not accidental JSON order unless the contract says it is ordered. The report identifies the rule that determined the result.

## Resource targets

Targets can refer to scene, entity/component source, C# source, asset source, manifest, generated artifact, package/report, or other registered project resource kinds. Each target resolves through its owner and reports current/missing/stale/invalid/unsupported status.

A resource target does not turn an output into source truth. Showing a generated C# projection, compiled assembly, or package report in a slice keeps its derived status visible.

## Visibility levels

Visibility levels describe authoring presentation, not filesystem permission or runtime authority. Durable levels can represent high-level, low-level, advanced, hidden-by-default, or unavailable/unsupported according to the current model.

High-level emphasizes intention-oriented project resources. Low-level reveals explicit engine/source/config details. Advanced reveals opaque/manual source that tools preserve but may not edit. Hidden removes an item from a view but does not delete it. Unsupported is a status, not a secret.

The current UI/profile maps resource kinds and user/workflow settings to an effective level. It must not describe visibility as security. A user can still access project files through the filesystem unless a real access-control system exists.

## Role and view compatibility

A role/profile can suggest a slice or visibility level through local policy. That helps reduce clutter and focus review. It does not authenticate the role or prevent source mutation outside the editor.

Changing personal visibility does not modify the shared slice. Changing a shared slice definition is itself a project transaction with review/source-control implications.

## Inventory and browser view

Project inventory records canonical resources, kinds, paths/keys, source/artifact relationship, validation state, and references sufficient for navigation. The browser derives sections and status badges from inventory/slice resolution.

The browser must show hidden/stale/missing counts when filters omit details. It never claims “project complete” because a slice hides failing resources.

## Failure behavior

Failures distinguish invalid schema, unsafe path, duplicate id, missing resource, unsupported target kind, stale artifact, unknown visibility level, conflicting rules, no matching resource, and owner validation failure. Reports contain project/slice/rule/resource context.

Resolver failure leaves project files unchanged. A stale slice report is refreshed against the latest inventory rather than patched heuristically.

## Evidence

Schema tests prove accepted/rejected scene/entity/slice forms. Path tests prove boundary handling. Resolver tests prove deterministic precedence and status. Authoring tests prove stale transaction rejection and preservation of unknown fields. Integration tests can open a sample project and build a browser/readiness view.

This evidence does not establish a complete scene editor, public scene SDK, secure role-based access control, production world persistence, or arbitrary old-project migration.

## Change checklist

- Keep project, scene/entity source, assets, artifacts, runtime, and views distinct.
- Use stable semantic ids and project-relative paths.
- Preserve unknown valid source data during limited authoring.
- Make structural operations freshness-checked transactions.
- Treat slices as views, not alternate projects or build truth.
- Treat visibility as presentation, not security.
- Keep derived status visible even when filtered.
- Add path, schema, precedence, staleness, and preservation tests.
