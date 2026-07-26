# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

from .support.environment import *
from .support.evidence import *
from .support.templates import *

def test_minimal_runtime_read_seam_plan_is_deterministic_and_planning_only() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evaluation_root = create_runtime_read_seam_evaluation_evidence(root, partial=True)
        first = evaluation_root / "minimal_runtime_read_seam_plan_report.json"
        second = root / "minimal_runtime_read_seam_plan_report_second.json"

        result = run_cli("minimal-runtime-read-seam-plan", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evaluation_root), "--out", str(second))

        assert result.returncode == 0, result.stderr + result.stdout
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        report = load_report(first)
        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["futureSeamName"] == "RuntimeReadSeamV1"
        assert report["runtimeImplementationClaim"] == "NotImplemented"
        assert report["clientHostImplementationClaim"] == "NotImplemented"
        assert report["clientEvaluationImplementationClaim"] == "NotImplemented"
        assert any(item["inputId"] == "validated-project-scene-source-truth" for item in report["firstRuntimeConsumedSourceTruth"])
        assert any(item["classification"] == "EvidenceOnlyWhenFreshOrExplicitlyPartial" for item in report["evidenceOnlyGeneratedProjections"])
        assert "Runtime read seam adapter files" in report["futureTouchCandidates"]
        assert "Server implementation" in report["forbiddenTouchTargets"]

def test_minimal_runtime_read_seam_plan_missing_required_evidence_does_not_pass() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evidence_root = root / "Build" / "Evaluation"
        evidence_root.mkdir(parents=True)
        out = root / "minimal_runtime_read_seam_plan_report.json"

        result = run_cli("minimal-runtime-read-seam-plan", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(out))

        assert result.returncode == 1
        report = load_report(out)
        assert report["readinessStatus"] == "MissingEvidence"
        assert all(item["readinessStatus"] == "MissingEvidence" for item in report["requiredReports"])
        assert_report_only_invariants(report)

def test_minimal_clienthost_evaluation_shell_plan_keeps_clienthost_unimplemented() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evaluation_root = create_runtime_read_seam_evaluation_evidence(root, partial=True)
        out = evaluation_root / "minimal_clienthost_evaluation_shell_plan_report.json"

        report = load_report(out)

        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["futureSeamName"] == "ClientHostEvaluationShellV1"
        assert report["clientHostImplementationClaim"] == "NotImplemented"
        assert report["runtimeImplementationClaim"] == "NotImplemented"
        assert report["clientEvaluationImplementationClaim"] == "NotImplemented"
        assert any(item["responsibility"] == "Local evaluation lifecycle shell" for item in report["clientHostFutureOwnership"])
        assert "scene/entity source truth ownership" in report["outsideClientHost"]
        assert "no server launched" in report["startupDiagnosticsWithoutGameplay"]

def test_package_launch_evaluation_alignment_plan_does_not_stage_or_launch() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evaluation_root = create_runtime_read_seam_evaluation_evidence(root, partial=True)
        out = evaluation_root / "package_launch_evaluation_alignment_plan_report.json"

        report = load_report(out)

        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["requiredLaunchProfileId"] == "client-evaluation-local-no-network"
        assert report["requiredLaunchProfileStatus"] == "DeferredProfileNotImplemented"
        assert report["requiredPackageProfileId"] == "project-readiness-client-evaluation-local"
        assert report["requiredPackageProfileStatus"] == "DeferredProfileNotImplemented"
        assert any(item["classification"] == "ServerHeadlessEvidenceOnly" for item in report["currentLaunchProfiles"])
        assert any(item["classification"] == "ServerHeadlessPackageEvidenceOnly" for item in report["currentPackageProfiles"])
        assert "package staging" in report["deferredWork"]
        assert "LaunchLab execution" in report["deferredWork"]
        assert "server-headless profiles" in report["notRuntimeProof"]

def test_editorless_evaluation_workflow_plan_does_not_claim_playable_editor() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evaluation_root = create_runtime_read_seam_evaluation_evidence(root, partial=True)
        out = evaluation_root / "editorless_evaluation_workflow_plan_report.json"

        report = load_report(out)

        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["clientEvaluationImplementationClaim"] == "NotImplemented"
        assert report["playableEditorClaim"] == "NotClaimed"
        assert report["futureTrigger"] == "Future CLI/report workflow only."
        assert "Playable Editor workflow" in report["impossibleWithoutEditorUi"]
        assert "No Playable Editor is claimed." in report["nonClaims"]
        assert any(item["inputId"] == "evaluation-planning-reports" for item in report["consumedInputs"])

