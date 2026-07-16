---
title: Collaboration semantics
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Collaboration semantics

EditorCollaboration models concepts useful for coordinated authoring: semantic operations, presence, locks, permissions, review state, and workspace identity. These concepts can improve local architecture even before a hosted collaborative product exists.

## Semantic transactions

Collaboration should exchange or record domain operations rather than widget events. A transaction identifies the project object affected, the intended change, its base state, and the result. This creates a reviewable boundary for local undo/redo, approval, conflict handling, and possible future transport.

## Presence and locks

Presence is an observation about participants and focus; it is not authority. Locks are explicit, scoped tokens with lifecycle and failure semantics; they are not a substitute for validating the underlying source version. Permission policies decide whether an operation is allowed, while transport and UI remain separate concerns.

Current local evidence includes workspace transaction reports and review/comment metadata. These surfaces record project/workspace/actor identity, operation intent, source hashes, lock or review state, diagnostics, and explicit limitations. A local `ApprovedMetadataOnly` decision records review metadata; it does not execute publishing, grant remote authority, or prove that the source is still fresh.

## Local policy metadata

Role profiles and policy decisions can classify dangerous operations such as deleting scene/source data, changing package profiles, overriding locks, or approving a publish gate. Identical inputs should produce deterministic local results, and unknown actors or operations remain explicit diagnostics. `Allow`, `Deny`, or `RequiresReview` in this model is report/local-metadata state; it does not authenticate a user, protect the filesystem, enforce remote permissions, or perform the operation.

## Shared versus personal state

Project changes and review comments may be shared domain state. Panel layout, local selection, and personal editor preferences remain personal view state unless a contract deliberately promotes them.

## Non-claim

The existence of server, router, client, sync, CRDT, audit, or manager contracts does not establish a shipped real-time multi-user editor, hosted service, production security model, tenant isolation, encrypted transport, or operational collaboration backend. The concrete server/router/client, sync transport, CRDT/log, manager, workspace, session, and audit implementations are private; their presence still proves only the focused local behavior covered by tests.

| Surface | Contract reading |
| --- | --- |
| Workspace, transaction, presence, lock, permission, role, review, and event values | Durable candidates when they remain transport- and UI-neutral. |
| Interfaces and bounded policy inputs/results | Cross-module contract candidates with focused deterministic evidence. |
| Concrete server/router/client, sync transport, CRDT/log, manager, workspace, session, and audit implementations | Private implementation; not a public product or hosted-service claim. |

See [Collaboration transactions and local policy](../reference/collaboration-transactions-and-policy.md) for workspace/transaction records, presence, locks, reviews, roles, dangerous operations, conflicts, sessions, and the concrete/private boundary.
