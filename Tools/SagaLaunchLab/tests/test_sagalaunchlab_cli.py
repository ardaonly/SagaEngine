#!/usr/bin/env python3
"""Focused CLI tests for SagaLaunchLab."""

from __future__ import annotations

import json
import os
import shutil
import stat
import subprocess
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
PROJECT = REPO_ROOT / "Tools" / "SagaLaunchLab" / "SagaLaunchLab.csproj"
SAGAPROJECT = REPO_ROOT / "Tools" / "SagaProjectKit" / "sagaproject"
SAMPLE_ROOT = REPO_ROOT / "samples" / "MultiplayerSandbox"
NATIVE_BIN_DIR = REPO_ROOT / "build" / "RelWithDebInfo-0.0.9" / "bin"


def run_cli(*args: str) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagalaunchlab-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagalaunchlab-nuget")
    env.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
    return subprocess.run(
        ["dotnet", "run", "--project", str(PROJECT), "--", *args],
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


def write_script(path: Path, body: str) -> Path:
    path.write_text(body, encoding="utf-8")
    path.chmod(path.stat().st_mode | stat.S_IXUSR)
    return path


def make_project(root: Path, executable: Path | str) -> Path:
    (root / "Assets").mkdir(parents=True)
    (root / "Scripts").mkdir(parents=True)
    (root / "Diagnostics").mkdir(parents=True)
    (root / "Build" / "Reports").mkdir(parents=True)
    write_json(
        root / "Fixture.sagaproj",
        {
            "schemaVersion": 0,
            "projectId": "fixture-launch",
            "displayName": "Fixture Launch",
            "engineCompatibility": {
                "minimumVersion": "0.0.9",
                "targetVersion": "0.1-technical-preview",
                "channel": "technical-preview",
            },
            "paths": {
                "diagnostics": "Diagnostics",
                "generatedReports": "Build/Reports",
            },
            "scenes": [],
            "assets": [
                {"id": "fixture.assets", "path": "Assets", "kind": "directory"}
            ],
            "scriptFolders": [
                {"id": "fixture.scripts", "path": "Scripts"}
            ],
            "launchProfiles": [
                {"id": "local-server-headless", "path": "launch_profiles.json"}
            ],
            "packageProfiles": [
                {"id": "fixture.package", "path": "package_profiles.json"}
            ],
        },
    )
    write_json(root / "package_profiles.json", {"schemaVersion": 0, "profiles": []})
    write_json(
        root / "launch_profiles.json",
        {
            "schemaVersion": 0,
            "profiles": [
                {
                    "id": "local-server-headless",
                    "displayName": "Local Server Headless",
                    "role": "server",
                    "mode": "bounded-local-process",
                    "executable": str(executable),
                    "reportPath": "Build/Reports/headless_server_report.json",
                    "diagnosticsPath": "Diagnostics",
                    "arguments": [
                        "--project",
                        "{project}",
                        "--report-out",
                        "{headlessReport}",
                        "--diagnostics-out",
                        "{diagnosticsOut}",
                    ],
                }
            ],
        },
    )
    return root / "Fixture.sagaproj"


def load(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def write_alignment_evidence(path: Path, command: str, status: str) -> None:
    write_json(
        path,
        {
            "schemaVersion": 1,
            "tool": "sagaproject",
            "command": command,
            "status": status,
            "localOnly": True,
            "networkExposure": "None",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )


def test_server_launch_report_emitted() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        script = write_script(
            root / "fake_server.sh",
            """#!/usr/bin/env bash
set -euo pipefail
report=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --report-out) report="$2"; shift 2 ;;
    *) shift ;;
  esac
done
mkdir -p "$(dirname "$report")"
printf '{"schemaVersion":1,"status":"Passed"}\n' > "$report"
echo "fake server ok"
""",
        )
        manifest = make_project(root / "Project", script)
        out = root / "launch_preview_report.json"

        result = run_cli(
            "server",
            "--project",
            str(manifest),
            "--launch-profile",
            "local-server-headless",
            "--out",
            str(out),
            "--timeout-sec",
            "5",
        )

        assert result.returncode == 0, result.stderr + result.stdout
        report = load(out)
        assert report["status"] == "Passed"
        assert report["process"]["exitCode"] == 0
        assert report["process"]["timedOut"] is False
        assert Path(report["artifacts"]["headlessReportPath"]).exists()
        assert Path(report["artifacts"]["stdoutPath"]).read_text(encoding="utf-8").strip() == "fake server ok"


def test_invalid_executable_path_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        manifest = make_project(root / "Project", root / "missing_executable")
        out = root / "launch_preview_report.json"

        result = run_cli(
            "server",
            "--project",
            str(manifest),
            "--launch-profile",
            "local-server-headless",
            "--out",
            str(out),
            "--timeout-sec",
            "5",
        )

        assert result.returncode == 1
        report = load(out)
        assert report["status"] == "Failed"
        assert any(d["code"] == "Launch.Process.ExecutableMissing" for d in report["diagnostics"])


def test_timeout_failure_is_deterministic() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        script = write_script(root / "slow_server.sh", "#!/usr/bin/env bash\nsleep 5\n")
        manifest = make_project(root / "Project", script)
        out = root / "launch_preview_report.json"

        result = run_cli(
            "server",
            "--project",
            str(manifest),
            "--launch-profile",
            "local-server-headless",
            "--out",
            str(out),
            "--timeout-sec",
            "1",
        )

        assert result.returncode == 1
        report = load(out)
        assert report["process"]["timedOut"] is True
        assert any(d["code"] == "Launch.Process.Timeout" for d in report["diagnostics"])


def test_profile_matrix_reports_server_runnable_and_client_preview_deferred() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project_root = root / "MultiplayerSandbox"
        shutil.copytree(SAMPLE_ROOT, project_root)
        out = root / "launch_profile_matrix_report.json"

        result = run_cli(
            "profile-matrix",
            "--project",
            str(project_root / "MultiplayerSandbox.sagaproj"),
            "--out",
            str(out),
        )

        assert result.returncode == 0, result.stderr + result.stdout
        report = load(out)
        assert report["status"] == "PassedWithDeferredProfiles"
        rows = {row["profileId"]: row for row in report["profiles"]}
        assert rows["local-server-headless"]["status"] == "Runnable"
        assert rows["local-server-plus-one-client"]["status"] == "Deferred"
        assert "ClientHost" in rows["local-server-plus-two-clients"]["reason"]


def test_profile_matrix_reports_missing_profile_and_invalid_executable() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        manifest = make_project(root / "Project", root / "missing_executable")
        out = root / "launch_profile_matrix_report.json"

        result = run_cli(
            "profile-matrix",
            "--project",
            str(manifest),
            "--out",
            str(out),
        )

        assert result.returncode == 1
        report = load(out)
        rows = {row["profileId"]: row for row in report["profiles"]}
        assert rows["local-server-headless"]["status"] == "Blocked"
        assert any(d["code"] == "Launch.Process.ExecutableMissing" for d in report["diagnostics"])

        launch_profiles = load(manifest.parent / "launch_profiles.json")
        launch_profiles["profiles"] = []
        write_json(manifest.parent / "launch_profiles.json", launch_profiles)
        missing_out = root / "missing_launch_profile_matrix_report.json"

        missing = run_cli(
            "profile-matrix",
            "--project",
            str(manifest),
            "--out",
            str(missing_out),
        )

        assert missing.returncode == 1
        missing_report = load(missing_out)
        missing_rows = {row["profileId"]: row for row in missing_report["profiles"]}
        assert missing_rows["local-server-headless"]["status"] == "Missing"
        assert any(d["code"] == "Launch.Profile.NotFound" for d in missing_report["diagnostics"])


def test_source_truth_alignment_reads_profiles_without_launching_and_preserves_client_deferral() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project_root = root / "MultiplayerSandbox"
        shutil.copytree(SAMPLE_ROOT, project_root)
        source_truth_gate = root / "source_truth_gate_report.json"
        runtime_readiness = root / "runtime_readiness_gate_report.json"
        out = root / "launch_source_truth_alignment_report.json"
        write_alignment_evidence(source_truth_gate, "source-truth-gate", "PartiallyProven")
        write_alignment_evidence(runtime_readiness, "runtime-readiness-gate", "PartiallyProven")
        headless_report = project_root / "Build" / "Reports" / "headless_server_report.json"
        if headless_report.exists():
            headless_report.unlink()

        result = run_cli(
            "source-truth-alignment",
            "--project",
            str(project_root / "MultiplayerSandbox.sagaproj"),
            "--source-truth-gate",
            str(source_truth_gate),
            "--runtime-readiness",
            str(runtime_readiness),
            "--out",
            str(out),
        )

        assert result.returncode == 0, result.stderr + result.stdout
        report = load(out)
        assert report["status"] == "PartiallyProven"
        assert report["localOnly"] is True
        assert report["networkExposure"] == "None"
        assert report["mutatesSource"] is False
        assert report["enforcement"] == "ReportOnly"
        row = report["launchProfiles"][0]
        assert row["profileId"] == "local-server-headless"
        assert row["sourceTruthCompatibility"] == "ServerHeadlessEvidenceOnly"
        assert report["deferredStages"][0]["name"] == "Client Preview"
        assert "ClientHost" in report["deferredStages"][0]["reason"]
        assert not headless_report.exists()


def test_acceptance_with_multiplayer_sandbox_records_deferred_client_stages() -> None:
    native_executable = NATIVE_BIN_DIR / "MultiplayerSandboxHeadless"
    if not native_executable.exists():
        raise AssertionError(f"missing native executable: {native_executable}")

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project_root = root / "MultiplayerSandbox"
        shutil.copytree(SAMPLE_ROOT, project_root)
        out = root / "acceptance_report.json"

        result = run_cli(
            "accept",
            "--project",
            str(project_root / "MultiplayerSandbox.sagaproj"),
            "--launch-profile",
            "local-server-headless",
            "--out",
            str(out),
            "--timeout-sec",
            "5",
            "--bin-dir",
            str(NATIVE_BIN_DIR),
            "--sagaproject",
            str(SAGAPROJECT),
        )

        assert result.returncode == 0, result.stderr + result.stdout
        report = load(out)
        assert report["status"] == "Passed"
        assert report["projectValidation"]["status"] == "Passed"
        assert report["projectResolution"]["status"] == "Passed"
        assert report["serverPreview"]["status"] == "Passed"
        assert [stage["status"] for stage in report["deferredStages"]] == ["Deferred", "Deferred"]


def run_all() -> None:
    tests = [
        test_server_launch_report_emitted,
        test_invalid_executable_path_fails,
        test_timeout_failure_is_deterministic,
        test_profile_matrix_reports_server_runnable_and_client_preview_deferred,
        test_profile_matrix_reports_missing_profile_and_invalid_executable,
        test_source_truth_alignment_reads_profiles_without_launching_and_preserves_client_deferral,
        test_acceptance_with_multiplayer_sandbox_records_deferred_client_stages,
    ]
    for test in tests:
        test()
    print(f"{len(tests)} SagaLaunchLab CLI tests passed")


if __name__ == "__main__":
    run_all()
