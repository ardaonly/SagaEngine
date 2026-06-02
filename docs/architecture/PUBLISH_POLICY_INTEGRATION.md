# Publish Policy Integration

Phase 115 extends `Tools/SagaPackager` `publish-check` with optional governance
evidence inputs.

Command shape:

```sh
Tools/SagaPackager/sagapack publish-check --project <.sagaproj> --profile <id> --package-report <package_report.json> --diagnostics-summary <diagnostics_summary.json> [--script-validation <script_artifact_validation_report.json>] [--policy-report <policy_evaluation_report.json>] [--review-approval-report <review_approval_report.json>] [--audit-report <audit_report.json>] [--restricted-export-report <restricted_export_report.json>] --out <publish_report.json>
```

When governance reports are omitted, existing publish-check behavior is
unchanged. When they are supplied, `publish_report.json` adds matching
`requiredEvidence` entries and these gates:

- `PolicyReportAccepted`
- `ReviewApprovalAccepted`
- `AuditEvidenceAccepted`
- `RestrictedExportAccepted`

Policy `Allow` and `Warn` pass the policy gate. Policy `Deny`,
`RequiresReview`, missing evidence, malformed evidence, blocked review approval,
failed audit evidence, or blocked restricted export evidence blocks
`publish-check`.

This integration does not publish, sign, upload, rewrite package formats, run
installers/updaters, or mutate source. It is a deterministic local report gate.
