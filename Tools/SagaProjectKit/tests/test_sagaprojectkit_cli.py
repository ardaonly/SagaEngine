#!/usr/bin/env python3
"""Focused CLI tests for the SagaProjectKit project manifest tool."""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
PROJECT = REPO_ROOT / "Tools" / "SagaProjectKit" / "SagaProjectKit.csproj"
SAMPLE_PROJECT = REPO_ROOT / "Samples" / "MultiplayerSandbox" / "MultiplayerSandbox.sagaproj"
SAMPLE_SLICE = REPO_ROOT / "Samples" / "MultiplayerSandbox" / ".saga" / "slices" / "designer-local.slice.json"


def run_cli(*args: str) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagaprojectkit-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagaprojectkit-nuget")
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


def valid_manifest() -> dict[str, object]:
    return {
        "schemaVersion": 0,
        "projectId": "fixture-project",
        "displayName": "Fixture Project",
        "engineCompatibility": {
            "minimumVersion": "0.0.9",
            "targetVersion": "0.1-project-readiness",
            "channel": "project-readiness",
        },
        "paths": {
            "diagnostics": "Diagnostics",
            "generatedReports": "Build/Reports",
        },
        "scenes": [],
        "assets": [
            {
                "id": "fixture.assets",
                "path": "Assets",
                "kind": "directory",
            }
        ],
        "scriptFolders": [
            {
                "id": "fixture.scripts",
                "path": "Scripts",
            }
        ],
        "launchProfiles": [
            {
                "id": "local",
                "path": "launch_profiles.json",
            }
        ],
        "packageProfiles": [
            {
                "id": "package",
                "path": "package_profiles.json",
            }
        ],
    }


def source_truth_scene(
    *,
    asset_owner: str = "scene-entity",
    scene_id: str = "fixture-scene",
    entity_id: str = "fixture-entity",
    generated_canonical: bool = False,
    expected_hash: str = "source-hash-v1",
) -> dict[str, object]:
    asset_ref: dict[str, object] = {
        "assetId": "fixture.assets",
        "path": "Assets/README.md",
        "usage": "placeholder-reference",
    }
    if asset_owner:
        asset_ref["owner"] = asset_owner
    return {
        "schemaVersion": 1,
        "sceneId": scene_id,
        "displayName": "Fixture Scene",
        "sourceKind": "SceneSourceTruth",
        "entities": [
            {
                "entityId": entity_id,
                "displayName": "Fixture Entity",
                "sourceKind": "EntitySourceTruth",
                "components": [{"type": "Transform", "properties": {}}],
                "assetReferences": [asset_ref],
            }
        ],
        "generatedArtifacts": [
            {
                "artifactId": "fixture-projection",
                "path": "Generated/SourceTruth/fixture.generated.json",
                "artifactKind": "GeneratedProjection",
                "canonical": generated_canonical,
                "expectedSourceHash": expected_hash,
            }
        ],
        "readBoundaries": {
            "runtime": {"status": "DocumentedReportOnly", "summary": "Runtime read is deferred."},
            "editor": {"status": "DocumentedReportOnly", "summary": "Editor inspection is report-only."},
            "clientEvaluation": {"status": "Deferred", "summary": "Client evaluation is deferred."},
        },
    }


def source_truth_manifest() -> dict[str, object]:
    manifest = valid_manifest()
    manifest["scenes"] = [
        {
            "id": "fixture-scene",
            "path": "Scenes/fixture.scene.json",
            "kind": "scene",
        }
    ]
    return manifest


def create_project(root: Path, manifest: dict[str, object] | None = None) -> Path:
    (root / "Assets").mkdir(parents=True)
    (root / "Scripts").mkdir(parents=True)
    (root / "Diagnostics").mkdir(parents=True)
    (root / "Build" / "Reports").mkdir(parents=True)
    (root / "Scripts" / "DoorLogic.High.cs").write_text("// high-level door logic\n", encoding="utf-8")
    (root / "Scripts" / "AdvancedUnsupported.cs").write_text("// advanced unsupported source\n", encoding="utf-8")
    (root / "Assets" / "README.md").write_text("asset root\n", encoding="utf-8")
    write_json(root / "launch_profiles.json", {"schemaVersion": 0, "profiles": []})
    write_json(root / "package_profiles.json", {"schemaVersion": 0, "profiles": []})
    manifest_path = root / "Fixture.sagaproj"
    write_json(manifest_path, manifest or valid_manifest())
    return manifest_path


def create_source_truth_project(root: Path, *, asset_owner: str = "scene-entity") -> Path:
    manifest = create_project(root, source_truth_manifest())
    write_json(root / "Scenes" / "fixture.scene.json", source_truth_scene(asset_owner=asset_owner))
    write_json(
        root / "Generated" / "SourceTruth" / "fixture.generated.json",
        {
            "schemaVersion": 1,
            "artifactId": "fixture-projection",
            "artifactKind": "GeneratedProjection",
            "canonical": False,
            "sourceSceneId": "fixture-scene",
            "sourceHash": "source-hash-v0",
        },
    )
    return manifest


def write_asset_source_manifest(root: Path, *, owner: str = "project-assets") -> None:
    write_json(
        root / "Assets" / "assets.source.json",
        {
            "schemaVersion": 1,
            "manifestId": "fixture.asset-source",
            "canonical": True,
            "assetOwners": [
                {
                    "assetId": "fixture.assets",
                    "owner": owner,
                    "path": "Assets/README.md",
                }
            ],
            "assetReferences": [],
        },
    )


