# Saga Collaboration Current Boundary

> Status: Current compact boundary

This is the canonical short boundary for collaboration wording. It separates
the durable architecture principle from the current local/report-only proof
surface.

## Architecture Principle

```txt
Editor view is personal.
Project model is shared.
Changes must become semantic transactions.
```

Personal editor layouts, profiles, local panel state, and view preferences are
not shared project truth. Shared project truth remains project manifests,
scenes, scripts, generated evidence reports, package contracts, and future
durable project-owned metadata.

## Current Evidence Surface

Current collaboration evidence is local and report-oriented:

- [Saga Local Workspace Transaction Boundary](SAGA_LOCAL_WORKSPACE_TRANSACTION_BOUNDARY.md)
- [Saga Collaboration Presence And Lock Boundary](SAGA_COLLABORATION_PRESENCE_LOCK_BOUNDARY.md)
- [Saga Collaboration Review And Audit Boundary](SAGA_COLLABORATION_REVIEW_AUDIT_BOUNDARY.md)
- [Saga Collaboration Role / Permission Boundary](SAGA_COLLABORATION_ROLE_PERMISSION_BOUNDARY.md)
- [Saga Project Slice Visibility Boundary](SAGA_PROJECT_SLICE_VISIBILITY_BOUNDARY.md)
- [Project Slice Schema v0](PROJECT_SLICE_SCHEMA_V0.md)
- [Project Role Profiles v1](PROJECT_ROLE_PROFILES_V1.md)
- [Project Slice Resolver v1](PROJECT_SLICE_RESOLVER_V1.md)

These documents describe local metadata, schema, and report boundaries. They do
not establish a production collaboration service.

## Explicit Non-Claims

- No cloud workspace.
- No account, organization, or identity provider integration.
- No authenticated collaboration transport.
- No real-time multi-user editing.
- No CRDT/OT implementation.
- No durable lock service.
- No enterprise permission enforcement.
- No secure source hiding or restricted project download boundary.
- No production team-editing workflow.

## Proposed Appendix

[SagaEngine Collaborative Workspace Architecture](../internal/architecture-appendices/SAGA_COLLABORATIVE_WORKSPACE_ARCHITECTURE.md)
is a proposed direction appendix. It preserves the broader model and future
risks, but current claims must come from the compact boundary and focused local
report/schema contracts listed above.
