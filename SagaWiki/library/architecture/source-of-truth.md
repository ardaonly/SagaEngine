---
title: Source-of-truth map
status: policy
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Source-of-truth map

SagaEngine follows one rule across projects, scripts, scenes, generated files, packages, and evidence: many views may exist, but each behavior or decision has one declared authority.

| Concern | Authority | Derived or supporting artifacts |
| --- | --- | --- |
| Project identity and roots | `.sagaproj` manifest | Resolved paths, package staging plans, sample manifests |
| Script behavior | C# source | Visual projections, binding descriptors, compiled assemblies, source maps |
| Scene/entity authoring | Project-owned scene source | Imported/runtime representations and evidence fixtures |
| Build graph | CMake declarations and presets | Generated build directory and target listings |
| License policy | `LICENSE_POLICY.toml` and recorded license texts | Path rules, checksums, validation reports |
| Wiki knowledge | `SagaWiki/library/*.md` plus registry metadata | Generated `SagaWiki/index.html` reader |
| Runtime claims | Implemented code plus relevant tests/evidence | Reports and screenshots from a specific run |

## Freshness

Generated output must carry enough origin information to detect stale inputs where the owning contract requires it. A generated file being present does not prove it matches its source. Builders should reject or report mismatched hashes, spans, schema versions, or package inputs rather than silently treating stale output as current.

## Project manifest contract

Schema version `0` of `.sagaproj` is validated by SagaProjectKit. SagaPackager consumes that same manifest contract; it does not define a competing schema. Sample manifests demonstrate valid use but are not specification authorities.

## Evidence is not authority

A report is immutable evidence of one invocation. It may support a claim when its producer, inputs, result, and freshness are known. Editing a report cannot change the source contract it was meant to verify.

## Structured artifact envelope

Tool reports commonly identify a schema version, producer/tool, command, status, diagnostics, and relevant inputs. Source hashes, evidence links, and generator identity are added when freshness or provenance requires them. The schema version belongs to the artifact format, not to the whole product. This vocabulary is useful only when each owning tool validates its own fields; it is not one universal report schema imposed on every subsystem.

## Project slices and visibility

SagaProjectKit can resolve project-local slices without mutating project source. A slice selects or excludes resources and classifies their visibility as `FullSource`, `SourceLinkOnly`, `SummaryOnly`, `OpaqueRestricted`, or `Hidden`. Paths must remain project-relative: absolute paths and parent traversal are rejected. These levels control report/view disclosure; they are not authentication, identity, or remote permission enforcement.

## Detailed references

- [Source, artifact, and project contracts](../reference/source-artifact-and-project-contracts.md)
- [Scene, entity, and project-slice contracts](../reference/scene-entity-and-project-slice-contracts.md)
- [C# and Visual Blocks contracts](../reference/csharp-and-visual-blocks-contracts.md)
