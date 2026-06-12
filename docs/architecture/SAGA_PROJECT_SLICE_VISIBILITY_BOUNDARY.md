# Saga Project Slice Visibility Boundary

> Status: Local project-slice visibility report boundary

This document adds a Product Shell no-UI report for local project slice visibility
metadata. The report is a read-only metadata proof over a caller-provided target
artifact. It does not implement secure source hiding, permission enforcement,
restricted project resolution, a durable project slice service, cloud workspace,
real-time multi-user editing, CRDT/OT, or a collaboration server.

## Boundary Model

```text
local project slice visibility metadata
read-only preview/result
project truth remains unchanged
no enterprise enforcement
no collaboration server
no cloud sync
no real-time multi-user editing
```

## Report Command Shape

The report is produced by a local Product Shell no-UI slice smoke command over a
caller-provided project, workspace, actor, slice id, target artifact, and report
output path. The exact build directory and output path are local evidence
details, not architecture truth.

The report contains:

- `schemaVersion`
- `tool`
- `command`
- `status`
- `verified: false`
- `workspace`
- `project`
- `actor`
- `slice`
- `visibility`
- `diagnostics`
- `knownLimitations`
- `nonClaims`

The `slice` object records a deterministic report-only `sliceId`, the slice
display name, target artifact, `sliceMode: "MetadataOnly"`, status
`Represented` when the project manifest, slice name, and target artifact are
present, and `durable` and `mutatesProject` flags set to `false`.

The `visibility` object records whether the local actor can see the report
target in this metadata preview, plus `visibilitySource: "report-only"` and
`permissionEnforced`, `policyBacked`, and `networked` flags set to `false`.

## Non-Goals

This document does not resolve `.saga/slices`, does not produce a restricted project
view, and does not hide files from a local user. Existing project slice
architecture documents remain future/local tool concepts for broader slice
resolution work.

The command writes only the caller-provided report output path. It does not
mutate `.sagaproj`, scenes, scripts, SDE files, package profiles, diagnostics
folders, report folders, workspace files, or durable project slice metadata.

This document may reference local slice/visibility reports as related metadata, but
that does not create restricted project resolution or source hiding.
