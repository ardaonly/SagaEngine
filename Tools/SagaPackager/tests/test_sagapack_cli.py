#!/usr/bin/env python3
"""Focused CLI tests for SagaPackager."""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import tempfile
import time
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
PROJECT = REPO_ROOT / "Tools" / "SagaPackager" / "SagaPackager.csproj"
CLI_ASSEMBLY = PROJECT.parent / "bin" / "Release" / "net10.0" / "sagapack.dll"
SAMPLE_ROOT = REPO_ROOT / "Samples" / "MultiplayerSandbox"
STARTER_ROOT = REPO_ROOT / "Samples" / "StarterArena"
RUNTIME_BRIDGE_ASSEMBLY = (
    REPO_ROOT / "build" / "RelWithDebInfo-0.0.11" / "Managed" /
    "SagaScript.RuntimeBridge" / "SagaScript.RuntimeBridge.dll"
)
NATIVE_BIN_DIR = Path(
    os.environ.get(
        "SAGA_NATIVE_BIN_DIR",
        REPO_ROOT / "build" / "RelWithDebInfo-0.0.11" / "bin",
    )
)
PROFILE = "project-readiness-server-headless"
_cli_built = False


def ensure_cli_built(env: dict[str, str]) -> None:
    global _cli_built
    if _cli_built:
        return
    result = subprocess.run(
        ["dotnet", "build", str(PROJECT), "--configuration", "Release", "--nologo", "--verbosity", "quiet"],
        cwd=REPO_ROOT,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    assert result.returncode == 0, result.stderr + result.stdout
    assert CLI_ASSEMBLY.is_file(), f"missing SagaPackager assembly: {CLI_ASSEMBLY}"
    _cli_built = True


def run_cli(*args: str) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagapack-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagapack-nuget")
    env.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
    ensure_cli_built(env)
    return subprocess.run(
        ["dotnet", str(CLI_ASSEMBLY), *args],
        cwd=REPO_ROOT,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def run_cli_env(overrides: dict[str, str], *args: str) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.update(overrides)
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagapack-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagapack-nuget")
    env.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
    ensure_cli_built(env)
    return subprocess.run(["dotnet", str(CLI_ASSEMBLY), *args], cwd=REPO_ROOT, env=env,
                          text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)


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
            "tool": "diagnostic-check",
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
            "tool": "policy-validation",
            "command": "evaluate",
            "status": status,
            "decision": decision,
            "subject": "local-user",
            "resource": "package://project-readiness-server-headless",
            "action": "PublishCheck",
            "role": "ReleaseOperator",
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


def test_non_sagaproj_manifest_is_rejected() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project_root = copy_sample(root)
        legacy = project_root / ("saga.project" + ".json")
        manifest_path(project_root).rename(legacy)
        out = root / "package_report.json"

        result = run_cli(
            "validate",
            "--project",
            str(legacy),
            "--profile",
            PROFILE,
            "--out",
            str(out),
        )

        assert result.returncode == 1
        assert load(out)["diagnostics"][0]["code"] == "Package.Project.UnsupportedFileType"


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
                "tool": "diagnostic-check",
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


def test_publish_check_accepts_and_blocks_policy_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project_root = copy_sample(root)
        stage_report = root / "package_report.json"
        assert run_stage(project_root, stage_report).returncode == 0
        summary = root / "diagnostics_summary.json"
        write_clean_summary(summary)
        policy = root / "policy_evaluation_report.json"
        write_policy_report(policy)
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
            "--out",
            str(publish),
        )

        assert result.returncode == 0, result.stderr + result.stdout
        report = load(publish)
        assert report["status"] == "Passed"
        assert any(e["kind"] == "policyReport" and e["status"] == "Present" for e in report["requiredEvidence"])
        assert any(g["name"] == "PolicyReportAccepted" and g["status"] == "Passed" for g in report["gates"])

        for name, decision in [("policy-deny", "Deny"), ("policy-review", "RequiresReview")]:
            write_policy_report(policy)
            write_policy_report(policy, decision)
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
                "--out",
                str(blocked),
            )
            blocked_report = load(blocked)
            assert blocked_result.returncode == 1
            assert blocked_report["status"] == "Blocked"
            assert any(g["name"] == "PolicyReportAccepted" and g["status"] == "Blocked" for g in blocked_report["gates"])


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


