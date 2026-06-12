# Saga Artifact Contracts V1

Status: Shared JSON artifact envelope contract.

This document defines the common JSON artifact envelope for future Saga tools.
It does not rewrite existing artifacts or claim legacy conformance.

## Common Envelope

Future JSON artifacts should use this envelope unless a narrower schema already
exists and is documented:

```json
{
  "schemaVersion": 1,
  "tool": "tool-name",
  "command": "command-name",
  "status": "Passed",
  "diagnostics": []
}
```

Recommended optional fields:

- `sourceHash`;
- `evidence`;
- `inputs`;
- `generatedBy`.

The envelope may be extended with tool-specific fields, but the shared fields
must keep stable names and deterministic ordering where the local writer
supports it.

## Field Rules

`schemaVersion` identifies the artifact schema version, not the product version.

`tool` names the producing tool or subsystem.

`command` names the operation that produced the artifact.

`status` uses a stable status vocabulary such as `Passed`, `Failed`,
`Accepted`, `Blocked`, `Deferred`, or `Generated` when the tool defines that
meaning.

`diagnostics` is present even when empty. Diagnostics should include severity,
code, message, and path when available.

`sourceHash` is optional, but required when freshness or source-backed behavior
depends on a specific source file.

`evidence` is optional structured support for the result. It should reference
paths, checks, hashes, or report ids rather than vague prose.

`inputs` is optional structured input metadata such as project path, profile
path, report path, command options, or timeout.

`generatedBy` is optional metadata for generator version, build configuration,
or tool version.

## Current Tool Classification

Conform now or near-conform:

- SagaProjectKit;
- SagaLaunchLab;
- SagaProbe;
- SagaPackager;
- SagaScript and SagaWeaver artifacts;
- SagaViewKit;
- SagaDocGuard;
- SagaPreviewGate;
- SagaAlphaGate.

Partial or legacy:

- native diagnostics artifacts that predate the envelope;
- native package artifacts that predate the envelope;
- native runtime artifacts that predate the envelope.

Partial and legacy artifacts may remain valid evidence when their owning docs
explain their schema and limitations. They must not be described as fully
conforming unless the artifact actually carries the envelope fields.

## Artifact Envelope Requirement

Future 75 artifacts must follow this envelope:

- node extraction reports;
- node metadata diagnostics;
- C# compatibility profile reports;
- source patch request and preview reports;
- runtime binding validation reports;
- publish integration reports for script and node metadata.

If a milestone cannot use the envelope, it must document the exception before the
artifact supports an acceptance claim.
