---
title: Source, artifact, and project contracts
status: current
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Source, artifact, and project contracts

SagaEngine uses multiple representations of the same project information. This reference defines which representation is authoritative, how derived artifacts prove freshness, and how project-relative boundaries prevent tools from silently mutating or escaping their inputs.

## One authority per concern

| Concern | Authority | Derived consumers |
| --- | --- | --- |
| Project identity and roots | `.sagaproj` | resolved paths, project reports, package staging |
| C# behavior | authored C# source | compiled assembly, node metadata, Visual Blocks projection, source map |
| Scene and entity authoring | project-owned scene source | validation report, runtime representation, generated evidence |
| Asset identity and ownership | project asset source manifest | inventory, runtime manifest, package artifact |
| Package graph | validated package manifests and project inputs | staged package, startup validation, distribution layout |
| Build graph | CMake sources, targets, presets | generated build directory and target graph |
| License policy | `LICENSE_POLICY.toml` and authoritative license texts | generated path rules, checksums, validation reports |
| Wiki knowledge | `SagaWiki/library` plus `wiki-library.json` | generated offline `index.html` |
| Runtime claim | current implementation plus applicable evidence | report, screenshot, or summary of a specific run |

Derived data can be required and valuable without becoming authoritative. Removing this distinction creates silent divergence: a generated graph can disagree with C#, a staged package can outlive its manifest, or a report can be edited into a state the producer never observed.

## `.sagaproj` manifest

SagaProjectKit validates schema version `0` of `.sagaproj`. The manifest identifies the project and declares project-relative roots and inputs used by project tools. SagaPackager consumes the same contract rather than defining another project schema. Sample manifests demonstrate valid shape but cannot override the validator.

Validation must reject:

- unsupported or unknown schema versions;
- missing required project identity;
- absolute paths where a project-relative path is required;
- parent traversal that escapes the project root;
- references to required files that do not exist;
- malformed JSON or incompatible field kinds;
- ambiguous duplicate identifiers where uniqueness is part of the contract.

Resolution normalizes paths under the project root and reports diagnostics. A validator must not silently rewrite source to make an invalid manifest pass.

### Validation phases

Project validation is easiest to reason about when phases remain visible:

1. read the exact selected manifest path;
2. parse JSON without accepting duplicate/ambiguous fields silently;
3. validate schema version and required scalar/collection shapes;
4. establish the canonical project root and identity;
5. normalize each declared project-relative path;
6. reject path escape and invalid/missing required targets;
7. load/validate referenced scene, asset, script, slice, profile, or package records through their owners;
8. build a deterministic readiness report without mutating source.

A report can contain multiple diagnostics, but later phases do not dereference data whose prerequisite failed. This avoids a malformed root field producing dozens of misleading missing-file errors.

Project identity is stable content/declared identity, not the current directory name. Moving the whole project tree does not change the project solely because its absolute host path changed.

## Common artifact vocabulary

Many tool artifacts use a common envelope vocabulary:

- `schemaVersion` identifies the artifact format, not the SagaEngine product version;
- `tool` or producer identity identifies who created the artifact;
- `command` identifies the operation;
- `status` records the producer-defined result;
- `diagnostics` remains present or explicitly empty according to the owning schema;
- `inputs` records the inputs relevant to reproducing or interpreting the result;
- `sourceHash` records source identity where freshness matters;
- `evidence` links supporting artifacts without embedding a mutable truth claim;
- `generatedBy` can record generator version/configuration.

This vocabulary does not impose one universal JSON schema. Each tool owns its exact fields, allowed statuses, sorting, and failure semantics. Unknown status or schema values fail clearly instead of being interpreted as success.

Artifact kinds distinguish source-derived compile output, asset/cooked data, package/staging output, report/evidence, source map/projection, and other registered domains. A consumer validates the kind before interpreting fields. Renaming a `.json` file cannot turn a report into a package manifest.

Statuses use positive success only for a completed operation. `Blocked`, `Failed`, `Invalid`, `Unsupported`, `Incomplete`, `Skipped`, `Stale`, and `NotRequested` are not synonyms. A gate defines which states are acceptable for each required/optional input.

## Generated artifact origin

A generated artifact that can affect behavior or a public claim needs origin metadata sufficient to answer:

1. Which source files and project manifest produced it?
2. Which generator, command, configuration, and schema were used?
3. Which source hash or content span was expected?
4. Has the relevant source changed since generation?
5. Is the artifact canonical, derived runtime input, cache, or evidence only?

Presence is not freshness. A file at the expected path can still be stale, produced for another project, or generated by an incompatible tool version.

Origin metadata should avoid machine-specific nondeterminism. Store project-relative source identities and content hashes rather than absolute developer paths. Tool version/contract and normalized configuration are included when they affect output. Timestamps can aid operations but are not the sole freshness signal.

If an artifact has multiple inputs, freshness covers the complete ordered/set identity defined by the generator: source files, manifest, settings/profile, relevant dependency artifacts, and generator contract. Checking only the primary file can accept output built with old settings or dependencies.

## Freshness decisions

Freshness validation should produce explicit states such as current, stale, missing, invalid, unknown, or not applicable according to the owning schema. It should never downgrade a mismatch into success merely because regeneration would be convenient.

Hash checks compare the expected source identity to actual source identity. Span checks compare the expected text/range before a patch. Manifest checks compare the expected schema and referenced inputs. A fresh artifact can be consumed; a stale artifact is blocked or reported according to the operation's safety requirements.

Generated evidence remains non-canonical even when fresh. Freshness proves correspondence to an input, not correctness of the behavior described by the input.

### Stale handling

