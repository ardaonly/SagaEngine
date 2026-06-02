# Dangerous Operation Policy Gate v1

## Status

Phase 102 defines a local/report-only gate for dangerous operations. It does not
execute operations, mutate files, apply patches, publish packages, enforce
security, or call Editor UI.

## Dangerous Operations

- delete scene
- delete behavior source
- change package profile
- override lock
- approve publish gate
- modify runtime binding metadata manually
- export restricted artifact

## Report-Only Behavior

The gate evaluates a policy request and emits a deterministic report. It may
return `Allow`, `Deny`, `Warn`, `RequiresReview`, `MissingEvidence`,
`UnknownAction`, `UnknownResource`, or `UnknownSubject`.

The Phase 102 dangerous-operation fixture is
`Tools/SagaPolicyKit/fixtures/delete_scene_request.json`.

An `Allow` result is not permission enforcement. It is local evidence that a
later tool may consume after that later tool has its own implementation and
tests.

## Non-Goals

- filesystem mutation
- SagaScript patch apply or rollback
- package publish execution
- editor apply path
- security enforcement claim
- auth/login/account identity
- cloud workspace
- WorkspaceHub
- Phase 104+ Project Slice work
