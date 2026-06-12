# Saga Small-Team Alpha Acceptance Test Plan

> Status: Future acceptance matrix

This plan defines a future Small-Team Alpha pass/fail path. It is not a current
acceptance harness, product beta, release candidate, or completed alpha
workflow.

## Scenario Matrix

| Step | Scenario | Classification | Required evidence |
|---|---|---|---|
| 1 | Two or three simulated users are represented in local/offline workflow evidence. | Deferred | `workspace_session_report.json` |
| 2 | Open MultiplayerSandbox from shared project truth. | Automated candidate | `project_validation_report.json` |
| 3 | Edit a simple supported behavior through blocks. | Deferred | patch/diff evidence |
| 4 | Inspect and review the same behavior through C# source/diff views. | Deferred | `diff_review_report.json` |
| 5 | Run local preview from accepted launch profiles. | Automated candidate | `launch_preview_report.json` |
| 6 | Summarize diagnostics for the preview run. | Automated candidate | `diagnostics_summary.json` |
| 7 | Package and publish-check the project. | Automated candidate | `publish_report.json` |
| 8 | Use a persistent artifact lock. | Deferred | `workspace_lock_report.json` |
| 9 | Add and resolve an artifact comment. | Deferred | `artifact_comments_report.json` |
| 10 | Request and review a change. | Deferred | `review_report.json` |
| 11 | Show Team Room activity and review queue status. | Manual | `team_room_status_report.json` |
| 12 | Produce the alpha acceptance report. | Deferred | `small_team_alpha_acceptance_report.json` |

## Future Required Reports

A future acceptance harness must collect:

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

Automated candidate entries may use existing focused CLI/native evidence only
after the exact command, report, and non-claims are accepted. Manual entries
remain explicit until a stable backend-neutral adapter or focused model test
exists. Deferred entries are not current acceptance evidence.

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
