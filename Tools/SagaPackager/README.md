# SagaPackager

SagaPackager is the local package proof CLI.

It validates `.sagaproj` package profiles, stages a deterministic local package
folder, checks required evidence, and runs the accepted server/headless smoke.

```sh
Tools/SagaPackager/sagapack validate --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --profile project-readiness-server-headless --out /tmp/package_report.json
Tools/SagaPackager/sagapack asset-validate --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --out /tmp/asset_workflow_report.json
Tools/SagaPackager/sagapack profile-matrix --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --out /tmp/package_profile_matrix_report.json
Tools/SagaPackager/sagapack stage --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --profile project-readiness-server-headless --out /tmp/package_report.json
Tools/SagaPackager/sagapack publish-check --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --profile project-readiness-server-headless --package-report /tmp/package_report.json --diagnostics-summary /tmp/diagnostics_summary.json --out /tmp/publish_report.json
Tools/SagaPackager/sagapack publish-check --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --profile project-readiness-server-headless --package-report /tmp/package_report.json --diagnostics-summary /tmp/diagnostics_summary.json --policy-report /tmp/policy_evaluation_report.json --review-approval-report /tmp/review_approval_report.json --audit-report /tmp/audit_report.json --restricted-export-report /tmp/restricted_export_report.json --out /tmp/publish_report.json
Tools/SagaPackager/sagapack smoke --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --profile project-readiness-server-headless --package-report /tmp/package_report.json --out /tmp/package_smoke_report.json --timeout-sec 5 --bin-dir build/RelWithDebInfo-0.0.11/bin
```

The tool is project-local and file-system deterministic. `asset-validate` and
`profile-matrix` are read-only report commands. Missing accepted asset source
truth is reported as `MissingSourceOfTruth`. The tool does not build shipping
distribution artifacts, upload anything, migrate ClientHost, import/cook
assets, or change runtime/server/editor gameplay code.

`publish-check` accepts optional governance reports. When supplied,
they add report-only gates for policy, review approval, audit evidence, and
restricted export evidence. Missing, malformed, denied, review-required, or
blocked supplied evidence blocks the check; omitting these inputs preserves the
existing publish-check behavior.
