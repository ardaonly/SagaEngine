# SagaProjectKit

SagaProjectKit owns the reusable `.sagaproj` schema, loader, path validation,
and canonical StarterArena project template.

It validates and resolves `.sagaproj` files without creating, repairing,
rewriting, or formatting project files.

```sh
Tools/SagaProjectKit/sagaproject validate --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --out project_validation_report.json
Tools/SagaProjectKit/sagaproject resolve --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --out project_resolution.json
Tools/SagaProjectKit/sagaproject doctor --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --out project_doctor_report.json
Tools/SagaProjectKit/sagaproject resolve-slice --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --slice Samples/MultiplayerSandbox/.saga/slices/designer-local.slice.json --out project_slice_resolution_report.json
Tools/SagaProjectKit/sagaproject restricted-resolve --project Samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --slice Samples/MultiplayerSandbox/.saga/slices/designer-local.slice.json --out restricted_project_resolution_report.json
Tools/SagaProjectKit/sagaproject template-plan --destination /tmp/MyStarterArena --display-name MyStarterArena --out /tmp/template_plan.json
Tools/SagaProjectKit/sagaproject template-create --plan /tmp/template_plan.json --sagapack Tools/SagaPackager/sagapack --engine-root . --runtime-bridge build/RelWithDebInfo-0.0.11/Managed/SagaScript.RuntimeBridge/SagaScript.RuntimeBridge.dll --out /tmp/template_create_report.json
```

The tool is intentionally project-truth focused. It does not launch runtime or
server processes, implement package resolution, or open the editor; template
creation invokes the SagaPackager owner as a child process. `.sagaproj` schemaVersion
0 is the project manifest contract. Optional input maps, exact dependency
intent, and explicit SourceTruth/GeneratedState/PersonalState/
SharedWorkspaceState policies extend schema 0 without invalidating existing
projects. `SagaProjectKit.Core` is referenced by SagaProjectKit, SagaPackager,
and SagaScript so these tools do not maintain competing project parsers.

Template planning does not touch the destination. It creates a new UUID project
identity, inventories the seed by hash, and records a canonical plan hash.
Creation verifies the plan and every seed input, writes a sibling staging
directory, delegates offline lock generation to SagaPackager, verifies the new
project-bound lock, and atomically renames it. A failure removes staging and
leaves the destination absent. Machine-local paths are transport-only inputs.
The child process drains both output streams concurrently, has a bounded
timeout, kills its complete process tree on timeout, and reports structured
exit/timeout diagnostics without leaving template staging residue.

Slice commands are local report-only resolution modes. They classify visible,
restricted, excluded, and missing resources for a project slice without changing
project manifests or source files.
