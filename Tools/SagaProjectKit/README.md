# SagaProjectKit

SagaProjectKit is the standalone Technical Preview project manifest tool.

It validates and resolves `.sagaproj` files without creating, repairing,
rewriting, or formatting project files.

```sh
Tools/SagaProjectKit/sagaproject validate --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --out project_validation_report.json
Tools/SagaProjectKit/sagaproject resolve --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --out project_resolution.json
Tools/SagaProjectKit/sagaproject doctor --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --out project_doctor_report.json
Tools/SagaProjectKit/sagaproject resolve-slice --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --slice samples/MultiplayerSandbox/.saga/slices/designer-local.slice.json --out project_slice_resolution_report.json
Tools/SagaProjectKit/sagaproject restricted-resolve --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --slice samples/MultiplayerSandbox/.saga/slices/designer-local.slice.json --out restricted_project_resolution_report.json
```

The tool is intentionally project-truth focused. It does not launch runtime or
server processes, stage packages, open the editor, or replace existing
`saga.project.json` product-shell behavior.

Slice commands are local report-only resolution modes. They classify visible,
restricted, excluded, and missing resources for a project slice without changing
project manifests or source files.