def test_evaluation_evidence_gate_current_partial_missing_and_blocked_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evaluation_root = create_runtime_read_seam_evaluation_evidence(root, partial=True)
        out = evaluation_root / "evaluation_evidence_gate_report.json"

        result = run_cli("evaluation-evidence-gate", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evaluation_root), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["runtimeImplementationClaim"] == "NotImplemented"
        assert report["clientHostImplementationClaim"] == "NotImplemented"
        assert report["clientEvaluationImplementationClaim"] == "NotImplemented"
        assert "ReadyForImplementationPlanning does not mean Client Evaluation is implemented." in report["nonClaims"]

        missing_root = root / "MissingEvaluation"
        missing_root.mkdir()
        missing_out = root / "missing_evaluation_evidence_gate_report.json"
        missing_result = run_cli("evaluation-evidence-gate", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(missing_root), "--out", str(missing_out))
        assert missing_result.returncode == 1
        assert load_report(missing_out)["readinessStatus"] == "MissingEvidence"

        blocked_root = root / "BlockedEvaluation"
        create_runtime_read_seam_evaluation_evidence(root / "BlockedSeed", partial=False)
        shutil.copytree(root / "BlockedSeed" / "Build" / "Evaluation", blocked_root)
        write_json(
            blocked_root / "minimal_runtime_read_seam_plan_report.json",
            {
                "command": "minimal-runtime-read-seam-plan",
                "readinessStatus": "Blocked",
                "status": "Blocked",
                "localOnly": True,
                "networkExposure": "None",
                "mutatesSource": False,
                "enforcement": "ReportOnly",
            },
        )
        blocked_out = root / "blocked_evaluation_evidence_gate_report.json"
        blocked_result = run_cli("evaluation-evidence-gate", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(blocked_root), "--out", str(blocked_out))
        assert blocked_result.returncode == 1
        assert load_report(blocked_out)["readinessStatus"] == "Blocked"

