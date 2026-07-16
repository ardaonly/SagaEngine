---
title: Collaboration semantics
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Collaboration semantics

EditorCollaboration exposes the modern `SagaCollaboration/**`, `SagaShared/Collaboration/**`, and `SagaShared/Workspace/**` contracts for bounded session, presence, claim, lock, permission, conflict, change-stream, and workspace state. The former `SagaEditor::Collaboration` public/private tree is removed. These concepts can improve local architecture even before a hosted collaborative product exists.

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

The current service/model contracts and local reports do not establish a shipped real-time multi-user editor, hosted service, production security model, tenant isolation, encrypted transport, persistence, or operational collaboration backend. No removed legacy namespace or implementation tree should be read back into the current contract.

| Surface | Contract reading |
| --- | --- |
| `SagaShared` participant/session/workspace/presence/claim/lock/permission values | Transport- and UI-neutral shared records. |
| `SagaCollaboration` service/model interfaces | Bounded in-process contracts with focused tests. |
| Local workspace/review metadata reports | Private read-only product evidence; no mutation or hosted-service claim. |

See [Collaboration transactions and local policy](../reference/collaboration-transactions-and-policy.md) for workspace/transaction records, presence, locks, reviews, roles, dangerous operations, conflicts, sessions, and the concrete/private boundary.
