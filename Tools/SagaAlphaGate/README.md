# SagaAlphaGate

SagaAlphaGate is the Small-Team Alpha evidence CLI for Target 2 / Small-Team Alpha.

It reads existing focused evidence and writes deterministic reports for opening,
budget, script, gameplay blocker, acceptance, matrix, and closure gates. It does
not implement editor features, source-changing patch behavior, raw full CTest,
enterprise policy, or Target 3 / Enterprise-Evolvable Foundation work.

```sh
Tools/SagaAlphaGate/sagaalphagate opening-check --technical-preview-root Build/TechnicalPreview --out Build/SmallTeamAlpha/small_team_alpha_opening_report.json
Tools/SagaAlphaGate/sagaalphagate acceptance-plan --out Build/SmallTeamAlpha/alpha_acceptance_plan_report.json
Tools/SagaAlphaGate/sagaalphagate budget-report --out Build/SmallTeamAlpha/performance_budget_report.json --evidence-root Build/SmallTeamAlpha
Tools/SagaAlphaGate/sagaalphagate editor-workflow-evidence --out Build/SmallTeamAlpha/editor_workflow_model_report.json
Tools/SagaAlphaGate/sagaalphagate collaboration-evidence --out-root Build/SmallTeamAlpha
Tools/SagaAlphaGate/sagaalphagate gameplay-expansion-blockers --out Build/SmallTeamAlpha/gameplay_expansion_blocker_report.json
Tools/SagaAlphaGate/sagaalphagate accept-alpha --evidence-root Build/SmallTeamAlpha --out Build/SmallTeamAlpha/small_team_alpha_acceptance_report.json
Tools/SagaAlphaGate/sagaalphagate evidence-matrix --evidence-root Build/SmallTeamAlpha --out Build/SmallTeamAlpha/small_team_alpha_evidence_matrix.json
Tools/SagaAlphaGate/sagaalphagate close-alpha --evidence-root Build/SmallTeamAlpha --out Build/SmallTeamAlpha/small_team_alpha_closure_report.json
```

`editor-workflow-evidence` and `collaboration-evidence` are report-only
commands. They record backend-neutral editor workflow and local/offline
collaboration model evidence backed by focused tests, while preserving blocked
claims for Editor UI, Qt UI, graph editing, source mutation, full
collaboration, cloud workspace, account/auth, permission, and security work.

`gameplay-expansion-blockers` records Phase 86 as deferred. It does not mutate
gameplay systems, server authority, Runtime, ClientHost, scripts, scene/entity
data, or package/runtime behavior.

`accept-alpha`, `evidence-matrix`, and `close-alpha` close Phase 93-95 only as
focused local evidence. They keep deferred gameplay, deferred ClientHost
preview, deferred editor UI, missing evidence, and unverified broad health
visible.
