# Saga Collaboration Role / Permission Boundary

> Status: Local role/permission intent report boundary

This document adds a Product Shell no-UI report for local actor role and permission
intent metadata. The report is local, read-only, and report-only. It does not
implement enterprise permission enforcement, secure access control, an
enterprise policy engine, durable role or permission services, networked
authorization, cloud workspace, real-time multi-user editing, CRDT/OT, or a
collaboration server.

## Boundary Model

```text
local actor role metadata
local permission intent metadata
read-only preview/result
project truth remains unchanged
no enterprise enforcement
no collaboration server
no cloud sync
no real-time multi-user editing
```

## Report Command Shape

The report is produced by a local Product Shell no-UI role smoke over a
caller-provided project, workspace, actor, role label, permission label, and
report output path. The exact build directory and output file are local evidence
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
- `role`
- `permission`
- `diagnostics`
- `knownLimitations`
- `nonClaims`

The `role` object records a deterministic report-only `roleId`, the actor id,
the role name, and `durable`, `enforced`, and `networked` flags set to `false`.

The `permission` object records a deterministic report-only `permissionId`, the
permission name, the project manifest as `targetArtifact`, status
`Represented` when the project manifest and permission name are present, and
`enforced`, `policyBacked`, and `mutatesProject` flags set to `false`.

## Non-Goals

This document does not activate the Editor collaboration permission scaffolds and
does not link Product Shell behavior to `PermissionManager`,
`PermissionPolicy`, or `CollaboratorRole`.

The command writes only the caller-provided report output path. It does not
mutate `.sagaproj`, scenes, scripts files, package profiles, diagnostics
folders, report folders, workspace files, or durable collaboration metadata.

This document may reference role labels in a local approval gate preview, but that
does not turn role metadata into permission enforcement or enterprise policy.