A read-only inspection workflow can display stale output with a visible state. Runtime/package/publish consumption of required stale output fails. A developer workflow can explicitly invoke the owner to regenerate, then revalidate. No consumer updates an expected hash field to match whatever file it found.

Race-safe consumption rechecks the relevant identity at the handoff. A validator that hashes a file and a later compiler/package step that reopens it must prevent or detect replacement between check and use, for example through staged immutable inputs, open-handle identity, or another final hash.

## C# and generated behavior artifacts

Authored C# is behavior truth. Compiled assemblies, bindings, descriptors, node metadata, source maps, and Visual Blocks projections are derived views. A visual edit becomes authoritative only when it produces and applies a reviewed C# source change under the source-patch contract.

Generated C# must remain traceable to its origin. A descriptor may name a script, capability, binding, or generated span, but cannot silently create behavior absent from source. Runtime packages validate the assemblies and binding manifests they consume; they do not reconstruct authority from an editor graph.

## Scene and entity source truth

Project scene source identifies a scene with stable project-local identity and `SceneSourceTruth`. Entities carry unique scene-local identity, display metadata, `EntitySourceTruth`, component metadata, and optional asset references. Generated artifacts are explicitly non-canonical and may carry expected source hashes.

The validation path reports malformed files, missing identities, duplicate entity IDs, invalid source kinds, unsafe paths, missing asset owners, and stale generated evidence. It does not apply source repairs. Runtime `WorldState`, snapshots, deltas, imported data, and editor view models are consumers or projections of scene source, not replacement authoring truth.

## Asset source manifests

Asset source manifests record project-owned asset identifiers, owners, paths, usage, and references. An inventory report may list asset roots, discovered source manifests, owners, references, missing owners, and generated artifacts. Report flags such as local-only, no-network-exposure, no-source-mutation, or report-only enforcement describe the evidence path; they do not establish security enforcement.

Generated runtime asset manifests and package manifests are consumption inputs. They remain traceable to the source manifest and project. Runtime code should reject unsafe or unresolved references rather than search arbitrary filesystem locations.

An inventory report should distinguish canonical manifest entries from discovered-but-unowned files. Discovery can reveal cleanup/authoring gaps; it cannot silently register those files. Missing owner/reference and duplicate key/path are diagnostics that an author fixes in source.

## Project slices

A project slice describes a filtered, project-local view. It can include or exclude resources and assign visibility classifications. Its schema identifies:

- a stable slice ID and display name;
- an intended role label, which is not authenticated identity;
- included and excluded resources;
- visibility rules;
- optional description and author-provided notes.

Resource kinds can cover project, script folders/files, behaviors, nodes, asset roots/files, package profiles, launch profiles, report artifacts, and collaboration artifacts. The resolver checks known kinds, required target fields, duplicate resources, existence, and path safety.

Absolute paths, parent traversal, implicit path rewriting, source deletion, and source modification are forbidden. Resolution reports included, excluded, restricted, missing, invalid, and unknown resources without changing the project.

## Visibility levels

- `FullSource` permits a source-aware view to expose source content or a project-relative reference as allowed by the caller.
- `SourceLinkOnly` exposes identity and a project-relative link but not embedded source text.
- `SummaryOnly` exposes bounded metadata.
- `OpaqueRestricted` exposes a placeholder and diagnostics without the restricted path/content.
- `Hidden` omits the item from visible output and records the decision where the schema requires it.

These are local view/report classifications. They do not authenticate users, encrypt content, secure the filesystem, enforce server permissions, or prove redaction against an adversary.

## Evidence and reports

A report is an immutable record of one invocation. It should identify its inputs and producer, sort deterministic collections predictably, record incomplete/missing evidence honestly, and distinguish failure from unsupported or deferred behavior. A report consumer validates schema and freshness before using it in another gate.

Optional governance reports accepted by SagaPackager are externally produced inputs. SagaPackager does not produce them and no bundled governance workflow is implied. A packaging decision can require the report without changing its authority.

### Deterministic reports

Collections are sorted by stable project-relative identity unless order itself is semantic. JSON field ordering may be normalized for golden evidence. Paths use repository/project-relative POSIX form in cross-platform contracts. Numeric values use defined units. Messages do not include random temporary directories when a stable relative context is available.

A report write uses a temporary file and atomic replace where it could be consumed concurrently. Failed production does not leave an old successful report at the requested output path without marking/removing it. The producer's process exit agrees with the report status contract.

### Evidence linking

One report can reference another by path plus hash/schema/producer identity. A path alone can resolve to a newer unrelated file. Aggregating gates validate every link and keep the upstream result/non-claims visible rather than copying only `passed: true`.

## Wiki generation

Markdown under `SagaWiki/library` is current documentation truth. `wiki-library.json` owns article metadata and historical traceability. `scripts/wiki build` renders a single offline HTML reader and the required DCO policy bridge. `scripts/wiki verify` checks registry completeness, links, source metadata, generated freshness, historical source availability, claim language, and the contribution-policy bridge.

Generated `SagaWiki/index.html` is committed output for browsing but must not be edited as source. The historical 0.0.10 docs are evidence inputs for this rewrite; they do not become a second live documentation root.

## Change checklist

For a new source or artifact contract:

- name the authority and every derived representation;
- define schema and version ownership;
- constrain paths to the project/repository boundary;
- define required identifiers and uniqueness;
- define deterministic diagnostics and status values;
- record origin/freshness fields where behavior can go stale;
- state whether the operation mutates source;
- state whether policy output is advisory, report-only, or enforced;
- add malformed, missing, stale, duplicate, and traversal evidence;
- update the Wiki reference without treating generated output as canonical.
