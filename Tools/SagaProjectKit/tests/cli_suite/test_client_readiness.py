# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

from .support.environment import *
from .support.evidence import *
from .support.templates import *

def test_client_readiness_155_reports_are_deterministic_report_only_and_deferred() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evidence_root = root / "Build" / "SourceTruth"
        write_client_readiness_seed_evidence(evidence_root)

        prereq = evidence_root / "client_evaluation_prerequisite_audit_report.json"
        prereq_second = root / "client_evaluation_prerequisite_audit_report_second.json"
        first_prereq = run_cli("client-evaluation-prerequisite-audit", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(prereq))
        second_prereq = run_cli("client-evaluation-prerequisite-audit", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(prereq_second))
        assert first_prereq.returncode == 0, first_prereq.stderr + first_prereq.stdout
        assert second_prereq.returncode == 0, second_prereq.stderr + second_prereq.stdout
        assert prereq.read_text(encoding="utf-8") == prereq_second.read_text(encoding="utf-8")
        prereq_report = load_report(prereq)
        assert_report_only_invariants(prereq_report)
        assert prereq_report["clientEvaluationStatus"] == "Deferred"
        assert prereq_report["requiredFutureSeams"][0]["seamName"] == "ClientEvaluationRuntimeReadSeam"
        assert "No Product Beta is claimed." in prereq_report["nonClaims"]
        assert "No Client Evaluation implementation is claimed." in prereq_report["nonClaims"]

        boundary = evidence_root / "clienthost_boundary_plan_report.json"
        boundary_result = run_cli(
            "clienthost-boundary-plan",
            "--project",
            str(SAMPLE_PROJECT),
            "--client-evaluation-prerequisite",
            str(prereq),
            "--out",
            str(boundary),
        )
        assert boundary_result.returncode == 0, boundary_result.stderr + boundary_result.stdout
        boundary_report = load_report(boundary)
        assert_report_only_invariants(boundary_report)
        assert boundary_report["futureSeamName"] == "ClientEvaluationRuntimeReadSeam"
        assert "NetworkTransport" in boundary_report["deferredConcerns"]
        assert any("must not own scene/entity source truth" in item for item in boundary_report["forbiddenDirectOwnership"])

        deferral = evidence_root / "asset_import_cook_deferral_report.json"
        deferral_result = run_cli(
            "asset-import-cook-deferral",
            "--project",
            str(SAMPLE_PROJECT),
            "--asset-manifest-inventory",
            str(evidence_root / "asset_source_manifest_inventory_report.json"),
            "--out",
            str(deferral),
        )
        assert deferral_result.returncode == 0, deferral_result.stderr + deferral_result.stdout
        deferral_report = load_report(deferral)
        assert_report_only_invariants(deferral_report)
        assert deferral_report["outcome"] == "Deferred"
        assert {item["itemId"]: item["status"] for item in deferral_report["deferredPipelines"]} == {
            "asset-import": "Deferred",
            "asset-cook": "Deferred",
        }

        health = evidence_root / "broad_test_health_preflight_report.json"
        health_result = run_cli("broad-test-health-preflight", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(health))
        assert health_result.returncode == 0, health_result.stderr + health_result.stdout
        health_report = load_report(health)
        assert_report_only_invariants(health_report)
        assert health_report["unclaimedEvidence"][0]["evidenceId"] == "raw-full-ctest"
        assert health_report["unclaimedEvidence"][0]["status"] == "Unclaimed"
        assert {item["status"] for item in health_report["deferredEvidence"]} == {"Deferred"}

        matrix = evidence_root / "asset_workflow_evidence_matrix_report.json"
        closure = evidence_root / "asset_workflow_closure_report.json"
        matrix_first = run_cli("asset-workflow-evidence-matrix", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(matrix))
        matrix_second_path = root / "asset_workflow_evidence_matrix_report_second.json"
        matrix_second = run_cli("asset-workflow-evidence-matrix", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(matrix_second_path))
        closure_result = run_cli("asset-workflow-closure", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(closure))

        assert matrix_first.returncode == 0, matrix_first.stderr + matrix_first.stdout
        assert matrix_second.returncode == 0, matrix_second.stderr + matrix_second.stdout
        assert closure_result.returncode == 0, closure_result.stderr + closure_result.stdout
        assert matrix.read_text(encoding="utf-8") == matrix_second_path.read_text(encoding="utf-8")
        matrix_report = load_report(matrix)
        closure_report = load_report(closure)
        assert_report_only_invariants(matrix_report)
        assert_report_only_invariants(closure_report)
        assert matrix_report["outcome"] == "PartiallyProven"
        assert any(item["evidenceId"] == "raw-full-ctest" and item["matrixStatus"] == "Unclaimed" for item in matrix_report["evidence"])
        assert all(item["matrixStatus"] != "Passed" for item in matrix_report["evidence"] if item["sourceStatus"] == "Missing")
        assert closure_report["outcome"] in {"PartiallyProven", "SourceTruthFoundationEstablished"}
        assert closure_report["limitationsDocPresent"] is True
        assert closure_report["nextTargetRecommendations"] == ["Future Client Evaluation / Runtime Read Seam planning only."]

