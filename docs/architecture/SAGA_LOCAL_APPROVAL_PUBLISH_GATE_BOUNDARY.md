# Saga Local Approval / Publish Gate Boundary

> Status: Local approval/publish gate report boundary

This document adds a Product Shell no-UI report for local approval intent and publish
gate preview metadata. The report composes the existing local collaboration
metadata surfaces into a report-only readiness decision. It does not implement a
real approval workflow, secure permission enforcement, enterprise policy,
durable approval storage, package readiness, distribution readiness, cloud
workspace, real-time multi-user editing, CRDT/OT, or a collaboration server.

## Boundary Model

```text
local approval intent metadata
local publish gate preview metadata
report-only readiness decision
no actual enforcement
no package/distribution output
no enterprise policy engine
project truth remains unchanged
```

## Report Command Shape

The report is produced by a local Product Shell no-UI approval-gate smoke over a
caller-provided project, workspace, actor, role label, gate target, approval
state, and report output path. The exact build directory and output file are
local evidence details, not architecture truth.

The report contains:

- `schemaVersion`
- `tool`
- `command`
- `status`
- `verified: false`
- `workspace`
- `project`
- `actor`
- `approval`
- `publishGate`
- `readiness`
- `diagnostics`
- `knownLimitations`
- `nonClaims`

The `approval` object records a deterministic report-only `approvalId`, actor,
role, target artifact, local preview approval state, and `durable`, `enforced`,
`requiresServer`, and `mutatesProject` flags set to `false`.

The `publishGate` object records a deterministic report-only `gateId`,
`gateMode: "MetadataOnly"`, `status: "Blocked"` for a valid metadata preview,
`packagePreflightStatus: "Blocked"`, `distributionReady: false`, and
`policyBacked`, `enforced`, `durable`, and `mutatesProject` flags set to
`false`.

The `readiness` object always records `canPublish: false` for this document because
package and distribution readiness are not implemented by this report.

## Non-Goals

This document does not run package or distribution scripts, does not create package
output, does not block a real publish pipeline, and does not claim package or
distribution readiness. `scripts/package-linux-saga` remains an optional
diagnostic outside this report.

The command writes only the caller-provided report output path. It does not
mutate `.sagaproj`, scenes, scripts files, package profiles, diagnostics
folders, report folders, workspace files, package output, or durable approval,
policy, audit, or collaboration metadata.
