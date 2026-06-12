# Saga Collaboration Presence And Lock Boundary

> Status: Local presence/lock report boundary

This document adds a Product Shell no-UI local presence and lock metadata report. It
does not implement real-time collaboration, networked presence, a durable lock
service, cloud workspace, enterprise access control, CRDT/OT, a collaboration
server, full team editing, product beta, package readiness, or distribution
readiness.

## Boundary Model

```text
local actor presence metadata
local lock intent metadata
read-only preview/result
project truth remains unchanged
no collaboration server
no cloud sync
no real-time multi-user editing
```

The report is local evidence only. Presence is represented as report metadata
for the selected actor. Lock data is an intent record over one artifact. It does
not acquire, persist, enforce, broadcast, or conflict-check a lock.

## Product Shell Report

This document describes a local Product Shell no-UI presence/lock smoke over a
caller-provided project, workspace, actor, lock target, and report output path.
The exact build directory and output file are local evidence details, not
architecture truth.

The report contains:

- `schemaVersion`
- `tool`
- `command`
- `status`
- `verified: false`
- `workspace`
- `project`
- `actor`
- `presence`
- `lock`
- `diagnostics`
- `knownLimitations`
- `nonClaims`

The `presence` object records `actorId`, `displayName`, `status:
PresentLocal`, `source: report-only`, `durable: false`, and `networked: false`.

The `lock` object records a deterministic `lockId`, `targetArtifact`,
`lockMode: ReadOnlyPreview`, `status`, `conflictStatus: NotChecked`,
`durable: false`, and `mutatesProject: false`.

## Shared Project Truth

The report does not mutate `.sagaproj`, scenes, scripts, SDE files, package
profiles, diagnostics folders, report folders, or workspace files. The only
write is the caller-provided report output path. Local report output is not
shared project truth.

## Risks And Guardrails

- Report-level presence can be mistaken for real-time or networked presence.
- Report-level lock intent can be mistaken for a durable lock service.
- Lock metadata can be mistaken for permission enforcement.
- Local workspace wording can be mistaken for cloud/team collaboration.
- Collaboration language must not drift into enterprise or product readiness
  claims.
