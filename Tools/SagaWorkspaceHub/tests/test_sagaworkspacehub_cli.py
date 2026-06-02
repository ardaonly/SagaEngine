#!/usr/bin/env python3
"""Focused CLI tests for SagaWorkspaceHub."""

from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parents[2].parent
SAGAWORKSPACEHUB = REPO_ROOT / "Tools" / "SagaWorkspaceHub" / "sagaworkspacehub"


def run_cli(*args: str) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagaworkspacehub-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagaworkspacehub-nuget")
    env.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
    return subprocess.run(
        [str(SAGAWORKSPACEHUB), *args],
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


def write_jsonl(path: Path, payloads: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("".join(json.dumps(payload, sort_keys=True) + "\n" for payload in payloads), encoding="utf-8")


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def project_manifest() -> dict:
    return {
        "schemaVersion": 1,
        "projectId": "workspace-fixture",
        "displayName": "Workspace Fixture",
        "engineCompatibility": {},
        "paths": {},
        "scenes": [],
        "assets": [],
        "scriptFolders": [],
        "launchProfiles": [],
        "packageProfiles": [],
    }


def policy_report(decision: str = "Allow") -> dict:
    status = {
        "Allow": "Passed",
        "Deny": "Denied",
        "RequiresReview": "ReviewRequired",
        "MissingEvidence": "MissingEvidence",
    }.get(decision, "Failed")
    return {
        "schemaVersion": 1,
        "tool": "sagapolicy",
        "command": "evaluate",
        "status": status,
        "decision": decision,
        "subject": "local-user",
        "resource": "scene://door",
        "action": "OpenWorkspaceView",
        "role": "Designer",
        "scope": {"projectId": "workspace-fixture", "workspaceId": "local"},
        "diagnostics": [],
        "evidence": [],
        "mutatesSource": False,
        "enforcement": "ReportOnly",
    }


def slice_resolution(resources: list[dict[str, Any]]) -> dict:
    return {
        "schemaVersion": 1,
        "tool": "sagaproject",
        "command": "restricted-resolve",
        "status": "Passed",
        "project": {
            "projectId": "workspace-fixture",
            "displayName": "Workspace Fixture",
            "projectRoot": "/tmp/workspace-fixture",
            "manifestPath": "/tmp/workspace-fixture/fixture.sagaproj",
            "schemaVersion": 1,
        },
        "slice": {
            "sliceId": "designer-local",
            "displayName": "Designer Local",
            "intendedRole": "Designer",
            "slicePath": ".saga/slices/designer-local.slice.json",
            "schemaVersion": 1,
        },
        "resolvedResources": [],
        "restrictedResources": [],
        "resourceVisibility": resources,
        "mutatesSource": False,
        "enforcement": "ReportOnly",
        "diagnostics": [],
    }


def view_compatibility() -> dict:
    return {
        "schemaVersion": 1,
        "tool": "sagaviewkit",
        "command": "slice-compatibility",
        "status": "Passed",
        "view": "Simple",
        "sliceResolution": "restricted_project_resolution_report.json",
        "decision": "Compatible",
        "visibleCount": 1,
        "restrictedCount": 3,
        "hiddenCount": 1,
        "mutatesSource": False,
        "enforcement": "ReportOnly",
        "diagnostics": [],
    }


def review_report(expected_hash: str = "target-hash", actual_hash: str = "target-hash") -> dict:
    return {
        "schemaVersion": 1,
        "tool": "sagareview",
        "command": "review",
        "status": "Ready",
        "reviewId": "review-001",
        "requester": "designer-a",
        "reviewers": ["reviewer-b"],
        "target": {
            "targetRef": "scene://door",
            "expectedHash": expected_hash,
            "actualHash": actual_hash,
        },
        "sourceReportPaths": ["review_input_report.json"],
        "mutatesSource": False,
        "enforcement": "ReportOnly",
        "diagnostics": [],
    }


def export_request(artifact_id: str, visibility: str = "") -> dict:
    return {
        "schemaVersion": 1,
        "artifactId": artifact_id,
        "visibility": visibility,
        "role": "Designer",
        "outputTarget": "local-report",
    }


def fixture_files(root: Path, policy_decision: str = "Allow") -> tuple[Path, Path, Path, Path]:
    project = root / "Fixture.sagaproj"
    policy = root / "policy_evaluation_report.json"
    slice_report = root / "restricted_project_resolution_report.json"
    view = root / "view_slice_compatibility_report.json"
    write_json(project, project_manifest())
    write_json(policy, policy_report(policy_decision))
    write_json(
        slice_report,
        slice_resolution(
            [
                {
                    "kind": "ScriptFile",
                    "id": "visible",
                    "relativePath": "Scripts/Visible.cs",
                    "status": "Included",
                    "visibility": "FullSource",
                    "sourceText": "VISIBLE_SOURCE_SHOULD_BE_IGNORED",
                },
                {
                    "kind": "ScriptFile",
                    "id": "summary",
                    "relativePath": "Scripts/SummaryOnly.cs",
                    "status": "Included",
                    "visibility": "SummaryOnly",
                    "sourceText": "SECRET_SUMMARY_SOURCE",
                },
                {
                    "kind": "ScriptFile",
                    "id": "opaque",
                    "relativePath": "Scripts/Opaque.cs",
                    "status": "Restricted",
                    "visibility": "OpaqueRestricted",
                    "sourceText": "SECRET_OPAQUE_SOURCE",
                },
                {
                    "kind": "ScriptFile",
                    "id": "hidden",
                    "relativePath": "Scripts/Hidden.cs",
                    "status": "Restricted",
                    "visibility": "Hidden",
                    "sourceText": "SECRET_HIDDEN_SOURCE",
                },
            ]
        ),
    )
    write_json(view, view_compatibility())
    return project, slice_report, policy, view


def summarize_args(project: Path, slice_report: Path, policy: Path, view: Path, out: Path) -> list[str]:
    return [
        "summarize",
        "--project",
        str(project),
        "--slice-resolution",
        str(slice_report),
        "--policy-report",
        str(policy),
        "--view-compatibility",
        str(view),
        "--out",
        str(out),
    ]


def assert_report_only(payload: dict) -> None:
    assert payload["localOnly"] is True
    assert payload["networkExposure"] == "None"
    assert payload["enforcement"] == "ReportOnly"
    assert payload["mutatesSource"] is False


def codes(payload: dict) -> set[str]:
    return {item["code"] for item in payload["diagnostics"]}


def test_summary_report_is_deterministic() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project, slice_report, policy, view = fixture_files(root)
        first = root / "first_summary.json"
        second = root / "second_summary.json"

        assert run_cli(*summarize_args(project, slice_report, policy, view, first)).returncode == 0
        assert run_cli(*summarize_args(project, slice_report, policy, view, second)).returncode == 0

        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        payload = read_json(first)
        assert payload["status"] == "Passed"
        assert payload["project"]["projectId"] == "workspace-fixture"
        assert payload["slice"]["sliceId"] == "designer-local"
        assert_report_only(payload)


def test_missing_project_diagnostic() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project, slice_report, policy, view = fixture_files(root)
        project.unlink()
        out = root / "summary.json"

        result = run_cli(*summarize_args(project, slice_report, policy, view, out))

        assert result.returncode == 1
        payload = read_json(out)
        assert "WorkspaceHub.ProjectMissing" in codes(payload)


def test_missing_policy_report_diagnostic() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project, slice_report, policy, view = fixture_files(root)
        policy.unlink()
        out = root / "summary.json"

        result = run_cli(*summarize_args(project, slice_report, policy, view, out))

        assert result.returncode == 1
        payload = read_json(out)
        assert "WorkspaceHub.PolicyReportMissing" in codes(payload)
        assert payload["policy"]["adapterDecision"] == "MissingPolicyEvidence"


def test_missing_slice_report_diagnostic() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project, slice_report, policy, view = fixture_files(root)
        slice_report.unlink()
        out = root / "summary.json"

        result = run_cli(*summarize_args(project, slice_report, policy, view, out))

        assert result.returncode == 1
        payload = read_json(out)
        assert "WorkspaceHub.SliceResolutionMissing" in codes(payload)


def test_malformed_policy_report_diagnostic() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        policy = root / "policy_evaluation_report.json"
        out = root / "policy_adapter.json"
        policy.write_text("{ invalid json", encoding="utf-8")

        result = run_cli("policy-adapter", "--policy-report", str(policy), "--out", str(out))

        assert result.returncode == 1
        payload = read_json(out)
        assert payload["policy"]["adapterDecision"] == "UnknownPolicyState"
        assert "WorkspaceHub.PolicyReportMalformed" in codes(payload)


def test_policy_adapter_maps_allow_deny_and_requires_review() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        expected = {
            "Allow": ("AllowedByReport", 0),
            "Deny": ("DeniedByReport", 1),
            "RequiresReview": ("RequiresReviewByReport", 1),
        }

        for decision, (adapter_decision, returncode) in expected.items():
            policy = root / f"{decision}.json"
            out = root / f"{decision}.adapter.json"
            write_json(policy, policy_report(decision))

            result = run_cli("policy-adapter", "--policy-report", str(policy), "--out", str(out))

            assert result.returncode == returncode
            payload = read_json(out)
            assert payload["policy"]["sourceDecision"] == decision
            assert payload["policy"]["adapterDecision"] == adapter_decision
            assert_report_only(payload)


def test_policy_adapter_emits_missing_policy_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        policy = root / "missing_evidence.json"
        out = root / "adapter.json"
        write_json(policy, policy_report("MissingEvidence"))

        result = run_cli("policy-adapter", "--policy-report", str(policy), "--out", str(out))

        assert result.returncode == 1
        payload = read_json(out)
        assert payload["policy"]["adapterDecision"] == "MissingPolicyEvidence"
        assert "WorkspaceHub.PolicyEvidenceMissing" in codes(payload)


def test_slice_scoped_view_includes_visible_resources_and_counts_restricted() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        _, slice_report, policy, view = fixture_files(root)
        out = root / "slice_view.json"

        result = run_cli(
            "slice-view",
            "--slice-resolution",
            str(slice_report),
            "--view-compatibility",
            str(view),
            "--policy-report",
            str(policy),
            "--out",
            str(out),
        )

        assert result.returncode == 0, result.stderr + result.stdout
        payload = read_json(out)
        assert payload["status"] == "Passed"
        assert payload["resources"]["visibleResources"][0]["id"] == "visible"
        assert payload["counts"]["visible"] == 1
        assert payload["counts"]["restricted"] == 3
        assert payload["counts"]["summaryOnly"] == 1
        assert payload["counts"]["opaqueRestricted"] == 1
        assert payload["counts"]["hidden"] == 1
        assert len(payload["resources"]["restrictedPlaceholders"]) == 3
        assert_report_only(payload)


def test_restricted_source_text_is_absent_from_slice_view() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        _, slice_report, policy, view = fixture_files(root)
        out = root / "slice_view.json"

        result = run_cli(
            "slice-view",
            "--slice-resolution",
            str(slice_report),
            "--view-compatibility",
            str(view),
            "--policy-report",
            str(policy),
            "--out",
            str(out),
        )

        assert result.returncode == 0, result.stderr + result.stdout
        serialized = out.read_text(encoding="utf-8")
        assert "SECRET_SUMMARY_SOURCE" not in serialized
        assert "SECRET_OPAQUE_SOURCE" not in serialized
        assert "SECRET_HIDDEN_SOURCE" not in serialized
        payload = read_json(out)
        for resource in payload["resources"]["restrictedPlaceholders"]:
            assert "sourceText" not in resource
            assert "sourceContents" not in resource
            if resource["visibility"] == "Hidden":
                assert resource["relativePath"] == ""


def test_workspacehub_does_not_mutate_project_or_source_inputs() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project, slice_report, policy, view = fixture_files(root)
        source = root / "Scripts" / "Visible.cs"
        source.parent.mkdir()
        source.write_text("public class Visible {}\n", encoding="utf-8")
        before = {
            project: project.read_bytes(),
            slice_report: slice_report.read_bytes(),
            policy: policy.read_bytes(),
            view: view.read_bytes(),
            source: source.read_bytes(),
        }
        out = root / "summary.json"

        result = run_cli(*summarize_args(project, slice_report, policy, view, out))

        assert result.returncode == 0, result.stderr + result.stdout
        for path, content in before.items():
            assert path.read_bytes() == content


def test_audit_log_report_is_deterministic_and_counts_policy_denial() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        events = root / "audit_events.jsonl"
        first = root / "audit_first.json"
        second = root / "audit_second.json"
        write_jsonl(
            events,
            [
                {
                    "eventId": "evt-2",
                    "eventType": "PolicyDenied",
                    "actorRef": "local-user",
                    "targetRef": "scene://door",
                    "sequence": 2,
                    "previousRecordHash": "h1",
                    "recordHash": "h2",
                    "evidenceRefs": ["policy_evaluation_report.json"],
                },
                {
                    "eventId": "evt-1",
                    "eventType": "ReviewRequested",
                    "actorRef": "designer-a",
                    "targetRef": "scene://door",
                    "sequence": 1,
                    "recordHash": "h1",
                    "evidenceRefs": ["review_report.json"],
                },
            ],
        )

        assert run_cli("audit-log", "--events", str(events), "--out", str(first)).returncode == 0
        assert run_cli("audit-log", "--events", str(events), "--out", str(second)).returncode == 0

        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        payload = read_json(first)
        assert payload["events"][0]["eventId"] == "evt-1"
        assert payload["summary"]["countsByEventType"]["PolicyDenied"] == 1
        assert payload["summary"]["hashChainStatus"] == "Consistent"
        assert_report_only(payload)


def test_audit_log_detects_hash_chain_mismatch() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        events = root / "audit_events.jsonl"
        out = root / "audit_report.json"
        write_jsonl(
            events,
            [
                {"eventId": "evt-1", "eventType": "A", "sequence": 1, "recordHash": "h1"},
                {"eventId": "evt-2", "eventType": "B", "sequence": 2, "previousRecordHash": "wrong", "recordHash": "h2"},
            ],
        )

        result = run_cli("audit-log", "--events", str(events), "--out", str(out))

        assert result.returncode == 1
        payload = read_json(out)
        assert payload["summary"]["hashChainStatus"] == "Mismatch"
        assert "WorkspaceHub.AuditLog.HashChainMismatch" in codes(payload)


def test_review_approval_is_metadata_only_and_policy_deny_blocks() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        review = root / "review_report.json"
        policy = root / "policy_evaluation_report.json"
        audit_events = root / "audit_events.jsonl"
        audit = root / "audit_report.json"
        approved = root / "approved.json"
        blocked = root / "blocked.json"
        write_json(review, review_report())
        write_json(policy, policy_report("Allow"))
        write_jsonl(audit_events, [{"eventId": "evt-1", "eventType": "ReviewApproved", "sequence": 1}])
        assert run_cli("audit-log", "--events", str(audit_events), "--out", str(audit)).returncode == 0

        result = run_cli(
            "review-approval",
            "--review-report",
            str(review),
            "--policy-report",
            str(policy),
            "--audit-report",
            str(audit),
            "--out",
            str(approved),
        )
        assert result.returncode == 0, result.stderr + result.stdout
        payload = read_json(approved)
        assert payload["decision"] == "ApprovedMetadataOnly"
        assert "apply" not in approved.read_text(encoding="utf-8").lower()
        assert_report_only(payload)

        write_json(policy, policy_report("Deny"))
        denied = run_cli(
            "review-approval",
            "--review-report",
            str(review),
            "--policy-report",
            str(policy),
            "--out",
            str(blocked),
        )
        assert denied.returncode == 1
        assert read_json(blocked)["decision"] == "BlockedByPolicy"


def test_review_approval_requires_review_and_stale_hash_do_not_approve() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        review = root / "review_report.json"
        policy = root / "policy_evaluation_report.json"
        out = root / "review_approval.json"
        write_json(review, review_report())
        write_json(policy, policy_report("RequiresReview"))

        result = run_cli("review-approval", "--review-report", str(review), "--policy-report", str(policy), "--out", str(out))

        assert result.returncode == 1
        assert read_json(out)["decision"] == "RequiresReview"

        write_json(review, review_report("expected", "actual"))
        write_json(policy, policy_report("Allow"))
        stale = run_cli("review-approval", "--review-report", str(review), "--policy-report", str(policy), "--out", str(out))
        assert stale.returncode == 1
        payload = read_json(out)
        assert payload["decision"] == "Stale"
        assert "WorkspaceHub.ReviewApproval.StaleTargetHash" in codes(payload)


def test_review_approval_diagnoses_malformed_or_missing_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        review = root / "review_report.json"
        policy = root / "policy_evaluation_report.json"
        audit = root / "audit_report.json"
        out = root / "review_approval.json"
        review.write_text("{ invalid", encoding="utf-8")
        write_json(policy, policy_report("Allow"))
        audit.write_text("{ invalid", encoding="utf-8")

        result = run_cli(
            "review-approval",
            "--review-report",
            str(review),
            "--policy-report",
            str(policy),
            "--audit-report",
            str(audit),
            "--out",
            str(out),
        )

        assert result.returncode == 1
        payload = read_json(out)
        assert payload["decision"] == "MissingEvidence"
        assert "WorkspaceHub.ReviewReportMalformed" in codes(payload)
        assert "WorkspaceHub.AuditReportMalformed" in codes(payload)


def test_restricted_export_allows_full_source_only_with_policy_allow() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        _, slice_report, policy, _ = fixture_files(root)
        request = root / "export_request.json"
        out = root / "restricted_export.json"
        write_json(request, export_request("visible", "FullSource"))

        allowed = run_cli(
            "restricted-export",
            "--slice-resolution",
            str(slice_report),
            "--policy-report",
            str(policy),
            "--request",
            str(request),
            "--out",
            str(out),
        )
        assert allowed.returncode == 0, allowed.stderr + allowed.stdout
        payload = read_json(out)
        assert payload["counts"]["allowedExports"] == 1
        assert payload["counts"]["blockedExports"] == 0
        assert_report_only(payload)

        write_json(policy, policy_report("Deny"))
        blocked = run_cli(
            "restricted-export",
            "--slice-resolution",
            str(slice_report),
            "--policy-report",
            str(policy),
            "--request",
            str(request),
            "--out",
            str(out),
        )
        assert blocked.returncode == 1
        assert read_json(out)["blockedExports"][0]["disposition"] == "BlockedByPolicy"


def test_restricted_export_summary_only_excludes_source_and_hidden_is_blocked() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        _, slice_report, policy, _ = fixture_files(root)
        request = root / "export_request.json"
        out = root / "restricted_export.json"
        write_json(request, export_request("summary", "SummaryOnly"))

        summary = run_cli(
            "restricted-export",
            "--slice-resolution",
            str(slice_report),
            "--policy-report",
            str(policy),
            "--request",
            str(request),
            "--out",
            str(out),
        )

        assert summary.returncode == 0, summary.stderr + summary.stdout
        serialized = out.read_text(encoding="utf-8")
        assert "SECRET_SUMMARY_SOURCE" not in serialized
        assert read_json(out)["allowedExports"][0]["disposition"] == "SummaryMetadataOnly"

        for artifact_id, visibility in [("opaque", "OpaqueRestricted"), ("hidden", "Hidden")]:
            write_json(request, export_request(artifact_id, visibility))
            result = run_cli(
                "restricted-export",
                "--slice-resolution",
                str(slice_report),
                "--policy-report",
                str(policy),
                "--request",
                str(request),
                "--out",
                str(out),
            )
            assert result.returncode == 1
            payload = read_json(out)
            assert payload["counts"]["blockedExports"] == 1
            assert payload["blockedExports"][0]["placeholder"] != ""
            if visibility == "Hidden":
                assert payload["blockedExports"][0]["relativePath"] == ""


def test_phase_113_116_commands_do_not_mutate_inputs_or_invoke_sagascript() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        _, slice_report, policy, _ = fixture_files(root)
        events = root / "audit_events.jsonl"
        review = root / "review_report.json"
        request = root / "export_request.json"
        write_jsonl(events, [{"eventId": "evt-1", "eventType": "ReviewRequested", "sequence": 1}])
        write_json(review, review_report())
        write_json(request, export_request("summary", "SummaryOnly"))
        before = {
            slice_report: slice_report.read_bytes(),
            policy: policy.read_bytes(),
            events: events.read_bytes(),
            review: review.read_bytes(),
            request: request.read_bytes(),
        }
        outputs = [root / "audit.json", root / "approval.json", root / "export.json"]

        results = [
            run_cli("audit-log", "--events", str(events), "--out", str(outputs[0])),
            run_cli("review-approval", "--review-report", str(review), "--policy-report", str(policy), "--out", str(outputs[1])),
            run_cli("restricted-export", "--slice-resolution", str(slice_report), "--policy-report", str(policy), "--request", str(request), "--out", str(outputs[2])),
        ]

        assert all(result.returncode == 0 for result in results)
        for path, content in before.items():
            assert path.read_bytes() == content
        combined = "\n".join(path.read_text(encoding="utf-8") for path in outputs)
        assert "sagascript" not in combined.lower()


def test_presence_redaction_redacts_restricted_details_and_preserves_counts() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        _, slice_report, policy, _ = fixture_files(root)
        presence = root / "presence_report.json"
        out = root / "presence_redaction_report.json"
        write_json(
            presence,
            {
                "schemaVersion": 1,
                "actors": [
                    {"actorId": "actor-visible", "displayName": "Visible User", "role": "Designer", "resourceRef": "visible"},
                    {"actorId": "actor-secret", "displayName": "Secret User", "role": "Designer", "resourceRef": "hidden"},
                ],
                "resources": [
                    {"resourceId": "visible", "relativePath": "Scripts/Visible.cs", "visibility": "FullSource"},
                    {"resourceId": "hidden", "relativePath": "Scripts/Hidden.cs", "visibility": "Hidden"},
                ],
            },
        )

        first = run_cli("presence-redaction", "--presence-report", str(presence), "--slice-resolution", str(slice_report), "--policy-report", str(policy), "--out", str(out))
        second_out = root / "presence_redaction_second.json"
        second = run_cli("presence-redaction", "--presence-report", str(presence), "--slice-resolution", str(slice_report), "--policy-report", str(policy), "--out", str(second_out))

        assert first.returncode == 0, first.stderr + first.stdout
        assert second.returncode == 0, second.stderr + second.stdout
        assert out.read_text(encoding="utf-8") == second_out.read_text(encoding="utf-8")
        serialized = out.read_text(encoding="utf-8")
        assert "Secret User" not in serialized
        assert "Scripts/Hidden.cs" not in serialized
        payload = read_json(out)
        assert payload["counts"] == {"visibleActors": 1, "redactedActors": 1, "visibleResources": 1, "redactedResources": 1}
        assert payload["visibleActors"][0]["displayName"] == "Visible User"
        assert payload["redactedActors"][0]["placeholder"] == "RedactedActor"
        assert_report_only(payload)


def test_policy_actions_are_backend_neutral_and_follow_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        _, _, policy, _ = fixture_files(root)
        review = root / "review_approval_report.json"
        export = root / "restricted_export_report.json"
        out = root / "policy_actions.json"
        write_json(
            review,
            {
                "schemaVersion": 1,
                "tool": "sagaworkspacehub",
                "command": "review-approval",
                "status": "Passed",
                "decision": "ApprovedMetadataOnly",
                "mutatesSource": False,
                "enforcement": "ReportOnly",
            },
        )
        write_json(
            export,
            {
                "schemaVersion": 1,
                "tool": "sagaworkspacehub",
                "command": "restricted-export",
                "status": "Blocked",
                "counts": {"blockedExports": 1},
                "mutatesSource": False,
                "enforcement": "ReportOnly",
            },
        )

        result = run_cli(
            "policy-actions",
            "--policy-report",
            str(policy),
            "--review-approval-report",
            str(review),
            "--restricted-export-report",
            str(export),
            "--out",
            str(out),
        )

        assert result.returncode == 0, result.stderr + result.stdout
        payload = read_json(out)
        actions = {item["actionId"]: item for item in payload["actions"]}
        assert actions["ApplyPatch"]["status"] == "Disabled"
        assert actions["ApplyPatch"]["sourceMutationOwner"] == "SagaScript"
        assert actions["RollbackPatch"]["status"] == "Disabled"
        assert actions["DeleteBehaviorSource"]["status"] == "BlockedByPolicy"
        assert actions["ExportArtifact"]["status"] == "BlockedByPolicy"
        assert actions["ApproveReview"]["status"] == "Enabled"
        serialized = out.read_text(encoding="utf-8").lower()
        assert "qt" not in serialized
        assert "editor ui" not in serialized
        assert_report_only(payload)


def test_team_room_filters_restricted_comments_and_counts_hidden_resources() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        _, slice_report, policy, _ = fixture_files(root)
        presence = root / "presence_report.json"
        redaction = root / "presence_redaction_report.json"
        team_status = root / "team_room_status.json"
        out = root / "team_room_report.json"
        write_json(
            presence,
            {
                "actors": [{"actorId": "actor-visible", "displayName": "Visible User", "role": "Designer", "resourceRef": "visible"}],
                "resources": [{"resourceId": "visible", "relativePath": "Scripts/Visible.cs", "visibility": "FullSource"}],
            },
        )
        assert run_cli("presence-redaction", "--presence-report", str(presence), "--slice-resolution", str(slice_report), "--policy-report", str(policy), "--out", str(redaction)).returncode == 0
        write_json(
            team_status,
            {
                "schemaVersion": 1,
                "reviews": [
                    {"reviewId": "review-visible", "authorRef": "actor-visible", "targetRef": "visible", "summary": "Visible review"},
                    {"reviewId": "review-hidden", "authorRef": "actor-secret", "targetRef": "hidden", "summary": "Hidden review"},
                ],
                "comments": [
                    {"commentId": "comment-visible", "authorRef": "actor-visible", "targetRef": "visible", "summary": "Visible comment"},
                    {"commentId": "comment-hidden", "authorRef": "actor-secret", "targetRef": "hidden", "summary": "Hidden comment"},
                ],
            },
        )

        result = run_cli(
            "team-room",
            "--team-room-status",
            str(team_status),
            "--presence-redaction",
            str(redaction),
            "--slice-resolution",
            str(slice_report),
            "--policy-report",
            str(policy),
            "--out",
            str(out),
        )

        assert result.returncode == 0, result.stderr + result.stdout
        serialized = out.read_text(encoding="utf-8")
        assert "Visible review" in serialized
        assert "Hidden comment" not in serialized
        payload = read_json(out)
        assert payload["counts"]["visibleReviews"] == 1
        assert payload["counts"]["restrictedComments"] == 1
        assert payload["counts"]["hiddenResources"] == 1
        assert_report_only(payload)


def test_governance_panel_summarizes_evidence_missing_and_debt() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_json(root / "policy_deny.json", {**policy_report("Deny"), "action": "DeleteScene"})
        write_json(root / "review_approval_report.json", {"tool": "sagaworkspacehub", "command": "review-approval", "status": "Blocked", "decision": "Stale"})
        write_json(root / "audit_report.json", {"tool": "sagaworkspacehub", "command": "audit-log", "status": "Passed", "summary": {"eventCount": 1}})
        write_json(root / "restricted_export_report.json", {"tool": "sagaworkspacehub", "command": "restricted-export", "status": "Blocked", "counts": {"blockedExports": 1}})
        write_json(root / "presence_redaction_report.json", {"tool": "sagaworkspacehub", "command": "presence-redaction", "status": "Passed", "counts": {"redactedActors": 1}})
        write_json(root / "team_room_report.json", {"tool": "sagaworkspacehub", "command": "team-room", "status": "Passed", "counts": {"restrictedComments": 1}})
        write_json(root / "publish_report.json", {"tool": "sagapack", "command": "publish-check", "status": "Blocked", "gates": [{"name": "PolicyReportAccepted", "status": "Blocked", "message": "Denied"}]})
        write_json(root / "policy_regression_report.json", {"tool": "sagaenterprisegate", "command": "policy-regression", "status": "Failed", "checks": [{"checkId": "PolicyAllow", "status": "MissingEvidence", "summary": "Missing policy allow"}]})
        write_json(root / "debt_report.json", {"tool": "local", "command": "debt", "status": "Passed", "residualDebt": ["Later phase deferred"]})
        out = root / "governance_panel_report.json"

        first = run_cli("governance-panel", "--evidence-root", str(root), "--out", str(out))
        second_out = root / "governance_panel_report_second.json"
        second = run_cli("governance-panel", "--evidence-root", str(root), "--out", str(second_out))

        assert first.returncode == 0, first.stderr + first.stdout
        assert second.returncode == 0, second.stderr + second.stdout
        assert out.read_text(encoding="utf-8") == second_out.read_text(encoding="utf-8")
        payload = read_json(out)
        assert payload["model"] == "LocalGovernanceDashboardModel"
        assert payload["counts"]["dangerousOperations"] == 1
        assert payload["counts"]["missingEvidence"] == 1
        assert payload["counts"]["residualDebt"] == 1
        assert payload["dangerousOperationQueue"][0]["decision"] == "Deny"
        assert "not an admin console" in payload["nonClaim"].lower()
        serialized = out.read_text(encoding="utf-8").lower()
        assert "auth ui" in serialized
        assert "rbac ui" in serialized
        assert "security management ui" in serialized
        assert_report_only(payload)


def main() -> None:
    tests = [
        test_summary_report_is_deterministic,
        test_missing_project_diagnostic,
        test_missing_policy_report_diagnostic,
        test_missing_slice_report_diagnostic,
        test_malformed_policy_report_diagnostic,
        test_policy_adapter_maps_allow_deny_and_requires_review,
        test_policy_adapter_emits_missing_policy_evidence,
        test_slice_scoped_view_includes_visible_resources_and_counts_restricted,
        test_restricted_source_text_is_absent_from_slice_view,
        test_workspacehub_does_not_mutate_project_or_source_inputs,
        test_audit_log_report_is_deterministic_and_counts_policy_denial,
        test_audit_log_detects_hash_chain_mismatch,
        test_review_approval_is_metadata_only_and_policy_deny_blocks,
        test_review_approval_requires_review_and_stale_hash_do_not_approve,
        test_review_approval_diagnoses_malformed_or_missing_evidence,
        test_restricted_export_allows_full_source_only_with_policy_allow,
        test_restricted_export_summary_only_excludes_source_and_hidden_is_blocked,
        test_phase_113_116_commands_do_not_mutate_inputs_or_invoke_sagascript,
        test_presence_redaction_redacts_restricted_details_and_preserves_counts,
        test_policy_actions_are_backend_neutral_and_follow_evidence,
        test_team_room_filters_restricted_comments_and_counts_hidden_resources,
        test_governance_panel_summarizes_evidence_missing_and_debt,
    ]
    for test in tests:
        test()
    print(f"{len(tests)} SagaWorkspaceHub CLI tests passed")


if __name__ == "__main__":
    main()
