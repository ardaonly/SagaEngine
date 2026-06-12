# Saga Local Workspace Transaction Boundary

> Status: Local workspace transaction report boundary

This document defines the first local workspace/collaboration transaction boundary as
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

- Product Shell session/workspace metadata: `SagaSessionModel`,
  `SagaWorkspaceResolver`, built-in `BasicWorkspace` SDE data, and no-UI report
  commands.
- Editor read models: no-UI StarterArena inspection, project browser,
  diagnostics, script projection, patch preview/review, and profile/capability
  metadata.
- Collaboration inventory: editor collaboration scaffolds for locks, presence,
  audit, sessions, workspace, sync, and server boundaries.
- Local collaboration metadata reports: Product Shell no-UI presence/lock and
  review/comment/audit report commands for this document and Local role/permission and project slice visibility metadata reports: Product
  Shell no-UI commands for this document and Local approval and publish gate metadata report: Product Shell no-UI command
  for this document that keeps publish readiness blocked and report-only.
- Local/offline collaboration model evidence: `SagaCollaboration` unit tests for
  identity, presence reports, metadata-only reviews, comments, locks, semantic
  transaction logs, conflicts, and Team Room report models.

Current missing implementation:

- No cloud workspace.
- No collaboration server proof for No real-time multi-user editing.
- No CRDT/OT proof.
- No enterprise permission enforcement.
- No full editor UI or Visual Blocks editor UI.

## Local Transaction Report

This document describes a report-only local Product Shell transaction smoke over
a caller-provided project, workspace, actor, operation, and report output path.
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

Supported operation names in this document are report examples only:

```text
OpenProject
InspectProject
PlanScriptBlockEdit
ApplyCopiedSourcePatch
RunWorkflowSmokeReference
```

The current contract report does not write `.saga/collaboration/`, append a durable
transaction log, apply source patches, mutate `.sagaproj`, mutate scenes, mutate
scripts, regenerate SDE files, start a server, or synchronize with another
machine.

## Risks And Guardrails

- A local report can be mistaken for real-time collaboration; the report keeps
  `readOnlyPreview: true`, `verified: false`, and explicit non-claims.
- Personal editor profiles can be mistaken for shared workspace state; this document
  states they are personal view metadata only.
- Local workspace can be mistaken for cloud/team collaboration; this document is
  local and report-only.
- Semantic transaction metadata can be mistaken for a full review/audit
  workflow; this document records only the boundary proof.
- Enterprise collaboration claims must stay deferred until policy and server
  enforcement exist.
