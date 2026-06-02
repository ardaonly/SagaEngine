# SagaEngine Target 5 / Runtime Read Seam / Client Preview Planning Runtime Read Seam Roadmap

Target 5 / Runtime Read Seam / Client Preview Planning opens from the Target 4 / Source Truth Foundation `PartiallyProven` closure and is limited to
Runtime read seam foundation planning. Phase 156-160 is local, report-only, and
evidence-driven.

## Phase 156-160 Opening Batch

- Phase 156 freezes the scope boundary around Runtime read seam contracts only.
- Phase 157 defines future Runtime-readable scene/entity, asset reference,
  generated projection, and freshness inputs.
- Phase 158 audits required `Build/SourceTruth` evidence and records missing
  adapter seams without Runtime code changes.
- Phase 159 defines minimum fixture and projection freshness contracts without
  loading fixtures or generating projections.
- Phase 160 emits `runtime_readiness_v2_report.json` under `Build/Preview`
  using the optional SagaProjectKit `runtime-readiness-v2` command.

## Phase 161-165 Client Preview Planning

- Phase 161 records the ClientHost Preview ownership boundary and keeps
  ClientHost from owning scene/entity source truth, Runtime source truth,
  generated projections as canonical state, package/launch summaries, server
  state, networking/session transport, or asset import/cook outputs.
- Phase 162 records the Client Preview launch/profile contract. The current
  `local-server-headless` profile remains server-only evidence, while the
  future `client-preview-local-no-network` profile is deferred.
- Phase 163 records the no-network local mode plan with no sockets, no server
  process, no transport layer, no external network, and no multiplayer proof or
  network proof claim.
- Phase 164 records a diagnostics/report shell that aggregates existing JSON
  evidence only.
- Phase 165 records the Client Preview implementation blocker matrix and keeps
  Client Preview, ClientHost, Runtime adapters, networking, asset import, and
  asset cook as future work.

The Phase 161-165 reports are emitted only under `Build/Preview`:

- `clienthost_preview_ownership_boundary_report.json`
- `client_preview_launch_profile_contract_report.json`
- `client_preview_no_network_plan_report.json`
- `client_preview_diagnostics_shell_report.json`
- `client_preview_blocker_matrix_report.json`

## Phase 166-170 Implementation Path Planning

- Phase 166 records the minimal future `RuntimeReadSeamV1` implementation plan
  without touching Runtime files.
- Phase 167 records the minimal future `ClientHostPreviewShellV1` ownership and
  diagnostics plan without touching ClientHost files.
- Phase 168 records the future package/launch preview alignment plan. The
  planned launch profile is `client-preview-local-no-network`; the planned
  package profile is `technical-preview-client-preview-local`. Both remain
  deferred.
- Phase 169 records an editor-less CLI/report workflow and does not claim
  Playable Editor, Editor UI, or Qt UI capability.
- Phase 170 emits `preview_evidence_gate_report.json` to aggregate Phase
  156-169 evidence for a later first implementation batch.

The Phase 166-170 reports are emitted only under `Build/Preview`:

- `minimal_runtime_read_seam_plan_report.json`
- `minimal_clienthost_preview_shell_plan_report.json`
- `package_launch_preview_alignment_plan_report.json`
- `editorless_preview_workflow_plan_report.json`
- `preview_evidence_gate_report.json`

## Phase 171-175 Prerequisite Layer

- Phase 171 records asset import/cook prerequisites while keeping asset import
  and asset cook `NotImplemented`.
- Phase 172 records future Runtime asset consumption prerequisites while
  keeping Runtime asset consumption `NotImplemented`.
- Phase 173 records Runtime projection freshness readiness and keeps generated
  projections non-canonical.
- Phase 174 records future Client Preview regression fixtures as plan-only and
  not executed.
- Phase 175 records focused test health and keeps raw full CTest, heavy stress,
  and real transport proof unclaimed.

The Phase 171-175 reports are emitted only under `Build/Preview`:

- `asset_import_cook_prerequisite_report.json`
- `runtime_asset_consumption_prerequisite_report.json`
- `runtime_projection_freshness_gate_report.json`
- `client_preview_regression_fixture_plan_report.json`
- `preview_focused_test_health_gate_report.json`

## Success Meaning

Success for this batch means implementation planning readiness. It does not
mean Runtime, ClientHost, Client Preview, Runtime gameplay, Server gameplay,
asset import, asset cook, Editor UI, or Qt UI exists.

The Target 4 / Source Truth Foundation residual debt remains carried forward: Client Preview deferred,
ClientHost planning-only, asset import/cook deferred, Editor/Qt UI deferred,
and raw full CTest plus heavy/stress/soak/bot/transport proof unclaimed or
deferred.