def test_evaluation_evidence_gate_ready_planning_is_not_implementation() -> None:
    required_reports = [
        ("runtime-readiness-v2", "runtime_readiness_v2_report.json"),
        ("clienthost-evaluation-ownership-boundary", "clienthost_evaluation_ownership_boundary_report.json"),
        ("client-evaluation-launch-profile-contract", "client_evaluation_launch_profile_contract_report.json"),
        ("client-evaluation-no-network-plan", "client_evaluation_no_network_plan_report.json"),
        ("client-evaluation-diagnostics-shell", "client_evaluation_diagnostics_shell_report.json"),
        ("client-evaluation-blocker-matrix", "client_evaluation_blocker_matrix_report.json"),
        ("minimal-runtime-read-seam-plan", "minimal_runtime_read_seam_plan_report.json"),
        ("minimal-clienthost-evaluation-shell-plan", "minimal_clienthost_evaluation_shell_plan_report.json"),
        ("package-launch-evaluation-alignment-plan", "package_launch_evaluation_alignment_plan_report.json"),
        ("editorless-evaluation-workflow-plan", "editorless_evaluation_workflow_plan_report.json"),
    ]
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evidence_root = root / "Build" / "Evaluation"
        for command, filename in required_reports:
            write_json(
                evidence_root / filename,
                {
                    "command": command,
                    "readinessStatus": "ReadyForImplementationPlanning",
                    "status": "ReadyForImplementationPlanning",
                    "localOnly": True,
                    "networkExposure": "None",
                    "mutatesSource": False,
                    "enforcement": "ReportOnly",
                },
            )
        out = root / "evaluation_evidence_gate_report.json"

        result = run_cli("evaluation-evidence-gate", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        assert report["readinessStatus"] == "ReadyForImplementationPlanning"
        assert report["runtimeImplementationClaim"] == "NotImplemented"
        assert report["clientHostImplementationClaim"] == "NotImplemented"
        assert report["clientEvaluationImplementationClaim"] == "NotImplemented"
        assert "ReadyForImplementationPlanning does not mean Client Evaluation is implemented." in report["nonClaims"]
        assert_report_only_invariants(report)

def test_asset_import_cook_asset_import_cook_prerequisite_is_partial_and_evidence_only() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evidence_root = root / "Build" / "SourceTruth"
        write_asset_import_cook_source_truth_evidence(evidence_root, partial=True)
        first = root / "asset_import_cook_prerequisite_report.json"
        second = root / "asset_import_cook_prerequisite_report_second.json"

        first_result = run_cli("evaluation-asset-import-cook-prerequisite", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(first))
        second_result = run_cli("evaluation-asset-import-cook-prerequisite", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(second))

        assert first_result.returncode == 0, first_result.stderr + first_result.stdout
        assert second_result.returncode == 0, second_result.stderr + second_result.stdout
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        report = load_report(first)
        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["assetImportImplementationClaim"] == "NotImplemented"
        assert report["assetCookImplementationClaim"] == "NotImplemented"
        assert any(item["path"] == "Assets/assets.source.json" and item["canonical"] is True for item in report["assetSourceTruth"])
        assert any(item["classification"] == "EvidenceOnly" and item["canonical"] is False for item in report["evidenceOnlyArtifacts"])
        assert report["acceptedFutureEvaluationReferences"][0]["path"] == "Assets/README.md"
        assert {item["status"] for item in report["missingPipelines"]} == {"NotImplemented"}

def test_runtime_asset_consumption_runtime_asset_consumption_prerequisite_keeps_runtime_unimplemented() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source_truth_root, evaluation_root = create_asset_import_cook_workflow_evidence(root, partial=True)
        out = evaluation_root / "runtime_asset_consumption_prerequisite_report.json"

        report = load_report(out)

        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["runtimeAssetConsumptionSeamClaim"] == "NotImplemented"
        assert report["runtimeImplementationClaim"] == "NotImplemented"
        assert any(item["status"] == "NotImplemented" for item in report["missingRuntimeAssetConsumptionSeams"])
        assert any(item["kind"] == "PackageAssetManifest" for item in report["forbiddenRuntimeAssetInputs"])
        assert report["acceptedAssetReferences"][0]["path"] == "Assets/README.md"
        assert str(source_truth_root) in report["sourceTruthRoot"]

def test_runtime_projection_freshness_runtime_projection_freshness_gate_keeps_projections_noncanonical() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source_truth_root = root / "Build" / "SourceTruth"
        write_asset_import_cook_source_truth_evidence(source_truth_root, partial=True)
        first = root / "runtime_projection_freshness_gate_report.json"
        second = root / "runtime_projection_freshness_gate_report_second.json"

        first_result = run_cli("runtime-projection-freshness-gate", "--project", str(SAMPLE_PROJECT), "--source-truth-root", str(source_truth_root), "--out", str(first))
        second_result = run_cli("runtime-projection-freshness-gate", "--project", str(SAMPLE_PROJECT), "--source-truth-root", str(source_truth_root), "--out", str(second))

        assert first_result.returncode == 0, first_result.stderr + first_result.stdout
        assert second_result.returncode == 0, second_result.stderr + second_result.stdout
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        report = load_report(first)
        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["runtimeImplementationClaim"] == "NotImplemented"
        assert report["runtimeGameplayClaim"] == "NotImplemented"
        assert report["staleGeneratedProjections"][0]["status"] == "Stale"
        assert all(item["canonical"] is False for item in report["nonCanonicalGeneratedProjections"])

def test_client_regression_fixture_client_evaluation_regression_fixture_plan_is_plan_only() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        _, evaluation_root = create_asset_import_cook_workflow_evidence(root, partial=True)
        first = evaluation_root / "client_evaluation_regression_fixture_plan_report.json"
        second = root / "client_evaluation_regression_fixture_plan_report_second.json"

        result = run_cli("client-evaluation-regression-fixture-plan", "--project", str(SAMPLE_PROJECT), "--evaluation-root", str(evaluation_root), "--out", str(second))

        assert result.returncode == 0, result.stderr + result.stdout
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        report = load_report(first)
        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert {item["fixtureId"] for item in report["fixtures"]} == {
            "valid-minimal-scene-entity",
            "stale-projection",
            "missing-asset",
            "no-network-evaluation",
            "clienthost-not-implemented",
            "runtime-read-seam-missing",
        }
        assert all(item["currentStatus"] == "PlanOnly" and item["executionStatus"] == "NotExecuted" for item in report["fixtures"])
        assert "No sockets." in report["forbiddenExecution"]

def test_focused_test_health_evaluation_focused_test_health_gate_preserves_unclaimed_rows() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        _, evaluation_root = create_asset_import_cook_workflow_evidence(root, partial=True)
        first = evaluation_root / "evaluation_focused_test_health_gate_report.json"
        second = root / "evaluation_focused_test_health_gate_report_second.json"

        first_result = run_cli("evaluation-focused-test-health-gate", "--project", str(SAMPLE_PROJECT), "--evaluation-root", str(evaluation_root), "--out", str(first))
        second_result = run_cli("evaluation-focused-test-health-gate", "--project", str(SAMPLE_PROJECT), "--evaluation-root", str(evaluation_root), "--out", str(second))

        assert first_result.returncode == 0, first_result.stderr + first_result.stdout
        assert second_result.returncode == 0, second_result.stderr + second_result.stdout
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        report = load_report(first)
        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["rawFullCTestClaim"] == "Unclaimed"
        assert report["heavyStressClaim"] == "Unclaimed"
        assert report["realTransportProofClaim"] == "Unclaimed"
        assert any(item["evidenceId"] == "sagaprojectkit-focused-cli-coverage" for item in report["focusedEvidence"])
        assert {"raw-full-ctest", "heavy-stress", "real-transport-proof"}.issubset({item["evidenceId"] for item in report["unclaimedEvidence"]})

def test_asset_import_cook_workflow_missing_and_blocked_evidence_do_not_pass() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        missing_root = root / "Missing"
        missing_root.mkdir()
        missing_out = root / "asset_import_cook_missing.json"
        missing = run_cli("evaluation-asset-import-cook-prerequisite", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(missing_root), "--out", str(missing_out))
        assert missing.returncode == 1
        assert load_report(missing_out)["readinessStatus"] == "MissingEvidence"

        _, evaluation_root = create_asset_import_cook_workflow_evidence(root / "BlockedSeed", partial=False)
        write_json(
            evaluation_root / "runtime_asset_consumption_prerequisite_report.json",
            {
                "command": "runtime-asset-consumption-prerequisite",
                "readinessStatus": "Blocked",
                "status": "Blocked",
                "localOnly": True,
                "networkExposure": "None",
                "mutatesSource": False,
                "enforcement": "ReportOnly",
            },
        )
        blocked_out = root / "client_evaluation_regression_fixture_plan_blocked.json"
        blocked = run_cli("client-evaluation-regression-fixture-plan", "--project", str(SAMPLE_PROJECT), "--evaluation-root", str(evaluation_root), "--out", str(blocked_out))
        assert blocked.returncode == 1
        assert load_report(blocked_out)["readinessStatus"] == "Blocked"

TESTS = (
    test_minimal_runtime_read_seam_plan_is_deterministic_and_planning_only,
    test_minimal_runtime_read_seam_plan_missing_required_evidence_does_not_pass,
    test_minimal_clienthost_evaluation_shell_plan_keeps_clienthost_unimplemented,
    test_package_launch_evaluation_alignment_plan_does_not_stage_or_launch,
    test_editorless_evaluation_workflow_plan_does_not_claim_playable_editor,
    test_evaluation_evidence_gate_current_partial_missing_and_blocked_evidence,
    test_evaluation_evidence_gate_ready_planning_is_not_implementation,
    test_asset_import_cook_asset_import_cook_prerequisite_is_partial_and_evidence_only,
    test_runtime_asset_consumption_runtime_asset_consumption_prerequisite_keeps_runtime_unimplemented,
    test_runtime_projection_freshness_runtime_projection_freshness_gate_keeps_projections_noncanonical,
    test_client_regression_fixture_client_evaluation_regression_fixture_plan_is_plan_only,
    test_focused_test_health_evaluation_focused_test_health_gate_preserves_unclaimed_rows,
    test_asset_import_cook_workflow_missing_and_blocked_evidence_do_not_pass,
)
