# Client Preview Prerequisite Layer Phase 171-175

Phase 171-175 adds a report-only prerequisite layer before any Client Preview,
Runtime asset consumption, asset import, or asset cook implementation work.

## Phase Scope

- Phase 171 audits asset import/cook prerequisites from existing
  `Build/SourceTruth` evidence. It records source-controlled asset truth,
  accepted future preview references, generated/package artifacts as evidence
  only, and keeps asset import and cook as `NotImplemented`.
- Phase 172 audits future Runtime asset consumption prerequisites from
  `Build/SourceTruth` and `Build/Preview`. It keeps the Runtime asset
  consumption seam as `NotImplemented` and forbids generated/package manifests,
  unresolved references, server state, transport state, import/cook outputs,
  and live staging output as canonical Runtime asset state.
- Phase 173 gates Runtime-facing generated projection freshness. Generated
  projections remain non-canonical even when fresh; stale projections keep the
  gate `PartiallyProven`.
- Phase 174 defines six future Client Preview regression fixtures. Every row is
  `PlanOnly` and `NotExecuted`.
- Phase 175 records focused test health for this prerequisite layer. The
  required focused row is SagaProjectKit CLI coverage; raw full CTest, heavy
  stress, and real transport proof remain `Unclaimed`.

## Evidence

All Phase 171-175 reports are emitted under `Build/Preview`:

- `asset_import_cook_prerequisite_report.json`
- `runtime_asset_consumption_prerequisite_report.json`
- `runtime_projection_freshness_gate_report.json`
- `client_preview_regression_fixture_plan_report.json`
- `preview_focused_test_health_gate_report.json`

Every report preserves `localOnly=true`, `networkExposure=None`,
`mutatesSource=false`, and `enforcement=ReportOnly`.

## Non-Claims

These reports do not implement Runtime, ClientHost, Client Preview, Runtime
gameplay, Server gameplay, networking, asset import, asset cook, Editor UI, or
Qt UI. They do not launch Runtime, ClientHost, client, or server processes, and
they do not claim raw full CTest, heavy stress, or real transport proof.