def compile_starter(project_root: Path, output_root: Path, command: str = "compile-assets") -> subprocess.CompletedProcess[str]:
    return run_cli(
        command,
        "--project",
        str(project_root / "StarterArena.sagaproj"),
        "--out",
        str(output_root),
    )


def artifact_hashes(output_root: Path) -> dict[str, str]:
    manifest = load(output_root / "manifests" / "current.json")
    return {item["artifactId"]: item["contentHash"] for item in manifest["artifacts"]}


def copy_starter(root: Path, name: str = "StarterArena") -> Path:
    project_root = root / name
    shutil.copytree(STARTER_ROOT, project_root)
    return project_root


def run_resolve(project_root: Path, engine_root: Path, output_root: Path,
                runtime_bridge: Path = RUNTIME_BRIDGE_ASSEMBLY) -> subprocess.CompletedProcess[str]:
    return run_cli(
        "resolve",
        "--project",
        str(project_root / "StarterArena.sagaproj"),
        "--engine-root",
        str(engine_root),
        "--lock",
        str(output_root / "SagaPackage.lock"),
        "--generated-out",
        str(output_root / "generated"),
        "--runtime-bridge",
        str(runtime_bridge),
        "--out",
        str(output_root / "resolve-report.json"),
    )


def run_lock_verify(
    project_root: Path, engine_root: Path, lock_path: Path, report_path: Path,
    generated_out: Path | None = None, runtime_bridge: Path = RUNTIME_BRIDGE_ASSEMBLY,
) -> subprocess.CompletedProcess[str]:
    arguments = [
        "lock-verify",
        "--project",
        str(project_root / "StarterArena.sagaproj"),
        "--engine-root",
        str(engine_root),
        "--lock",
        str(lock_path),
        "--runtime-bridge",
        str(runtime_bridge),
        "--out",
        str(report_path),
    ]
    if generated_out is not None:
        arguments.extend(["--generated-out", str(generated_out)])
    return run_cli(*arguments)


def copy_distribution(engine_root: Path) -> Path:
    distribution = engine_root / "Engine" / "Distribution"
    shutil.copytree(REPO_ROOT / "Engine" / "Distribution", distribution)
    return distribution


def package_manifest(distribution: Path) -> Path:
    return (
        distribution
        / "Packages"
        / "com.saga.gameplay.foundation"
        / "0.0.11"
        / "package.json"
    )


def test_package_resolution_is_offline_and_deterministic_across_roots() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        first_project = copy_starter(root / "first")
        second_project = copy_starter(root / "second")
        first_output = root / "first-output"
        second_output = root / "second-output"

        first = run_resolve(first_project, REPO_ROOT, first_output)
        second = run_resolve(second_project, REPO_ROOT, second_output)

        assert first.returncode == 0, first.stderr + first.stdout
        assert second.returncode == 0, second.stderr + second.stdout
        assert (first_output / "SagaPackage.lock").read_bytes() == (
            second_output / "SagaPackage.lock"
        ).read_bytes()
        first_generated = first_output / "generated"
        second_generated = second_output / "generated"
        first_current = load(first_generated / "manifests" / "current.json")
        second_current = load(second_generated / "manifests" / "current.json")
        assert first_current == second_current
        first_generation = first_generated / "generations" / first_current["generationId"]
        second_generation = second_generated / "generations" / second_current["generationId"]
        for name in ["SagaPackage.lock", "csharp_references_v1.json", "api_surfaces_v1.json", "visual_blocks_surfaces_v1.json", "generation.json"]:
            assert (first_generation / name).read_bytes() == (second_generation / name).read_bytes()
        verified = run_lock_verify(
            first_project,
            REPO_ROOT,
            first_output / "SagaPackage.lock",
            root / "verify-report.json",
            first_generated,
        )
        assert verified.returncode == 0, verified.stderr + verified.stdout
        assert load(root / "verify-report.json")["status"] == "Passed"


