# Policy Domain Model v0

## Status

This document defines local policy semantics. It is not auth, login, security
enforcement, enterprise RBAC, filesystem protection, cloud policy, or
WorkspaceHub.

## Model Concepts

| Concept | Definition |
|---|---|
| `PolicySubject` | The local subject being evaluated. |
| `PolicyActor` | The local actor metadata supplied in a request. |
| `PolicyRole` | A local project role profile such as Owner or Observer. |
| `PolicyResource` | A project artifact, package profile, source file, scene, lock, or publish gate target. |
| `PolicyAction` | A named action such as `ApprovePublishGate` or `DeleteScene`. |
| `PolicyDecision` | The deterministic result of local policy evaluation. |
| `PolicyDiagnostic` | A stable diagnostic explaining policy status. |
| `DangerousOperation` | An action that needs warning, denial, or review before a later tool may execute it. |
| `EvidenceReference` | A report path or evidence id used to justify a decision. |
| `Scope` | The local project/workspace scope for evaluation. |
| `ProjectId` | The project identifier. |
| `WorkspaceId` | The local workspace identifier. |

## Decision Values

- `Allow`
- `Deny`
- `Warn`
- `RequiresReview`
- `NotApplicable`
- `MissingEvidence`
- `UnknownSubject`
- `UnknownAction`
- `UnknownResource`

## Required Semantics

- Evaluation is deterministic for identical policy and request input.
- Unknown roles, actions, resources, and missing evidence produce diagnostics.
- Dangerous operations may return `RequiresReview` even when the role is known.
- A report-only `Allow` does not enforce permissions or protect files.
- Policy profiles are local project data, not security identities.

## Non-Goals

- auth/login/account identity
- enterprise RBAC
- real permission enforcement
- source redaction security
- WorkspaceHub integration
- Project Slice or Restricted Project Resolution
- cloud policy service