def test_client_host_boundary_and_155_block_when_required_evidence_is_missing() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        missing_prereq = root / "missing_client_evaluation_prerequisite_report.json"
        boundary = root / "clienthost_boundary_plan_report.json"
        boundary_result = run_cli(
            "clienthost-boundary-plan",
            "--project",
            str(SAMPLE_PROJECT),
            "--client-evaluation-prerequisite",
            str(missing_prereq),
            "--out",
            str(boundary),
        )
        assert boundary_result.returncode == 1
        assert load_report(boundary)["outcome"] == "MissingEvidence"

        evidence_root = root / "Build" / "SourceTruth"
        evidence_root.mkdir(parents=True)
        closure = root / "asset_workflow_closure_report.json"
        closure_result = run_cli("asset-workflow-closure", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(closure))
        assert closure_result.returncode == 1
        closure_report = load_report(closure)
        assert closure_report["outcome"] == "Blocked"
        assert all(item["matrixStatus"] != "Passed" for item in closure_report["requiredEvidence"] if item["sourceStatus"] == "Missing")

def test_runtime_readiness_v2_reports_partial_from_asset_workflow_evidence_and_is_deterministic() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evidence_root = root / "Build" / "SourceTruth"
        write_runtime_readiness_seed_evidence(evidence_root, partial=True)
        first = root / "runtime_readiness_v2_report_first.json"
        second = root / "runtime_readiness_v2_report_second.json"

        first_result = run_cli("runtime-readiness-v2", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(first))
        second_result = run_cli("runtime-readiness-v2", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(second))

        assert first_result.returncode == 0, first_result.stderr + first_result.stdout
        assert second_result.returncode == 0, second_result.stderr + second_result.stdout
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        report = load_report(first)
        assert_report_only_invariants(report)
        assert report["command"] == "runtime-readiness-v2"
        assert report["status"] == "PartiallyProven"
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["clientEvaluationStatus"] == "Deferred"
        assert report["clientHostImplementationClaim"] == "NotImplemented"
        assert report["runtimeImplementationClaim"] == "NotImplemented"
        assert "No ClientHost implementation is claimed." in report["nonClaims"]
        assert "ReadyForImplementationPlanning does not mean Runtime, ClientHost, or Client Evaluation exists." in report["nonClaims"]
        assert {item["status"] for item in report["adapterAudit"]} == {"MissingAdapterSeamWithPartialEvidence", "MissingAdapterSeam"}
        assert any(item["kind"] == "StaleGeneratedProjection" for item in report["forbiddenRuntimeSourceTruth"])
        assert len(report["fixtureContract"]) == 6