def test_lock_verify_rejects_stale_intent_and_corrupt_artifact_hash() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project = copy_starter(root)
        output = root / "resolved"
        assert run_resolve(project, REPO_ROOT, output).returncode == 0
        lock_path = output / "SagaPackage.lock"
        lock = load(lock_path)
        lock["packages"][0]["artifacts"][0]["artifactHash"] = "sha256:" + "0" * 64
        write_json(lock_path, lock)
        corrupt = run_lock_verify(project, REPO_ROOT, lock_path, root / "corrupt.json")
        assert corrupt.returncode == 1
        codes = [item["code"] for item in load(root / "corrupt.json")["diagnostics"]]
        assert "Package.Lock.ArtifactHashMismatch" in codes

        manifest = load(project / "StarterArena.sagaproj")
        manifest["dependencies"][0]["version"] = "0.0.10"
        write_json(project / "StarterArena.sagaproj", manifest)
        stale = run_lock_verify(project, REPO_ROOT, lock_path, root / "stale.json")
        assert stale.returncode == 1
        stale_codes = [item["code"] for item in load(root / "stale.json")["diagnostics"]]
        assert "Package.Lock.StaleIntent" in stale_codes
        assert "Package.Resolve.VersionMismatch" in stale_codes


def test_lock_verify_rejects_mismatched_selected_generation() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project = copy_starter(root / "project")
        first_bridge = root / "bridge-first.dll"
        second_bridge = root / "bridge-second.dll"
        first_bridge.write_bytes(RUNTIME_BRIDGE_ASSEMBLY.read_bytes())
        second_bridge.write_bytes(RUNTIME_BRIDGE_ASSEMBLY.read_bytes() + b"alternate-generation")
        first = root / "first"
        second = root / "second"
        assert run_resolve(project, REPO_ROOT, first, first_bridge).returncode == 0
        assert run_resolve(project, REPO_ROOT, second, second_bridge).returncode == 0

        mismatch_report = root / "mismatch.json"
        mismatch = run_lock_verify(project, REPO_ROOT, first / "SagaPackage.lock", mismatch_report,
                                   second / "generated", first_bridge)
        assert mismatch.returncode == 1
        codes = [item["code"] for item in load(mismatch_report)["diagnostics"]]
        assert "Package.Generation.TrackedLockMismatch" in codes
        assert "Package.Generation.RuntimeBridgeMismatch" in codes

        current_path = first / "generated/manifests/current.json"
        current = load(current_path)
        current["generationManifestHash"] = "sha256:" + "0" * 64
        write_json(current_path, current)
        tampered_report = root / "tampered.json"
        tampered = run_lock_verify(project, REPO_ROOT, first / "SagaPackage.lock", tampered_report,
                                   first / "generated", first_bridge)
        assert tampered.returncode == 1
        tampered_codes = [item["code"] for item in load(tampered_report)["diagnostics"]]
        assert "Package.Generation.ManifestHashMismatch" in tampered_codes


