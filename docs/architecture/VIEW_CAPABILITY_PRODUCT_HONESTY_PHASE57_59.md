# View Capability Product Honesty Phase 57-59

Date: 2026-05-31

Block I adds report-only product honesty infrastructure. It defines what each
view is allowed to show, what it must disclose, and which claims documentation
must reject unless evidence exists. This block does not add editor UI, graph
editing, source rewriting, patch application, runtime interpretation, hosted
collaboration, or release-readiness behavior.

## Phase Matrix

| Phase | Scope | Evidence |
|---|---|---|
| 57 - SagaViewKit v1 | Standalone `Tools/SagaViewKit` validates view capability manifests and emits built-in profiles. | `test_sagaviewkit_cli.py` manifest/profile tests. |
| 58 - Simple View Must Not Lie Gate | `sagaviewkit check-simple` validates that Simple View projections disclose unsupported or advanced regions and do not mark them editable. | `simple_view_honesty_report.json` tests. |
| 59 - SagaDocGuard v1 | Standalone `Tools/SagaDocGuard` scans docs with deterministic regex/evidence checks. | `test_sagadocguard_cli.py` scan tests. |

## View Profiles

`SagaViewKit` uses schema version 1 capability manifests with:

```text
schemaVersion
viewId
viewKind
allowedApiDomains
allowedApiLevels
canShowOpaqueNodes
mustDiscloseOpaqueNodes
canEditSupportedNodes
canEditUnsupportedNodes
canApplyPatch
canMutateSource
canRegenerateSource
canShowSourceLinks
canShowDiagnostics
canShowCollaborationMetadata
requiredEvidenceArtifacts
```

Built-in profiles:

| Profile | Accepted capability |
|---|---|
| Simple View | Shows high-level gameplay projections and disclosed opaque/read-only regions. |
| Pro View | Shows high/low projections, source links, diagnostics, and disclosed opaque/read-only regions. |
| C# Source View | Shows source links/snippets without rewrite or regeneration. |
| Diagnostics View | Shows diagnostics reports and summaries. |
| Team Room View | Shows local/offline collaboration metadata only. |

## Simple View Honesty

`sagaviewkit check-simple` consumes `projection_report.json`, optionally
`node_metadata.json`, and a Simple View profile. It writes
`simple_view_honesty_report.json` with deterministic violations, opaque counts,
unsupported counts, and diagnostics.

Failure codes:

```text
View.Simple.UnsupportedNodeMarkedEditable
View.Simple.OpaqueDisclosureMissing
View.Simple.AdvancedRegionHidden
View.Simple.RequiredEvidenceMissing
View.Simple.SourceMutationCapabilityForbidden
View.Simple.PatchApplyCapabilityForbidden
```

The validator never modifies projection artifacts or source files.

## DocGuard

`SagaDocGuard` is a string/regex scanner. It records positive forbidden matches,
reviewed non-claim matches, missing required non-claims, and missing evidence
files. It does not rewrite docs, perform AI reasoning, or claim legal
compliance.

Required evidence includes the accepted product, diagnostics, package,
SagaWeaver, editor authoring, collaboration metadata, and Block I evidence
documents.

## Accepted Claim

SagaEngine now has deterministic view capability and documentation honesty
guards for Technical Preview evidence boundaries.

## Non-Claims

- no beta product status
- no candidate release status
- no production MMO server
- no complete visual scripting
- no arbitrary C# roundtrip
- no enterprise readiness
- no source rewrite path
- no graph editing path
- no patch application path
- no hosted collaboration path
- no public Qt/API expansion

## Checkpoint

Block I closes at Phase 59 when `SagaViewKit` and `SagaDocGuard` focused tests
pass, the Block I evidence document exists, and scans confirm no Phase 60 or UI
work started.
