#!/usr/bin/env python3
"""Focused CLI tests for SagaPackager."""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
PROJECT = REPO_ROOT / "Tools" / "SagaPackager" / "SagaPackager.csproj"
SAMPLE_ROOT = REPO_ROOT / "samples" / "MultiplayerSandbox"
NATIVE_BIN_DIR = REPO_ROOT / "build" / "RelWithDebInfo-0.0.9" / "bin"
PROFILE = "technical-preview-server-headless"


def run_cli(*args: str) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagapack-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagapack-nuget")
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


def load(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, payload: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def copy_sample(root: Path) -> Path:
    project_root = root / "MultiplayerSandbox"
    shutil.copytree(SAMPLE_ROOT, project_root)
    return project_root


def manifest_path(project_root: Path) -> Path:
    return project_root / "MultiplayerSandbox.sagaproj"


def package_profiles_path(project_root: Path) -> Path:
    return project_root / "package_profiles.json"


def run_validate(project_root: Path, out: Path) -> subprocess.CompletedProcess[str]:
    return run_cli(
        "validate",
        "--project",
        str(manifest_path(project_root)),
        "--profile",
        PROFILE,
        "--out",
        str(out),
    )


def run_stage(project_root: Path, out: Path) -> subprocess.CompletedProcess[str]:
    return run_cli(
        "stage",
        "--project",
        str(manifest_path(project_root)),
        "--profile",
        PROFILE,
        "--out",
        str(out),
    )


def run_asset_validate(project_root: Path, out: Path, package_report: Path | None = None) -> subprocess.CompletedProcess[str]:
    args = [
        "asset-validate",
        "--project",
        str(manifest_path(project_root)),
        "--out",
        str(out),
    ]
    if package_report is not None:
        args.extend(["--package-report", str(package_report)])
    return run_cli(*args)


def write_clean_summary(path: Path) -> None:
    write_json(
        path,
        {
            "schemaVersion": 1,
            "tool": "sagaprobe",
            "command": "summarize",
            "status": "Passed",
            "summary": {
                "criticalDiagnosticCount": 0,
                "warningDiagnosticCount": 0,
            },
        },
    )


def write_script_validation(path: Path, status: str = "Passed") -> None:
    write_json(
        path,
        {
            "schemaVersion": 1,
            "tool": "sagascript",
            "command": "validate-artifacts",
            "status": status,
            "artifactRoot": str(path.parent),
            "checkedArtifacts": [],
            "runtimeProofSummary": {
                "runtimeBackedWithEvidence": 0,
                "runtimeBackedMissingEvidence": 0,
                "projectionOnly": 2,
                "deferred": 1,
            },
            "summary": {},
            "diagnostics": [],
        },
    )


def write_policy_report(path: Path, decision: str = "Allow") -> None:
    status = {"Allow": "Passed", "Deny": "Denied", "RequiresReview": "ReviewRequired"}.get(decision, "Failed")
    write_json(
        path,
        {
            "schemaVersion": 1,
            "tool": "sagapolicy",
            "command": "evaluate",
            "status": status,
            "decision": decision,
            "subject": "local-user",
            "resource": "package://technical-preview-server-headless",
            "action": "PublishCheck",
            "role": "ReleaseOperator",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
            "diagnostics": [],
        },
    )


def write_review_approval_report(path: Path, decision: str = "ApprovedMetadataOnly") -> None:
    write_json(
        path,
        {
            "schemaVersion": 1,
            "tool": "sagaworkspacehub",
            "command": "review-approval",
            "status": "Passed" if decision == "ApprovedMetadataOnly" else "Blocked",
            "decision": decision,
            "review": {"reviewId": "review-001"},
            "mutatesSource": False,
            "enforcement": "ReportOnly",
            "diagnostics": [],
        },
    )


def write_audit_report(path: Path, status: str = "Passed") -> None:
    write_json(
        path,
        {
            "schemaVersion": 1,
            "tool": "sagaworkspacehub",
            "command": "audit-log",
            "status": status,
            "events": [],
            "summary": {"eventCount": 0, "countsByEventType": {}, "hashChainStatus": "NotSupplied"},
            "mutatesSource": False,
            "enforcement": "ReportOnly",
            "diagnostics": [],
        },
    )


def write_restricted_export_report(path: Path, blocked_exports: int = 0) -> None:
    write_json(
        path,
        {
            "schemaVersion": 1,
            "tool": "sagaworkspacehub",
            "command": "restricted-export",
            "status": "Passed" if blocked_exports == 0 else "Blocked",
            "allowedExports": [],
            "blockedExports": [{} for _ in range(blocked_exports)],
            "counts": {
                "allowedExports": 1 if blocked_exports == 0 else 0,
                "blockedExports": blocked_exports,
                "restrictedArtifacts": blocked_exports,
            },
            "mutatesSource": False,
            "enforcement": "ReportOnly",
            "diagnostics": [],
        },
    )


def write_source_truth_gate(path: Path, status: str = "PartiallyProven") -> None:
    write_json(
        path,
        {
            "schemaVersion": 1,
            "tool": "sagaproject",
            "command": "source-truth-gate",
            "status": status,
            "localOnly": True,
            "networkExposure": "None",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )


def test_validate_multiplayer_sandbox_passes() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project_root = copy_sample(root)
        before = manifest_path(project_root).read_text(encoding="utf-8")
        out = root / "package_report.json"

        result = run_validate(project_root, out)

        assert result.returncode == 0, result.stderr + result.stdout
        report = load(out)
        assert report["status"] == "Passed"
        assert report["packageProfile"]["id"] == PROFILE
        assert manifest_path(project_root).read_text(encoding="utf-8") == before


def test_asset_validate_reports_missing_source_truth_for_placeholder_assets() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project_root = copy_sample(root)
        (project_root / "Assets" / "assets.source.json").unlink()
        out = root / "asset_workflow_report.json"

        result = run_asset_validate(project_root, out)

        assert result.returncode == 1
        report = load(out)
        assert report["status"] == "MissingSourceOfTruth"
        assert report["assetReferences"][0]["placeholderStatus"] == "PlaceholderOnly"
        assert any(d["code"] == "Asset.Workflow.MissingSourceOfTruth" for d in report["diagnostics"])


def test_asset_validate_accepts_explicit_asset_source_manifest_and_package_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project_root = copy_sample(root)
        write_json(project_root / "Assets" / "asset_manifest.json", {"schemaVersion": 1, "assets": []})
        stage_report = root / "package_report.json"
        assert run_stage(project_root, stage_report).returncode == 0
        out = root / "asset_workflow_report.json"

        result = run_asset_validate(project_root, out, stage_report)

        assert result.returncode == 0, result.stderr + result.stdout
        report = load(out)
        assert report["status"] == "Passed"
        assert report["packageEvidence"]["stageStatus"] == "Passed"
        assert report["packageEvidence"]["assetManifestExists"] is True
        assert report["packageEvidence"]["assetIdentityManifestExists"] is True


def test_asset_validate_fails_when_supplied_package_asset_manifest_is_missing() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project_root = copy_sample(root)
        write_json(project_root / "Assets" / "asset_manifest.json", {"schemaVersion": 1, "assets": []})
        stage_report = root / "package_report.json"
        assert run_stage(project_root, stage_report).returncode == 0
        staged = load(stage_report)
        package_manifest = load(Path(staged["packageManifestPath"]))
        missing_asset_manifest = Path(staged["stagedPackagePath"]) / package_manifest["assetManifests"][0]["path"]
        missing_asset_manifest.unlink()
        out = root / "asset_workflow_report.json"

        result = run_asset_validate(project_root, out, stage_report)

        assert result.returncode == 1
        report = load(out)
        assert report["status"] == "Failed"
        assert report["packageEvidence"]["assetManifestExists"] is False
        assert any(d["code"] == "Asset.PackageAssetManifest.Missing" for d in report["diagnostics"])


def test_asset_validate_duplicate_id_and_missing_root_fail_deterministically() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project_root = copy_sample(root)
        manifest = load(manifest_path(project_root))
        manifest["assets"].append({"id": "multiplayer-sandbox.assets", "path": "MissingAssets", "kind": "directory"})
        write_json(manifest_path(project_root), manifest)
        out = root / "asset_workflow_report.json"

        result = run_asset_validate(project_root, out)

        assert result.returncode == 1
        codes = [d["code"] for d in load(out)["diagnostics"]]
        assert "Asset.Workflow.DuplicateAssetId" in codes
        assert "Asset.Workflow.AssetRootMissing" in codes


def test_profile_validation_failures_are_deterministic() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project_root = copy_sample(root)
        profile_path = package_profiles_path(project_root)

        payload = load(profile_path)
        payload["schemaVersion"] = 99
        payload["profiles"][0]["role"] = "editor"
        payload["profiles"][0]["outputDirectory"] = "../escape"
        write_json(profile_path, payload)
        out = root / "package_report.json"

        result = run_validate(project_root, out)

        assert result.returncode == 1
        report = load(out)
        codes = [d["code"] for d in report["diagnostics"]]
        assert "Package.Profile.UnsupportedSchemaVersion" in codes
        assert "Package.Profile.InvalidRole" in codes
        assert "Package.Path.ParentTraversalNotAllowed" in codes


def test_missing_profile_and_missing_launch_profile_fail() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project_root = copy_sample(root)
        missing_profile = run_cli(
            "validate",
            "--project",
            str(manifest_path(project_root)),
            "--profile",
            "missing",
            "--out",
            str(root / "missing_profile.json"),
        )
        assert missing_profile.returncode == 1
        assert any(
            d["code"] == "Package.Profile.NotFound"
            for d in load(root / "missing_profile.json")["diagnostics"]
        )

        payload = load(package_profiles_path(project_root))
        payload["profiles"][0]["launchProfileId"] = "missing-launch"
        write_json(package_profiles_path(project_root), payload)
        missing_launch = run_validate(project_root, root / "missing_launch.json")
        assert missing_launch.returncode == 1
        assert any(
            d["code"] == "Package.Profile.LaunchProfileMissing"
            for d in load(root / "missing_launch.json")["diagnostics"]
        )


def test_stage_creates_deterministic_package_and_preserves_sources() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project_root = copy_sample(root)
        manifest_before = manifest_path(project_root).read_text(encoding="utf-8")
        profile_before = package_profiles_path(project_root).read_text(encoding="utf-8")
        first = root / "package_report_first.json"
        second = root / "package_report_second.json"

        first_result = run_stage(project_root, first)
        second_result = run_stage(project_root, second)

        assert first_result.returncode == 0, first_result.stderr + first_result.stdout
        assert second_result.returncode == 0, second_result.stderr + second_result.stdout
        first_report = load(first)
        second_report = load(second)
        assert first_report["status"] == "Passed"
        assert second_report["stagedFiles"] == first_report["stagedFiles"]
        assert Path(first_report["stagedPackagePath"]).exists()
        assert Path(first_report["packageManifestPath"]).exists()
        assert Path(first_report["stageManifestPath"]).exists()
        package_manifest = load(Path(first_report["packageManifestPath"]))
        assert package_manifest["packageKind"] == "server"
        assert package_manifest["schemaVersion"] == 1
        assert manifest_path(project_root).read_text(encoding="utf-8") == manifest_before
        assert package_profiles_path(project_root).read_text(encoding="utf-8") == profile_before


def test_stage_rejects_output_outside_build_packages() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project_root = copy_sample(root)
        payload = load(package_profiles_path(project_root))
        payload["profiles"][0]["outputDirectory"] = "Outside"
        write_json(package_profiles_path(project_root), payload)
        out = root / "package_report.json"

        result = run_stage(project_root, out)

        assert result.returncode == 1
        assert any(
            d["code"] == "Package.Stage.OutputOutsideBuildPackages"
            for d in load(out)["diagnostics"]
        )


def test_profile_matrix_reports_supported_headless_profile_without_publish_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project_root = copy_sample(root)
        out = root / "package_profile_matrix_report.json"

        result = run_cli(
            "profile-matrix",
            "--project",
            str(manifest_path(project_root)),
            "--out",
            str(out),
        )

        assert result.returncode == 0, result.stderr + result.stdout
        report = load(out)
        assert report["status"] == "PassedWithMissingPublishEvidence"
        row = report["profiles"][0]
        assert row["profileId"] == PROFILE
        assert row["validationStatus"] == "Passed"
        assert row["stageSupport"] == "Supported"
        assert row["publishCheckEvidenceStatus"] == "EvidenceNotSupplied"


def test_source_truth_alignment_reads_profiles_and_supplied_gates_without_staging() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project_root = copy_sample(root)
        source_truth_gate = root / "source_truth_gate_report.json"
        asset_reference_gate = root / "asset_reference_gate_report.json"
        out = root / "package_source_truth_alignment_report.json"
        write_source_truth_gate(source_truth_gate, "PartiallyProven")
        write_source_truth_gate(asset_reference_gate, "Passed")
        before_manifest = manifest_path(project_root).read_text(encoding="utf-8")
        before_profiles = package_profiles_path(project_root).read_text(encoding="utf-8")
        stage_dir = project_root / "Build" / "Packages"
        before_stage_files = sorted(path.relative_to(project_root).as_posix() for path in stage_dir.rglob("*")) if stage_dir.exists() else []

        result = run_cli(
            "source-truth-alignment",
            "--project",
            str(manifest_path(project_root)),
            "--source-truth-gate",
            str(source_truth_gate),
            "--asset-reference-gate",
            str(asset_reference_gate),
            "--out",
            str(out),
        )

        assert result.returncode == 0, result.stderr + result.stdout
        report = load(out)
        assert report["status"] == "Passed"
        assert report["localOnly"] is True
        assert report["networkExposure"] == "None"
        assert report["mutatesSource"] is False
        assert report["enforcement"] == "ReportOnly"
        assert report["packageProfiles"][0]["id"] == PROFILE
        assert report["referencedLaunchProfileIds"] == ["local-server-headless"]
        assert report["sourceTruthGate"]["status"] == "PartiallyProven"
        assert report["assetReferenceGate"]["status"] == "Passed"
        assert report["packageGeneratedManifestCanonicalRejections"][0]["reason"].startswith("Package and generated manifests")
        assert manifest_path(project_root).read_text(encoding="utf-8") == before_manifest
        assert package_profiles_path(project_root).read_text(encoding="utf-8") == before_profiles
        after_stage_files = sorted(path.relative_to(project_root).as_posix() for path in stage_dir.rglob("*")) if stage_dir.exists() else []
        assert after_stage_files == before_stage_files


def test_publish_check_passes_and_blocks_on_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project_root = copy_sample(root)
        stage_report = root / "package_report.json"
        assert run_stage(project_root, stage_report).returncode == 0
        summary = root / "diagnostics_summary.json"
        write_clean_summary(summary)
        publish = root / "publish_report.json"

        result = run_cli(
            "publish-check",
            "--project",
            str(manifest_path(project_root)),
            "--profile",
            PROFILE,
            "--package-report",
            str(stage_report),
            "--diagnostics-summary",
            str(summary),
            "--out",
            str(publish),
        )

        assert result.returncode == 0, result.stderr + result.stdout
        report = load(publish)
        assert report["status"] == "Passed"
        assert any(g["name"] == "ScriptMetadataAccepted" and g["status"] == "Passed" for g in report["gates"])
        assert not any(g["name"] == "ScriptArtifactValidationAccepted" for g in report["gates"])

        script_validation = root / "script_artifact_validation_report.json"
        write_script_validation(script_validation)
        with_script_validation = root / "publish_with_script_validation.json"
        script_result = run_cli(
            "publish-check",
            "--project",
            str(manifest_path(project_root)),
            "--profile",
            PROFILE,
            "--package-report",
            str(stage_report),
            "--diagnostics-summary",
            str(summary),
            "--script-validation",
            str(script_validation),
            "--out",
            str(with_script_validation),
        )

        assert script_result.returncode == 0, script_result.stderr + script_result.stdout
        script_report = load(with_script_validation)
        assert script_report["status"] == "Passed"
        assert any(e["kind"] == "scriptValidation" and e["status"] == "Present" for e in script_report["requiredEvidence"])
        assert any(g["name"] == "ScriptArtifactValidationAccepted" and g["status"] == "Passed" for g in script_report["gates"])

        failed_script_validation = root / "failed_script_artifact_validation_report.json"
        write_script_validation(failed_script_validation, "Failed")
        blocked_script = root / "blocked_script_publish_report.json"
        blocked_script_result = run_cli(
            "publish-check",
            "--project",
            str(manifest_path(project_root)),
            "--profile",
            PROFILE,
            "--package-report",
            str(stage_report),
            "--diagnostics-summary",
            str(summary),
            "--script-validation",
            str(failed_script_validation),
            "--out",
            str(blocked_script),
        )

        assert blocked_script_result.returncode == 1
        blocked_script_report = load(blocked_script)
        assert blocked_script_report["status"] == "Blocked"
        assert any(g["name"] == "ScriptArtifactValidationAccepted" and g["status"] == "Blocked" for g in blocked_script_report["gates"])

        blocking_summary = root / "blocking_summary.json"
        write_json(
            blocking_summary,
            {
                "schemaVersion": 1,
                "tool": "sagaprobe",
                "status": "Failed",
                "summary": {"criticalDiagnosticCount": 1},
            },
        )
        blocked = root / "blocked_publish_report.json"
        blocked_result = run_cli(
            "publish-check",
            "--project",
            str(manifest_path(project_root)),
            "--profile",
            PROFILE,
            "--package-report",
            str(stage_report),
            "--diagnostics-summary",
            str(blocking_summary),
            "--out",
            str(blocked),
        )
        assert blocked_result.returncode == 1
        assert load(blocked)["status"] == "Blocked"


def test_publish_check_accepts_and_blocks_governance_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project_root = copy_sample(root)
        stage_report = root / "package_report.json"
        assert run_stage(project_root, stage_report).returncode == 0
        summary = root / "diagnostics_summary.json"
        write_clean_summary(summary)
        policy = root / "policy_evaluation_report.json"
        approval = root / "review_approval_report.json"
        audit = root / "audit_report.json"
        restricted = root / "restricted_export_report.json"
        write_policy_report(policy)
        write_review_approval_report(approval)
        write_audit_report(audit)
        write_restricted_export_report(restricted)
        publish = root / "publish_report.json"

        result = run_cli(
            "publish-check",
            "--project",
            str(manifest_path(project_root)),
            "--profile",
            PROFILE,
            "--package-report",
            str(stage_report),
            "--diagnostics-summary",
            str(summary),
            "--policy-report",
            str(policy),
            "--review-approval-report",
            str(approval),
            "--audit-report",
            str(audit),
            "--restricted-export-report",
            str(restricted),
            "--out",
            str(publish),
        )

        assert result.returncode == 0, result.stderr + result.stdout
        report = load(publish)
        assert report["status"] == "Passed"
        assert any(e["kind"] == "policyReport" and e["status"] == "Present" for e in report["requiredEvidence"])
        for gate in ["PolicyReportAccepted", "ReviewApprovalAccepted", "AuditEvidenceAccepted", "RestrictedExportAccepted"]:
            assert any(g["name"] == gate and g["status"] == "Passed" for g in report["gates"])

        cases = [
            ("policy-deny", lambda: write_policy_report(policy, "Deny"), "PolicyReportAccepted"),
            ("review-blocked", lambda: write_review_approval_report(approval, "BlockedByPolicy"), "ReviewApprovalAccepted"),
            ("audit-malformed", lambda: audit.write_text("{ invalid", encoding="utf-8"), "AuditEvidenceAccepted"),
            ("restricted-blocked", lambda: write_restricted_export_report(restricted, 1), "RestrictedExportAccepted"),
        ]
        for name, mutate, gate_name in cases:
            write_policy_report(policy)
            write_review_approval_report(approval)
            write_audit_report(audit)
            write_restricted_export_report(restricted)
            mutate()
            blocked = root / f"{name}_publish_report.json"
            blocked_result = run_cli(
                "publish-check",
                "--project",
                str(manifest_path(project_root)),
                "--profile",
                PROFILE,
                "--package-report",
                str(stage_report),
                "--diagnostics-summary",
                str(summary),
                "--policy-report",
                str(policy),
                "--review-approval-report",
                str(approval),
                "--audit-report",
                str(audit),
                "--restricted-export-report",
                str(restricted),
                "--out",
                str(blocked),
            )
            blocked_report = load(blocked)
            assert blocked_result.returncode == 1
            assert blocked_report["status"] == "Blocked"
            assert any(g["name"] == gate_name and g["status"] == "Blocked" for g in blocked_report["gates"])


def test_packaged_smoke_runs_headless_and_records_client_deferral() -> None:
    native_executable = NATIVE_BIN_DIR / "MultiplayerSandboxHeadless"
    if not native_executable.exists():
        raise AssertionError(f"missing native executable: {native_executable}")

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project_root = copy_sample(root)
        stage_report = root / "package_report.json"
        assert run_stage(project_root, stage_report).returncode == 0
        smoke = root / "package_smoke_report.json"

        result = run_cli(
            "smoke",
            "--project",
            str(manifest_path(project_root)),
            "--profile",
            PROFILE,
            "--package-report",
            str(stage_report),
            "--out",
            str(smoke),
            "--timeout-sec",
            "5",
            "--bin-dir",
            str(NATIVE_BIN_DIR),
        )

        assert result.returncode == 0, result.stderr + result.stdout
        report = load(smoke)
        assert report["status"] == "Passed"
        assert report["deferredStages"][0]["status"] == "Deferred"
        assert Path(report["diagnosticsReportPaths"][0]).exists()


def test_smoke_missing_package_or_executable_fails_clearly() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project_root = copy_sample(root)
        missing_report = root / "missing_stage.json"
        smoke = root / "package_smoke_report.json"

        result = run_cli(
            "smoke",
            "--project",
            str(manifest_path(project_root)),
            "--profile",
            PROFILE,
            "--package-report",
            str(missing_report),
            "--out",
            str(smoke),
            "--timeout-sec",
            "5",
            "--bin-dir",
            str(root / "missing-bin"),
        )

        assert result.returncode == 1
        codes = [d["code"] for d in load(smoke)["diagnostics"]]
        assert "Smoke.PackageReport.Missing" in codes
        assert "Smoke.Executable.Missing" in codes


def test_missing_project_returns_missing_input() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        result = run_cli(
            "validate",
            "--project",
            str(root / "missing.sagaproj"),
            "--profile",
            PROFILE,
            "--out",
            str(root / "package_report.json"),
        )
        assert result.returncode == 3


def run_all() -> None:
    tests = [
        test_validate_multiplayer_sandbox_passes,
        test_asset_validate_reports_missing_source_truth_for_placeholder_assets,
        test_asset_validate_accepts_explicit_asset_source_manifest_and_package_evidence,
        test_asset_validate_fails_when_supplied_package_asset_manifest_is_missing,
        test_asset_validate_duplicate_id_and_missing_root_fail_deterministically,
        test_profile_validation_failures_are_deterministic,
        test_missing_profile_and_missing_launch_profile_fail,
        test_stage_creates_deterministic_package_and_preserves_sources,
        test_stage_rejects_output_outside_build_packages,
        test_profile_matrix_reports_supported_headless_profile_without_publish_evidence,
        test_source_truth_alignment_reads_profiles_and_supplied_gates_without_staging,
        test_publish_check_passes_and_blocks_on_evidence,
        test_publish_check_accepts_and_blocks_governance_evidence,
        test_packaged_smoke_runs_headless_and_records_client_deferral,
        test_smoke_missing_package_or_executable_fails_clearly,
        test_missing_project_returns_missing_input,
    ]
    for test in tests:
        test()
    print(f"{len(tests)} SagaPackager CLI tests passed")


if __name__ == "__main__":
    run_all()
