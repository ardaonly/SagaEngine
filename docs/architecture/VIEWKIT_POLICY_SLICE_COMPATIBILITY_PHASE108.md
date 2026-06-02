# ViewKit Policy/Slice Compatibility

Hedef 3 Phase 108 adds a report-only SagaViewKit compatibility check for project
slice visibility decisions.

Command:

```sh
Tools/SagaViewKit/sagaviewkit slice-compatibility --view Simple --slice-resolution Build/Enterprise/restricted_project_resolution_report.json --out Build/Enterprise/view_slice_compatibility_report.json
```

Supported view names:

- `Simple`
- `Pro`
- `CSharpSource`
- `Diagnostics`

Simple View can show safe summaries and must disclose hidden or restricted
counts. Pro View can include diagnostics while retaining restricted placeholders.
CSharpSource View is blocked when slice visibility is `SummaryOnly`,
`OpaqueRestricted`, or `Hidden`. Diagnostics View reports visibility
diagnostics.

Report fields:

- `schemaVersion`
- `tool`
- `command`
- `status`
- `view`
- `sliceResolution`
- `decision`
- `visibleCount`
- `restrictedCount`
- `hiddenCount`
- `mutatesSource`
- `enforcement`
- `diagnostics`

The compatibility report does not open Editor UI, change view manifests, expose
source text, apply patches, or mutate project files.

## Non-Goals

Phase 108 does not implement UI enforcement, Runtime enforcement, Server
enforcement, cloud collaboration, identity, permission enforcement, source
encryption, or later-phase work.
