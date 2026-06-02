# Restricted Project Resolution in SagaProjectKit

Hedef 3 Phase 107 adds a bounded SagaProjectKit command for slice-scoped project
resolution.

Command:

```sh
Tools/SagaProjectKit/sagaproject restricted-resolve --project samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj --slice samples/MultiplayerSandbox/.saga/slices/designer-local.slice.json --out Build/Enterprise/restricted_project_resolution_report.json
```

Existing `validate`, `resolve`, and `doctor` behavior remains unchanged. The new
command consumes the same manifest loader, then applies a local project slice to
produce filtered report output.

Report fields:

- `schemaVersion`
- `tool`
- `command`
- `status`
- `project`
- `slice`
- `resolvedResources`
- `restrictedResources`
- `resourceVisibility`
- `mutatesSource`
- `enforcement`
- `diagnostics`

`enforcement` is `ReportOnly`. `mutatesSource` is `false`.

Hidden or opaque resources are omitted from visible resource lists and retained
as restricted placeholders or diagnostics. Missing included resources fail the
report because a slice should not silently reference absent artifacts.

## Non-Goals

This phase does not enforce edit permissions, hide files from direct filesystem
access, rewrite manifests, stage packages, apply patches, open an editor, or run
Runtime, Server, or ClientHost flows.