def test_runtime_readiness_v2_missing_and_blocked_evidence_do_not_pass() -> None:
    required_files = {
        "source_truth_inventory_report.json",
        "source_truth_gate_report.json",
        "scene_entity_validation_report.json",
        "asset_reference_gate_report.json",
        "generated_artifact_freshness_report.json",
        "runtime_readiness_gate_report.json",
        "package_source_truth_alignment_report.json",
        "launch_source_truth_alignment_report.json",
        "asset_workflow_closure_report.json",
    }
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        missing = root / "missing_runtime_readiness_v2_report.json"
        missing_result = run_cli(
            "runtime-readiness-v2",
            "--project",
            str(SAMPLE_PROJECT),
            "--evidence-root",
            str(root / "DoesNotExist"),
            "--out",
            str(missing),
        )
        assert missing_result.returncode == 1
        assert load_report(missing)["readinessStatus"] == "MissingEvidence"

        for filename in sorted(required_files):
            evidence_root = root / f"Evidence-{filename}"
            write_runtime_readiness_seed_evidence(evidence_root, partial=False)
            write_json(
                evidence_root / filename,
                {
                    "status": "Failed",
                    "localOnly": True,
                    "networkExposure": "None",
                    "mutatesSource": False,
                    "enforcement": "ReportOnly",
                },
            )
            out = root / f"{filename}.blocked.json"
            result = run_cli("runtime-readiness-v2", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(out))
            assert result.returncode == 1, filename
            report = load_report(out)
            assert report["readinessStatus"] == "Blocked", filename
            assert all(item["readinessStatus"] != "Passed" for item in report["requiredEvidence"] if item["sourceStatus"] == "Failed")

