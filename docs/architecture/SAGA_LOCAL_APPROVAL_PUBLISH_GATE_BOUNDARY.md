# Saga Local Approval / Publish Gate Boundary

Phase 30 status is `Implemented-Unverified`.

Phase 30 adds a Product Shell no-UI report for local approval intent and publish
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

## Report Command

```bash
build/RelWithDebInfo-0.0.9-sde/bin/Saga --local-workspace-approval-gate-smoke --project samples/StarterArena/StarterArena.sagaproj --workspace builtin:basic --actor local.actor --role local.reviewer --gate-target samples/StarterArena/StarterArena.sagaproj --approval-state approved-local-preview --approval-gate-report-out /tmp/starter_arena_approval_gate_report.json
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

The `readiness` object always records `canPublish: false` for Phase 30 because
package and distribution readiness are not implemented by this report.

## Non-Goals

Phase 30 does not run package or distribution scripts, does not create package
output, does not block a real publish pipeline, and does not claim package or
distribution readiness. `scripts/package-linux-saga` remains an optional
diagnostic outside this report.

The command writes only the caller-provided report output path. It does not
mutate `.sagaproj`, scenes, scripts, SDE files, package profiles, diagnostics
folders, report folders, workspace files, package output, or durable approval,
policy, audit, or collaboration metadata.

