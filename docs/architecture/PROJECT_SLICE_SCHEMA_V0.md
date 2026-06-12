# Project Slice Schema v0

This document defines a project slice as local metadata for a filtered project
view. A slice helps tools report which project artifacts are visible,
restricted, excluded, or missing for a role-oriented workflow.

Project slices are not authentication, identity, role enforcement, file
protection, encryption, cloud access control, or a boundary against direct
filesystem access. They are deterministic inputs for local reports.

## Storage

Source-controlled project slices live under:

```text
.saga/slices/*.json
```

Generated validation and resolution reports live under `Build/Enterprise` or
another caller-provided output path. Generated reports are evidence, not the
source of truth for the slice.

## Schema

```json
{
  "schemaVersion": 0,
  "sliceId": "designer-local",
  "displayName": "Designer Local Slice",
  "description": "Local report-only view for designer-facing project work.",
  "intendedRole": "Designer",
  "includedResources": [],
  "excludedResources": [],
  "visibilityRules": [],
  "diagnostics": []
}
```

Required fields:

- `schemaVersion`: must be `0`.
- `sliceId`: stable project-local identifier.
- `displayName`: human-readable name.
- `intendedRole`: local role profile label, not identity.
- `includedResources`: resources considered by the slice.
- `excludedResources`: resources explicitly outside the slice.
- `visibilityRules`: visibility classifications for included resources.

Optional fields:

- `description`: explanatory text.
- `diagnostics`: author-provided notes. Tool diagnostics are emitted in reports.

## Resource Targets

Resource entries use:

```json
{
  "kind": "ScriptFile",
  "id": "door-logic",
  "path": "Scripts/DoorLogic.High.cs",
  "visibility": "FullSource"
}
```

Supported target kinds:

- `Project`
- `ScriptFolder`
- `ScriptFile`
- `Behavior`
- `Node`
- `AssetRoot`
- `AssetFile`
- `PackageProfile`
- `LaunchProfile`
- `ReportArtifact`
- `CollaborationArtifact`

Each resource must provide either `id`, `path`, or both. Paths are always
project-root-relative.

## Path Safety

Slice paths must be deterministic and bounded:

- no absolute paths
- no parent traversal
- no empty path when path-based resolution is required
- no implicit path rewrite
- no source deletion or modification

Invalid paths produce diagnostics and fail validation or resolution.

## Resource Statuses

Slice reports use these statuses:

- `Included`: resource is part of the filtered view.
- `Excluded`: resource is intentionally outside the filtered view.
- `Restricted`: resource is represented by a placeholder or hidden entry.
- `Missing`: resource could not be found.
- `Invalid`: resource target is malformed.
- `Unknown`: resource kind or target cannot be classified.

## Exit Criteria

This document is satisfied when a local project slice can be parsed, validated, and
reported without changing project files or claiming runtime, editor, server, or
cloud behavior.
