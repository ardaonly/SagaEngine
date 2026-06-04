# Saga Local Workspace Transaction Boundary

Phase 25 status is `Implemented-Unverified`.

Phase 25 defines the first local workspace/collaboration transaction boundary as
a Product Shell no-UI report proof. It does not implement full multiplayer
collaboration, cloud workspace, enterprise permissions, real-time team editing,
CRDT/OT, a collaboration server, product beta, package readiness, or
distribution readiness.

## Boundary Model

```text
Editor view is personal.
Project model is shared.
Changes must become semantic transactions.
Local workspace is the first proof target.
Enterprise policy is later.
```

Personal editor profile, layout, panel, and capability metadata remain local
view metadata. They are not shared project truth. Shared project truth remains
the project manifest, scenes, scripts, generated evidence reports, package
contracts, and future durable project-owned metadata.

## Existing Surfaces

Real current surfaces:



Current missing implementation:

- No cloud workspace.
- No collaboration server proof for Phase 25.
- No real-time multi-user editing.
- No CRDT/OT proof.
- No enterprise permission enforcement.
- No full editor UI or Visual Blocks editor UI.

## Local Transaction Report

Phase 25 exposes the first report-only proof as:



The report contains:

- `schemaVersion`
- `tool`
- `command`
- `status`
- `verified: false`
- `workspace`
- `project`
- `transaction`
- `operationExamples`
- `diagnostics`
- `knownLimitations`
- `nonClaims`

The transaction object contains:

- `transactionId`
- `workspaceId`
- `projectId`
- `actorId`
- `operationKind`
- `targetArtifact`
- `readOnlyPreview: true`
- `status`

Supported operation names in Phase 25 are report examples only:

```text
OpenProject
InspectProject
PlanScriptBlockEdit
ApplyCopiedSourcePatch
RunWorkflowSmokeReference
```



## Risks And Guardrails

- A local report can be mistaken for real-time collaboration; the report keeps
  `readOnlyPreview: true`, `verified: false`, and explicit non-claims.
- Personal editor profiles can be mistaken for shared workspace state; Phase 25
  states they are personal view metadata only.
- Local workspace can be mistaken for cloud/team collaboration; Phase 25 is
  local and report-only.
- Semantic transaction metadata can be mistaken for a full review/audit
  workflow; Phase 25 records only the boundary proof.
- Enterprise collaboration claims must stay deferred until policy and server
  enforcement exist.
