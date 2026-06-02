# Small-Team Alpha Collaboration Guide

Status: Phase 94 onboarding evidence for local/offline collaboration workflow.

Small-Team Alpha collaboration is project-local metadata. Durable state belongs
under `.saga/collaboration/`; generated reports belong under
`Build/Collaboration` or `Build/SmallTeamAlpha`.

## Supported Metadata

- workspace state;
- actor and session references without private machine identity;
- artifact locks;
- artifact comments;
- review requests;
- local role checks for dangerous-operation diagnostics;
- Team Room status and review queue reports.

Review approval is metadata only. It does not mutate source, call SagaScript
patch application, merge source, or enforce real permissions.

## Non-Claims

- no product beta
- no release candidate
- no production MMO server
- no complete visual scripting
- no arbitrary C# roundtrip
- no enterprise readiness
- no account login or auth system
- no cloud workspace platform
- no CRDT or operational transform
