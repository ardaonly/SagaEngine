# Saga Collaboration Role / Permission Boundary

Phase 28 status is `Implemented-Unverified`.

Phase 28 adds a Product Shell no-UI report for local actor role and permission
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

## Report Command

```bash
build/RelWithDebInfo-0.0.9-sde/bin/Saga --local-workspace-role-smoke --project samples/StarterArena/StarterArena.sagaproj --workspace builtin:basic --actor local.actor --role local.reviewer --permission inspect_project --role-report-out /tmp/starter_arena_role_permission_report.json
```

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

Phase 28 does not activate the Editor collaboration permission scaffolds and
does not link Product Shell behavior to `PermissionManager`,
`PermissionPolicy`, or `CollaboratorRole`.

The command writes only the caller-provided report output path. It does not
mutate `.sagaproj`, scenes, scripts, SDE files, package profiles, diagnostics
folders, report folders, workspace files, or durable collaboration metadata.

