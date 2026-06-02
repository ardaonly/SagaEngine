# SagaViewKit

SagaViewKit is the Phase 57-58 view capability and Simple View honesty CLI.

It validates backend-neutral view capability manifests and checks that Simple
View projections disclose unsupported or advanced C# regions without claiming
edit, patch application, source rewrite, or regeneration behavior.

```sh
Tools/SagaViewKit/sagaviewkit emit-profiles --out Build/ViewCapabilities
Tools/SagaViewKit/sagaviewkit validate --manifest Build/ViewCapabilities/simple_view.json --out Build/ViewCapabilities/view_capability_report.json
Tools/SagaViewKit/sagaviewkit check-simple --projection Build/SagaScript/projection_report.json --profile Build/ViewCapabilities/simple_view.json --node-metadata Build/SagaScript/node_metadata.json --out Build/ViewCapabilities/simple_view_honesty_report.json
Tools/SagaViewKit/sagaviewkit slice-compatibility --view Simple --slice-resolution Build/Enterprise/restricted_project_resolution_report.json --out Build/Enterprise/view_slice_compatibility_report.json
```

Slice compatibility is a report-only check. It consumes SagaProjectKit
restricted resolution reports and states whether a view can disclose, summarize,
or block source visibility according to local slice metadata.
