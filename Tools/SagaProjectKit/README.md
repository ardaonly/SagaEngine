# SagaProjectKit

SagaProjectKit is the standalone Technical Evaluation project manifest tool.

It validates and resolves `.sagaproj` files without creating, repairing,
rewriting, or formatting project files.

```sh
Tools/SagaProjectKit/sagaproject validate --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --out project_validation_report.json
Tools/SagaProjectKit/sagaproject resolve --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --out project_resolution.json
Tools/SagaProjectKit/sagaproject doctor --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --out project_doctor_report.json
Tools/SagaProjectKit/sagaproject resolve-slice --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --slice Samples/MultiplayerSandbox/.saga/slices/designer-local.slice.json --out project_slice_resolution_report.json
Tools/SagaProjectKit/sagaproject restricted-resolve --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --slice Samples/MultiplayerSandbox/.saga/slices/designer-local.slice.json --out restricted_project_resolution_report.json
```

The tool is intentionally project-truth focused. It does not launch runtime or
server processes, stage packages, or open the editor. `.sagaproj` schemaVersion
0 is the only project manifest contract.

Slice commands are local report-only resolution modes. They classify visible,
restricted, excluded, and missing resources for a project slice without changing
project manifests or source files.