def test_runtime_bridge_mutation_before_generation_publish_preserves_current() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project = copy_starter(root / "project")
        bridge = root / "runtime-bridge.dll"
        bridge.write_bytes(RUNTIME_BRIDGE_ASSEMBLY.read_bytes())
        output = root / "resolved"
        assert run_resolve(project, REPO_ROOT, output, bridge).returncode == 0
        lock_path = output / "SagaPackage.lock"
        current_path = output / "generated/manifests/current.json"
        original_lock = lock_path.read_bytes()
        original_current = current_path.read_bytes()
        barrier = root / "bridge-publication"
        report = root / "barrier-report.json"
        arguments = ["resolve", "--project", str(project / "StarterArena.sagaproj"),
                     "--engine-root", str(REPO_ROOT), "--lock", str(lock_path),
                     "--generated-out", str(output / "generated"), "--runtime-bridge", str(bridge),
                     "--out", str(report)]
        env = os.environ.copy()
        env["SAGAPACK_RUNTIME_BRIDGE_BARRIER"] = str(barrier)
        env.setdefault("DOTNET_CLI_HOME", "/tmp/sagapack-dotnet-home")
        env.setdefault("NUGET_PACKAGES", "/tmp/sagapack-nuget")
        ensure_cli_built(env)
        process = subprocess.Popen(["dotnet", str(CLI_ASSEMBLY), *arguments], cwd=REPO_ROOT, env=env,
                                   text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        deadline = time.monotonic() + 20
        while not barrier.with_suffix(".reached").exists():
            if process.poll() is not None or time.monotonic() >= deadline:
                raise AssertionError("sagapack did not reach RuntimeBridge publication barrier")
            time.sleep(0.01)
        bridge.write_bytes(bridge.read_bytes() + b"mutated-after-graph")
        barrier.with_suffix(".release").write_text("release\n", encoding="utf-8")
        stdout, stderr = process.communicate(timeout=20)
        assert process.returncode == 1, stderr + stdout
        codes = [item["code"] for item in load(report)["diagnostics"]]
        assert "Package.Resolve.RuntimeBridgeStale" in codes
        assert lock_path.read_bytes() == original_lock
        assert current_path.read_bytes() == original_current


def test_resolution_rejects_missing_artifacts_modules_and_provenance_gaps() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project = copy_starter(root / "project")
        engine_root = root / "engine"
        distribution = copy_distribution(engine_root)
        manifest_path = package_manifest(distribution)
        package = load(manifest_path)
        package["license"] = ""
        package["modules"] = []
        missing_artifact = distribution / package["artifacts"][0]["path"]
        missing_artifact.unlink()
        write_json(manifest_path, package)

        result = run_resolve(project, engine_root, root / "output")

        assert result.returncode == 1
        diagnostics = load(root / "output" / "resolve-report.json")["diagnostics"]
        codes = [item["code"] for item in diagnostics]
        assert codes == sorted(codes)
        assert "Package.Resolve.ArtifactMissing" in codes
        assert "Package.Resolve.ModuleUnsupported" in codes
        assert "Package.Resolve.ProvenanceGap" in codes
        assert not (root / "output" / "SagaPackage.lock").exists()


def test_project_local_package_cannot_supply_executable_artifacts() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project = copy_starter(root / "project")
        copied_package = project / "Packages" / "com.saga.gameplay.foundation" / "0.0.11"
        shutil.copytree(
            REPO_ROOT
            / "Engine"
            / "Distribution"
            / "Packages"
            / "com.saga.gameplay.foundation"
            / "0.0.11",
            copied_package,
        )
        engine_root = root / "empty-engine"
        (engine_root / "Engine" / "Distribution" / "Packages").mkdir(parents=True)

        result = run_resolve(project, engine_root, root / "output")

        assert result.returncode == 1
        codes = [
            item["code"]
            for item in load(root / "output" / "resolve-report.json")["diagnostics"]
        ]
        assert "Package.Resolve.BuiltinNotFound" in codes
        assert not (root / "output" / "SagaPackage.lock").exists()


def test_resolution_failure_injection_preserves_complete_generation() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project = copy_starter(root / "project")
        output = root / "resolved"
        assert run_resolve(project, REPO_ROOT, output).returncode == 0
        lock_path = output / "SagaPackage.lock"
        current_path = output / "generated/manifests/current.json"
        original_lock = lock_path.read_bytes()
        original_current = current_path.read_bytes()
        arguments = ["resolve", "--project", str(project / "StarterArena.sagaproj"),
                     "--engine-root", str(REPO_ROOT), "--lock", str(lock_path),
                     "--generated-out", str(output / "generated"), "--runtime-bridge",
                     str(RUNTIME_BRIDGE_ASSEMBLY), "--out", str(output / "injected.json")]
        for stage in [*(f"after-write-{index}" for index in range(1, 7)),
                      "before-generation-publish", "before-current-publish"]:
            failed = run_cli_env({"SAGAPACK_FAIL_STAGE": stage}, *arguments)
            assert failed.returncode == 1, stage
            assert lock_path.read_bytes() == original_lock
            assert current_path.read_bytes() == original_current


def test_duplicate_package_artifact_identity_is_rejected() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project = copy_starter(root / "project")
        engine_root = root / "engine"
        distribution = copy_distribution(engine_root)
        manifest_path = package_manifest(distribution)
        package = load(manifest_path)
        package["artifacts"][1]["artifactId"] = package["artifacts"][0]["artifactId"]
        write_json(manifest_path, package)
        result = run_resolve(project, engine_root, root / "output")
        assert result.returncode == 1
        codes = [item["code"] for item in load(root / "output/resolve-report.json")["diagnostics"]]
        assert "Package.Resolve.ArtifactIdentityInvalid" in codes


def test_starter_content_is_deterministic_across_clean_roots() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        first_project = root / "first" / "StarterArena"
        second_project = root / "second" / "StarterArena"
        shutil.copytree(STARTER_ROOT, first_project)
        shutil.copytree(STARTER_ROOT, second_project)
        first_output = root / "first-output"
        second_output = root / "second-output"
        first = compile_starter(first_project, first_output)
        second = compile_starter(second_project, second_output)
        assert first.returncode == 0, first.stderr
        assert second.returncode == 0, second.stderr
        assert (first_output / "manifests" / "current.json").read_bytes() == (
            second_output / "manifests" / "current.json"
        ).read_bytes()
        assert artifact_hashes(first_output) == artifact_hashes(second_output)


def test_input_rebind_changes_only_input_artifact() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project = root / "StarterArena"
        shutil.copytree(STARTER_ROOT, project)
        before = root / "before"
        after = root / "after"
        assert compile_starter(project, before, "compile-inputs").returncode == 0
        input_path = project / "Input" / "Input.saga.json"
        input_source = load(input_path)
        binding = input_source["actions"][0]["bindings"][0]
        binding["control"] = "F"
        binding["displayName"] = "Keyboard F"
        write_json(input_path, input_source)
        assert compile_starter(project, after, "compile-inputs").returncode == 0
        before_hashes = artifact_hashes(before)
        after_hashes = artifact_hashes(after)
        changed = sorted(key for key in before_hashes if before_hashes[key] != after_hashes[key])
        assert changed == ["starter-arena.input"]


def test_invalid_scene_does_not_replace_current_manifest() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project = root / "StarterArena"
        output = root / "output"
        shutil.copytree(STARTER_ROOT, project)
        assert compile_starter(project, output).returncode == 0
        current = output / "manifests" / "current.json"
        original = current.read_bytes()
        scene_path = project / "Scenes" / "StarterArena.sagascene"
        scene = load(scene_path)
        scene["unknownField"] = True
        write_json(scene_path, scene)
        failed = compile_starter(project, output, "compile-scenes")
        assert failed.returncode == 1
        assert current.read_bytes() == original
        result = load(output / "compile-result.json")
        assert "Content.Schema.UnknownField" in [item["code"] for item in result["diagnostics"]]


def run_all() -> None:
    tests = [
        test_package_resolution_is_offline_and_deterministic_across_roots,
        test_lock_verify_rejects_stale_intent_and_corrupt_artifact_hash,
        test_lock_verify_rejects_mismatched_selected_generation,
        test_runtime_bridge_mutation_before_generation_publish_preserves_current,
        test_resolution_rejects_missing_artifacts_modules_and_provenance_gaps,
        test_project_local_package_cannot_supply_executable_artifacts,
        test_resolution_failure_injection_preserves_complete_generation,
        test_duplicate_package_artifact_identity_is_rejected,
        test_starter_content_is_deterministic_across_clean_roots,
        test_input_rebind_changes_only_input_artifact,
        test_invalid_scene_does_not_replace_current_manifest,
        test_validate_multiplayer_sandbox_passes,
        test_asset_validate_reports_missing_source_truth_for_placeholder_assets,
        test_asset_validate_accepts_explicit_asset_source_manifest_and_package_evidence,
        test_asset_validate_fails_when_supplied_package_asset_manifest_is_missing,
        test_asset_validate_duplicate_id_and_missing_root_fail_deterministically,
        test_profile_validation_failures_are_deterministic,
        test_non_sagaproj_manifest_is_rejected,
        test_missing_profile_and_missing_launch_profile_fail,
        test_stage_creates_deterministic_package_and_preserves_sources,
        test_stage_rejects_output_outside_build_packages,
        test_profile_matrix_reports_supported_headless_profile_without_publish_evidence,
        test_source_truth_alignment_reads_profiles_and_supplied_gates_without_staging,
        test_publish_check_passes_and_blocks_on_evidence,
        test_publish_check_accepts_and_blocks_policy_evidence,
        test_smoke_missing_package_or_executable_fails_clearly,
        test_missing_project_returns_missing_input,
    ]
    if os.environ.get("SAGA_RUN_PACKAGED_NATIVE_SMOKE") == "1":
        tests.append(test_packaged_smoke_runs_headless_and_records_client_deferral)
    else:
        print("SagaPackager native smoke skipped; set SAGA_RUN_PACKAGED_NATIVE_SMOKE=1 after building MultiplayerSandboxHeadless")
    for test in tests:
        test()
    print(f"{len(tests)} SagaPackager CLI tests passed")


if __name__ == "__main__":
    run_all()
