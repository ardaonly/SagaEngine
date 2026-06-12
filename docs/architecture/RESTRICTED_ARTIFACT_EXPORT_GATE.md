# Restricted Artifact Export Gate

This document adds a local restricted export evaluator in `Tools/SagaWorkspaceHub`.

Command:

```sh
Tools/SagaWorkspaceHub/sagaworkspacehub restricted-export --slice-resolution <restricted_project_resolution_report.json> --policy-report <policy_evaluation_report.json> --request <export_request.json> --out <restricted_export_report.json>
```

The command reads slice visibility metadata, a policy report, and a request for
an artifact id or path. It evaluates only and does not copy artifacts.

Visibility handling:

- `FullSource` can be allowed only when policy allows
- `SourceLinkOnly` emits reference metadata only
- `SummaryOnly` emits metadata and placeholders only
- `OpaqueRestricted` is blocked with a placeholder
- `Hidden` is blocked and omits the relative path

The output report contains request metadata, policy adapter decision,
`allowedExports`, `blockedExports`, `restrictedArtifacts`, `counts`, and the
common local/report-only fields. It does not provide DRM, encryption, source
protection, access control, or compliance enforcement.