def test_runtime_readiness_v2_ready_for_implementation_planning_is_not_implementation() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evidence_root = root / "Build" / "SourceTruth"
        write_runtime_readiness_seed_evidence(evidence_root, partial=False)
        out = root / "runtime_readiness_v2_report.json"

        result = run_cli("runtime-readiness-v2", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        assert report["readinessStatus"] == "ReadyForImplementationPlanning"
        assert report["runtimeImplementationClaim"] == "NotImplemented"
        assert report["clientHostImplementationClaim"] == "NotImplemented"
        assert report["clientEvaluationStatus"] == "Deferred"
        assert report["planningScope"] == "Runtime read seam implementation planning only."
        assert_report_only_invariants(report)

def test_clienthost_evaluation_ownership_boundary_reads_runtime_readiness_and_is_deterministic() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evidence_root = root / "Build" / "SourceTruth"
        write_runtime_readiness_seed_evidence(evidence_root, partial=True)
        runtime = root / "runtime_readiness_v2_report.json"
        runtime_result = run_cli("runtime-readiness-v2", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(runtime))
        assert runtime_result.returncode == 0, runtime_result.stderr + runtime_result.stdout

        first = root / "clienthost_evaluation_ownership_boundary_report.json"
        second = root / "clienthost_evaluation_ownership_boundary_report_second.json"
        first_result = run_cli(
            "clienthost-evaluation-ownership-boundary",
            "--project",
            str(SAMPLE_PROJECT),
            "--runtime-readiness-v2",
            str(runtime),
            "--out",
            str(first),
        )
        second_result = run_cli(
            "clienthost-evaluation-ownership-boundary",
            "--project",
            str(SAMPLE_PROJECT),
            "--runtime-readiness-v2",
            str(runtime),
            "--out",
            str(second),
        )

        assert first_result.returncode == 0, first_result.stderr + first_result.stdout
        assert second_result.returncode == 0, second_result.stderr + second_result.stdout
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        report = load_report(first)
        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["runtimeReadinessV2Status"] == "PartiallyProven"
        assert report["clientHostImplementationClaim"] == "NotImplemented"
        assert report["clientEvaluationStatus"] == "Deferred"
        assert any("scene/entity source truth" in item for item in report["forbiddenDirectOwnership"])
        assert any(item["inputId"] == "launch_profile_contract-launch-profile-contract" for item in report["allowedFutureInputs"])

def test_clienthost_evaluation_ownership_boundary_missing_runtime_is_missing_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        out = root / "clienthost_evaluation_ownership_boundary_report.json"

        result = run_cli(
            "clienthost-evaluation-ownership-boundary",
            "--project",
            str(SAMPLE_PROJECT),
            "--runtime-readiness-v2",
            str(root / "missing_runtime_readiness_v2_report.json"),
            "--out",
            str(out),
        )

        assert result.returncode == 1
        report = load_report(out)
        assert report["readinessStatus"] == "MissingEvidence"
        assert_report_only_invariants(report)

def test_client_evaluation_launch_profile_contract_classifies_server_profile_and_does_not_launch() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evidence_root = root / "Build" / "SourceTruth"
        write_runtime_readiness_seed_evidence(evidence_root, partial=True)
        runtime = root / "runtime_readiness_v2_report.json"
        assert run_cli("runtime-readiness-v2", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(runtime)).returncode == 0

        out = root / "client_evaluation_launch_profile_contract_report.json"
        result = run_cli(
            "client-evaluation-launch-profile-contract",
            "--project",
            str(SAMPLE_PROJECT),
            "--runtime-readiness-v2",
            str(runtime),
            "--out",
            str(out),
        )

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["clientEvaluationStatus"] == "Deferred"
        assert report["plannedProfileId"] == "client-evaluation-local-no-network"
        assert report["plannedProfileStatus"] == "DeferredProfileNotImplemented"
        assert any(
            item["profileId"] == "local-server-headless" and item["classification"] == "ServerHeadlessEvidenceOnly"
            for item in report["currentLaunchProfiles"]
        )
        assert "No process launch is permitted in the current planning state." in report["cannotLaunchYet"]
        assert any(item["checkId"] == "NoProcessLaunched" and item["status"] == "Passed" for item in report["checks"])

def test_client_evaluation_launch_profile_contract_missing_and_blocked_runtime_do_not_pass() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        missing = root / "missing_contract_report.json"
        missing_result = run_cli(
            "client-evaluation-launch-profile-contract",
            "--project",
            str(SAMPLE_PROJECT),
            "--runtime-readiness-v2",
            str(root / "missing_runtime_readiness_v2_report.json"),
            "--out",
            str(missing),
        )
        assert missing_result.returncode == 1
        assert load_report(missing)["readinessStatus"] == "MissingEvidence"

        blocked_runtime = root / "blocked_runtime_readiness_v2_report.json"
        write_json(
            blocked_runtime,
            {
                "command": "runtime-readiness-v2",
                "readinessStatus": "Blocked",
                "status": "Blocked",
                "localOnly": True,
                "networkExposure": "None",
                "mutatesSource": False,
                "enforcement": "ReportOnly",
            },
        )
        blocked = root / "blocked_contract_report.json"
        blocked_result = run_cli(
            "client-evaluation-launch-profile-contract",
            "--project",
            str(SAMPLE_PROJECT),
            "--runtime-readiness-v2",
            str(blocked_runtime),
            "--out",
            str(blocked),
        )
        assert blocked_result.returncode == 1
        assert load_report(blocked)["readinessStatus"] == "Blocked"

def test_client_evaluation_no_network_plan_is_local_only_and_not_network_proof() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "client_evaluation_no_network_plan_report.json"

        result = run_cli("client-evaluation-no-network-plan", "--project", str(SAMPLE_PROJECT), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["networkExposure"] == "None"
        assert "No multiplayer proof is claimed." in report["nonClaims"]
        assert "No network proof is claimed." in report["nonClaims"]
        assert set(report["notStartedSystems"]) == {"sockets", "server process", "transport layer", "external network"}
        assert any(item["kind"] == "ReplicationProof" for item in report["forbiddenLaterInputs"])

def test_client_evaluation_diagnostics_shell_aggregates_prior_reports_deterministically() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source_truth_root = root / "Build" / "SourceTruth"
        evaluation_root = root / "Build" / "Evaluation"
        write_runtime_readiness_seed_evidence(source_truth_root, partial=True)
        evaluation_root.mkdir(parents=True)

        runtime = evaluation_root / "runtime_readiness_v2_report.json"
        boundary = evaluation_root / "clienthost_evaluation_ownership_boundary_report.json"
        launch = evaluation_root / "client_evaluation_launch_profile_contract_report.json"
        no_network = evaluation_root / "client_evaluation_no_network_plan_report.json"
        shell = evaluation_root / "client_evaluation_diagnostics_shell_report.json"
        shell_second = root / "client_evaluation_diagnostics_shell_report_second.json"

        assert run_cli("runtime-readiness-v2", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(source_truth_root), "--out", str(runtime)).returncode == 0
        assert run_cli("clienthost-evaluation-ownership-boundary", "--project", str(SAMPLE_PROJECT), "--runtime-readiness-v2", str(runtime), "--out", str(boundary)).returncode == 0
        assert run_cli("client-evaluation-launch-profile-contract", "--project", str(SAMPLE_PROJECT), "--runtime-readiness-v2", str(runtime), "--out", str(launch)).returncode == 0
        assert run_cli("client-evaluation-no-network-plan", "--project", str(SAMPLE_PROJECT), "--out", str(no_network)).returncode == 0

        first_result = run_cli("client-evaluation-diagnostics-shell", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evaluation_root), "--out", str(shell))
        second_result = run_cli("client-evaluation-diagnostics-shell", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evaluation_root), "--out", str(shell_second))

        assert first_result.returncode == 0, first_result.stderr + first_result.stdout
        assert second_result.returncode == 0, second_result.stderr + second_result.stdout
        assert shell.read_text(encoding="utf-8") == shell_second.read_text(encoding="utf-8")
        report = load_report(shell)
        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["runtimeReadinessV2Status"] == "PartiallyProven"
        assert report["clientHostBoundaryStatus"] == "PartiallyProven"
        assert "asset import/cook" in report["deferredSystems"]
        assert "No Client Evaluation implementation is claimed." in report["nonClaims"]
        assert "ClientHost is not implemented." in report["implementationBlockers"]