def write_source_truth_inventory_for_freshness(root: Path, artifacts: list[dict[str, object]]) -> Path:
    inventory = root / "source_truth_inventory_report.json"
    write_json(
        inventory,
        {
            "schemaVersion": 1,
            "tool": "sagaproject",
            "command": "source-truth-inventory",
            "status": "Passed",
            "project": {},
            "scenes": [
                {
                    "sceneId": "fixture-scene",
                    "relativePath": "Scenes/fixture.scene.json",
                    "status": "Passed",
                }
            ],
            "generatedArtifacts": artifacts,
            "diagnostics": [],
            "localOnly": True,
            "networkExposure": "None",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )
    return inventory


def load_report(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def valid_slice(**overrides: object) -> dict[str, object]:
    payload: dict[str, object] = {
        "schemaVersion": 0,
        "sliceId": "designer-local",
        "displayName": "Designer Local Slice",
        "description": "Report-only project slice fixture.",
        "intendedRole": "Designer",
        "includedResources": [
            {
                "kind": "Project",
                "id": "fixture-project",
                "visibility": "SummaryOnly",
            },
            {
                "kind": "ScriptFile",
                "id": "door-logic",
                "path": "Scripts/DoorLogic.High.cs",
                "visibility": "FullSource",
            },
            {
                "kind": "ScriptFile",
                "id": "advanced",
                "path": "Scripts/AdvancedUnsupported.cs",
                "visibility": "SummaryOnly",
            },
            {
                "kind": "AssetRoot",
                "id": "fixture.assets",
                "path": "Assets",
                "visibility": "SourceLinkOnly",
            },
        ],
        "excludedResources": [
            {
                "kind": "ReportArtifact",
                "id": "raw-full-ctest",
                "path": "Build/Reports/raw_full_ctest_report.json",
                "visibility": "Hidden",
            }
        ],
        "visibilityRules": [],
        "diagnostics": [],
    }
    payload.update(overrides)
    return payload


def test_valid_project_report_passed() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest = create_project(Path(tmp) / "Project")
        out = Path(tmp) / "project_validation_report.json"

        result = run_cli("validate", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        assert report["schemaVersion"] == 1
        assert report["tool"] == "sagaproject"
        assert report["command"] == "validate"
        assert report["status"] == "Passed"
        assert report["diagnostics"] == []
        assert report["project"]["projectId"] == "fixture-project"


def test_missing_required_field_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest_payload = valid_manifest()
        del manifest_payload["projectId"]
        manifest = create_project(Path(tmp) / "Project", manifest_payload)
        out = Path(tmp) / "report.json"

        result = run_cli("validate", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 1
        report = load_report(out)
        assert report["status"] == "Failed"
        assert any(d["code"] == "Project.Manifest.MissingField" for d in report["diagnostics"])


def test_unknown_schema_version_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest_payload = valid_manifest()
        manifest_payload["schemaVersion"] = 99
        manifest = create_project(Path(tmp) / "Project", manifest_payload)
        out = Path(tmp) / "report.json"

        result = run_cli("validate", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 1
        report = load_report(out)
        assert any(
            d["code"] == "Project.Manifest.SchemaVersionUnsupported"
            for d in report["diagnostics"]
        )


def test_project_relative_path_rules_are_enforced() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest_payload = valid_manifest()
        manifest_payload["assets"] = [
            {"id": "absolute", "path": "/tmp/not-project", "kind": "directory"},
            {"id": "escape", "path": "../escape", "kind": "directory"},
        ]
        manifest = create_project(Path(tmp) / "Project", manifest_payload)
        out = Path(tmp) / "report.json"

        result = run_cli("validate", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 1
        codes = {d["code"] for d in load_report(out)["diagnostics"]}
        assert "Project.Path.AbsoluteNotAllowed" in codes
        assert "Project.Path.ParentTraversalNotAllowed" in codes


def test_missing_referenced_profile_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest = create_project(Path(tmp) / "Project")
        (manifest.parent / "launch_profiles.json").unlink()
        out = Path(tmp) / "report.json"

        result = run_cli("validate", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 1
        report = load_report(out)
        assert any(d["code"] == "Project.Reference.Missing" for d in report["diagnostics"])


def test_missing_input_uses_deterministic_exit_code() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "report.json"

        result = run_cli("validate", "--project", str(Path(tmp) / "missing.sagaproj"), "--out", str(out))

        assert result.returncode == 3
        assert load_report(out)["diagnostics"][0]["code"] == "Project.Input.Missing"


def test_invalid_usage_uses_deterministic_exit_code() -> None:
    result = run_cli("validate", "--project")

    assert result.returncode == 2


def test_project_open_does_not_mutate_manifest() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest = create_project(Path(tmp) / "Project")
        before = manifest.read_text(encoding="utf-8")
        out = Path(tmp) / "report.json"

        result = run_cli("validate", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        assert manifest.read_text(encoding="utf-8") == before


def test_resolve_output_is_stable_and_ordered() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest = create_project(Path(tmp) / "Project")
        first = Path(tmp) / "first.json"
        second = Path(tmp) / "second.json"

        first_result = run_cli("resolve", "--project", str(manifest), "--out", str(first))
        second_result = run_cli("resolve", "--project", str(manifest), "--out", str(second))

        assert first_result.returncode == 0, first_result.stderr + first_result.stdout
        assert second_result.returncode == 0, second_result.stderr + second_result.stdout
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        paths = load_report(first)["resolvedPaths"]
        keys = [(p["category"], p["id"], p["relativePath"]) for p in paths]
        assert keys == sorted(keys)


def test_resolve_slice_reports_visibility_and_report_only_mode() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Project"
        manifest = create_project(root)
        slice_path = root / ".saga" / "slices" / "designer-local.slice.json"
        out = Path(tmp) / "project_slice_resolution_report.json"
        write_json(slice_path, valid_slice())

        result = run_cli("resolve-slice", "--project", str(manifest), "--slice", str(slice_path), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        assert report["command"] == "resolve-slice"
        assert report["status"] == "Passed"
        assert report["mutatesSource"] is False
        assert report["enforcement"] == "ReportOnly"
        visibility = {(item["kind"], item["id"]): item for item in report["resourceVisibility"]}
        assert visibility[("ScriptFile", "door-logic")]["visibility"] == "FullSource"
        assert visibility[("ScriptFile", "advanced")]["visibility"] == "SummaryOnly"
        assert visibility[("ReportArtifact", "raw-full-ctest")]["status"] == "Excluded"


def test_resolve_slice_rejects_path_traversal_and_unknown_kind() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Project"
        manifest = create_project(root)
        slice_path = root / ".saga" / "slices" / "bad.slice.json"
        out = Path(tmp) / "project_slice_resolution_report.json"
        write_json(
            slice_path,
            valid_slice(
                includedResources=[
                    {
                        "kind": "ScriptFile",
                        "id": "escape",
                        "path": "../Outside.cs",
                        "visibility": "FullSource",
                    },
                    {
                        "kind": "Impossible",
                        "id": "unknown",
                        "visibility": "SummaryOnly",
                    },
                ],
                excludedResources=[],
            ),
        )

        result = run_cli("resolve-slice", "--project", str(manifest), "--slice", str(slice_path), "--out", str(out))

        assert result.returncode == 1
        codes = {d["code"] for d in load_report(out)["diagnostics"]}
        assert "ProjectSlice.Path.ParentTraversalNotAllowed" in codes
        assert "ProjectSlice.Resource.UnknownKind" in codes


def test_restricted_resolve_records_hidden_resources_without_changing_validate() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Project"
        manifest = create_project(root)
        slice_path = root / ".saga" / "slices" / "restricted.slice.json"
        restricted_out = Path(tmp) / "restricted_project_resolution_report.json"
        validate_out = Path(tmp) / "project_validation_report.json"
        write_json(
            slice_path,
            valid_slice(
                includedResources=[
                    {
                        "kind": "ScriptFile",
                        "id": "advanced",
                        "path": "Scripts/AdvancedUnsupported.cs",
                        "visibility": "Hidden",
                    },
                    {
                        "kind": "ScriptFile",
                        "id": "door-logic",
                        "path": "Scripts/DoorLogic.High.cs",
                        "visibility": "FullSource",
                    },
                ],
                excludedResources=[],
            ),
        )
        before = (root / "Scripts" / "AdvancedUnsupported.cs").read_text(encoding="utf-8")

        restricted = run_cli(
            "restricted-resolve",
            "--project",
            str(manifest),
            "--slice",
            str(slice_path),
            "--out",
            str(restricted_out),
        )
        validate = run_cli("validate", "--project", str(manifest), "--out", str(validate_out))

        assert restricted.returncode == 0, restricted.stderr + restricted.stdout
        assert validate.returncode == 0, validate.stderr + validate.stdout
        report = load_report(restricted_out)
        assert report["command"] == "restricted-resolve"
        assert report["mutatesSource"] is False
        assert report["enforcement"] == "ReportOnly"
        assert any(item["id"] == "advanced" and item["relativePath"] == "" for item in report["restrictedResources"])
        assert load_report(validate_out)["command"] == "validate"
        assert (root / "Scripts" / "AdvancedUnsupported.cs").read_text(encoding="utf-8") == before


def test_resolve_slice_missing_resource_fails_deterministically() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Project"
        manifest = create_project(root)
        slice_path = root / ".saga" / "slices" / "missing.slice.json"
        first = Path(tmp) / "first.json"
        second = Path(tmp) / "second.json"
        write_json(
            slice_path,
            valid_slice(
                includedResources=[
                    {
                        "kind": "ScriptFile",
                        "id": "missing",
                        "path": "Scripts/Missing.cs",
                        "visibility": "FullSource",
                    }
                ],
                excludedResources=[],
            ),
        )

        first_result = run_cli("resolve-slice", "--project", str(manifest), "--slice", str(slice_path), "--out", str(first))
        second_result = run_cli("resolve-slice", "--project", str(manifest), "--slice", str(slice_path), "--out", str(second))

        assert first_result.returncode == 1
        assert second_result.returncode == 1
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        report = load_report(first)
        assert any(d["code"] == "ProjectSlice.Resource.Missing" for d in report["diagnostics"])
        assert report["missingArtifacts"][0]["status"] == "Missing"


def test_doctor_detects_missing_scripts_folder() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest = create_project(Path(tmp) / "Project")
        shutil.rmtree(manifest.parent / "Scripts")
        out = Path(tmp) / "doctor.json"

        result = run_cli("doctor", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 1
        report = load_report(out)
        assert report["command"] == "doctor"
        assert any(d["code"] == "Project.Reference.Missing" for d in report["diagnostics"])


def test_doctor_detects_invalid_profile_json() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest = create_project(Path(tmp) / "Project")
        (manifest.parent / "package_profiles.json").write_text("{ invalid json", encoding="utf-8")
        out = Path(tmp) / "doctor.json"

        result = run_cli("doctor", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 1
        assert any(
            d["code"] == "Project.Profile.ParseFailed"
            for d in load_report(out)["diagnostics"]
        )


def test_multiplayer_sandbox_validate_resolve_doctor_pass() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        validate_out = Path(tmp) / "validate.json"
        resolve_out = Path(tmp) / "resolve.json"
        doctor_out = Path(tmp) / "doctor.json"

        validate = run_cli("validate", "--project", str(SAMPLE_PROJECT), "--out", str(validate_out))
        resolve = run_cli("resolve", "--project", str(SAMPLE_PROJECT), "--out", str(resolve_out))
        doctor = run_cli("doctor", "--project", str(SAMPLE_PROJECT), "--out", str(doctor_out))

        assert validate.returncode == 0, validate.stderr + validate.stdout
        assert resolve.returncode == 0, resolve.stderr + resolve.stdout
        assert doctor.returncode == 0, doctor.stderr + doctor.stdout
        assert load_report(validate_out)["status"] == "Passed"
        assert load_report(resolve_out)["status"] == "Passed"
        assert load_report(doctor_out)["status"] == "Passed"
        assert load_report(doctor_out)["diagnostics"] == []


def test_multiplayer_sandbox_sample_slice_resolves() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "project_slice_resolution_report.json"

        result = run_cli("resolve-slice", "--project", str(SAMPLE_PROJECT), "--slice", str(SAMPLE_SLICE), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        assert report["slice"]["sliceId"] == "designer-local"
        assert report["status"] == "Passed"


def test_source_truth_inventory_classifies_scene_entity_and_generated_artifacts() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "source_truth_inventory_report.json"

        result = run_cli("source-truth-inventory", "--project", str(SAMPLE_PROJECT), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        assert report["command"] == "source-truth-inventory"
        assert report["status"] == "PartiallyProven"
        assert report["localOnly"] is True
        assert report["networkExposure"] == "None"
        assert report["mutatesSource"] is False
        assert report["enforcement"] == "ReportOnly"
        assert report["scenes"][0]["classification"] == "SourceTruth"
        assert report["entities"][0]["classification"] == "SourceTruth"
        assert report["generatedArtifacts"][0]["canonical"] is False
        assert report["summary"]["nonCanonicalGeneratedArtifacts"] == 1
        assert report["summary"]["staleGeneratedEvidence"] == 1
        assert any(d["code"] == "Project.SourceTruth.GeneratedArtifactStale" for d in report["diagnostics"])
        boundaries = {item["boundary"]: item["status"] for item in report["readBoundaries"]}
        assert boundaries == {"clientEvaluation": "Deferred", "editor": "DocumentedReportOnly", "runtime": "DocumentedReportOnly"}


def test_source_truth_gate_and_opening_are_report_only_and_partial() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        inventory = root / "source_truth_inventory_report.json"
        gate = root / "source_truth_gate_report.json"
        closure = root / "enterprise_closure_report.json"
        opening = root / "asset_workflow_opening_report.json"
        write_json(closure, {"outcome": "PartiallyProven", "residualDebt": ["preserved debt"]})

        assert run_cli("source-truth-inventory", "--project", str(SAMPLE_PROJECT), "--out", str(inventory)).returncode == 0
        gate_result = run_cli("source-truth-gate", "--project", str(SAMPLE_PROJECT), "--inventory-report", str(inventory), "--out", str(gate))
        opening_result = run_cli(
            "asset-workflow-opening",
            "--project",
            str(SAMPLE_PROJECT),
            "--inventory-report",
            str(inventory),
            "--gate-report",
            str(gate),
            "--enterprise-closure",
            str(closure),
            "--out",
            str(opening),
        )

        assert gate_result.returncode == 0, gate_result.stderr + gate_result.stdout
        assert opening_result.returncode == 0, opening_result.stderr + opening_result.stdout
        gate_report = load_report(gate)
        opening_report = load_report(opening)
        assert gate_report["status"] == "PartiallyProven"
        assert all(check["status"] == "Passed" for check in gate_report["checks"])
        assert opening_report["status"] == "PartiallyProven"
        assert opening_report["outcome"] == "PartiallyProven"
        assert len(opening_report["openedResponsibilities"]) == 8
        assert opening_report["reservedFollowUps"] == ["Follow-up source-truth and runtime-read work remains planning/report-only until implemented"]
        assert opening_report["residualDebt"] == ["preserved debt"]
        assert opening_report["mutatesSource"] is False


def test_source_truth_inventory_diagnoses_missing_scene_truth() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest = create_project(Path(tmp) / "Project")
        out = Path(tmp) / "source_truth_inventory_report.json"

        result = run_cli("source-truth-inventory", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 1
        report = load_report(out)
        assert report["status"] == "Failed"
        assert any(d["code"] == "Project.SourceTruth.MissingSceneSourceTruth" for d in report["diagnostics"])


def test_source_truth_inventory_diagnoses_missing_asset_reference_owner() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest = create_source_truth_project(Path(tmp) / "Project", asset_owner="")
        out = Path(tmp) / "source_truth_inventory_report.json"

        result = run_cli("source-truth-inventory", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 1
        report = load_report(out)
        assert report["status"] == "Failed"
        assert any(d["code"] == "Project.SourceTruth.AssetReferenceOwnerMissing" for d in report["diagnostics"])


def test_asset_source_manifest_inventory_detects_declared_root_and_assets_source() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Project"
        manifest = create_source_truth_project(root)
        write_asset_source_manifest(root)
        out = Path(tmp) / "asset_source_manifest_inventory_report.json"

        result = run_cli("asset-source-manifest-inventory", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        assert report["command"] == "asset-source-manifest-inventory"
        assert report["localOnly"] is True
        assert report["networkExposure"] == "None"
        assert report["mutatesSource"] is False
        assert report["enforcement"] == "ReportOnly"
        assert report["assetRoots"][0]["assetId"] == "fixture.assets"
        assert report["assetRoots"][0]["classification"] == "SourceAssetRoot"
        assert report["assetSourceManifests"][0]["manifestKind"] == "assets.source.json"
        assert report["assetSourceManifests"][0]["canonical"] is True
        assert report["assetOwners"][0]["owner"] == "project-assets"


def test_asset_source_manifest_inventory_diagnoses_placeholder_root() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest = create_source_truth_project(Path(tmp) / "Project")
        out = Path(tmp) / "asset_source_manifest_inventory_report.json"

        result = run_cli("asset-source-manifest-inventory", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        assert report["status"] == "PartiallyProven"
        assert report["assetRoots"][0]["classification"] == "PlaceholderAssetRoot"
        assert any(d["code"] == "Project.AssetSource.PlaceholderAssetRoot" for d in report["diagnostics"])


def test_asset_source_manifest_inventory_classifies_generated_manifest_noncanonical() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Project"
        manifest = create_source_truth_project(root)
        write_asset_source_manifest(root)
        write_json(
            root / "Generated" / "Assets" / "asset_manifest.json",
            {
                "schemaVersion": 1,
                "manifestId": "generated-assets",
                "canonical": True,
                "assetOwners": [
                    {
                        "assetId": "generated.asset",
                        "owner": "generated-artifact",
                        "path": "Assets/generated.asset",
                    }
                ],
            },
        )
        out = Path(tmp) / "asset_source_manifest_inventory_report.json"

        result = run_cli("asset-source-manifest-inventory", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        generated = [item for item in report["assetSourceManifests"] if item["relativePath"].startswith("Generated/")]
        assert generated[0]["canonical"] is False
        assert generated[0]["classification"] == "GeneratedNonCanonical"
        assert any(d["code"] == "Project.AssetSource.GeneratedManifestNonCanonical" for d in report["diagnostics"])


def test_asset_reference_gate_resolves_scene_entity_reference() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Project"
        manifest = create_source_truth_project(root)
        write_asset_source_manifest(root)
        source_truth = Path(tmp) / "source_truth_inventory_report.json"
        asset_inventory = Path(tmp) / "asset_source_manifest_inventory_report.json"
        out = Path(tmp) / "asset_reference_gate_report.json"

        assert run_cli("source-truth-inventory", "--project", str(manifest), "--out", str(source_truth)).returncode == 0
        assert run_cli("asset-source-manifest-inventory", "--project", str(manifest), "--out", str(asset_inventory)).returncode == 0
        result = run_cli(
            "asset-reference-gate",
            "--project",
            str(manifest),
            "--source-truth-inventory",
            str(source_truth),
            "--asset-manifest-inventory",
            str(asset_inventory),
            "--out",
            str(out),
        )

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        assert report["status"] == "Passed"
        assert report["resolvedReferences"][0]["assetId"] == "fixture.assets"
        assert report["unresolvedReferences"] == []
        assert all(check["status"] == "Passed" for check in report["checks"])


def test_asset_reference_gate_diagnoses_missing_owner_and_generated_owner() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Project"
        manifest = create_source_truth_project(root, asset_owner="")
        write_asset_source_manifest(root)
        source_truth = Path(tmp) / "source_truth_inventory_report.json"
        asset_inventory = Path(tmp) / "asset_source_manifest_inventory_report.json"
        missing_owner_out = Path(tmp) / "missing_owner_asset_reference_gate_report.json"

        run_cli("source-truth-inventory", "--project", str(manifest), "--out", str(source_truth))
        run_cli("asset-source-manifest-inventory", "--project", str(manifest), "--out", str(asset_inventory))
        missing_owner = run_cli(
            "asset-reference-gate",
            "--project",
            str(manifest),
            "--source-truth-inventory",
            str(source_truth),
            "--asset-manifest-inventory",
            str(asset_inventory),
            "--out",
            str(missing_owner_out),
        )

        assert missing_owner.returncode == 1
        missing_report = load_report(missing_owner_out)
        assert missing_report["missingOwners"][0]["assetId"] == "fixture.assets"
        assert any(d["code"] == "Project.AssetReference.OwnerMissing" for d in missing_report["diagnostics"])

        root2 = Path(tmp) / "GeneratedOwner"
        manifest2 = create_source_truth_project(root2, asset_owner="generated-artifact")
        write_asset_source_manifest(root2)
        source_truth2 = Path(tmp) / "source_truth_inventory_generated.json"
        asset_inventory2 = Path(tmp) / "asset_source_manifest_inventory_generated.json"
        generated_out = Path(tmp) / "generated_owner_asset_reference_gate_report.json"

        run_cli("source-truth-inventory", "--project", str(manifest2), "--out", str(source_truth2))
        run_cli("asset-source-manifest-inventory", "--project", str(manifest2), "--out", str(asset_inventory2))
        generated_owner = run_cli(
            "asset-reference-gate",
            "--project",
            str(manifest2),
            "--source-truth-inventory",
            str(source_truth2),
            "--asset-manifest-inventory",
            str(asset_inventory2),
            "--out",
            str(generated_out),
        )

        assert generated_owner.returncode == 1
        generated_report = load_report(generated_out)
        assert generated_report["rejectedGeneratedOwners"][0]["owner"] == "generated-artifact"
        assert any(d["code"] == "Project.AssetReference.GeneratedOwnerRejected" for d in generated_report["diagnostics"])


def test_generated_freshness_gate_reports_all_statuses_and_is_deterministic() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Project"
        manifest = create_project(root, source_truth_manifest())
        write_json(root / "Scenes" / "fixture.scene.json", source_truth_scene())
        write_json(root / "Generated" / "SourceTruth" / "fresh.generated.json", {"sourceHash": "fresh-hash"})
        write_json(root / "Generated" / "SourceTruth" / "stale.generated.json", {"inputHash": "old-hash"})
        write_json(root / "Generated" / "SourceTruth" / "unknown.generated.json", {"artifactId": "unknown"})
        inventory = write_source_truth_inventory_for_freshness(
            root,
            [
                {
                    "sceneId": "fixture-scene",
                    "artifactId": "fresh",
                    "path": "Generated/SourceTruth/fresh.generated.json",
                    "canonical": False,
                    "expectedSourceHash": "fresh-hash",
                },
                {
                    "sceneId": "fixture-scene",
                    "artifactId": "stale",
                    "path": "Generated/SourceTruth/stale.generated.json",
                    "canonical": False,
                    "expectedSourceHash": "new-hash",
                },
                {
                    "sceneId": "fixture-scene",
                    "artifactId": "missing-source",
                    "path": "Generated/SourceTruth/fresh.generated.json",
                    "canonical": False,
                    "expectedSourceHash": "",
                },
                {
                    "sceneId": "fixture-scene",
                    "artifactId": "missing-generated",
                    "path": "Generated/SourceTruth/missing.generated.json",
                    "canonical": False,
                    "expectedSourceHash": "missing-hash",
                },
                {
                    "sceneId": "fixture-scene",
                    "artifactId": "unknown",
                    "path": "Generated/SourceTruth/unknown.generated.json",
                    "canonical": False,
                    "expectedSourceHash": "unknown-hash",
                },
            ],
        )
        first = Path(tmp) / "generated_artifact_freshness_report_first.json"
        second = Path(tmp) / "generated_artifact_freshness_report_second.json"

        first_result = run_cli("generated-freshness-gate", "--project", str(manifest), "--inventory", str(inventory), "--out", str(first))
        second_result = run_cli("generated-freshness-gate", "--project", str(manifest), "--inventory", str(inventory), "--out", str(second))

        assert first_result.returncode == 0, first_result.stderr + first_result.stdout
        assert second_result.returncode == 0, second_result.stderr + second_result.stdout
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        report = load_report(first)
        statuses = {item["artifactId"]: item["status"] for item in report["generatedArtifacts"]}
        assert statuses == {
            "fresh": "Fresh",
            "stale": "Stale",
            "missing-source": "MissingSource",
            "missing-generated": "MissingGenerated",
            "unknown": "UnknownFreshness",
        }
        assert report["status"] == "PartiallyProven"
        assert report["mutatesSource"] is False


def test_generated_freshness_gate_rejects_invalid_generated_canonical() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Project"
        manifest = create_project(root, source_truth_manifest())
        write_json(root / "Scenes" / "fixture.scene.json", source_truth_scene())
        write_json(root / "Generated" / "SourceTruth" / "invalid.generated.json", {"sourceHash": "hash"})
        inventory = write_source_truth_inventory_for_freshness(
            root,
            [
                {
                    "sceneId": "fixture-scene",
                    "artifactId": "invalid",
                    "path": "Generated/SourceTruth/invalid.generated.json",
                    "canonical": True,
                    "expectedSourceHash": "hash",
                }
            ],
        )
        out = Path(tmp) / "generated_artifact_freshness_report.json"

        result = run_cli("generated-freshness-gate", "--project", str(manifest), "--inventory", str(inventory), "--out", str(out))

        assert result.returncode == 1
        report = load_report(out)
        assert report["generatedArtifacts"][0]["status"] == "Invalid"
        assert any(d["code"] == "Project.GeneratedFreshness.Invalid" for d in report["diagnostics"])


def test_scene_entity_validate_accepts_valid_scene_and_reports_invariants() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest = create_source_truth_project(Path(tmp) / "Project")
        out = Path(tmp) / "scene_entity_validation_report.json"

        result = run_cli("scene-entity-validate", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        assert report["status"] == "Passed"
        assert report["localOnly"] is True
        assert report["networkExposure"] == "None"
        assert report["mutatesSource"] is False
        assert report["enforcement"] == "ReportOnly"
        assert report["scenes"][0]["status"] == "Passed"
        assert report["entities"][0]["status"] == "Passed"
        assert all(check["status"] == "Passed" for check in report["schemaChecks"])


def test_scene_entity_validate_diagnoses_duplicate_missing_and_malformed_schema() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Project"
        manifest = create_project(root, source_truth_manifest())
        duplicate = source_truth_scene()
        duplicate["entities"].append(dict(duplicate["entities"][0]))  # type: ignore[index, union-attr]
        write_json(root / "Scenes" / "fixture.scene.json", duplicate)
        out = Path(tmp) / "duplicate_scene_entity_validation_report.json"

        duplicate_result = run_cli("scene-entity-validate", "--project", str(manifest), "--out", str(out))

        assert duplicate_result.returncode == 1
        assert any(d["code"] == "Project.SceneEntity.DuplicateEntityId" for d in load_report(out)["diagnostics"])

        missing_scene = source_truth_scene()
        del missing_scene["sceneId"]
        write_json(root / "Scenes" / "fixture.scene.json", missing_scene)
        missing_scene_out = Path(tmp) / "missing_scene_id_report.json"

        missing_scene_result = run_cli("scene-entity-validate", "--project", str(manifest), "--out", str(missing_scene_out))

        assert missing_scene_result.returncode == 1
        assert any(d["code"] == "Project.SceneEntity.SceneIdMissing" for d in load_report(missing_scene_out)["diagnostics"])

        missing_entity = source_truth_scene(entity_id="")
        write_json(root / "Scenes" / "fixture.scene.json", missing_entity)
        missing_entity_out = Path(tmp) / "missing_entity_id_report.json"

        missing_entity_result = run_cli("scene-entity-validate", "--project", str(manifest), "--out", str(missing_entity_out))

        assert missing_entity_result.returncode == 1
        assert any(d["code"] == "Project.SceneEntity.EntityIdMissing" for d in load_report(missing_entity_out)["diagnostics"])

        (root / "Scenes" / "fixture.scene.json").write_text("{ invalid json", encoding="utf-8")
        malformed_out = Path(tmp) / "malformed_scene_report.json"

        malformed_result = run_cli("scene-entity-validate", "--project", str(manifest), "--out", str(malformed_out))

        assert malformed_result.returncode == 1
        assert any(d["code"] == "Project.SceneEntity.SceneMalformed" for d in load_report(malformed_out)["diagnostics"])


def test_scene_entity_validate_diagnoses_missing_asset_owner_and_generated_canonical() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Project"
        manifest = create_project(root, source_truth_manifest())
        write_json(
            root / "Scenes" / "fixture.scene.json",
            source_truth_scene(asset_owner="", generated_canonical=True),
        )
        out = Path(tmp) / "scene_entity_validation_report.json"

        result = run_cli("scene-entity-validate", "--project", str(manifest), "--out", str(out))

        assert result.returncode == 1
        codes = {d["code"] for d in load_report(out)["diagnostics"]}
        assert "Project.SceneEntity.AssetReferenceOwnerMissing" in codes
        assert "Project.SceneEntity.GeneratedArtifactCanonical" in codes


def test_asset_source_manifest_reports_do_not_mutate_source_files() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Project"
        manifest = create_source_truth_project(root)
        write_asset_source_manifest(root)
        source_truth = Path(tmp) / "source_truth_inventory_report.json"
        asset_inventory = Path(tmp) / "asset_source_manifest_inventory_report.json"
        before = {
            "manifest": manifest.read_text(encoding="utf-8"),
            "scene": (root / "Scenes" / "fixture.scene.json").read_text(encoding="utf-8"),
            "asset_source": (root / "Assets" / "assets.source.json").read_text(encoding="utf-8"),
            "generated": (root / "Generated" / "SourceTruth" / "fixture.generated.json").read_text(encoding="utf-8"),
        }

        run_cli("source-truth-inventory", "--project", str(manifest), "--out", str(source_truth))
        run_cli("asset-source-manifest-inventory", "--project", str(manifest), "--out", str(asset_inventory))
        run_cli(
            "asset-reference-gate",
            "--project",
            str(manifest),
            "--source-truth-inventory",
            str(source_truth),
            "--asset-manifest-inventory",
            str(asset_inventory),
            "--out",
            str(Path(tmp) / "asset_reference_gate_report.json"),
        )
        run_cli("generated-freshness-gate", "--project", str(manifest), "--inventory", str(source_truth), "--out", str(Path(tmp) / "freshness.json"))
        run_cli("scene-entity-validate", "--project", str(manifest), "--out", str(Path(tmp) / "schema.json"))

        assert manifest.read_text(encoding="utf-8") == before["manifest"]
        assert (root / "Scenes" / "fixture.scene.json").read_text(encoding="utf-8") == before["scene"]
        assert (root / "Assets" / "assets.source.json").read_text(encoding="utf-8") == before["asset_source"]
        assert (root / "Generated" / "SourceTruth" / "fixture.generated.json").read_text(encoding="utf-8") == before["generated"]


def write_metadata_ownership_inputs(root: Path, manifest: Path) -> tuple[Path, Path]:
    source_truth = root / "source_truth_inventory_report.json"
    scene_validation = root / "scene_entity_validation_report.json"
    freshness = root / "generated_artifact_freshness_report.json"
    assert run_cli("source-truth-inventory", "--project", str(manifest), "--out", str(source_truth)).returncode == 0
    assert run_cli("scene-entity-validate", "--project", str(manifest), "--out", str(scene_validation)).returncode == 0
    assert run_cli("generated-freshness-gate", "--project", str(manifest), "--inventory", str(source_truth), "--out", str(freshness)).returncode == 0
    return scene_validation, freshness


def test_component_metadata_ownership_accepts_known_components_and_evidence_only_generated() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        scene_validation, freshness = write_metadata_ownership_inputs(root, SAMPLE_PROJECT)
        out = root / "component_metadata_ownership_report.json"

        result = run_cli(
            "component-metadata-ownership",
            "--project",
            str(SAMPLE_PROJECT),
            "--scene-validation",
            str(scene_validation),
            "--freshness",
            str(freshness),
            "--out",
            str(out),
        )

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        assert report["status"] == "PartiallyProven"
        assert {item["componentType"] for item in report["components"]} == {"BehaviorReference", "Transform"}
        assert all(item["classification"] == "SceneEntitySourceTruth" for item in report["components"])
        assert report["generatedEvidence"][0]["classification"] == "EvidenceOnly"
        assert report["freshnessDebt"][0]["status"] == "Stale"
        assert report["mutatesSource"] is False


def test_component_metadata_ownership_diagnoses_unknown_component_type() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Project"
        manifest = create_project(root, source_truth_manifest())
        scene = source_truth_scene()
        scene["entities"][0]["components"].append({"type": "UnknownComponent", "properties": {}})  # type: ignore[index]
        write_json(root / "Scenes" / "fixture.scene.json", scene)
        write_json(root / "Generated" / "SourceTruth" / "fixture.generated.json", {"sourceHash": "source-hash-v1"})
        scene_validation, freshness = write_metadata_ownership_inputs(Path(tmp), manifest)
        out = Path(tmp) / "component_metadata_ownership_report.json"

        result = run_cli(
            "component-metadata-ownership",
            "--project",
            str(manifest),
            "--scene-validation",
            str(scene_validation),
            "--freshness",
            str(freshness),
            "--out",
            str(out),
        )

        assert result.returncode == 1
        report = load_report(out)
        assert any(d["code"] == "Project.ComponentOwnership.UnknownComponentType" for d in report["diagnostics"])


def test_editor_inspection_and_read_are_report_only_and_source_backed() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        scene_validation, freshness = write_metadata_ownership_inputs(root, SAMPLE_PROJECT)
        inspection = root / "editor_inspection_model_report.json"
        read = root / "editor_source_truth_read_report.json"

        inspection_result = run_cli(
            "editor-inspection-model",
            "--project",
            str(SAMPLE_PROJECT),
            "--scene-validation",
            str(scene_validation),
            "--freshness",
            str(freshness),
            "--out",
            str(inspection),
        )
        read_result = run_cli(
            "editor-source-truth-read",
            "--project",
            str(SAMPLE_PROJECT),
            "--inspection-model",
            str(inspection),
            "--out",
            str(read),
        )

        assert inspection_result.returncode == 0, inspection_result.stderr + inspection_result.stdout
        assert read_result.returncode == 0, read_result.stderr + read_result.stdout
        inspection_report = load_report(inspection)
        read_report = load_report(read)
        assert inspection_report["editorMayMutate"] is False
        assert inspection_report["scenes"]
        assert inspection_report["entities"]
        assert inspection_report["components"]
        assert inspection_report["assetReferences"]
        assert inspection_report["freshnessStatus"][0]["status"] == "Stale"
        assert read_report["readSources"][0]["status"] == "ReadOnlyAccepted"
        assert "QtUiMutation" in read_report["prohibitedActions"]


def test_editor_source_truth_read_fails_missing_scene_and_generated_canonical_claim() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "Project"
        manifest = create_project(root, source_truth_manifest())
        scene = source_truth_scene(generated_canonical=True)
        write_json(root / "Scenes" / "fixture.scene.json", scene)
        inspection = Path(tmp) / "inspection.json"
        write_json(inspection, {"scenes": [{"sceneId": "fixture-scene", "relativePath": "Scenes/fixture.scene.json"}]})
        canonical_out = Path(tmp) / "canonical_read.json"

        canonical = run_cli("editor-source-truth-read", "--project", str(manifest), "--inspection-model", str(inspection), "--out", str(canonical_out))
        assert canonical.returncode == 1
        assert any(d["code"] == "Project.EditorRead.GeneratedCanonicalRejected" for d in load_report(canonical_out)["diagnostics"])

        (root / "Scenes" / "fixture.scene.json").unlink()
        missing_out = Path(tmp) / "missing_read.json"
        missing = run_cli("editor-source-truth-read", "--project", str(manifest), "--inspection-model", str(inspection), "--out", str(missing_out))
        assert missing.returncode == 1
        assert any(d["code"] == "Project.EditorRead.SceneSourceMissing" for d in load_report(missing_out)["diagnostics"])


def test_runtime_audit_and_readiness_are_planning_only_with_stale_generated_debt() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        scene_validation, freshness = write_metadata_ownership_inputs(root, SAMPLE_PROJECT)
        audit = root / "runtime_read_model_audit_report.json"
        readiness = root / "runtime_readiness_gate_report.json"

        audit_result = run_cli(
            "runtime-read-model-audit",
            "--project",
            str(SAMPLE_PROJECT),
            "--scene-validation",
            str(scene_validation),
            "--freshness",
            str(freshness),
            "--out",
            str(audit),
        )
        readiness_result = run_cli(
            "runtime-readiness-gate",
            "--project",
            str(SAMPLE_PROJECT),
            "--scene-validation",
            str(scene_validation),
            "--freshness",
            str(freshness),
            "--runtime-read-model",
            str(audit),
            "--out",
            str(readiness),
        )

        assert audit_result.returncode == 0, audit_result.stderr + audit_result.stdout
        assert readiness_result.returncode == 0, readiness_result.stderr + readiness_result.stdout
        audit_report = load_report(audit)
        readiness_report = load_report(readiness)
        assert any(item["kind"] == "StaleGeneratedEvidence" for item in audit_report["forbiddenRuntimeSourceTruth"])
        assert readiness_report["readinessStatus"] == "PartiallyProven"
        assert readiness_report["planningScope"] == "Planning allowed only; Runtime not implemented."
        assert readiness_report["runtimeImplementationClaim"] == "Runtime not implemented."


def test_source_truth_scenario_is_deterministic_and_preserves_client_evaluation_deferral() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        evidence_root = Path(tmp) / "Build" / "SourceTruth"
        evidence_root.mkdir(parents=True)
        for filename, status in [
            ("scene_entity_validation_report.json", "Passed"),
            ("asset_reference_gate_report.json", "Passed"),
            ("generated_artifact_freshness_report.json", "PartiallyProven"),
            ("component_metadata_ownership_report.json", "PartiallyProven"),
            ("editor_inspection_model_report.json", "PartiallyProven"),
            ("editor_source_truth_read_report.json", "Passed"),
            ("runtime_read_model_audit_report.json", "PartiallyProven"),
            ("runtime_readiness_gate_report.json", "PartiallyProven"),
            ("package_source_truth_alignment_report.json", "Passed"),
            ("launch_source_truth_alignment_report.json", "PartiallyProven"),
        ]:
            write_json(evidence_root / filename, {"status": status, "localOnly": True, "networkExposure": "None", "mutatesSource": False, "enforcement": "ReportOnly"})
        first = Path(tmp) / "scenario_first.json"
        second = Path(tmp) / "scenario_second.json"

        first_result = run_cli("source-truth-scenario", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(first))
        second_result = run_cli("source-truth-scenario", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(second))

        assert first_result.returncode == 0, first_result.stderr + first_result.stdout
        assert second_result.returncode == 0, second_result.stderr + second_result.stdout
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        report = load_report(first)
        assert report["outcome"] == "PartiallyProven"
        assert "Client Evaluation deferral is explicit." in report["explicitDeferrals"]

        (evidence_root / "component_metadata_ownership_report.json").unlink()
        blocked = Path(tmp) / "scenario_blocked.json"
        blocked_result = run_cli("source-truth-scenario", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(blocked))
        assert blocked_result.returncode == 1
        assert load_report(blocked)["outcome"] == "Blocked"


def write_client_readiness_seed_evidence(evidence_root: Path) -> None:
    evidence_root.mkdir(parents=True, exist_ok=True)
    for filename, status in [
        ("source_truth_inventory_report.json", "PartiallyProven"),
        ("source_truth_gate_report.json", "PartiallyProven"),
        ("asset_source_manifest_inventory_report.json", "PartiallyProven"),
        ("asset_reference_gate_report.json", "Passed"),
        ("generated_artifact_freshness_report.json", "PartiallyProven"),
        ("scene_entity_validation_report.json", "Passed"),
        ("component_metadata_ownership_report.json", "PartiallyProven"),
        ("editor_inspection_model_report.json", "PartiallyProven"),
        ("editor_source_truth_read_report.json", "Passed"),
        ("runtime_read_model_audit_report.json", "PartiallyProven"),
        ("runtime_readiness_gate_report.json", "PartiallyProven"),
        ("package_source_truth_alignment_report.json", "Passed"),
        ("launch_source_truth_alignment_report.json", "PartiallyProven"),
        ("source_truth_scenario_report.json", "PartiallyProven"),
    ]:
        payload: dict[str, object] = {
            "status": status,
            "localOnly": True,
            "networkExposure": "None",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        }
        if filename == "asset_source_manifest_inventory_report.json":
            payload["assetRoots"] = [
                {
                    "assetId": "fixture.assets",
                    "relativePath": "Assets",
                    "kind": "directory",
                    "exists": True,
                    "classification": "PlaceholderAssetRoot",
                    "status": "Placeholder",
                }
            ]
            payload["assetSourceManifests"] = [
                {
                    "manifestId": "fixture.asset-source",
                    "relativePath": "Assets/assets.source.json",
                    "manifestKind": "assets.source.json",
                    "classification": "SourceControlled",
                    "canonical": True,
                    "status": "Passed",
                }
            ]
        write_json(evidence_root / filename, payload)


def write_runtime_readiness_seed_evidence(evidence_root: Path, *, partial: bool = True) -> None:
    evidence_root.mkdir(parents=True, exist_ok=True)
    statuses = {
        "source_truth_inventory_report.json": "PartiallyProven" if partial else "Passed",
        "source_truth_gate_report.json": "PartiallyProven" if partial else "Passed",
        "scene_entity_validation_report.json": "Passed",
        "asset_reference_gate_report.json": "Passed",
        "generated_artifact_freshness_report.json": "PartiallyProven" if partial else "Passed",
        "runtime_readiness_gate_report.json": "PartiallyProven" if partial else "ReadyForPlanning",
        "package_source_truth_alignment_report.json": "Passed",
        "launch_source_truth_alignment_report.json": "PartiallyProven" if partial else "Passed",
        "asset_workflow_closure_report.json": "PartiallyProven" if partial else "SourceTruthFoundationEstablished",
    }
    for filename, status in statuses.items():
        write_json(
            evidence_root / filename,
            {
                "status": status,
                "outcome": status,
                "localOnly": True,
                "networkExposure": "None",
                "mutatesSource": False,
                "enforcement": "ReportOnly",
            },
        )


def assert_report_only_invariants(report: dict[str, object]) -> None:
    assert report["localOnly"] is True
    assert report["networkExposure"] == "None"
    assert report["mutatesSource"] is False
    assert report["enforcement"] == "ReportOnly"


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


def create_runtime_read_seam_evaluation_evidence(root: Path, *, partial: bool = True) -> Path:
    source_truth_root = root / "Build" / "SourceTruth"
    evaluation_root = root / "Build" / "Evaluation"
    write_runtime_readiness_seed_evidence(source_truth_root, partial=partial)
    evaluation_root.mkdir(parents=True, exist_ok=True)

    runtime = evaluation_root / "runtime_readiness_v2_report.json"
    boundary = evaluation_root / "clienthost_evaluation_ownership_boundary_report.json"
    launch = evaluation_root / "client_evaluation_launch_profile_contract_report.json"
    no_network = evaluation_root / "client_evaluation_no_network_plan_report.json"
    diagnostics = evaluation_root / "client_evaluation_diagnostics_shell_report.json"
    blocker = evaluation_root / "client_evaluation_blocker_matrix_report.json"
    runtime_plan = evaluation_root / "minimal_runtime_read_seam_plan_report.json"
    shell_plan = evaluation_root / "minimal_clienthost_evaluation_shell_plan_report.json"
    package_launch = evaluation_root / "package_launch_evaluation_alignment_plan_report.json"
    editorless = evaluation_root / "editorless_evaluation_workflow_plan_report.json"

    assert run_cli("runtime-readiness-v2", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(source_truth_root), "--out", str(runtime)).returncode == 0
    assert run_cli("clienthost-evaluation-ownership-boundary", "--project", str(SAMPLE_PROJECT), "--runtime-readiness-v2", str(runtime), "--out", str(boundary)).returncode == 0
    assert run_cli("client-evaluation-launch-profile-contract", "--project", str(SAMPLE_PROJECT), "--runtime-readiness-v2", str(runtime), "--out", str(launch)).returncode == 0
    assert run_cli("client-evaluation-no-network-plan", "--project", str(SAMPLE_PROJECT), "--out", str(no_network)).returncode == 0
    assert run_cli("client-evaluation-diagnostics-shell", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evaluation_root), "--out", str(diagnostics)).returncode == 0
    assert run_cli("client-evaluation-blocker-matrix", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evaluation_root), "--out", str(blocker)).returncode == 0
    assert run_cli("minimal-runtime-read-seam-plan", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evaluation_root), "--out", str(runtime_plan)).returncode == 0
    assert run_cli("minimal-clienthost-evaluation-shell-plan", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evaluation_root), "--out", str(shell_plan)).returncode == 0
    assert run_cli("package-launch-evaluation-alignment-plan", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evaluation_root), "--out", str(package_launch)).returncode == 0
    assert run_cli("editorless-evaluation-workflow-plan", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evaluation_root), "--out", str(editorless)).returncode == 0
    return evaluation_root


def write_asset_import_cook_source_truth_evidence(evidence_root: Path, *, partial: bool = True) -> None:
    write_runtime_readiness_seed_evidence(evidence_root, partial=partial)
    write_json(
        evidence_root / "asset_source_manifest_inventory_report.json",
        {
            "command": "asset-source-manifest-inventory",
            "status": "Passed",
            "assetRoots": [
                {
                    "assetId": "multiplayer-sandbox.assets",
                    "relativePath": "Assets",
                    "kind": "directory",
                    "exists": True,
                    "classification": "SourceAssetRoot",
                    "status": "Passed",
                }
            ],
            "assetSourceManifests": [
                {
                    "manifestId": "multiplayer-sandbox.asset-source",
                    "relativePath": "Assets/assets.source.json",
                    "manifestKind": "assets.source.json",
                    "classification": "SourceControlledAssetMetadata",
                    "canonical": True,
                    "status": "Accepted",
                }
            ],
            "assetOwners": [
                {
                    "assetId": "multiplayer-sandbox.assets",
                    "owner": "project-assets",
                    "path": "Assets/README.md",
                    "sourceManifest": "Assets/assets.source.json",
                    "classification": "SourceControlledAssetMetadata",
                    "status": "Passed",
                }
            ],
            "generatedArtifacts": [
                {
                    "artifactId": "generated-assets",
                    "path": "Generated/Assets/asset_manifest.json",
                    "classification": "GeneratedNonCanonical",
                    "canonical": False,
                    "status": "Present",
                }
            ],
            "localOnly": True,
            "networkExposure": "None",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )
    write_json(
        evidence_root / "asset_reference_gate_report.json",
        {
            "command": "asset-reference-gate",
            "status": "Passed",
            "resolvedReferences": [
                {
                    "sceneId": "source-truth-foundation",
                    "entityId": "entity-door",
                    "assetId": "multiplayer-sandbox.assets",
                    "owner": "scene-entity",
                    "path": "Assets/README.md",
                    "resolution": "AssetSourceManifest",
                }
            ],
            "unresolvedReferences": [],
            "localOnly": True,
            "networkExposure": "None",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )
    write_json(
        evidence_root / "asset_import_cook_deferral_report.json",
        {
            "command": "asset-import-cook-deferral",
            "status": "PartiallyProven" if partial else "Passed",
            "outcome": "Deferred" if partial else "ReadyForPlanning",
            "deferredPipelines": [
                {
                    "itemId": "asset-import",
                    "kind": "AssetImportPipeline",
                    "classification": "NotImplemented",
                    "status": "Deferred" if partial else "ReadyForPlanning",
                }
            ],
            "localOnly": True,
            "networkExposure": "None",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )
    write_json(
        evidence_root / "generated_artifact_freshness_report.json",
        {
            "command": "generated-freshness-gate",
            "status": "PartiallyProven" if partial else "Passed",
            "generatedArtifacts": [
                {
                    "sceneId": "source-truth-foundation",
                    "artifactId": "source-truth-foundation-projection",
                    "path": "Generated/SourceTruth/source_truth_foundation.generated.json",
                    "expectedSourceHash": "scene-source-hash-v1",
                    "actualSourceHash": "scene-source-hash-v0" if partial else "scene-source-hash-v1",
                    "status": "Stale" if partial else "Fresh",
                }
            ],
            "sourceInputs": [
                {
                    "sceneId": "source-truth-foundation",
                    "sourceHash": "scene-source-hash-v1",
                    "relativePath": "Scenes/source_truth_foundation.scene.json",
                }
            ],
            "freshnessStatus": [
                {
                    "artifactId": "source-truth-foundation-projection",
                    "status": "Stale" if partial else "Fresh",
                }
            ],
            "localOnly": True,
            "networkExposure": "None",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )
    write_json(
        evidence_root / "package_source_truth_alignment_report.json",
        {
            "command": "source-truth-alignment",
            "status": "Passed",
            "localOnly": True,
            "networkExposure": "None",
            "mutatesSource": False,
            "enforcement": "ReportOnly",
        },
    )


def create_asset_import_cook_workflow_evidence(root: Path, *, partial: bool = True) -> tuple[Path, Path]:
    source_truth_root = root / "Build" / "SourceTruth"
    evaluation_root = create_runtime_read_seam_evaluation_evidence(root, partial=partial)
    write_asset_import_cook_source_truth_evidence(source_truth_root, partial=partial)
    assert run_cli(
        "evaluation-asset-import-cook-prerequisite",
        "--project",
        str(SAMPLE_PROJECT),
        "--evidence-root",
        str(source_truth_root),
        "--out",
        str(evaluation_root / "asset_import_cook_prerequisite_report.json"),
    ).returncode == 0
    assert run_cli(
        "runtime-asset-consumption-prerequisite",
        "--project",
        str(SAMPLE_PROJECT),
        "--source-truth-root",
        str(source_truth_root),
        "--evaluation-root",
        str(evaluation_root),
        "--out",
        str(evaluation_root / "runtime_asset_consumption_prerequisite_report.json"),
    ).returncode == 0
    assert run_cli(
        "runtime-projection-freshness-gate",
        "--project",
        str(SAMPLE_PROJECT),
        "--source-truth-root",
        str(source_truth_root),
        "--out",
        str(evaluation_root / "runtime_projection_freshness_gate_report.json"),
    ).returncode == 0
    assert run_cli(
        "evaluation-evidence-gate",
        "--project",
        str(SAMPLE_PROJECT),
        "--evidence-root",
        str(evaluation_root),
        "--out",
        str(evaluation_root / "evaluation_evidence_gate_report.json"),
    ).returncode == 0
    assert run_cli(
        "client-evaluation-regression-fixture-plan",
        "--project",
        str(SAMPLE_PROJECT),
        "--evaluation-root",
        str(evaluation_root),
        "--out",
        str(evaluation_root / "client_evaluation_regression_fixture_plan_report.json"),
    ).returncode == 0
    return source_truth_root, evaluation_root


def test_minimal_runtime_read_seam_plan_is_deterministic_and_planning_only() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evaluation_root = create_runtime_read_seam_evaluation_evidence(root, partial=True)
        first = evaluation_root / "minimal_runtime_read_seam_plan_report.json"
        second = root / "minimal_runtime_read_seam_plan_report_second.json"

        result = run_cli("minimal-runtime-read-seam-plan", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evaluation_root), "--out", str(second))

        assert result.returncode == 0, result.stderr + result.stdout
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        report = load_report(first)
        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["futureSeamName"] == "RuntimeReadSeamV1"
        assert report["runtimeImplementationClaim"] == "NotImplemented"
        assert report["clientHostImplementationClaim"] == "NotImplemented"
        assert report["clientEvaluationImplementationClaim"] == "NotImplemented"
        assert any(item["inputId"] == "validated-project-scene-source-truth" for item in report["firstRuntimeConsumedSourceTruth"])
        assert any(item["classification"] == "EvidenceOnlyWhenFreshOrExplicitlyPartial" for item in report["evidenceOnlyGeneratedProjections"])
        assert "Runtime read seam adapter files" in report["futureTouchCandidates"]
        assert "Server implementation" in report["forbiddenTouchTargets"]


def test_minimal_runtime_read_seam_plan_missing_required_evidence_does_not_pass() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evidence_root = root / "Build" / "Evaluation"
        evidence_root.mkdir(parents=True)
        out = root / "minimal_runtime_read_seam_plan_report.json"

        result = run_cli("minimal-runtime-read-seam-plan", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(out))

        assert result.returncode == 1
        report = load_report(out)
        assert report["readinessStatus"] == "MissingEvidence"
        assert all(item["readinessStatus"] == "MissingEvidence" for item in report["requiredReports"])
        assert_report_only_invariants(report)


def test_minimal_clienthost_evaluation_shell_plan_keeps_clienthost_unimplemented() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evaluation_root = create_runtime_read_seam_evaluation_evidence(root, partial=True)
        out = evaluation_root / "minimal_clienthost_evaluation_shell_plan_report.json"

        report = load_report(out)

        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["futureSeamName"] == "ClientHostEvaluationShellV1"
        assert report["clientHostImplementationClaim"] == "NotImplemented"
        assert report["runtimeImplementationClaim"] == "NotImplemented"
        assert report["clientEvaluationImplementationClaim"] == "NotImplemented"
        assert any(item["responsibility"] == "Local evaluation lifecycle shell" for item in report["clientHostFutureOwnership"])
        assert "scene/entity source truth ownership" in report["outsideClientHost"]
        assert "no server launched" in report["startupDiagnosticsWithoutGameplay"]


def test_package_launch_evaluation_alignment_plan_does_not_stage_or_launch() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evaluation_root = create_runtime_read_seam_evaluation_evidence(root, partial=True)
        out = evaluation_root / "package_launch_evaluation_alignment_plan_report.json"

        report = load_report(out)

        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["requiredLaunchProfileId"] == "client-evaluation-local-no-network"
        assert report["requiredLaunchProfileStatus"] == "DeferredProfileNotImplemented"
        assert report["requiredPackageProfileId"] == "project-readiness-client-evaluation-local"
        assert report["requiredPackageProfileStatus"] == "DeferredProfileNotImplemented"
        assert any(item["classification"] == "ServerHeadlessEvidenceOnly" for item in report["currentLaunchProfiles"])
        assert any(item["classification"] == "ServerHeadlessPackageEvidenceOnly" for item in report["currentPackageProfiles"])
        assert "package staging" in report["deferredWork"]
        assert "LaunchLab execution" in report["deferredWork"]
        assert "server-headless profiles" in report["notRuntimeProof"]


def test_editorless_evaluation_workflow_plan_does_not_claim_playable_editor() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evaluation_root = create_runtime_read_seam_evaluation_evidence(root, partial=True)
        out = evaluation_root / "editorless_evaluation_workflow_plan_report.json"

        report = load_report(out)

        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["clientEvaluationImplementationClaim"] == "NotImplemented"
        assert report["playableEditorClaim"] == "NotClaimed"
        assert report["futureTrigger"] == "Future CLI/report workflow only."
        assert "Playable Editor workflow" in report["impossibleWithoutEditorUi"]
        assert "No Playable Editor is claimed." in report["nonClaims"]
        assert any(item["inputId"] == "evaluation-planning-reports" for item in report["consumedInputs"])


def test_evaluation_evidence_gate_current_partial_missing_and_blocked_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evaluation_root = create_runtime_read_seam_evaluation_evidence(root, partial=True)
        out = evaluation_root / "evaluation_evidence_gate_report.json"

        result = run_cli("evaluation-evidence-gate", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evaluation_root), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["runtimeImplementationClaim"] == "NotImplemented"
        assert report["clientHostImplementationClaim"] == "NotImplemented"
        assert report["clientEvaluationImplementationClaim"] == "NotImplemented"
        assert "ReadyForImplementationPlanning does not mean Client Evaluation is implemented." in report["nonClaims"]

        missing_root = root / "MissingEvaluation"
        missing_root.mkdir()
        missing_out = root / "missing_evaluation_evidence_gate_report.json"
        missing_result = run_cli("evaluation-evidence-gate", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(missing_root), "--out", str(missing_out))
        assert missing_result.returncode == 1
        assert load_report(missing_out)["readinessStatus"] == "MissingEvidence"

        blocked_root = root / "BlockedEvaluation"
        create_runtime_read_seam_evaluation_evidence(root / "BlockedSeed", partial=False)
        shutil.copytree(root / "BlockedSeed" / "Build" / "Evaluation", blocked_root)
        write_json(
            blocked_root / "minimal_runtime_read_seam_plan_report.json",
            {
                "command": "minimal-runtime-read-seam-plan",
                "readinessStatus": "Blocked",
                "status": "Blocked",
                "localOnly": True,
                "networkExposure": "None",
                "mutatesSource": False,
                "enforcement": "ReportOnly",
            },
        )
        blocked_out = root / "blocked_evaluation_evidence_gate_report.json"
        blocked_result = run_cli("evaluation-evidence-gate", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(blocked_root), "--out", str(blocked_out))
        assert blocked_result.returncode == 1
        assert load_report(blocked_out)["readinessStatus"] == "Blocked"


def test_evaluation_evidence_gate_ready_planning_is_not_implementation() -> None:
    required_reports = [
        ("runtime-readiness-v2", "runtime_readiness_v2_report.json"),
        ("clienthost-evaluation-ownership-boundary", "clienthost_evaluation_ownership_boundary_report.json"),
        ("client-evaluation-launch-profile-contract", "client_evaluation_launch_profile_contract_report.json"),
        ("client-evaluation-no-network-plan", "client_evaluation_no_network_plan_report.json"),
        ("client-evaluation-diagnostics-shell", "client_evaluation_diagnostics_shell_report.json"),
        ("client-evaluation-blocker-matrix", "client_evaluation_blocker_matrix_report.json"),
        ("minimal-runtime-read-seam-plan", "minimal_runtime_read_seam_plan_report.json"),
        ("minimal-clienthost-evaluation-shell-plan", "minimal_clienthost_evaluation_shell_plan_report.json"),
        ("package-launch-evaluation-alignment-plan", "package_launch_evaluation_alignment_plan_report.json"),
        ("editorless-evaluation-workflow-plan", "editorless_evaluation_workflow_plan_report.json"),
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
        out = root / "evaluation_evidence_gate_report.json"

        result = run_cli("evaluation-evidence-gate", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(out))

        assert result.returncode == 0, result.stderr + result.stdout
        report = load_report(out)
        assert report["readinessStatus"] == "ReadyForImplementationPlanning"
        assert report["runtimeImplementationClaim"] == "NotImplemented"
        assert report["clientHostImplementationClaim"] == "NotImplemented"
        assert report["clientEvaluationImplementationClaim"] == "NotImplemented"
        assert "ReadyForImplementationPlanning does not mean Client Evaluation is implemented." in report["nonClaims"]
        assert_report_only_invariants(report)


def test_asset_import_cook_asset_import_cook_prerequisite_is_partial_and_evidence_only() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        evidence_root = root / "Build" / "SourceTruth"
        write_asset_import_cook_source_truth_evidence(evidence_root, partial=True)
        first = root / "asset_import_cook_prerequisite_report.json"
        second = root / "asset_import_cook_prerequisite_report_second.json"

        first_result = run_cli("evaluation-asset-import-cook-prerequisite", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(first))
        second_result = run_cli("evaluation-asset-import-cook-prerequisite", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(evidence_root), "--out", str(second))

        assert first_result.returncode == 0, first_result.stderr + first_result.stdout
        assert second_result.returncode == 0, second_result.stderr + second_result.stdout
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        report = load_report(first)
        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["assetImportImplementationClaim"] == "NotImplemented"
        assert report["assetCookImplementationClaim"] == "NotImplemented"
        assert any(item["path"] == "Assets/assets.source.json" and item["canonical"] is True for item in report["assetSourceTruth"])
        assert any(item["classification"] == "EvidenceOnly" and item["canonical"] is False for item in report["evidenceOnlyArtifacts"])
        assert report["acceptedFutureEvaluationReferences"][0]["path"] == "Assets/README.md"
        assert {item["status"] for item in report["missingPipelines"]} == {"NotImplemented"}


def test_runtime_asset_consumption_runtime_asset_consumption_prerequisite_keeps_runtime_unimplemented() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source_truth_root, evaluation_root = create_asset_import_cook_workflow_evidence(root, partial=True)
        out = evaluation_root / "runtime_asset_consumption_prerequisite_report.json"

        report = load_report(out)

        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["runtimeAssetConsumptionSeamClaim"] == "NotImplemented"
        assert report["runtimeImplementationClaim"] == "NotImplemented"
        assert any(item["status"] == "NotImplemented" for item in report["missingRuntimeAssetConsumptionSeams"])
        assert any(item["kind"] == "PackageAssetManifest" for item in report["forbiddenRuntimeAssetInputs"])
        assert report["acceptedAssetReferences"][0]["path"] == "Assets/README.md"
        assert str(source_truth_root) in report["sourceTruthRoot"]


def test_runtime_projection_freshness_runtime_projection_freshness_gate_keeps_projections_noncanonical() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source_truth_root = root / "Build" / "SourceTruth"
        write_asset_import_cook_source_truth_evidence(source_truth_root, partial=True)
        first = root / "runtime_projection_freshness_gate_report.json"
        second = root / "runtime_projection_freshness_gate_report_second.json"

        first_result = run_cli("runtime-projection-freshness-gate", "--project", str(SAMPLE_PROJECT), "--source-truth-root", str(source_truth_root), "--out", str(first))
        second_result = run_cli("runtime-projection-freshness-gate", "--project", str(SAMPLE_PROJECT), "--source-truth-root", str(source_truth_root), "--out", str(second))

        assert first_result.returncode == 0, first_result.stderr + first_result.stdout
        assert second_result.returncode == 0, second_result.stderr + second_result.stdout
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        report = load_report(first)
        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["runtimeImplementationClaim"] == "NotImplemented"
        assert report["runtimeGameplayClaim"] == "NotImplemented"
        assert report["staleGeneratedProjections"][0]["status"] == "Stale"
        assert all(item["canonical"] is False for item in report["nonCanonicalGeneratedProjections"])


def test_client_regression_fixture_client_evaluation_regression_fixture_plan_is_plan_only() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        _, evaluation_root = create_asset_import_cook_workflow_evidence(root, partial=True)
        first = evaluation_root / "client_evaluation_regression_fixture_plan_report.json"
        second = root / "client_evaluation_regression_fixture_plan_report_second.json"

        result = run_cli("client-evaluation-regression-fixture-plan", "--project", str(SAMPLE_PROJECT), "--evaluation-root", str(evaluation_root), "--out", str(second))

        assert result.returncode == 0, result.stderr + result.stdout
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        report = load_report(first)
        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert {item["fixtureId"] for item in report["fixtures"]} == {
            "valid-minimal-scene-entity",
            "stale-projection",
            "missing-asset",
            "no-network-evaluation",
            "clienthost-not-implemented",
            "runtime-read-seam-missing",
        }
        assert all(item["currentStatus"] == "PlanOnly" and item["executionStatus"] == "NotExecuted" for item in report["fixtures"])
        assert "No sockets." in report["forbiddenExecution"]


def test_focused_test_health_evaluation_focused_test_health_gate_preserves_unclaimed_rows() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        _, evaluation_root = create_asset_import_cook_workflow_evidence(root, partial=True)
        first = evaluation_root / "evaluation_focused_test_health_gate_report.json"
        second = root / "evaluation_focused_test_health_gate_report_second.json"

        first_result = run_cli("evaluation-focused-test-health-gate", "--project", str(SAMPLE_PROJECT), "--evaluation-root", str(evaluation_root), "--out", str(first))
        second_result = run_cli("evaluation-focused-test-health-gate", "--project", str(SAMPLE_PROJECT), "--evaluation-root", str(evaluation_root), "--out", str(second))

        assert first_result.returncode == 0, first_result.stderr + first_result.stdout
        assert second_result.returncode == 0, second_result.stderr + second_result.stdout
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")
        report = load_report(first)
        assert_report_only_invariants(report)
        assert report["readinessStatus"] == "PartiallyProven"
        assert report["rawFullCTestClaim"] == "Unclaimed"
        assert report["heavyStressClaim"] == "Unclaimed"
        assert report["realTransportProofClaim"] == "Unclaimed"
        assert any(item["evidenceId"] == "sagaprojectkit-focused-cli-coverage" for item in report["focusedEvidence"])
        assert {"raw-full-ctest", "heavy-stress", "real-transport-proof"}.issubset({item["evidenceId"] for item in report["unclaimedEvidence"]})


def test_asset_import_cook_workflow_missing_and_blocked_evidence_do_not_pass() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        missing_root = root / "Missing"
        missing_root.mkdir()
        missing_out = root / "asset_import_cook_missing.json"
        missing = run_cli("evaluation-asset-import-cook-prerequisite", "--project", str(SAMPLE_PROJECT), "--evidence-root", str(missing_root), "--out", str(missing_out))
        assert missing.returncode == 1
        assert load_report(missing_out)["readinessStatus"] == "MissingEvidence"

        _, evaluation_root = create_asset_import_cook_workflow_evidence(root / "BlockedSeed", partial=False)
        write_json(
            evaluation_root / "runtime_asset_consumption_prerequisite_report.json",
            {
                "command": "runtime-asset-consumption-prerequisite",
                "readinessStatus": "Blocked",
                "status": "Blocked",
                "localOnly": True,
                "networkExposure": "None",
                "mutatesSource": False,
                "enforcement": "ReportOnly",
            },
        )
        blocked_out = root / "client_evaluation_regression_fixture_plan_blocked.json"
        blocked = run_cli("client-evaluation-regression-fixture-plan", "--project", str(SAMPLE_PROJECT), "--evaluation-root", str(evaluation_root), "--out", str(blocked_out))
        assert blocked.returncode == 1
        assert load_report(blocked_out)["readinessStatus"] == "Blocked"


def run_all() -> None:
    tests = [
        test_valid_project_report_passed,
        test_missing_required_field_fails,
        test_unknown_schema_version_fails,
        test_project_relative_path_rules_are_enforced,
        test_missing_referenced_profile_fails,
        test_missing_input_uses_deterministic_exit_code,
        test_invalid_usage_uses_deterministic_exit_code,
        test_project_open_does_not_mutate_manifest,
        test_resolve_output_is_stable_and_ordered,
        test_resolve_slice_reports_visibility_and_report_only_mode,
        test_resolve_slice_rejects_path_traversal_and_unknown_kind,
        test_restricted_resolve_records_hidden_resources_without_changing_validate,
        test_resolve_slice_missing_resource_fails_deterministically,
        test_doctor_detects_missing_scripts_folder,
        test_doctor_detects_invalid_profile_json,
        test_multiplayer_sandbox_validate_resolve_doctor_pass,
        test_multiplayer_sandbox_sample_slice_resolves,
        test_source_truth_inventory_classifies_scene_entity_and_generated_artifacts,
        test_source_truth_gate_and_opening_are_report_only_and_partial,
        test_source_truth_inventory_diagnoses_missing_scene_truth,
        test_source_truth_inventory_diagnoses_missing_asset_reference_owner,
        test_asset_source_manifest_inventory_detects_declared_root_and_assets_source,
        test_asset_source_manifest_inventory_diagnoses_placeholder_root,
        test_asset_source_manifest_inventory_classifies_generated_manifest_noncanonical,
        test_asset_reference_gate_resolves_scene_entity_reference,
        test_asset_reference_gate_diagnoses_missing_owner_and_generated_owner,
        test_generated_freshness_gate_reports_all_statuses_and_is_deterministic,
        test_generated_freshness_gate_rejects_invalid_generated_canonical,
        test_scene_entity_validate_accepts_valid_scene_and_reports_invariants,
        test_scene_entity_validate_diagnoses_duplicate_missing_and_malformed_schema,
        test_scene_entity_validate_diagnoses_missing_asset_owner_and_generated_canonical,
        test_asset_source_manifest_reports_do_not_mutate_source_files,
        test_component_metadata_ownership_accepts_known_components_and_evidence_only_generated,
        test_component_metadata_ownership_diagnoses_unknown_component_type,
        test_editor_inspection_and_read_are_report_only_and_source_backed,
        test_editor_source_truth_read_fails_missing_scene_and_generated_canonical_claim,
        test_runtime_audit_and_readiness_are_planning_only_with_stale_generated_debt,
        test_source_truth_scenario_is_deterministic_and_preserves_client_evaluation_deferral,
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
        test_minimal_runtime_read_seam_plan_is_deterministic_and_planning_only,
        test_minimal_runtime_read_seam_plan_missing_required_evidence_does_not_pass,
        test_minimal_clienthost_evaluation_shell_plan_keeps_clienthost_unimplemented,
        test_package_launch_evaluation_alignment_plan_does_not_stage_or_launch,
        test_editorless_evaluation_workflow_plan_does_not_claim_playable_editor,
        test_evaluation_evidence_gate_current_partial_missing_and_blocked_evidence,
        test_evaluation_evidence_gate_ready_planning_is_not_implementation,
        test_asset_import_cook_asset_import_cook_prerequisite_is_partial_and_evidence_only,
        test_runtime_asset_consumption_runtime_asset_consumption_prerequisite_keeps_runtime_unimplemented,
        test_runtime_projection_freshness_runtime_projection_freshness_gate_keeps_projections_noncanonical,
        test_client_regression_fixture_client_evaluation_regression_fixture_plan_is_plan_only,
        test_focused_test_health_evaluation_focused_test_health_gate_preserves_unclaimed_rows,
        test_asset_import_cook_workflow_missing_and_blocked_evidence_do_not_pass,
    ]
    for test in tests:
        test()
    print(f"{len(tests)} SagaProjectKit CLI tests passed")


if __name__ == "__main__":
    run_all()
