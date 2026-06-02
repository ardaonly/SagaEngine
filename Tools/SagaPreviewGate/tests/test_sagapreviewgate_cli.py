#!/usr/bin/env python3
"""Focused CLI tests for SagaPreviewGate."""

from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2].parent
SAGAPREVIEWGATE = REPO_ROOT / "Tools" / "SagaPreviewGate" / "sagapreviewgate"


def run_cli(*args: str) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagapreviewgate-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagapreviewgate-nuget")
    env.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
    return subprocess.run(
        [str(SAGAPREVIEWGATE), *args],
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


def write_acceptance_fixture(root: Path) -> None:
    paths = [
        "project_validation_report.json",
        "project_resolution.json",
        "project_doctor_report.json",
        "SagaScript/analysis_report.json",
        "SagaScript/runtime_bindings.json",
        "SagaScript/source_map.json",
        "SagaScript/projection_report.json",
        "SagaScript/patch_preview.json",
        "ViewCapabilities/view_capability_report.json",
        "ViewCapabilities/simple_view_honesty_report.json",
        "acceptance_report.json",
        "diagnostics_summary.json",
        "package_report.json",
        "package_stage_report.json",
        "publish_report.json",
        "package_smoke_report.json",
        "docguard_report.json",
    ]
    for relative in paths:
        write_report(root / relative)


def test_quickstart_passes_with_current_repo_paths() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        report = Path(tmp) / "quickstart_report.json"

        result = run_cli("quickstart-check", "--out", str(report))

        assert result.returncode == 0, result.stderr + result.stdout
        payload = read_json(report)
        assert payload["status"] == "Passed"
        assert any(check["id"] == "sample-project" and check["status"] == "Present" for check in payload["checks"])


def test_quickstart_missing_project_fails_deterministically() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        report = root / "quickstart_report.json"

        result = run_cli(
            "quickstart-check",
            "--repo-root",
            str(root),
            "--project",
            str(root / "Missing.sagaproj"),
            "--out",
            str(report),
        )

        assert result.returncode == 1
        payload = read_json(report)
        assert payload["status"] == "Failed"
        assert any(d["code"] == "PreviewGate.Quickstart.MissingRequiredPath" for d in payload["diagnostics"])


def test_acceptance_evidence_only_report_is_deterministic_and_ordered() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out_root = Path(tmp) / "Acceptance"
        write_acceptance_fixture(out_root)

        first = run_cli("accept", "--out-root", str(out_root), "--evidence-only")
        first_payload = read_json(out_root / "mvp_acceptance_report.json")
        second = run_cli("accept", "--out-root", str(out_root), "--evidence-only")
        second_payload = read_json(out_root / "mvp_acceptance_report.json")

        assert first.returncode == 0, first.stderr + first.stdout
        assert second.returncode == 0, second.stderr + second.stdout
        assert first_payload == second_payload
        assert [step["sequence"] for step in first_payload["steps"]] == list(range(1, 18))
        assert first_payload["status"] == "Passed"


def test_acceptance_evidence_only_rejects_missing_report() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out_root = Path(tmp) / "Acceptance"
        write_acceptance_fixture(out_root)
        (out_root / "publish_report.json").unlink()

        result = run_cli("accept", "--out-root", str(out_root), "--evidence-only")

        assert result.returncode == 1
        payload = read_json(out_root / "mvp_acceptance_report.json")
        assert payload["status"] == "Failed"
        assert any(step["id"] == "publish-check" and step["status"] == "Missing" for step in payload["steps"])


def test_build_matrix_marks_unavailable_platforms_without_failing() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evidence = root / "acceptance.json"
        write_report(evidence)
        report = root / "build_matrix_report.json"

        result = run_cli("build-matrix", "--out", str(report), "--evidence", str(evidence))

        assert result.returncode == 0, result.stderr + result.stdout
        payload = read_json(report)
        assert payload["status"] == "Passed"
        assert any(entry["status"] == "Unavailable" for entry in payload["entries"])


def test_build_matrix_missing_evidence_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        report = Path(tmp) / "build_matrix_report.json"

        result = run_cli("build-matrix", "--out", str(report), "--evidence", str(Path(tmp) / "missing.json"))

        assert result.returncode == 1
        payload = read_json(report)
        assert any(d["code"] == "PreviewGate.BuildMatrix.EvidenceMissing" for d in payload["diagnostics"])


def test_package_contains_expected_docs_and_sample_manifest() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        result = run_cli("package", "--out-root", str(Path(tmp) / "Package"))

        assert result.returncode == 0, result.stderr + result.stdout
        report = Path(tmp) / "Package" / "SagaEngine-0.1-TechnicalPreview" / "technical_preview_package_report.json"
        payload = read_json(report)
        assert payload["status"] == "Passed"
        assert any(item["packagePath"] == "samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj" for item in payload["files"])


def test_close_blocks_when_required_evidence_is_missing() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_report(root / "quickstart_report.json")
        report = root / "closure.json"

        result = run_cli("close", "--evidence-root", str(root), "--out", str(report))

        assert result.returncode == 1
        payload = read_json(report)
        assert payload["status"] == "Blocked"
        assert any(d["code"] == "PreviewGate.Closure.EvidenceMissingOrFailed" for d in payload["diagnostics"])


def test_close_accepts_complete_fixture_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_report(root / "quickstart_report.json")
        write_report(root / "Acceptance" / "mvp_acceptance_report.json")
        write_report(root / "build_matrix_report.json")
        write_report(root / "docguard_report.json")
        write_report(root / "Package" / "SagaEngine-0.1-TechnicalPreview" / "technical_preview_package_report.json")
        report = root / "closure.json"

        result = run_cli("close", "--evidence-root", str(root), "--out", str(report))

        assert result.returncode == 0, result.stderr + result.stdout
        payload = read_json(report)
        assert payload["status"] == "Accepted"
        assert "raw full CTest passed" in payload["forbiddenClaimsPreserved"]


def main() -> None:
    tests = [
        test_quickstart_passes_with_current_repo_paths,
        test_quickstart_missing_project_fails_deterministically,
        test_acceptance_evidence_only_report_is_deterministic_and_ordered,
        test_acceptance_evidence_only_rejects_missing_report,
        test_build_matrix_marks_unavailable_platforms_without_failing,
        test_build_matrix_missing_evidence_fails,
        test_package_contains_expected_docs_and_sample_manifest,
        test_close_blocks_when_required_evidence_is_missing,
        test_close_accepts_complete_fixture_evidence,
    ]
    for test in tests:
        test()
    print(f"{len(tests)} SagaPreviewGate CLI tests passed")


if __name__ == "__main__":
    main()