def test_client_evaluation_diagnostics_shell_missing_inputs_are_missing_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evidence_root = root / "Build" / "Evaluation"
        evidence_root.mkdir(parents=True)
        out = root / "client_evaluation_diagnostics_shell_report.json"

        result = run_cli("client-evaluation-diagnostics-shell", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(out))

        assert result.returncode == 1
        report = load_report(out)
        assert report["readinessStatus"] == "MissingEvidence"
        assert all(item["readinessStatus"] == "MissingEvidence" for item in report["requiredInputs"])

def test_client_evaluation_blocker_matrix_current_partial_and_missing_reports_do_not_pass() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source_truth_root = root / "Build" / "SourceTruth"
        evaluation_root = root / "Build" / "Evaluation"
        write_runtime_readiness_seed_evidence(source_truth_root, partial=True)
        evaluation_root.mkdir(parents=True)

        runtime = evaluation_root / "runtime_readiness_v2_report.json"
        boundary = evaluation_root / "clienthost_evaluation_ownership_boundary_report.json"
        launch = evaluation_root / "client_evaluation_launch_profile_contract_report.json"
        no_network = evaluation_root / "client_evaluation_no_network_plan_report.json"
        shell = evaluation_root / "client_evaluation_diagnostics_shell_report.json"
        matrix = evaluation_root / "client_evaluation_blocker_matrix_report.json"

        assert run_cli("runtime-readiness-v2", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(source_truth_root), "--out", str(runtime)).returncode == 0
        assert run_cli("clienthost-evaluation-ownership-boundary", "--project", str(SAMPLE_PROJECT), "--runtime-readiness-v2", str(runtime), "--out", str(boundary)).returncode == 0
        assert run_cli("client-evaluation-launch-profile-contract", "--project", str(SAMPLE_PROJECT), "--runtime-readiness-v2", str(runtime), "--out", str(launch)).returncode == 0
        assert run_cli("client-evaluation-no-network-plan", "--project", str(SAMPLE_PROJECT), "--out", str(no_network)).returncode == 0
        assert run_cli("client-evaluation-diagnostics-shell", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evaluation_root), "--out", str(shell)).returncode == 0

        result = run_cli("client-evaluation-blocker-matrix", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evaluation_root), "--out", str(matrix))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(matrix)
        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["clientEvaluationImplementationClaim"] == "NotImplemented"
        assert report["clientHostImplementationClaim"] == "NotImplemented"
        assert report["runtimeImplementationClaim"] == "NotImplemented"
        assert "ReadyForImplementationPlanning does not mean Client Evaluation is implemented." in report["nonClaims"]
        assert any(item["blockerId"] == "networking-explicitly-forbidden" and item["implementationStatus"] == "Forbidden" for item in report["blockers"])

        missing_root = root / "MissingEvaluation"
        missing_root.mkdir()
        missing_matrix = root / "missing_client_evaluation_blocker_matrix_report.json"
        missing_result = run_cli("client-evaluation-blocker-matrix", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(missing_root), "--out", str(missing_matrix))
        assert missing_result.returncode == 1
        assert load_report(missing_matrix)["readinessStatus"] == "MissingEvidence"

