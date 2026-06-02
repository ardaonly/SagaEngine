#!/usr/bin/env python3
"""Focused CLI tests for SagaPolicyKit."""

from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2].parent
SAGAPOLICY = REPO_ROOT / "Tools" / "SagaPolicyKit" / "sagapolicy"


def run_cli(*args: str) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagapolicykit-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagapolicykit-nuget")
    env.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
    return subprocess.run(
        [str(SAGAPOLICY), *args],
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


def policy_payload() -> dict:
    return {
        "roles": [
            {
                "id": "Owner",
                "allow": ["ApprovePublishGate", "ChangePackageProfile"],
                "deny": [],
                "warn": ["ExportRestrictedArtifact"],
                "requiresReview": ["DeleteScene"],
            },
            {
                "id": "Programmer",
                "allow": ["ModifyRuntimeBindingMetadata"],
                "deny": ["ApprovePublishGate"],
                "warn": [],
                "requiresReview": ["OverrideLock"],
            },
            {
                "id": "Observer",
                "allow": [],
                "deny": ["ApprovePublishGate", "DeleteScene"],
                "warn": [],
                "requiresReview": [],
            },
        ],
        "resources": [
            {"id": "publish:technical-preview", "kind": "PublishGate"},
            {"id": "scene:main", "kind": "Scene"},
            {"id": "package:server-headless", "kind": "PackageProfile"},
            {"id": "binding:runtime", "kind": "RuntimeBindingMetadata"},
            {"id": "lock:door", "kind": "WorkspaceLock"},
            {"id": "artifact:restricted", "kind": "RestrictedArtifact"},
        ],
        "dangerousOperations": [
            "DeleteScene",
            "DeleteBehaviorSource",
            "ChangePackageProfile",
            "OverrideLock",
            "ApprovePublishGate",
            "ModifyRuntimeBindingMetadata",
            "ExportRestrictedArtifact",
        ],
        "requiredEvidence": [
            {
                "action": "ApprovePublishGate",
                "resource": "publish:technical-preview",
                "evidenceId": "publish-report",
                "path": "Build/SmallTeamAlpha/publish_report.json",
            }
        ],
    }


def request_payload(role: str = "Owner", action: str = "ApprovePublishGate", resource: str = "publish:technical-preview") -> dict:
    return {
        "subject": "actor://local/alex",
        "actor": "alex",
        "role": role,
        "action": action,
        "resource": resource,
        "scope": {"projectId": "multiplayer-sandbox", "workspaceId": "local"},
        "evidence": [{"evidenceId": "publish-report", "path": "Build/SmallTeamAlpha/publish_report.json", "status": "Passed"}],
    }


def run_eval(root: Path, request: dict) -> tuple[subprocess.CompletedProcess[str], Path]:
    policy = root / "policy.json"
    request_path = root / "request.json"
    out = root / "policy_evaluation_report.json"
    write_json(policy, policy_payload())
    write_json(request_path, request)
    result = run_cli("evaluate", "--policy", str(policy), "--request", str(request_path), "--out", str(out))
    return result, out


def test_allowed_role_action_decision() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        result, out = run_eval(Path(tmp), request_payload())
        assert result.returncode == 0, result.stderr + result.stdout
        report = read_json(out)
        assert report["status"] == "Passed"
        assert report["decision"] == "Allow"
        assert report["mutatesSource"] is False
        assert report["enforcement"] == "ReportOnly"


def test_denied_role_action_decision() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        result, out = run_eval(Path(tmp), request_payload(role="Observer"))
        assert result.returncode == 1
        report = read_json(out)
        assert report["decision"] == "Deny"
        assert any(d["code"] == "Policy.Denied" for d in report["diagnostics"])


def test_unknown_role_action_and_resource() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        result, out = run_eval(root, request_payload(role="ExternalReviewer"))
        assert result.returncode == 1
        assert read_json(out)["decision"] == "UnknownSubject"

        result, out = run_eval(root, request_payload(action="LaunchWorkspaceHub"))
        assert result.returncode == 1
        assert read_json(out)["decision"] == "UnknownAction"

        result, out = run_eval(root, request_payload(resource="slice:future"))
        assert result.returncode == 1
        assert read_json(out)["decision"] == "UnknownResource"


def test_missing_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        request = request_payload()
        request["evidence"] = []
        result, out = run_eval(Path(tmp), request)
        assert result.returncode == 1
        report = read_json(out)
        assert report["decision"] == "MissingEvidence"
        assert any(d["code"] == "Policy.MissingEvidence" for d in report["diagnostics"])


def test_dangerous_operation_requires_review() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        request = request_payload(role="Programmer", action="OverrideLock", resource="lock:door")
        request["evidence"] = []
        result, out = run_eval(Path(tmp), request)
        assert result.returncode == 1
        report = read_json(out)
        assert report["decision"] == "RequiresReview"
        assert report["status"] == "ReviewRequired"


def test_allowed_warning_operation_is_report_only() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        request = request_payload(role="Owner", action="ExportRestrictedArtifact", resource="artifact:restricted")
        request["evidence"] = []
        result, out = run_eval(Path(tmp), request)
        assert result.returncode == 0, result.stderr + result.stdout
        report = read_json(out)
        assert report["decision"] == "Warn"
        assert report["enforcement"] == "ReportOnly"
        assert report["mutatesSource"] is False


def test_missing_policy_emits_report() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        request = root / "request.json"
        out = root / "policy_evaluation_report.json"
        write_json(request, request_payload())
        result = run_cli("evaluate", "--policy", str(root / "missing.json"), "--request", str(request), "--out", str(out))
        assert result.returncode == 1
        report = read_json(out)
        assert report["decision"] == "MissingEvidence"
        assert any(d["code"] == "Policy.Missing" for d in report["diagnostics"])


def test_report_is_deterministic_and_does_not_mutate_inputs() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        policy = root / "policy.json"
        request = root / "request.json"
        first = root / "first.json"
        second = root / "second.json"
        write_json(policy, policy_payload())
        write_json(request, request_payload())
        policy_before = policy.read_bytes()
        request_before = request.read_bytes()

        assert run_cli("evaluate", "--policy", str(policy), "--request", str(request), "--out", str(first)).returncode == 0
        assert run_cli("evaluate", "--policy", str(policy), "--request", str(request), "--out", str(second)).returncode == 0

        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        assert policy.read_bytes() == policy_before
        assert request.read_bytes() == request_before


def main() -> None:
    tests = [
        test_allowed_role_action_decision,
        test_denied_role_action_decision,
        test_unknown_role_action_and_resource,
        test_missing_evidence,
        test_dangerous_operation_requires_review,
        test_allowed_warning_operation_is_report_only,
        test_missing_policy_emits_report,
        test_report_is_deterministic_and_does_not_mutate_inputs,
    ]
    for test in tests:
        test()
    print(f"{len(tests)} SagaPolicyKit CLI tests passed")


if __name__ == "__main__":
    main()
