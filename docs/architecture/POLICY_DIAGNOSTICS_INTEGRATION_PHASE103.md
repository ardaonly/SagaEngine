# Policy Diagnostics Integration Phase 103

## Status

Phase 103 makes local policy results visible as evidence artifacts. It does not
integrate Runtime, Server, ClientHost, Editor UI, Qt UI, WorkspaceHub, cloud,
auth, permission enforcement, or publish enforcement.

## Diagnostic Codes

Policy reports should use stable diagnostics for:

- missing policy
- unknown actor
- unknown role
- unknown action
- unknown resource
- denied dangerous operation
- required review
- missing evidence
- non-enforcing report-only mode

## Evidence Flow

`Tools/SagaPolicyKit` may emit:

- `policy_evaluation_report.json`
- `dangerous_operation_policy_report.json`
- `policy_diagnostics_report.json`

All three are produced with `sagapolicy evaluate` over local policy/request
fixtures in Phase 100-103.

These reports are evidence inputs only. They do not enforce access control,
redact project data, execute operations, or protect files.

## Future Integration Boundaries

Later phases may define how SagaProjectKit, SagaPackager, publish gates,
WorkspaceHub, Team Room, or project slices consume policy diagnostics. Phase 103
does not implement those integrations.

## Exit Criteria

Policy diagnostics are deterministic, report-only, and explicit about missing
policy, denied dangerous operations, required review, missing evidence, and
non-enforcing mode.