def test_client_evaluation_blocker_matrix_ready_planning_is_not_implementation() -> None:
    required_reports = [
        ("runtime-readiness-v2", "runtime_readiness_v2_report.json"),
        ("clienthost-evaluation-ownership-boundary", "clienthost_evaluation_ownership_boundary_report.json"),
        ("client-evaluation-launch-profile-contract", "client_evaluation_launch_profile_contract_report.json"),
        ("client-evaluation-no-network-plan", "client_evaluation_no_network_plan_report.json"),
        ("client-evaluation-diagnostics-shell", "client_evaluation_diagnostics_shell_report.json"),
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
        out = root / "client_evaluation_blocker_matrix_report.json"

        result = run_cli("client-evaluation-blocker-matrix", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        assert report["readinessStatus"] == "ReadyForImplementationPlanning"
        assert report["clientEvaluationImplementationClaim"] == "NotImplemented"
        assert report["clientHostImplementationClaim"] == "NotImplemented"
        assert report["runtimeImplementationClaim"] == "NotImplemented"
        assert "ReadyForImplementationPlanning does not mean Client Evaluation is implemented." in report["nonClaims"]
        assert_report_only_invariants(report)

TESTS = (
    test_client_readiness_155_reports_are_deterministic_report_only_and_deferred,
    test_client_host_boundary_and_155_block_when_required_evidence_is_missing,
    test_runtime_readiness_v2_reports_partial_from_asset_workflow_evidence_and_is_deterministic,
    test_runtime_readiness_v2_missing_and_blocked_evidence_do_not_pass,
    test_runtime_readiness_v2_ready_for_implementation_planning_is_not_implementation,
    test_clienthost_evaluation_ownership_boundary_reads_runtime_readiness_and_is_deterministic,
    test_clienthost_evaluation_ownership_boundary_missing_runtime_is_missing_evidence,
    test_client_evaluation_launch_profile_contract_classifies_server_profile_and_does_not_launch,
    test_client_evaluation_launch_profile_contract_missing_and_blocked_runtime_do_not_pass,
    test_client_evaluation_no_network_plan_is_local_only_and_not_network_proof,
    test_client_evaluation_diagnostics_shell_aggregates_prior_reports_deterministically,
    test_client_evaluation_diagnostics_shell_missing_inputs_are_missing_evidence,
    test_client_evaluation_blocker_matrix_current_partial_and_missing_reports_do_not_pass,
    test_client_evaluation_blocker_matrix_ready_planning_is_not_implementation,
)
