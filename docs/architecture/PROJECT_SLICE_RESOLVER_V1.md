# Project Slice Resolver v1

This document defines deterministic local resolution for project slices. The
resolver reads a `.sagaproj` manifest and a project slice file, then writes a
JSON report describing visible, restricted, excluded, and missing artifacts.

Command:

```sh
Tools/SagaProjectKit/sagaproject resolve-slice --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --slice samples/MultiplayerSandbox/.saga/slices/designer-local.slice.json --out Build/Enterprise/project_slice_resolution_report.json
```

Report fields:

- `schemaVersion`
- `tool`
- `command`
- `status`
- `project`
- `slice`
- `visibleArtifacts`
- `restrictedArtifacts`
- `missingArtifacts`
- `excludedArtifacts`
- `resourceVisibility`
- `mutatesSource`
- `enforcement`
- `diagnostics`

The resolver never edits project files, source files, package profiles, launch
profiles, or assets. It writes only the requested report.

Restricted resources are represented by placeholders. Hidden resources are not
silently erased; they remain visible as counts, placeholders, or diagnostics in
report output.

## Non-Goals

This document does not add Runtime behavior, Server behavior, client preview
behavior, Editor UI, Qt UI, workspace service work, cloud collaboration,
identity, permission enforcement, or source encryption.
