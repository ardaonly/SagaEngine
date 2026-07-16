---
title: Collaboration transactions and local policy
status: bounded
owner: SagaWiki
reviewed_against: 0.0.11-post-cutover
---

# Collaboration transactions and local policy

EditorCollaboration contains durable value models and a large set of concrete implementation-shaped types. The defensible current contract is local semantic transaction, presence/lock/review metadata, and deterministic policy modeling. It is not a claim of a shipped hosted real-time collaborative editor.

## Architecture principle

Collaboration operates on editor/project semantics, not widget events. A transaction says which workspace/project artifact is affected, what operation is intended, which actor requested it, which source/base state was observed, and what result/conflict occurred.

This model is useful locally for auditability, stale detection, review, and future transport. It remains useful even when no remote peer exists. Networked multi-user behavior requires separate service, transport, identity, security, persistence, conflict, and operations evidence.

## Current public model

Durable value candidates include participant/session/workspace identity, artifact target/kind, semantic operation/transaction, status, conflict category, presence snapshot, lock token/record, comment, review request, role assignment, permission decision, dangerous-operation check, diagnostics, and copied workspace state.

Interfaces for presence, locks, permissions, sessions, workspaces, audit, conflict, and transport can be contracts when their lifecycle and semantics are deliberate. Concrete server, router, client, sync transport, CRDT, operation log, manager, election, audit logger, and backend implementations are not automatically stable because their headers are public. They require visibility review and may belong private or experimental.

## Workspace identity

A workspace descriptor identifies project, local workspace/session, and actor context. It does not authenticate the actor. A local actor id is metadata supplied by the workflow. Remote authentication, tenant identity, key management, and authorization enforcement require a security boundary not established by this model.

All transactions and metadata are scoped to a workspace/project identity. Records from another workspace are rejected or displayed as external evidence; they are never merged because file names happen to match.

## Semantic transaction

A transaction should include:

- transaction and operation id;
- actor and workspace identity;
- artifact target and operation kind;
- expected source/artifact hash or version;
- typed operation payload or reference;
- creation/submission time and local ordering id;
- lock/review/policy evidence where applicable;
- status, diagnostics, and resulting version/hash if committed.

Statuses distinguish previewed, submitted, accepted/committed, rejected, conflicted, stale, cancelled, and failed as supported. A metadata-only approval does not imply the source operation executed.

The source-owning tool/editor transaction applies the actual change. Collaboration metadata references the operation and result; it does not gain a second file-writing implementation.

## Local workspace transaction

The local transaction workflow is report-first. It resolves the project/workspace, actor, target, source version, operation, and current local metadata. It checks path, freshness, lock/review/policy state and emits a preview or result.

Read-only smoke evidence must say it did not mutate project files, persist collaboration metadata, contact a server, or synchronize peers. A report can be useful without overstating behavior.

If persistence is enabled later, storage location, schema, atomicity, source-control behavior, and cleanup become explicit. Hidden files inside project source must not appear without a declared authority.

## Presence

Presence reports an actor's current availability/focus/selection as a bounded snapshot. It is ephemeral observation, not source truth and not authority. A stale snapshot expires according to timestamp/heartbeat policy.

Presence can include workspace, artifact target, cursor/selection descriptors, display identity, and last-seen state. It must avoid broadcasting arbitrary source content or personal paths. Cursor/selection broadcast is UI metadata; it does not lock or modify the selected object.

Local presence fixtures do not prove remote live updates, delivery ordering, reconnection, privacy, or scale.

## Locks

A lock identifies workspace, resource/artifact target, owner, token, mode/scope, acquisition version/time, expiry/heartbeat, and status. Locks have explicit acquire, renew, release, expiry, and conflict outcomes.

A lock is advisory or enforced according to the actual owner contract. Local metadata cannot prevent another process from editing a file unless the source writer checks it. Even with a lock, apply validates source freshness; a lock never replaces the hash/version check.

Lock stealing/override is a dangerous operation requiring policy/review evidence. Expired or unknown tokens cannot release another actor's lock. Shutdown/reconnect behavior is explicit so locks do not remain indefinitely because a client vanished.

## Comments and reviews

Comments reference an artifact, source span/object, author, body, source version, and status. A comment tied to an old hash remains historical/stale rather than moving silently to a similar line.

A review request groups target version, requested reviewers/role policy, comments, decision state, and resulting metadata. Approval values distinguish metadata-only approval from operation/publish execution. Changing source after approval invalidates or stales the decision according to policy.

Review records are not code signatures, legal approvals, or secure attestations. Local JSON metadata can be edited unless a stronger integrity system exists.

## Roles and permissions

Role profiles map named project roles to allowed/review-required/denied semantic operations. Useful bounded roles can describe author, reviewer, maintainer, observer, or automation identities according to current policy. The exact list remains project policy, not hard-coded product marketing.

A permission decision includes actor/role, operation, target/domain, input policy version, decision, reason, and diagnostics. Deterministic identical inputs produce identical local decisions. Unknown role, operation, or domain fails explicitly.

`Allow`, `Deny`, and `RequiresReview` are policy decisions. They do not authenticate identity, set filesystem ACLs, secure a network service, or execute the operation. The source/publish owner consumes the decision and still validates all operational inputs.

## Policy domain model

