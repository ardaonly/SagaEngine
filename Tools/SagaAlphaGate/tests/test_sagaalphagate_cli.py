#!/usr/bin/env python3
"""Focused CLI tests for SagaAlphaGate."""

from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2].parent
SAGAALPHAGATE = REPO_ROOT / "Tools" / "SagaAlphaGate" / "sagaalphagate"


def run_cli(*args: str) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagaalphagate-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagaalphagate-nuget")
    env.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
    return subprocess.run(
        [str(SAGAALPHAGATE), *args],
        cwd=REPO_ROOT,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def write_report(path: Path, status: str = "Passed") -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps({"schemaVersion": 1, "status": status}) + "\n", encoding="utf-8")


def write_technical_preview_fixture(root: Path) -> None:
    write_report(root / "quickstart_report.json")
    write_report(root / "Acceptance" / "mvp_acceptance_report.json")
    write_report(root / "build_matrix_report.json")
    write_report(root / "docguard_report.json")
    write_report(root / "Package" / "SagaEngine-0.1-TechnicalPreview" / "technical_preview_package_report.json")
    write_report(root / "technical_preview_closure_report.json", "Accepted")


def write_script_validation(path: Path, status: str = "Passed", missing_runtime: int = 0) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps({
        "schemaVersion": 1,
        "tool": "sagascript",
        "command": "validate-artifacts",
        "status": status,
        "runtimeProofSummary": {
            "runtimeBackedWithEvidence": 0,
            "runtimeBackedMissingEvidence": missing_runtime,
            "projectionOnly": 3,
            "deferred": 1,
        },
    }) + "\n", encoding="utf-8")


def write_small_team_alpha_fixture(root: Path) -> None:
    reports = [
        "small_team_alpha_opening_report.json",
        "project_validation_report.json",
        "alpha_acceptance_plan_report.json",
        "projection_report.json",
        "patch_diff_report.json",
        "patch_review_report.json",
        "script_artifact_validation_report.json",
        "editor_workflow_model_report.json",
        "asset_workflow_report.json",
        "package_profile_matrix_report.json",
        "launch_profile_matrix_report.json",
        "launch_preview_report.json",
        "publish_report.json",
        "diagnostics_summary.json",
        "performance_budget_report.json",
        "workspace_session_report.json",
        "workspace_lock_report.json",
        "artifact_comments_report.json",
        "review_report.json",
        "team_room_status_report.json",
        "docguard_report.json",
    ]
    for report in reports:
        write_report(root / report)
    write_report(root / "gameplay_expansion_blocker_report.json", "Deferred")


