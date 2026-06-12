#!/usr/bin/env python3
"""Focused CLI tests for SagaDocGuard."""

from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2].parent
SAGADOCGUARD = REPO_ROOT / "Tools" / "SagaDocGuard" / "sagadocguard"


def run_cli(*args: str) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagadocguard-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagadocguard-nuget")
    env.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
    return subprocess.run(
        [str(SAGADOCGUARD), *args],
        cwd=REPO_ROOT,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def write_doc(root: Path, text: str, name: str = "README.md") -> Path:
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


def create_evidence(root: Path) -> None:
    paths = [
        "docs/architecture/DIAGNOSTICS_MIGRATION_AUDIT.md",
        "docs/internal/product-history/SAGA_PROJECT_MODEL_INVENTORY_MILESTONE14.md",
        "docs/internal/product-history/MULTIPLAYER_SANDBOX_PLAYABLE_VERTICAL_SLICE_MILESTONE20_25.md",
        "docs/internal/product-history/DIAGNOSTICS_VISIBILITY_PRODUCT_LAYER_MILESTONE26_28.md",
        "docs/internal/product-history/PACKAGING_PUBLISH_PROOF_MILESTONE29_33.md",
        "docs/internal/product-history/SAGAWEAVER_SAGASCRIPT_MVP_MILESTONE34_43.md",
        "docs/architecture/EDITOR.md",
        "docs/architecture/SAGA_COLLABORATIVE_WORKSPACE_ARCHITECTURE.md",
        "docs/architecture/SAGA_PRODUCT_SHELL_WORKFLOW_CONTRACT.md",
        "docs/internal/product-history/SAGAENGINE_0_1_TECHNICAL_PREVIEW_QUICKSTART.md",
        "docs/internal/product-history/SAGA_RELEASE_VOCABULARY_AND_CLAIM_LEVELS.md",
        "docs/internal/product-history/SAGAENGINE_0_1_TECHNICAL_PREVIEW_CLOSURE_MILESTONE60_65.md",
        "docs/architecture/SAGA_PRODUCT_SHELL_WORKFLOW_CONTRACT.md",
        "docs/testing/SAGA_SMALL_TEAM_ALPHA_ACCEPTANCE_TEST_PLAN.md",
        "docs/architecture/SAGA_PERFORMANCE_BUDGETS.md",
        "docs/architecture/SAGA_GAMEPLAY_NODE_LIBRARY_V1.md",
        "docs/architecture/SOURCE_PATCH_APPLY_POLICY.md",
        "docs/architecture/EDITOR_UI_IMPLEMENTATION_STRATEGY.md",
        "docs/architecture/CLIENT_PREVIEW_AND_RUNTIME_UI_STRATEGY.md",
        "docs/architecture/SAGA_GENERATED_FILES_POLICY.md",
        "docs/architecture/SAGA_ARTIFACT_CONTRACTS_V1.md",
        "docs/architecture/PUBLISH.md",
        "docs/architecture/SAGA_COLLABORATIVE_WORKSPACE_ARCHITECTURE.md",
        "docs/internal/product-history/SAGAENGINE_SMALL_TEAM_ALPHA_QUICKSTART.md",
        "docs/internal/product-history/MULTIPLAYER_SANDBOX_SMALL_TEAM_ALPHA_TUTORIAL.md",
        "docs/internal/product-history/SMALL_TEAM_ALPHA_CSHARP_BLOCKS_AUTHORING_GUIDE.md",
        "docs/internal/product-history/SMALL_TEAM_ALPHA_COLLABORATION_GUIDE.md",
        "docs/internal/product-history/SMALL_TEAM_ALPHA_KNOWN_LIMITATIONS.md",
        "docs/internal/product-history/SMALL_TEAM_ALPHA_TROUBLESHOOTING.md",
        "docs/architecture/CURRENT_STATUS.md",
        "docs/architecture/SAGA_ENTERPRISE_THREAT_MODEL_V0.md",
        "docs/architecture/SAGA_ENTERPRISE_THREAT_MODEL_V0.md",
        "docs/dev/SAGA_ENTERPRISE_CLAIM_LEVELS.md",
        "docs/architecture/POLICY_DOMAIN_MODEL_V0.md",
        "docs/architecture/PROJECT_ROLE_PROFILES_V1.md",
        "docs/architecture/DANGEROUS_OPERATION_POLICY_GATE_V1.md",
        "docs/architecture/CLAIM_AND_EVIDENCE_POLICY.md",
        "docs/architecture/PROJECT_SLICE_SCHEMA_V0.md",
        "docs/architecture/PROJECT_SLICE_RESOLVER_V1.md",
        "docs/architecture/SOURCE_VISIBILITY_LEVELS_V1.md",
        "docs/architecture/PROJECT_SLICE_RESOLVER_V1.md",
        "docs/architecture/PROJECT_SLICE_SCHEMA_V0.md",
        "docs/architecture/SAGA_WORKSPACEHUB_SERVER_ARCHITECTURE.md",
        "docs/architecture/IMMUTABLE_AUDIT_LOG_V1.md",
        "docs/architecture/REVIEW_APPROVAL_WORKFLOW_V2.md",
        "docs/architecture/PUBLISH_POLICY_INTEGRATION.md",
        "docs/architecture/RESTRICTED_ARTIFACT_EXPORT_GATE.md",
        "docs/architecture/POLICY_DOMAIN_MODEL_V0.md",
        "docs/architecture/SAGA_COLLABORATION_PRESENCE_LOCK_BOUNDARY.md",
        "docs/architecture/DANGEROUS_OPERATION_POLICY_GATE_V1.md",
        "docs/architecture/SAGA_WORKSPACEHUB_SERVER_ARCHITECTURE.md",
        "docs/architecture/AUTHORING_AUTHORITY_MODEL.md",
        "docs/architecture/ENTERPRISE_SECURITY_LIMITATIONS.md",
        "docs/architecture/CURRENT_STATUS.md",
    ]
    for relative in paths:
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text("evidence\n", encoding="utf-8")


def non_claim_text(extra: str = "") -> str:
    return f"""
Accepted claims are bounded.

- no beta product status
- no candidate release status
- no production MMO server
- no complete visual scripting
- no arbitrary C# roundtrip
- no enterprise readiness

{extra}
"""


def test_positive_production_claim_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        docs = root / "docs"
        evidence = root / "evidence"
        create_evidence(evidence)
        write_doc(docs, non_claim_text("This is a " + "production-ready " + "MMO server."))
        report = root / "docguard_report.json"

        result = run_cli("scan", "--docs", str(docs), "--evidence-root", str(evidence), "--out", str(report))

        assert result.returncode == 1
        payload = read_json(report)
        assert any(match["ruleId"] == "claim.productionMmo" for match in payload["forbiddenMatches"])


def test_required_non_claims_detected_and_scan_passes() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        docs = root / "docs"
        evidence = root / "evidence"
        create_evidence(evidence)
        write_doc(docs, non_claim_text())
        report = root / "docguard_report.json"

        result = run_cli("scan", "--docs", str(docs), "--evidence-root", str(evidence), "--out", str(report))

        assert result.returncode == 0, result.stderr + result.stdout
        payload = read_json(report)
        assert payload["status"] == "Passed"
        assert payload["missingRequiredNonClaims"] == []


def test_missing_evidence_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        docs = root / "docs"
        evidence = root / "evidence"
        evidence.mkdir()
        write_doc(docs, non_claim_text())
        report = root / "docguard_report.json"

        result = run_cli("scan", "--docs", str(docs), "--evidence-root", str(evidence), "--out", str(report))

        assert result.returncode == 1
        assert len(read_json(report)["missingEvidence"]) > 0


def test_limitations_doc_is_required_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        docs = root / "docs"
        evidence = root / "evidence"
        create_evidence(evidence)
        (evidence / "docs" / "architecture" / "ENTERPRISE_SECURITY_LIMITATIONS.md").unlink()
        write_doc(docs, non_claim_text())
        report = root / "docguard_report.json"

        result = run_cli("scan", "--docs", str(docs), "--evidence-root", str(evidence), "--out", str(report))

        assert result.returncode == 1
        missing = {item["evidenceId"] for item in read_json(report)["missingEvidence"]}
        assert "enterprise-security-limitations" in missing


def test_forbidden_phrase_in_non_claim_context_is_reviewed() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        docs = root / "docs"
        evidence = root / "evidence"
        create_evidence(evidence)
        write_doc(docs, non_claim_text("Non-claim: no " + "cloud " + "workspace platform."))
        report = root / "docguard_report.json"

        result = run_cli("scan", "--docs", str(docs), "--evidence-root", str(evidence), "--out", str(report))

        assert result.returncode == 0, result.stderr + result.stdout
        payload = read_json(report)
        assert payload["forbiddenMatches"] == []
        assert any(match["ruleId"] == "claim.cloudWorkspace" for match in payload["reviewedNonClaimMatches"])


def test_raw_full_ctest_claim_fails_without_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        docs = root / "docs"
        evidence = root / "evidence"
        create_evidence(evidence)
        write_doc(docs, non_claim_text("The " + "raw full " + "CTest passed yesterday."))
        report = root / "docguard_report.json"

        result = run_cli("scan", "--docs", str(docs), "--evidence-root", str(evidence), "--out", str(report))

        assert result.returncode == 1
        assert any(match["ruleId"] == "claim.rawFullCtest" for match in read_json(report)["forbiddenMatches"])


def test_positive_hedef3_started_claim_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        docs = root / "docs"
        evidence = root / "evidence"
        create_evidence(evidence)
        write_doc(docs, non_claim_text("Hedef " + "3 started."))
        report = root / "docguard_report.json"

        result = run_cli("scan", "--docs", str(docs), "--evidence-root", str(evidence), "--out", str(report))

        assert result.returncode == 1
        assert any(match["ruleId"] == "claim.hedef3Started" for match in read_json(report)["forbiddenMatches"])


def test_enterprise_ready_claim_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        docs = root / "docs"
        evidence = root / "evidence"
        create_evidence(evidence)
        write_doc(docs, non_claim_text("The product is " + "enterprise-ready."))
        report = root / "docguard_report.json"

        result = run_cli("scan", "--docs", str(docs), "--evidence-root", str(evidence), "--out", str(report))

        assert result.returncode == 1
        assert any(match["ruleId"] == "claim.enterpriseReady" for match in read_json(report)["forbiddenMatches"])


def test_security_and_compliance_claims_fail() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        docs = root / "docs"
        evidence = root / "evidence"
        create_evidence(evidence)
        write_doc(docs, non_claim_text(
            "This is a secure cloud platform with SOC2, ISO 27001, and zero-trust coverage."
        ))
        report = root / "docguard_report.json"

        result = run_cli("scan", "--docs", str(docs), "--evidence-root", str(evidence), "--out", str(report))

        assert result.returncode == 1
        rule_ids = {match["ruleId"] for match in read_json(report)["forbiddenMatches"]}
        assert "claim.secureCloudPlatform" in rule_ids
        assert "claim.complianceReady" in rule_ids
        assert "claim.zeroTrust" in rule_ids


def test_auth_permission_and_stress_claims_fail() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        docs = root / "docs"
        evidence = root / "evidence"
        create_evidence(evidence)
        write_doc(docs, non_claim_text(
            "The auth system implemented, permission system implemented, "
            "heavy stress passed, and real transport stress passed."
        ))
        report = root / "docguard_report.json"

        result = run_cli("scan", "--docs", str(docs), "--evidence-root", str(evidence), "--out", str(report))

        assert result.returncode == 1
        rule_ids = {match["ruleId"] for match in read_json(report)["forbiddenMatches"]}
        assert "claim.authSystemImplemented" in rule_ids
        assert "claim.permissionSystemImplemented" in rule_ids
        assert "claim.heavyStress" in rule_ids
        assert "claim.realTransportStress" in rule_ids


def test_architecture_progress_filename_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        docs = root / "docs"
        evidence = root / "evidence"
        create_evidence(evidence)
        write_doc(docs, non_claim_text())
        write_doc(docs / "architecture", "Architecture boundary.\n", "RUNTIME_MILESTONE.md")
        report = root / "docguard_report.json"

        result = run_cli("scan", "--docs", str(docs), "--evidence-root", str(evidence), "--out", str(report))

        assert result.returncode == 1
        assert any(match["ruleId"] == "architecture.progressFilename" for match in read_json(report)["forbiddenMatches"])


def test_architecture_progress_language_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        docs = root / "docs"
        evidence = root / "evidence"
        create_evidence(evidence)
        write_doc(docs, non_claim_text())
        write_doc(docs / "architecture", "Core Product " + "Vision\n", "COLLABORATION.md")
        report = root / "docguard_report.json"

        result = run_cli("scan", "--docs", str(docs), "--evidence-root", str(evidence), "--out", str(report))

        assert result.returncode == 1
        assert any(match["ruleId"] == "architecture.progressLanguage" for match in read_json(report)["forbiddenMatches"])


def test_architecture_hedef_language_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        docs = root / "docs"
        evidence = root / "evidence"
        create_evidence(evidence)
        write_doc(docs, non_claim_text())
        write_doc(docs / "architecture", "Hedef " + "4 source truth closure\n", "SOURCE_TRUTH.md")
        report = root / "docguard_report.json"

        result = run_cli("scan", "--docs", str(docs), "--evidence-root", str(evidence), "--out", str(report))

        assert result.returncode == 1
        assert any(match["ruleId"] == "architecture.progressLanguage" for match in read_json(report)["forbiddenMatches"])


def test_report_is_deterministic() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        docs = root / "docs"
        evidence = root / "evidence"
        create_evidence(evidence)
        write_doc(docs, non_claim_text())
        first = root / "first.json"
        second = root / "second.json"

        assert run_cli("scan", "--docs", str(docs), "--evidence-root", str(evidence), "--out", str(first)).returncode == 0
        assert run_cli("scan", "--docs", str(docs), "--evidence-root", str(evidence), "--out", str(second)).returncode == 0
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")


def test_scanner_does_not_rewrite_docs() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        docs = root / "docs"
        evidence = root / "evidence"
        create_evidence(evidence)
        doc = write_doc(docs, non_claim_text())
        before = doc.read_bytes()

        result = run_cli("scan", "--docs", str(docs), "--evidence-root", str(evidence), "--out", str(root / "report.json"))

        assert result.returncode == 0
        assert doc.read_bytes() == before


def main() -> None:
    tests = [
        test_positive_production_claim_fails,
        test_required_non_claims_detected_and_scan_passes,
        test_missing_evidence_fails,
        test_limitations_doc_is_required_evidence,
        test_forbidden_phrase_in_non_claim_context_is_reviewed,
        test_raw_full_ctest_claim_fails_without_evidence,
        test_positive_hedef3_started_claim_fails,
        test_enterprise_ready_claim_fails,
        test_security_and_compliance_claims_fail,
        test_auth_permission_and_stress_claims_fail,
        test_architecture_progress_filename_fails,
        test_architecture_progress_language_fails,
        test_architecture_hedef_language_fails,
        test_report_is_deterministic,
        test_scanner_does_not_rewrite_docs,
    ]
    for test in tests:
        test()
    print(f"{len(tests)} SagaDocGuard CLI tests passed")


if __name__ == "__main__":
    main()
