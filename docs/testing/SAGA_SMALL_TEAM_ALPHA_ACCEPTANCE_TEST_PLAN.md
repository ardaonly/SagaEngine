# Saga Small-Team Alpha Acceptance Test Plan

Status: Phase 67 acceptance plan for Target 2 / Small-Team Alpha.

This plan defines the future Small-Team Alpha pass/fail path before later
features are added. It is not the Phase 93 acceptance harness.

## Scenario Matrix

| Step | Scenario | Classification | Required evidence | Roadmap phase |
|---|---|---|---|---|
| 1 | Two or three simulated users are represented in local/offline workflow evidence. | DeferredToLaterPhase | `workspace_session_report.json` | Phase 87 |
| 2 | Open MultiplayerSandbox from shared project truth. | Automated | `project_validation_report.json` | Phase 66 |
| 3 | Edit a simple supported behavior through blocks. | DeferredToLaterPhase | patch/diff evidence | Phase 72 |
| 4 | Inspect and review the same behavior through C# source/diff views. | DeferredToLaterPhase | `diff_review_report.json` | Phase 79 |
| 5 | Run local preview from accepted launch profiles. | Automated | `launch_preview_report.json` | Phase 84 |
| 6 | Summarize diagnostics for the preview run. | Automated | `diagnostics_summary.json` | Phase 78 |
| 7 | Package and publish-check the project. | Automated | `publish_report.json` | Phase 83 |
| 8 | Use a persistent artifact lock. | DeferredToLaterPhase | `workspace_lock_report.json` | Phase 88 |
| 9 | Add and resolve an artifact comment. | DeferredToLaterPhase | `artifact_comments_report.json` | Phase 89 |
| 10 | Request and review a change. | DeferredToLaterPhase | `review_report.json` | Phase 90 |
| 11 | Show Team Room activity and review queue status. | Manual | `team_room_status_report.json` | Phase 92 |
| 12 | Produce the alpha acceptance report. | DeferredToLaterPhase | `small_team_alpha_acceptance_report.json` | Phase 93 |

## Required Reports

Phase 93 must eventually collect:

- `small_team_alpha_acceptance_report.json`;
- `project_validation_report.json`;
- `projection_report.json`;
- patch/diff evidence;
- `launch_preview_report.json`;
- `diagnostics_summary.json`;
- `publish_report.json`;
- `workspace_session_report.json`;
- `review_report.json`;
- `docguard_report.json`.

## Manual vs Automated Classification

Automated entries may use existing focused CLI/native evidence. Manual entries
remain explicit until a stable backend-neutral adapter or focused model test
exists. Deferred entries belong to later Target 2 / Small-Team Alpha phases and must not be claimed
by Phase 67.

## Non-Claims

- no product beta;
- no release candidate;
- no production MMO server;
- no complete visual scripting;
- no arbitrary C# roundtrip;
- no enterprise readiness.

Additional boundaries:

- no full UI automation;
- no source-changing patch behavior;
- no full collaboration;
- no cloud workspace;
- no raw full CTest claim.
