# Saga Review, Comment, and Transaction Model

## Status

Local review, comment, and transaction metadata policy.

## Storage Contract

Durable local collaboration metadata belongs under:

```text
.saga/collaboration/
```

Generated collaboration reports belong under:

```text
Build/Collaboration/
Build/SmallTeamAlpha/
```

Durable metadata is project-owned source truth. Generated reports are evidence
snapshots and must not replace durable metadata.

## Local Metadata Only

Collaboration remains local/offline metadata. It does not provide live shared
editing, hosted sync, account identity, security enforcement, source merge,
CRDT, operational transform, sockets, websocket, HTTP, editor UI, Qt UI, or
public Qt ABI expansion.

## Locks

Persistent locks are coordination metadata. They may target artifact paths,
behavior ids, node ids, source spans, patch review/report artifacts, diagnostics
artifacts, and other project metadata resources. A lock conflict creates
deterministic diagnostics. Locks do not prevent filesystem writes outside the
model.

## Comments

Comments are metadata only. Valid targets are artifact path, behavior id, node
id, source span, patch diff/review report, and diagnostic id. Resolve and reopen
only update comment metadata. Comments do not edit source, regenerate artifacts,
or sync over a network.

## Reviews

Review requests may reference patch diff, patch review, patch apply reports,
behavior ids, node ids, source spans, and artifact paths. Review states are
`Requested`, `InReview`, `ApprovedMetadataOnly`, `Rejected`, `Blocked`, `Stale`,
and `Superseded`.

`ApprovedMetadataOnly` means a local review decision was recorded. It does not
apply a patch, invoke SagaScript, mutate C# source, enable an editor apply
button, or change package/publish behavior.

## Roles

Roles are local metadata for dangerous-operation diagnostics. They are not a
security boundary. The supported roles are `Owner`, `Programmer`, `Designer`,
`Artist`, and `QA`.

Dangerous-operation checks can report local metadata decisions for delete scene,
delete behavior source, change package profile, override lock, approve publish
gate, and manual runtime binding metadata changes.