Policy separates domain, action, subject/actor, resource/target, context, rule, and decision. Domains can represent source changes, scene/asset operations, package/publish requests, locks, reviews, or other bounded project actions.

Policy files/records are versioned inputs. Resolution order and conflict behavior are deterministic: an explicit deny or required review must not be accidentally overridden by rule iteration order unless the contract intentionally defines precedence. The report identifies the matching rule(s).

Local policy is not a remote policy service. It does not claim tenant administration, enterprise governance, legal compliance, or tamper resistance.

## Dangerous operations

Dangerous operations are those with destructive, irreversible, authority, packaging, or shared-state impact: deleting source/scene content, overriding a lock, changing package/publish profile, applying a broad source rewrite, accepting a stale operation, or approving a release gate.

The check is report-only unless integrated with the operation owner. It returns allowed, denied, requires review, invalid input, or unsupported as appropriate. Default policy should fail closed for unknown dangerous operations.

The check does not perform the operation and does not grant remote permission. UI must present “allowed by local policy” rather than “securely authorized.”

## Approval and publish boundary

A local approval/publish-gate report references project/build/package/policy evidence and source/artifact hashes. It can state ready, blocked, or requires review according to current inputs. It does not build, package, upload, sign, or publish.

SagaPackager remains owner of package/preflight behavior. Optional governance reports are externally produced inputs consumed by packager policy; SagaPackager does not generate them and no bundled SagaPolicyKit workflow exists.

An approval becomes stale when referenced source, manifest, artifact, policy, or report changes. A UI cannot cache an old green badge after freshness changes.

## Conflict model

Conflicts identify competing operations/base versions and category: stale base, concurrent property edit, hierarchy/structural conflict, lock conflict, deletion versus edit, or unsupported merge. A resolution records strategy and resulting source version.

Last-write-wins is a strategy, not a universally safe default. Three-way/property/hierarchy/scene merge requires schema-aware validation. A merge success must produce valid canonical source and preserve unrecognized data according to the owning format.

CRDT and operational-transform classes in the tree do not prove a correct general collaborative document implementation. They should be treated as concrete/experimental until focused semantics, persistence, transport, and convergence evidence exist.

## Operation logs and audit

An operation log records semantic transaction metadata and ordered results. Audit events record who/what/when/context according to bounded local identity. Both are derived operational/history data, not canonical scene/script source.

Logs are append/retention artifacts only where an implementation and storage policy prove it. In-memory test logs do not establish durability. Export output includes schema/version and redacts sensitive values.

Audit wording must avoid implying tamper-proof security. A local exporter/logger is useful diagnostics/history, not a compliant audit service.

## Session and transport

A session contract can define descriptor, room/session code, participant set, state, handshake, heartbeat, and close reasons. A transport interface can move operation/presence messages. Security and deployment claims require actual authenticated/encrypted transport and operational evidence.

Handshake validates version/workspace/project compatibility before accepting operations. Heartbeat freshness does not prove identity. Reconnect resets or reconciles session generation so late messages from an old connection cannot mutate current state.

Concrete `CollaborationServer`, router, client, backend, sync transport, CRDT/log, manager, workspace, session, and audit implementations live under `EditorCollaboration/Private`. Their interface and value contracts can support focused local evidence, but neither layer is public product evidence for a hosted service.

## Shared and personal state

Shared candidates are canonical project changes and deliberately versioned collaboration metadata such as accepted comments/reviews/locks when policy establishes them. Personal editor layout, selection, focus, local filters, and shortcuts remain personal.

Presence can describe personal focus to others, but broadcasting it does not make it canonical project state. Collaboration must not serialize an entire editor window model into project source.

## Failure and offline behavior

Missing workspace, unknown actor, stale source, invalid target, conflicting lock, denied policy, required review, malformed metadata, persistence failure, transport loss, protocol mismatch, and merge failure remain separate diagnostics.

Offline/local mode can continue to create local transactions where supported. It must not present them as remotely synchronized. Pending operations have explicit state and do not remain “sent” forever after session loss.

## Evidence

Unit tests can prove deterministic role/policy decisions, lock lifecycle, stale transaction rejection, comment/review versioning, conflict classification, local metadata serialization, and bounded session state. Integration tests can prove a specific local/in-memory client/server path. Convergence, persistence, security, deployment, and scale require separate evidence.

## Non-claims

The current repository does not establish a shipped production collaborative editing service, hosted backend, real-time multi-user product, secure authentication/authorization, encrypted transport, tenant isolation, tamper-proof audit, durable remote operation log, CRDT convergence at production scale, cloud workspace, or enterprise governance.

## Public-surface note

Value types and intentional interfaces are the strongest public-contract candidates. Concrete managers, server/router/client, transport implementation, operation-log implementation, authority election, audit logger, and CRDT/OT implementations should be reviewed for private or experimental placement. Documentation does not freeze their current public paths.

## Change checklist

- Model semantic operations, not UI events.
- Scope every record to workspace/project and source version.
- Keep presence observational and locks freshness-aware.
- Distinguish metadata approval from operation/publish execution.
- Treat local policy as a decision input, not authentication or enforcement proof.
- Keep concrete collaboration implementations from becoming product claims.
- Preserve offline/pending/stale/failure state honestly.
- Add security, convergence, persistence, and scale claims only with their own evidence.