def test_opening_check_passes_with_phase65_fixture() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        tp = root / "TechnicalPreview"
        write_technical_preview_fixture(tp)
        out = root / "opening_report.json"

        result = run_cli("opening-check", "--technical-preview-root", str(tp), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        payload = read_json(out)
        assert payload["status"] == "Passed"
        assert any(item["id"] == "phase65" and item["status"] == "Accepted" for item in payload["phase65Evidence"])


def test_opening_check_fails_without_phase65_closure() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        tp = root / "TechnicalPreview"
        write_technical_preview_fixture(tp)
        (tp / "technical_preview_closure_report.json").unlink()
        out = root / "opening_report.json"

        result = run_cli("opening-check", "--technical-preview-root", str(tp), "--out", str(out))

        assert result.returncode == 1
        payload = read_json(out)
        assert payload["status"] == "Blocked"
        assert any(d["code"] == "Alpha.Opening.TechnicalPreviewEvidenceMissingOrBlocked" for d in payload["diagnostics"])


def test_opening_report_records_claim_boundaries() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        tp = root / "TechnicalPreview"
        write_technical_preview_fixture(tp)
        out = root / "opening_report.json"

        assert run_cli("opening-check", "--technical-preview-root", str(tp), "--out", str(out)).returncode == 0
        payload = read_json(out)

        assert "product beta" in payload["blockedAlphaClaims"]
        assert "Hedef 3 started" in payload["blockedAlphaClaims"]
        assert any("small-team" in claim.lower() for claim in payload["allowedAlphaClaims"])


def test_acceptance_plan_contains_required_scenario_steps() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "alpha_acceptance_plan_report.json"

        result = run_cli("acceptance-plan", "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        payload = read_json(out)
        ids = [step["id"] for step in payload["scenarioMatrix"]]
        assert ids == [
            "simulated-users",
            "open-project",
            "simple-block-edit",
            "csharp-diff-review",
            "local-preview",
            "diagnostics-summary",
            "package-publish-check",
            "persistent-lock",
            "comment",
            "review-request",
            "team-room-status",
            "alpha-acceptance-report",
        ]


def test_acceptance_plan_keeps_ui_workflows_manual_or_later_phase() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "alpha_acceptance_plan_report.json"

        assert run_cli("acceptance-plan", "--out", str(out)).returncode == 0
        payload = read_json(out)
        classifications = {step["id"]: step["classification"] for step in payload["scenarioMatrix"]}
        assert classifications["team-room-status"] == "Manual"
        assert classifications["simple-block-edit"] == "DeferredToLaterPhase"
        assert classifications["review-request"] == "DeferredToLaterPhase"


def test_budget_report_emits_missing_measurements_explicitly() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "performance_budget_report.json"

        result = run_cli("budget-report", "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        payload = read_json(out)
        assert payload["status"] == "PassedWithMissingMeasurements"
        assert all(item["status"] == "MeasurementMissing" for item in payload["budgets"])
        assert any(d["code"] == "Alpha.Budget.MeasurementMissing" for d in payload["diagnostics"])


def test_budget_report_detects_violation_deterministically() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        measurements = root / "alpha_budget_measurements.json"
        measurements.write_text(json.dumps({
            "schemaVersion": 1,
            "measurements": [
                {"id": "project-validation", "measuredMs": 5000}
            ],
        }) + "\n", encoding="utf-8")
        out = root / "performance_budget_report.json"

        result = run_cli("budget-report", "--evidence-root", str(root), "--out", str(out))

        assert result.returncode == 1
        payload = read_json(out)
        assert payload["status"] == "Failed"
        assert any(item["id"] == "project-validation" and item["status"] == "BudgetExceeded" for item in payload["budgets"])


def test_budget_report_passes_with_all_measurements_inside_budget() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        measurements = root / "alpha_budget_measurements.json"
        measurements.write_text(json.dumps({
            "schemaVersion": 1,
            "measurements": [
                {"id": "editor-open", "measuredMs": 1},
                {"id": "project-validation", "measuredMs": 1},
                {"id": "script-analysis", "measuredMs": 1},
                {"id": "preview-launch", "measuredMs": 1},
                {"id": "package-check", "measuredMs": 1},
                {"id": "diagnostics-summary", "measuredMs": 1},
                {"id": "collaboration-update", "measuredMs": 1}
            ],
        }) + "\n", encoding="utf-8")
        out = root / "performance_budget_report.json"

        result = run_cli("budget-report", "--evidence-root", str(root), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        payload = read_json(out)
        assert payload["status"] == "Passed"
        assert payload["diagnostics"] == []


def test_script_evidence_records_valid_validation_without_alpha_overclaim() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        validation = root / "script_artifact_validation_report.json"
        write_script_validation(validation)
        out = root / "script_authoring_evidence_report.json"

        result = run_cli("script-evidence", "--script-validation", str(validation), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        payload = read_json(out)
        assert payload["status"] == "Passed"
        assert payload["scriptValidationStatus"] == "Passed"
        assert payload["runtimeProofSummary"]["projectionOnly"] == 3
        assert "full visual scripting" in payload["blockedClaims"]
        assert "arbitrary C# to blocks" in payload["blockedClaims"]


def test_script_evidence_blocks_invalid_validation_state() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        validation = root / "script_artifact_validation_report.json"
        write_script_validation(validation, status="Failed", missing_runtime=1)
        out = root / "script_authoring_evidence_report.json"

        result = run_cli("script-evidence", "--script-validation", str(validation), "--out", str(out))

        assert result.returncode == 1
        payload = read_json(out)
        codes = {diagnostic["code"] for diagnostic in payload["diagnostics"]}
        assert payload["status"] == "Blocked"
        assert "Alpha.ScriptEvidence.ValidationBlocked" in codes
        assert "Alpha.ScriptEvidence.RuntimeProofMissing" in codes


def test_editor_workflow_evidence_is_model_only() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "editor_workflow_model_report.json"

        result = run_cli("editor-workflow-evidence", "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        payload = read_json(out)
        assert payload["status"] == "Passed"
        assert payload["reportKind"] == "EditorWorkflowModel"
        assert "EditorAuthoringSpineTests" in payload["focusedTests"]
        assert "full editor MVP" in payload["blockedClaims"]
        assert "source mutation" in payload["blockedClaims"]


def test_collaboration_evidence_writes_local_model_reports() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)

        result = run_cli("collaboration-evidence", "--out-root", str(root))

        assert result.returncode == 0, result.stderr + result.stdout
        expected = [
            "workspace_session_report.json",
            "workspace_lock_report.json",
            "artifact_comments_report.json",
            "review_report.json",
            "team_room_status_report.json",
        ]
        for name in expected:
            payload = read_json(root / name)
            assert payload["status"] == "Passed"
            assert "CollaborationModelTests" in payload["focusedTests"]
            assert "cloud workspace" in payload["blockedClaims"]


def test_gameplay_expansion_blocker_report_defers_phase86_without_runtime_claims() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "gameplay_expansion_blocker_report.json"

        result = run_cli("gameplay-expansion-blockers", "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        payload = read_json(out)
        assert payload["status"] == "Deferred"
        assert payload["roadmapPhase"] == "Phase 86"
        assert payload["roadmapTitle"] == "MultiplayerSandbox Gameplay Expansion v1"
        ids = {blocker["id"] for blocker in payload["blockers"]}
        assert {"pickup", "door-trigger", "score-reward", "spawn-reset", "two-client-local-preview", "diagnostics-markers"} <= ids
        assert any("ClientHost" in blocker["requiredSeam"] for blocker in payload["blockers"])
        assert any(d["code"] == "Alpha.GameplayExpansion.Deferred" for d in payload["diagnostics"])


def test_accept_alpha_passes_with_complete_focused_evidence_fixture() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_small_team_alpha_fixture(root)
        out = root / "small_team_alpha_acceptance_report.json"

        result = run_cli("accept-alpha", "--evidence-root", str(root), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        payload = read_json(out)
        assert payload["status"] == "Passed"
        categories = {item["category"]: item["status"] for item in payload["categories"]}
        assert categories["ProjectTruth"] == "Passed"
        assert categories["CollaborationReviewEvidence"] == "Passed"
        assert categories["DeferredGameplayExpansion"] == "Deferred"
        assert categories["DeferredClientPreview"] == "Deferred"
        assert categories["DeferredEditorUI"] == "Deferred"
        assert "product beta" in payload["blockedAlphaClaims"]


def test_accept_alpha_blocks_missing_required_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_small_team_alpha_fixture(root)
        (root / "project_validation_report.json").unlink()
        out = root / "small_team_alpha_acceptance_report.json"

        result = run_cli("accept-alpha", "--evidence-root", str(root), "--out", str(out))

        assert result.returncode == 1
        payload = read_json(out)
        assert payload["status"] == "Blocked"
        assert any(item["id"] == "project-validation" and item["status"] == "MissingEvidence" for item in payload["evidence"])
        assert any(d["code"] == "Alpha.Acceptance.MissingEvidence" for d in payload["diagnostics"])


def test_evidence_matrix_is_deterministic_and_keeps_deferred_items_non_passing() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_small_team_alpha_fixture(root)
        first = root / "first_matrix.json"
        second = root / "second_matrix.json"

        assert run_cli("evidence-matrix", "--evidence-root", str(root), "--out", str(first)).returncode == 0
        assert run_cli("evidence-matrix", "--evidence-root", str(root), "--out", str(second)).returncode == 0
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        payload = read_json(first)
        assert payload["status"] == "PassedWithDeferredEvidence"
        statuses = {item["id"]: item["status"] for item in payload["matrix"]}
        assert statuses["deferred-gameplay-expansion"] == "Deferred"
        assert statuses["deferred-client-preview"] == "Deferred"
        assert statuses["deferred-editor-ui"] == "Deferred"


def test_close_alpha_requires_acceptance_and_matrix_reports() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_small_team_alpha_fixture(root)
        out = root / "small_team_alpha_closure_report.json"

        result = run_cli("close-alpha", "--evidence-root", str(root), "--out", str(out))

        assert result.returncode == 1
        payload = read_json(out)
        assert payload["status"] == "RejectedOrBlocked"
        codes = {diagnostic["code"] for diagnostic in payload["diagnostics"]}
        assert "Alpha.Closure.AcceptanceReportMissingOrBlocked" in codes
        assert "Alpha.Closure.EvidenceMatrixMissingOrBlocked" in codes


def test_close_alpha_partially_proven_after_acceptance_and_matrix() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_small_team_alpha_fixture(root)
        acceptance = root / "small_team_alpha_acceptance_report.json"
        matrix = root / "small_team_alpha_evidence_matrix.json"
        closure = root / "small_team_alpha_closure_report.json"

        assert run_cli("accept-alpha", "--evidence-root", str(root), "--out", str(acceptance)).returncode == 0
        assert run_cli("evidence-matrix", "--evidence-root", str(root), "--out", str(matrix)).returncode == 0
        result = run_cli("close-alpha", "--evidence-root", str(root), "--out", str(closure))

        assert result.returncode == 0, result.stderr + result.stdout
        payload = read_json(closure)
        assert payload["status"] == "PartiallyProven"
        assert payload["decision"] == "Small-Team Alpha partially proven"
        assert "Hedef 3 started" in payload["blockedClaims"]
        assert "does not start Phase 96" in payload["hedef3OpeningRecommendation"]


def test_missing_asset_source_truth_is_deferred_closure_debt() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_small_team_alpha_fixture(root)
        write_report(root / "asset_workflow_report.json", "MissingSourceOfTruth")
        write_report(root / "launch_profile_matrix_report.json", "PassedWithDeferredProfiles")
        acceptance = root / "small_team_alpha_acceptance_report.json"
        matrix = root / "small_team_alpha_evidence_matrix.json"
        closure = root / "small_team_alpha_closure_report.json"

        assert run_cli("accept-alpha", "--evidence-root", str(root), "--out", str(acceptance)).returncode == 0
        assert run_cli("evidence-matrix", "--evidence-root", str(root), "--out", str(matrix)).returncode == 0
        result = run_cli("close-alpha", "--evidence-root", str(root), "--out", str(closure))

        assert result.returncode == 0, result.stderr + result.stdout
        accepted = read_json(acceptance)
        matrix_payload = read_json(matrix)
        closed = read_json(closure)
        assert accepted["status"] == "Passed"
        assert any(item["id"] == "asset-workflow" and item["status"] == "Deferred" for item in accepted["evidence"])
        assert any(item["id"] == "launch-profile-matrix" and item["status"] == "Passed" for item in accepted["evidence"])
        assert matrix_payload["status"] == "PassedWithDeferredEvidence"
        assert closed["status"] == "PartiallyProven"


def test_reports_do_not_claim_forbidden_statuses() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        tp = root / "TechnicalPreview"
        write_technical_preview_fixture(tp)
        reports = [
            root / "opening.json",
            root / "acceptance.json",
            root / "budget.json",
            root / "script_evidence.json",
            root / "gameplay_expansion_blockers.json",
        ]

        assert run_cli("opening-check", "--technical-preview-root", str(tp), "--out", str(reports[0])).returncode == 0
        assert run_cli("acceptance-plan", "--out", str(reports[1])).returncode == 0
        assert run_cli("budget-report", "--out", str(reports[2])).returncode == 0
        validation = root / "script_artifact_validation_report.json"
        write_script_validation(validation)
        assert run_cli("script-evidence", "--script-validation", str(validation), "--out", str(reports[3])).returncode == 0
        assert run_cli("gameplay-expansion-blockers", "--out", str(reports[4])).returncode == 0
        combined = "\n".join(path.read_text(encoding="utf-8") for path in reports).lower()

        assert "product beta" in combined
        assert "blockedalphaclaims" in combined
        assert "raw full ctest passed" in combined
        assert "status\": \"passed\"" in combined


def main() -> None:
    tests = [
        test_opening_check_passes_with_phase65_fixture,
        test_opening_check_fails_without_phase65_closure,
        test_opening_report_records_claim_boundaries,
        test_acceptance_plan_contains_required_scenario_steps,
        test_acceptance_plan_keeps_ui_workflows_manual_or_later_phase,
        test_budget_report_emits_missing_measurements_explicitly,
        test_budget_report_detects_violation_deterministically,
        test_budget_report_passes_with_all_measurements_inside_budget,
        test_script_evidence_records_valid_validation_without_alpha_overclaim,
        test_script_evidence_blocks_invalid_validation_state,
        test_editor_workflow_evidence_is_model_only,
        test_collaboration_evidence_writes_local_model_reports,
        test_gameplay_expansion_blocker_report_defers_phase86_without_runtime_claims,
        test_accept_alpha_passes_with_complete_focused_evidence_fixture,
        test_accept_alpha_blocks_missing_required_evidence,
        test_evidence_matrix_is_deterministic_and_keeps_deferred_items_non_passing,
        test_close_alpha_requires_acceptance_and_matrix_reports,
        test_close_alpha_partially_proven_after_acceptance_and_matrix,
        test_missing_asset_source_truth_is_deferred_closure_debt,
        test_reports_do_not_claim_forbidden_statuses,
    ]
    for test in tests:
        test()
    print(f"{len(tests)} SagaAlphaGate CLI tests passed")


if __name__ == "__main__":
    main()
