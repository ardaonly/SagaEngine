# Client Preview Implementation Path Phase 166-170

Phase 166-170 plans the first possible implementation path toward Client
Preview without implementing Runtime read seams, ClientHost, Client Preview,
launch execution, networking, asset import, or asset cook.

## Phase Scope

- Phase 166 defines `RuntimeReadSeamV1` as the minimal future Runtime read
  seam. The first future inputs are validated scene source truth, validated
  entities, component metadata, and accepted asset references. Generated
  projections remain evidence-only when freshness is proven or explicitly
  partial.
- Phase 167 defines `ClientHostPreviewShellV1` as the minimal future ClientHost
  preview shell. ClientHost may later own a local preview lifecycle shell,
  diagnostics envelope, Runtime read seam handoff request, and local
  no-network state label.
- Phase 168 defines the future package/launch alignment path. The planned
  launch profile is `client-preview-local-no-network`; the planned package
  profile is `technical-preview-client-preview-local`. Both remain deferred.
- Phase 169 defines an editor-less workflow that is CLI/report based only and
  avoids any Playable Editor, Editor UI, or Qt UI claim.
- Phase 170 aggregates Phase 156-169 evidence in `preview_evidence_gate`.

## Evidence

All generated Phase 166-170 evidence is emitted under `Build/Preview`:

- `minimal_runtime_read_seam_plan_report.json`
- `minimal_clienthost_preview_shell_plan_report.json`
- `package_launch_preview_alignment_plan_report.json`
- `editorless_preview_workflow_plan_report.json`
- `preview_evidence_gate_report.json`

Every report preserves:

- `localOnly=true`
- `networkExposure=None`
- `mutatesSource=false`
- `enforcement=ReportOnly`

## Non-Claims

These reports do not implement Runtime, ClientHost, Client Preview, Runtime
gameplay, Server gameplay, Editor UI, Qt UI, networking, asset import, or asset
cook. `ReadyForImplementationPlanning` means a future implementation batch can
be planned; it does not mean Client Preview is implemented.
