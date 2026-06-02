# Project Role Profiles v1

## Status

Phase 101 defines local role profiles for report-only policy evaluation. Roles
are not authentication, security identity, enterprise RBAC, or filesystem
protection.

## Roles

| Role | Intended local use |
|---|---|
| `Owner` | May approve high-risk local policy decisions in reports. |
| `Lead` | May review and approve bounded project workflow decisions. |
| `Programmer` | May modify script/runtime-binding metadata when policy allows. |
| `Designer` | May work with design-facing artifacts but not package governance by default. |
| `Artist` | May work with asset-facing artifacts when source truth is available. |
| `QA` | May review, comment, and inspect diagnostics. |
| `Observer` | Read-only/report-only role with no dangerous-operation approval. |

## Example Actions

- approve publish gate
- override lock
- modify runtime binding metadata
- change package profile
- delete behavior source
- delete scene
- export restricted artifact

The Phase 101 local fixture is
`Tools/SagaPolicyKit/fixtures/default_project_roles_policy.json`.

## Required Boundaries

- Role profiles are local policy profiles only.
- Role profiles do not authenticate users.
- Role profiles do not prove security identity.
- Role profiles do not implement enterprise RBAC.
- Role profiles do not protect the filesystem.
- Role profiles do not make SagaEngine enterprise-ready.

## Deferred Role Expansion

External reviewer, restricted contractor, organization hierarchy, group sync,
custom claims providers, and identity provider integration are deferred.
