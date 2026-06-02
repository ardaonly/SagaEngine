#!/usr/bin/env python3
"""Focused CLI tests for SagaProbe."""

from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
PROJECT = REPO_ROOT / "Tools" / "SagaProbe" / "SagaProbe.csproj"


def run_cli(*args: str) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagaprobe-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagaprobe-nuget")
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


def write_json(path: Path, payload: object) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return path


def load(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def operational_report(metric_value: float = 2.0) -> dict[str, object]:
    return {
        "schemaVersion": 1,
        "reportKind": "operational_snapshot",
        "generationSequence": 1,
        "summary": {
            "healthMetricCount": 1,
            "lifetimeLeakCount": 0,
            "memorySystemCount": 0,
            "activeResourceCount": 0,
            "recentLogEventCount": 1,
            "faultReportCount": 1,
        },
        "healthMetrics": [
            {"name": "server.tick.count", "type": "counter", "value": metric_value}
        ],
        "lifetimeLeaks": [],
        "memoryRecords": [],
        "resourceSummary": {"totalActive": 0, "byType": {}, "byOwnerSystem": {}},
        "activeResources": [],
        "recentLogEvents": [
            {"level": "Warn", "tag": "Net.Session", "message": "warning", "context": []}
        ],
        "serverLifecycle": {
            "events": [
                {
                    "sequence": 1,
                    "eventName": "TickSlow",
                    "category": "Tick",
                    "severity": "Warning",
                    "message": "slow tick",
                    "zoneId": 1,
                    "tick": 4,
                    "payload": [],
                }
            ],
            "records": [],
            "leaks": [],
            "summary": {
                "eventCount": 1,
                "recordCount": 0,
                "activeRecordCount": 0,
                "leakCount": 0,
                "droppedEventCount": 0,
                "droppedRecordCount": 0,
            },
        },
        "faults": {
            "reports": [
                {
                    "sequence": 1,
                    "faultId": "Runtime.Startup.ManifestMissing",
                    "subsystem": "Runtime",
                    "severity": "Error",
                    "policy": "DeferToCaller",
                    "recommendedAction": "BlockStartup",
                    "message": "manifest missing",
                    "diagnosticCode": "Runtime.ManifestMissing",
                    "metadata": [],
                }
            ],
            "summary": {"faultCount": 1, "droppedFaultCount": 0},
        },
        "metadata": {},
    }


def reliability_report() -> dict[str, object]:
    return {
        "schemaVersion": 1,
        "reportKind": "reliability_failure_report",
        "reason": "manual",
        "generationSequence": 1,
        "threadId": None,
        "summary": {
            "healthMetricCount": 1,
            "healthRuleViolationCount": 1,
            "lifetimeLeakCount": 1,
            "memorySystemCount": 0,
            "activeResourceCount": 0,
            "recentLogEventCount": 0,
        },
        "healthMetrics": [
            {"name": "server.tick.ms", "type": "timing", "value": 20.0}
        ],
        "healthRuleViolations": [
            {
                "ruleName": "tick budget",
                "metricName": "server.tick.ms",
                "type": "Max",
                "severity": "Error",
                "threshold": 16,
                "observedValue": 20,
                "evaluated": True,
                "passed": False,
                "message": "tick budget exceeded",
            }
        ],
        "lifetimeLeaks": [
            {
                "objectId": 1,
                "externalId": 1001,
                "ownerObjectId": 0,
                "typeName": "PlayerEntity",
                "ownerSystem": "World",
                "createdTick": 1,
            }
        ],
        "lifetimeLeakSummary": {
            "totalActive": 1,
            "byType": {"PlayerEntity": 1},
            "byOwnerSystem": {"World": 1},
        },
        "memoryRecords": [],
        "resourceSummary": {"totalActive": 0, "byType": {}, "byOwnerSystem": {}},
        "activeResources": [],
        "recentLogEvents": [],
        "metadata": {},
    }


def headless_report(status: str = "Passed") -> dict[str, object]:
    return {
        "schemaVersion": 1,
        "tool": "MultiplayerSandboxHeadless",
        "status": status,
        "projectId": "multiplayer-sandbox",
        "projectPath": "samples/MultiplayerSandbox/MultiplayerSandbox.sagaproj",
        "tickCount": 1,
        "entityCount": 1,
        "inputQueuedCount": 1,
        "inputAcceptedCount": 1,
        "dirtyEntityIds": [1001],
        "initialPosition": {"x": 0, "y": 0, "z": 0},
        "beforeTickPosition": {"x": 0, "y": 0, "z": 0},
        "finalPosition": {"x": 0, "y": 1, "z": 0},
        "diagnostics": [],
    }


def launch_report() -> dict[str, object]:
    return {
        "schemaVersion": 1,
        "tool": "sagalaunch",
        "command": "server",
        "status": "Passed",
        "project": {"projectId": "multiplayer-sandbox"},
        "launchProfile": {"id": "local-server-headless"},
        "process": {
            "executable": "MultiplayerSandboxHeadless",
            "arguments": [],
            "workingDirectory": ".",
            "exitCode": 0,
            "timedOut": False,
            "durationMs": 12,
        },
        "artifacts": {"headlessReportPath": "headless_server_report.json"},
        "diagnostics": [],
    }


def acceptance_report() -> dict[str, object]:
    return {
        "schemaVersion": 1,
        "tool": "sagalaunch",
        "command": "accept",
        "status": "Passed",
        "project": {"projectId": "multiplayer-sandbox"},
        "projectValidation": {"name": "validate", "status": "Passed"},
        "projectResolution": {"name": "resolve", "status": "Passed"},
        "serverPreview": {"name": "server", "status": "Passed"},
        "deferredStages": [
            {
                "name": "Phase 23 server plus one runtime client",
                "status": "Deferred",
                "reason": "ClientHost seam unavailable.",
            }
        ],
        "diagnostics": [],
    }


def test_summarize_operational_report_extracts_metrics_and_diagnostics() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_json(root / "operational.json", operational_report())
        out = root / "diagnostics_summary.json"

        result = run_cli("summarize", "--input", str(source), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load(out)
        assert report["sourceReport"]["reportKind"] == "engine_diagnostics_report"
        assert report["summary"]["faultCount"] == 1
        assert report["summary"]["lifecycleEventCount"] == 1
        assert any(d["code"] == "Runtime.ManifestMissing" for d in report["criticalDiagnostics"])
        assert any(m["name"] == "health.server.tick.count" for m in report["metrics"])
        assert "SagaProbe summarize" in result.stdout


def test_summarize_reliability_report_extracts_rule_and_leak_counts() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_json(root / "reliability.json", reliability_report())
        out = root / "diagnostics_summary.json"

        result = run_cli("summarize", "--input", str(source), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load(out)
        assert report["sourceReport"]["reportKind"] == "engine_reliability_report"
        assert report["summary"]["leakCount"] == 1
        assert any(d["code"] == "Health.RuleViolation" for d in report["criticalDiagnostics"])


def test_summarize_headless_launch_and_acceptance_reports() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        fixtures = [
            ("headless.json", headless_report(), "multiplayer_sandbox_headless_report"),
            ("launch.json", launch_report(), "sagalaunch_launch_report"),
            ("acceptance.json", acceptance_report(), "sagalaunch_acceptance_report"),
        ]
        for name, payload, kind in fixtures:
            source = write_json(root / name, payload)
            out = root / f"{name}.summary.json"
            result = run_cli("summarize", "--input", str(source), "--out", str(out))
            assert result.returncode == 0, result.stderr + result.stdout
            report = load(out)
            assert report["sourceReport"]["reportKind"] == kind
        acceptance_summary = load(root / "acceptance.json.summary.json")
        assert acceptance_summary["status"] == "Attention"
        assert any(
            d["code"] == "Acceptance.StageDeferred"
            for d in acceptance_summary["warningDiagnostics"]
        )


def test_invalid_missing_and_unsupported_reports_fail_clearly() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        invalid = root / "invalid.json"
        invalid.write_text("{", encoding="utf-8")
        unsupported = write_json(root / "unsupported.json", {"schemaVersion": 1})

        invalid_result = run_cli(
            "summarize", "--input", str(invalid), "--out", str(root / "invalid_summary.json")
        )
        missing_result = run_cli(
            "summarize", "--input", str(root / "missing.json"), "--out", str(root / "missing_summary.json")
        )
        unsupported_result = run_cli(
            "summarize", "--input", str(unsupported), "--out", str(root / "unsupported_summary.json")
        )

        assert invalid_result.returncode == 1
        assert "Probe.Report.InvalidJson" in invalid_result.stderr
        assert missing_result.returncode == 3
        assert "Probe.Input.ReportMissing" in missing_result.stderr
        assert unsupported_result.returncode == 1
        assert "Probe.Report.UnsupportedKind" in unsupported_result.stderr


def test_compare_same_report_has_no_diff() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_json(root / "operational.json", operational_report())
        out = root / "report_diff.json"

        result = run_cli(
            "compare",
            "--baseline",
            str(source),
            "--candidate",
            str(source),
            "--out",
            str(out),
        )

        assert result.returncode == 0, result.stderr + result.stdout
        report = load(out)
        assert report["status"] == "Passed"
        assert report["summary"]["changedMetricCount"] == 0
        assert report["summary"]["addedDiagnosticCount"] == 0


def test_compare_detects_changed_metric_new_diagnostic_and_missing_section() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        baseline = write_json(root / "baseline.json", operational_report(2.0))
        candidate_payload = operational_report(5.0)
        del candidate_payload["faults"]
        candidate_payload["diagnostics"] = [
            {
                "severity": "Error",
                "code": "Preview.NewCritical",
                "category": "Preview",
                "path": "",
                "message": "new critical",
            }
        ]
        candidate = write_json(root / "candidate.json", candidate_payload)
        out = root / "report_diff.json"

        result = run_cli(
            "compare",
            "--baseline",
            str(baseline),
            "--candidate",
            str(candidate),
            "--out",
            str(out),
        )

        assert result.returncode == 1
        report = load(out)
        assert report["status"] == "Changed"
        assert any(m["name"] == "health.server.tick.count" for m in report["changedMetrics"])
        assert any(d["code"] == "Preview.NewCritical" for d in report["addedDiagnostics"])
        assert any(s["name"] == "faults" and s["missingFrom"] == "candidate" for s in report["missingSections"])


def test_latest_selects_supported_report_by_lexical_relative_path() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        reports = root / "reports"
        write_json(reports / "001" / "headless_server_report.json", headless_report())
        write_json(reports / "002" / "launch_preview_report.json", launch_report())
        write_json(reports / "notes.json", {"schemaVersion": 1})
        out = root / "diagnostics_summary.json"

        result = run_cli("latest", "--reports-dir", str(reports), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load(out)
        assert report["sourceReport"]["reportKind"] == "sagalaunch_launch_report"
        assert report["sourceReport"]["path"].endswith("002/launch_preview_report.json")


def test_latest_missing_or_empty_reports_fail_clearly() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        missing = run_cli("latest", "--reports-dir", str(root / "missing"), "--out", str(root / "summary.json"))
        empty_dir = root / "empty"
        empty_dir.mkdir()
        empty = run_cli("latest", "--reports-dir", str(empty_dir), "--out", str(root / "summary.json"))

        assert missing.returncode == 1
        assert "Probe.Input.ReportsDirectoryMissing" in missing.stderr
        assert empty.returncode == 1
        assert "Probe.Latest.NoSupportedReports" in empty.stderr


def test_summary_output_is_stable() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_json(root / "headless.json", headless_report())
        first = root / "first.json"
        second = root / "second.json"

        first_result = run_cli("summarize", "--input", str(source), "--out", str(first))
        second_result = run_cli("summarize", "--input", str(source), "--out", str(second))

        assert first_result.returncode == 0
        assert second_result.returncode == 0
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")


def run_all() -> None:
    tests = [
        test_summarize_operational_report_extracts_metrics_and_diagnostics,
        test_summarize_reliability_report_extracts_rule_and_leak_counts,
        test_summarize_headless_launch_and_acceptance_reports,
        test_invalid_missing_and_unsupported_reports_fail_clearly,
        test_compare_same_report_has_no_diff,
        test_compare_detects_changed_metric_new_diagnostic_and_missing_section,
        test_latest_selects_supported_report_by_lexical_relative_path,
        test_latest_missing_or_empty_reports_fail_clearly,
        test_summary_output_is_stable,
    ]
    for test in tests:
        test()
    print(f"{len(tests)} SagaProbe CLI tests passed")


if __name__ == "__main__":
    run_all()
