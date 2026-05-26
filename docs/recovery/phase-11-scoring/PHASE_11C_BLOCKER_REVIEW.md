# Phase 11C Blocker Review

> Last updated: 2026-05-26
> Status: Phase 11C blocker review complete
> Phase 11: Scoring Re-Audit

Phase 11C classifies blockers. It does not fix them.

The hard blocker and soft blocker classifications below define the score caps.

## Hard Blockers

| Blocker | Affected category | Severity | Why it blocks | Removal condition | Future slice |
|---|---|---|---|---|---|
| Raw full CTest unresolved | Test health, CI/release | Hard evidence blocker | Prevents full local suite health and release candidate claims. | Safe replacement policy or complete passing raw full CTest. | Test/CI hardening audit. |
| Heavy gates unresolved | Stress/load/performance, networking/replication | Hard evidence blocker | `StressTests` and slow/long-running `ReplicationTests` are not normal evidence. | Stabilized opt-in heavy runs with documented pass evidence. | Heavy gate stabilization. |
| No stress/load/performance evidence | Release readiness | Hard blocker | Release/stress/performance readiness cannot be claimed. | Dedicated stress/load/perf evidence and policy. | Performance gate track. |
| Full AssetPipeline source import/cook incomplete | AssetPipeline, product packaging | Hard product blocker | Manifest writers exist, but source discovery/import/cook is not complete. | Source discovery/import/cook pipeline with generated artifacts and tests. | AssetPipeline source import/cook track. |
| ClientHost/runtime asset consumption incomplete | Runtime/product vertical | Hard product blocker | Runtime asset catalog exists, but product client consumption is not proven. | ClientHost-facing asset access path and focused product test. | Runtime/ClientHost asset consumption track. |
| RuntimeServiceRegistry asset service incomplete | Runtime ownership | Hard product blocker | Asset lifetime/context is not a Runtime service yet. | Runtime asset service design, registration, lifecycle, and tests. | Runtime service asset integration. |
| Full MultiplayerSandbox/playable product proof missing | Product beta | Hard blocker | No complete playable packaged vertical is proven. | Minimal packaged playable product loop. | Product Vertical Proof. |
| Full editor product readiness incomplete | Editor maturity | Hard blocker | Editor scaffolding and workflows remain incomplete. | Productized editor workflow evidence. | Editor Productization. |
| Full server replication/snapshot/reconciliation loop incomplete | Multiplayer/product | Hard blocker | Foundation exists, but product multiplayer loop is incomplete. | Accepted-state snapshot, reconciliation, and product loop tests. | Server/Replication Product Loop. |
| CI hard enforcement absent | CI/release | Hard release blocker | CI candidate policy exists, but hard enforcement is not proven. | Deterministic CI workflow with accepted gates. | Test/CI Hardening. |
| Release packaging not proven | Release readiness | Hard blocker | No install/update/release package proof. | Release packaging, dependency/license, and install smoke evidence. | Release/Packaging Hardening. |

## Soft Blockers

| Blocker | Impact | Future slice |
|---|---|---|
| Docs drift guard remains report-only | Limits docs/code alignment maturity. | Docs drift scanner implementation. |
| `GraphCanvas`, `QtPanel`, and CMake Qt visibility debt | Caps Editor public API maturity. | GraphCanvas boundary and QtPanel privatization. |
| `ui` and `collaboration` labels have zero tests | Limits test architecture clarity. | Test label coverage audit. |
| Editor stubs and dashboard rows remain incomplete | Caps editor product readiness. | Editor Productization. |
| ProjectBrowser real file-tree/workspace behavior incomplete | Caps editor workflow maturity. | ProjectBrowser workflow slice. |
| Publish hard gate is limited/report-first | Caps release readiness. | Publish enforcement follow-up after product proof. |
| Scripting product flow incomplete | Caps scripting maturity. | Scripting product workflow. |

## Deferred Debt

Deferred debt includes source cook/import, RuntimeServiceRegistry asset service,
ClientHost asset consumption, full product package proof, heavy gate
stabilization, raw full CTest policy resolution, editor productization,
release packaging, and CI hard enforcement.

## Evidence Blockers

- Raw full CTest lacks a complete passing run.
- Heavy gates lack accepted pass evidence.
- Product beta lacks a packaged playable vertical proof.
- Release candidate lacks product, stress/load/performance, packaging, and CI
  evidence.

## Environment/Test Blockers

The raw full CTest issue is both an evidence blocker and an environment/test
blocker because previous raw full CTest attempts ended in terminal/session
instability before a complete summary.

## Classification Impact

The blockers allow Foundation Established, may prevent Foundation Complete
Candidate, and block Product Beta Candidate and Release Candidate.

## Recommended Phase 11D

Proceed to final scoring with blocker caps applied. Do not score above caps and
do not convert deferred debt into accepted claims.
