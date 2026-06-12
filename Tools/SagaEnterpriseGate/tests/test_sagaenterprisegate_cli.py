#!/usr/bin/env python3
"""Focused CLI tests for SagaEnterpriseGate."""

from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
SAGAENTERPRISEGATE = REPO_ROOT / "Tools" / "SagaEnterpriseGate" / "sagaenterprisegate"


def run_cli(*args: str) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagaenterprisegate-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagaenterprisegate-nuget")
    env.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
    return subprocess.run(
        [str(SAGAENTERPRISEGATE), *args],
        cwd=REPO_ROOT,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def write_json(path: Path, payload: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def policy(decision: str, name: str) -> dict:
    return {
        "schemaVersion": 1,
        "tool": "sagapolicy",
        "command": "evaluate",
        "status": {"Allow": "Passed", "Deny": "Denied", "RequiresReview": "ReviewRequired"}[decision],
        "decision": decision,
        "action": name,
        "resource": "scene://door",
        "role": "Designer",
        "mutatesSource": False,
        "enforcement": "ReportOnly",
        "diagnostics": [],
    }


def write_complete_fixture(root: Path) -> None:
    write_json(root / "policy_allow.json", policy("Allow", "OpenWorkspaceView"))
    write_json(root / "policy_deny.json", policy("Deny", "DeleteScene"))
    write_json(root / "policy_requires_review.json", policy("RequiresReview", "DangerousOperation"))
    write_json(
        root / "restricted_project_resolution_report.json",
        {
            "schemaVersion": 1,
            "tool": "sagaproject",
            "command": "restricted-resolve",
            "status": "Passed",
            "resourceVisibility": [
                {"id": "visible", "relativePath": "Scripts/Visible.cs", "visibility": "FullSource"},
                {"id": "hidden", "relativePath": "Scripts/Hidden.cs", "visibility": "Hidden"},
            ],
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )
    write_json(
        root / "blocked_publish_report.json",
        {
            "schemaVersion": 1,
            "tool": "sagapack",
            "command": "publish-check",
            "status": "Blocked",
            "gates": [{"name": "PolicyReportAccepted", "status": "Blocked"}],
            "diagnostics": [],
        },
    )
    write_json(
        root / "stale_review_approval_report.json",
        {
            "schemaVersion": 1,
            "tool": "sagaworkspacehub",
            "command": "review-approval",
            "status": "Blocked",
            "decision": "Stale",
            "localOnly": True,
            "networkExposure": "None",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )
    write_json(
        root / "restricted_export_report.json",
        {
            "schemaVersion": 1,
            "tool": "sagaworkspacehub",
            "command": "restricted-export",
            "status": "Blocked",
            "blockedExports": [{"id": "hidden", "visibility": "Hidden"}],
            "restrictedArtifacts": [{"id": "hidden", "visibility": "Hidden"}],
            "localOnly": True,
            "networkExposure": "None",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )


ENTERPRISE_DOCS = [
    "docs/architecture/SAGA_ENTERPRISE_THREAT_MODEL_V0.md",
    "docs/dev/SAGA_ENTERPRISE_CLAIM_LEVELS.md",
    "docs/architecture/POLICY_DOMAIN_MODEL_V0.md",
    "docs/architecture/ENTERPRISE_SECURITY_LIMITATIONS.md",
    "docs/architecture/CURRENT_STATUS.md",
]


def write_enterprise_docs(root: Path) -> None:
    for relative in ENTERPRISE_DOCS:
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("local/report-only evidence doc\n", encoding="utf-8")


def write_enterprise_fixture(root: Path) -> None:
    write_enterprise_docs(root)
    write_json(
        root / "small_team_alpha_closure_report.json",
        {
            "tool": "sagaalphagate",
            "command": "small-team-alpha-closure",
            "status": "PartiallyProven",
            "residualDebt": ["custom residual debt"],
        },
    )
    write_json(root / "policy_allow.json", policy("Allow", "OpenWorkspaceView"))
    write_json(root / "policy_review.json", policy("RequiresReview", "DangerousOperation"))
    write_json(
        root / "restricted_project_resolution_report.json",
        {
            "schemaVersion": 1,
            "tool": "sagaproject",
            "command": "restricted-resolve",
            "status": "Passed",
            "resourceVisibility": [
                {"id": "visible", "relativePath": "Scripts/Visible.cs", "visibility": "FullSource"},
                {"id": "hidden", "relativePath": "", "visibility": "Hidden"},
            ],
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )
    write_json(
        root / "workspace_session_report.json",
        {
            "schemaVersion": 1,
            "tool": "sagaworkspacehub",
            "command": "summarize",
            "status": "Passed",
            "localOnly": True,
            "networkExposure": "None",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )
    write_json(
        root / "audit_report.json",
        {
            "schemaVersion": 1,
            "tool": "sagaworkspacehub",
            "command": "audit-log",
            "status": "Passed",
            "summary": {"eventCount": 2, "hashChainStatus": "Consistent"},
            "localOnly": True,
            "networkExposure": "None",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )
    write_json(
        root / "review_approval_report.json",
        {
            "schemaVersion": 1,
            "tool": "sagaworkspacehub",
            "command": "review-approval",
            "status": "Passed",
            "decision": "ApprovedMetadataOnly",
            "localOnly": True,
            "networkExposure": "None",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )
    write_json(
        root / "publish_report.json",
        {
            "schemaVersion": 1,
            "tool": "sagapack",
            "command": "publish-check",
            "status": "Passed",
            "gates": [{"name": "PolicyReportAccepted", "status": "Passed"}],
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )
    write_json(
        root / "restricted_export_report.json",
        {
            "schemaVersion": 1,
            "tool": "sagaworkspacehub",
            "command": "restricted-export",
            "status": "Passed",
            "allowedExports": [{"id": "visible", "visibility": "FullSource"}],
            "blockedExports": [],
            "counts": {"allowedExports": 1, "blockedExports": 0, "restrictedArtifacts": 0},
            "localOnly": True,
            "networkExposure": "None",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )
    write_json(
        root / "presence_redaction_report.json",
        {
            "schemaVersion": 1,
            "tool": "sagaworkspacehub",
            "command": "presence-redaction",
            "status": "Passed",
            "counts": {"visibleActors": 1, "redactedActors": 1},
            "localOnly": True,
            "networkExposure": "None",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )
    write_json(
        root / "policy_actions_report.json",
        {
            "schemaVersion": 1,
            "tool": "sagaworkspacehub",
            "command": "policy-actions",
            "status": "Passed",
            "actions": [{"actionId": "ApproveReview", "status": "Enabled"}],
            "localOnly": True,
            "networkExposure": "None",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )
    write_json(
        root / "team_room_report.json",
        {
            "schemaVersion": 1,
            "tool": "sagaworkspacehub",
            "command": "team-room",
            "status": "Passed",
            "counts": {"visibleReviews": 1, "restrictedComments": 1},
            "localOnly": True,
            "networkExposure": "None",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )
    write_json(
        root / "governance_panel_report.json",
        {
            "schemaVersion": 1,
            "tool": "sagaworkspacehub",
            "command": "governance-panel",
            "status": "Passed",
            "counts": {"missingEvidence": 0, "residualDebt": 1},
            "localOnly": True,
            "networkExposure": "None",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )
    write_json(
        root / "policy_regression_report.json",
        {
            "schemaVersion": 1,
            "tool": "sagaenterprisegate",
            "command": "policy-regression",
            "status": "Passed",
            "checks": [],
            "summary": {"passed": 9, "failed": 0, "missingEvidence": 0, "total": 9},
            "localOnly": True,
            "networkExposure": "None",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )
    write_json(
        root / "docguard_report.json",
        {
            "schemaVersion": 1,
            "tool": "sagadocguard",
            "command": "scan",
            "status": "Passed",
            "forbiddenMatches": [],
            "missingRequiredNonClaims": [],
            "missingEvidence": [],
        },
    )


def check_status(report: dict, check_id: str) -> str:
    for check in report["checks"]:
        if check["checkId"] == check_id:
            return check["status"]
    raise AssertionError(f"missing check {check_id}")


def category_status(report: dict, category: str) -> str:
    for item in report["categories"]:
        if item["category"] == category:
            return item["status"]
    raise AssertionError(f"missing category {category}")


def step_status(report: dict, step_id: str) -> str:
    for item in report["steps"]:
        if item["stepId"] == step_id:
            return item["status"]
    raise AssertionError(f"missing step {step_id}")


def test_complete_fixture_passes_and_is_deterministic() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_complete_fixture(root)
        first = root / "first_policy_regression_report.json"
        second = root / "second_policy_regression_report.json"

        assert run_cli("policy-regression", "--evidence-root", str(root), "--out", str(first)).returncode == 0
        assert run_cli("policy-regression", "--evidence-root", str(root), "--out", str(second)).returncode == 0

        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        report = read_json(first)
        assert report["status"] == "Passed"
        assert report["summary"]["total"] == 9
        assert report["localOnly"] is True
        assert report["networkExposure"] == "None"
        assert report["mutatesSource"] is False
        assert report["enforcement"] == "ReportOnly"


def test_missing_policy_evidence_reports_missing_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_complete_fixture(root)
        (root / "policy_allow.json").unlink()
        out = root / "policy_regression_report.json"

        result = run_cli("policy-regression", "--evidence-root", str(root), "--out", str(out))

        assert result.returncode == 1
        report = read_json(out)
        assert check_status(report, "PolicyAllow") == "MissingEvidence"
        assert report["summary"]["missingEvidence"] == 1


def test_denied_dangerous_operation_remains_deterministic_and_no_claim() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_complete_fixture(root)
        out = root / "policy_regression_report.json"
        before = {path: path.read_bytes() for path in root.glob("*.json")}

        result = run_cli("policy-regression", "--evidence-root", str(root), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report_text = out.read_text(encoding="utf-8")
        assert check_status(read_json(out), "PolicyDeny") == "Passed"
        assert ("enterprise" + "-ready") not in report_text.lower()
        assert ("production" + "-ready") not in report_text.lower()
        for path, content in before.items():
            assert path.read_bytes() == content


def test_evidence_gate_complete_fixture_is_partially_proven_and_deterministic() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_enterprise_fixture(root)
        first = root / "first_enterprise_evidence_report.json"
        second = root / "second_enterprise_evidence_report.json"

        assert run_cli("evidence-gate", "--evidence-root", str(root), "--out", str(first)).returncode == 0
        assert run_cli("evidence-gate", "--evidence-root", str(root), "--out", str(second)).returncode == 0

        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        report = read_json(first)
        assert report["status"] in {"Passed", "PartiallyProven"}
        assert report["localOnly"] is True
        assert report["networkExposure"] == "None"
        assert category_status(report, "AuditLog") == "Passed"
        assert "custom residual debt" in report["residualDebt"]


def test_evidence_gate_missing_audit_log_reports_missing_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_enterprise_fixture(root)
        (root / "audit_report.json").unlink()
        out = root / "enterprise_evidence_report.json"

        result = run_cli("evidence-gate", "--evidence-root", str(root), "--out", str(out))

        assert result.returncode == 1
        report = read_json(out)
        assert category_status(report, "AuditLog") == "MissingEvidence"


def test_evidence_gate_missing_policy_regression_reports_missing_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_enterprise_fixture(root)
        (root / "policy_regression_report.json").unlink()
        out = root / "enterprise_evidence_report.json"

        result = run_cli("evidence-gate", "--evidence-root", str(root), "--out", str(out))

        assert result.returncode == 1
        report = read_json(out)
        assert category_status(report, "PolicyRegression") == "MissingEvidence"


def test_evidence_gate_does_not_emit_forbidden_positive_claim_text() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_enterprise_fixture(root)
        write_json(root / "bad_claim.json", {"tool": "fixture", "status": "Passed", "claim": "enterprise" + "-ready"})
        out = root / "enterprise_evidence_report.json"

        result = run_cli("evidence-gate", "--evidence-root", str(root), "--out", str(out))

        assert result.returncode == 1
        text = out.read_text(encoding="utf-8").lower()
        assert ("enterprise" + "-ready") not in text
        assert read_json(out)["status"] == "Blocked"


def test_acceptance_scenario_complete_fixture_is_partially_proven() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_enterprise_fixture(root)
        gate = root / "enterprise_evidence_report.json"
        out = root / "enterprise_acceptance_report.json"
        assert run_cli("evidence-gate", "--evidence-root", str(root), "--out", str(gate)).returncode == 0

        result = run_cli("acceptance-scenario", "--evidence-root", str(root), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = read_json(out)
        assert report["status"] in {"Passed", "PartiallyProven"}
        assert step_status(report, "EvidenceGateReport") in {"Passed", "PartiallyProven"}


def test_acceptance_missing_limitations_doc_reports_missing_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_enterprise_fixture(root)
        (root / "docs" / "architecture" / "ENTERPRISE_SECURITY_LIMITATIONS.md").unlink()
        gate = root / "enterprise_evidence_report.json"
        out = root / "enterprise_acceptance_report.json"
        run_cli("evidence-gate", "--evidence-root", str(root), "--out", str(gate))

        result = run_cli("acceptance-scenario", "--evidence-root", str(root), "--out", str(out))

        assert result.returncode == 1
        assert step_status(read_json(out), "LimitationsFreeze") == "MissingEvidence"


def test_acceptance_missing_evidence_gate_report_reports_missing_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_enterprise_fixture(root)
        out = root / "enterprise_acceptance_report.json"

        result = run_cli("acceptance-scenario", "--evidence-root", str(root), "--out", str(out))

        assert result.returncode == 1
        assert step_status(read_json(out), "EvidenceGateReport") == "MissingEvidence"


def test_closure_complete_fixture_is_partially_proven() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_enterprise_fixture(root)
        gate = root / "enterprise_evidence_report.json"
        acceptance = root / "enterprise_acceptance_report.json"
        out = root / "enterprise_closure_report.json"
        assert run_cli("evidence-gate", "--evidence-root", str(root), "--out", str(gate)).returncode == 0
        assert run_cli("acceptance-scenario", "--evidence-root", str(root), "--out", str(acceptance)).returncode == 0

        result = run_cli("closure-check", "--evidence-root", str(root), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        assert read_json(out)["outcome"] in {"FoundationEstablished", "PartiallyProven"}


def test_closure_missing_required_evidence_is_blocked() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_enterprise_fixture(root)
        gate = root / "enterprise_evidence_report.json"
        out = root / "enterprise_closure_report.json"
        assert run_cli("evidence-gate", "--evidence-root", str(root), "--out", str(gate)).returncode == 0

        result = run_cli("closure-check", "--evidence-root", str(root), "--out", str(out))

        assert result.returncode == 1
        assert read_json(out)["outcome"] == "Blocked"


def test_closure_flags_positive_enterprise_claim_in_input_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_enterprise_fixture(root)
        gate = root / "enterprise_evidence_report.json"
        acceptance = root / "enterprise_acceptance_report.json"
        assert run_cli("evidence-gate", "--evidence-root", str(root), "--out", str(gate)).returncode == 0
        assert run_cli("acceptance-scenario", "--evidence-root", str(root), "--out", str(acceptance)).returncode == 0
        write_json(root / "bad_claim.json", {"tool": "fixture", "status": "Passed", "claim": "enterprise" + "-ready"})
        out = root / "enterprise_closure_report.json"

        result = run_cli("closure-check", "--evidence-root", str(root), "--out", str(out))

        assert result.returncode == 1
        payload = read_json(out)
        assert payload["outcome"] == "Blocked"
        assert any(item["code"] == "EnterpriseGate.Claim.PositiveClaim" for item in payload["diagnostics"])


def main() -> None:
    tests = [
        test_complete_fixture_passes_and_is_deterministic,
        test_missing_policy_evidence_reports_missing_evidence,
        test_denied_dangerous_operation_remains_deterministic_and_no_claim,
        test_evidence_gate_complete_fixture_is_partially_proven_and_deterministic,
        test_evidence_gate_missing_audit_log_reports_missing_evidence,
        test_evidence_gate_missing_policy_regression_reports_missing_evidence,
        test_evidence_gate_does_not_emit_forbidden_positive_claim_text,
        test_acceptance_scenario_complete_fixture_is_partially_proven,
        test_acceptance_missing_limitations_doc_reports_missing_evidence,
        test_acceptance_missing_evidence_gate_report_reports_missing_evidence,
        test_closure_complete_fixture_is_partially_proven,
        test_closure_missing_required_evidence_is_blocked,
        test_closure_flags_positive_enterprise_claim_in_input_evidence,
    ]
    for test in tests:
        test()
    print(f"{len(tests)} SagaEnterpriseGate CLI tests passed")


if __name__ == "__main__":
    main()
