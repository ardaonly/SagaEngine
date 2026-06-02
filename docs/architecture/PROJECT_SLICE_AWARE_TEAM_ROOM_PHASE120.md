# Project Slice-Aware Team Room Phase 120

Phase 120 extends `Tools/SagaWorkspaceHub` with:

```sh
Tools/SagaWorkspaceHub/sagaworkspacehub team-room --team-room-status <report> --presence-redaction <report> --slice-resolution <report> [--policy-report <report>] --out <project_slice_aware_team_room_report.json>
```

The command reads local Team Room status, presence redaction, slice resolution,
and optional policy evidence. Visible actors, resources, reviews, and comments
are shown as summaries. Restricted reviews/comments/resources are represented
as counts and placeholders.

The report includes `visibleActors`, `visibleResources`, `visibleReviews`,
`visibleComments`, `counts`, `diagnostics`, `localOnly: true`,
`networkExposure: "None"`, `mutatesSource: false`, and
`enforcement: "ReportOnly"`.

This is not realtime collaboration, websocket collaboration, cloud
collaboration, UI, or a privacy guarantee.
