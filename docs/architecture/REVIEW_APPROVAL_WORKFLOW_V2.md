# Review Approval Workflow v2

Phase 114 adds a metadata-only review approval adapter in
`Tools/SagaWorkspaceHub`.

Command:

```sh
Tools/SagaWorkspaceHub/sagaworkspacehub review-approval --review-report <review_report.json> --policy-report <policy_evaluation_report.json> [--audit-report <audit_report.json>] --out <review_approval_report.json>
```

The command reads review metadata and a policy report. An optional audit report
is summarized as evidence. It never invokes SagaScript, applies patches, rolls
back patches, publishes artifacts, or mutates project/source files.

Decisions:

- `ApprovedMetadataOnly` when policy allows and target hash evidence is current
- `BlockedByPolicy` when policy denies
- `RequiresReview` when policy requires review
- `Stale` when expected and actual target hashes differ
- `Superseded` when review metadata is superseded
- `MissingEvidence` for missing or malformed required evidence
- `UnknownPolicyState` for unrecognized policy decisions

The output report includes common local/report-only fields:
`localOnly: true`, `networkExposure: "None"`, `mutatesSource: false`, and
`enforcement: "ReportOnly"`.
