# SagaWorkspaceHub

SagaWorkspaceHub is the Target 3 / Enterprise-Evolvable Foundation Phase 109-121 local WorkspaceHub evidence tool.
It reads existing project, policy, slice, and view reports and writes bounded
report-only WorkspaceHub summaries.

Commands:

```sh
Tools/SagaWorkspaceHub/sagaworkspacehub summarize --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --slice-resolution Build/Enterprise/restricted_project_resolution_report.json --policy-report Build/Enterprise/policy_evaluation_report.json --view-compatibility Build/Enterprise/view_slice_compatibility_report.json --out Build/WorkspaceHub/workspacehub_summary_report.json
Tools/SagaWorkspaceHub/sagaworkspacehub policy-adapter --policy-report Build/Enterprise/policy_evaluation_report.json --out Build/WorkspaceHub/workspacehub_policy_adapter_report.json
Tools/SagaWorkspaceHub/sagaworkspacehub slice-view --slice-resolution Build/Enterprise/restricted_project_resolution_report.json --view-compatibility Build/Enterprise/view_slice_compatibility_report.json --policy-report Build/Enterprise/policy_evaluation_report.json --out Build/WorkspaceHub/workspacehub_slice_scoped_view_report.json
Tools/SagaWorkspaceHub/sagaworkspacehub audit-log --events Build/WorkspaceHub/audit_events.jsonl --out Build/WorkspaceHub/audit_report.json
Tools/SagaWorkspaceHub/sagaworkspacehub review-approval --review-report Build/WorkspaceHub/review_report.json --policy-report Build/Enterprise/policy_evaluation_report.json --audit-report Build/WorkspaceHub/audit_report.json --out Build/WorkspaceHub/review_approval_report.json
Tools/SagaWorkspaceHub/sagaworkspacehub restricted-export --slice-resolution Build/Enterprise/restricted_project_resolution_report.json --policy-report Build/Enterprise/policy_evaluation_report.json --request Build/WorkspaceHub/export_request.json --out Build/WorkspaceHub/restricted_export_report.json
Tools/SagaWorkspaceHub/sagaworkspacehub presence-redaction --presence-report Build/WorkspaceHub/presence_report.json --slice-resolution Build/Enterprise/restricted_project_resolution_report.json --policy-report Build/Enterprise/policy_evaluation_report.json --out Build/WorkspaceHub/presence_redaction_report.json
Tools/SagaWorkspaceHub/sagaworkspacehub policy-actions --policy-report Build/Enterprise/policy_evaluation_report.json --review-approval-report Build/WorkspaceHub/review_approval_report.json --restricted-export-report Build/WorkspaceHub/restricted_export_report.json --out Build/WorkspaceHub/policy_aware_editor_actions_report.json
Tools/SagaWorkspaceHub/sagaworkspacehub team-room --team-room-status Build/WorkspaceHub/team_room_status.json --presence-redaction Build/WorkspaceHub/presence_redaction_report.json --slice-resolution Build/Enterprise/restricted_project_resolution_report.json --policy-report Build/Enterprise/policy_evaluation_report.json --out Build/WorkspaceHub/project_slice_aware_team_room_report.json
Tools/SagaWorkspaceHub/sagaworkspacehub governance-panel --evidence-root Build/Enterprise --out Build/WorkspaceHub/governance_panel_report.json
```

All reports include:

- `localOnly: true`
- `networkExposure: "None"`
- `mutatesSource: false`
- `enforcement: "ReportOnly"`

The first implementation deliberately has no `serve` command. Any future server
mode must be local loopback only, read-only, and explicitly documented before it
is added.

Phase 113-116 commands are local/report-only governance adapters. `audit-log`
reads JSONL evidence and can detect local hash-chain mismatches when hashes are
present, but it does not claim production tamper-proofing or compliance.
`review-approval` returns metadata-only approval decisions and never applies or
rolls back source changes. `restricted-export` evaluates requested artifact
visibility and policy metadata only; it does not copy artifacts or protect
source.

Phase 118-121 commands add restricted presence redaction, backend-neutral policy
action descriptors, a slice-aware Team Room report, and a local governance
dashboard model. They do not add Editor UI, Qt UI, auth, RBAC, realtime
collaboration, cloud collaboration, or permission enforcement.
