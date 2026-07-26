# SPDX-License-Identifier: MPL-2.0
"""Shared process and project fixtures for SagaProjectKit CLI tests."""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[5]
PROJECT = REPO_ROOT / "Tools" / "SagaProjectKit" / "SagaProjectKit.csproj"
CLI_ASSEMBLY = PROJECT.parent / "bin" / "Release" / "net10.0" / "sagaproject.dll"
SAMPLE_PROJECT = REPO_ROOT / "Samples" / "MultiplayerSandbox" / "MultiplayerSandbox.sagaproj"
SAMPLE_SLICE = REPO_ROOT / "Samples" / "MultiplayerSandbox" / ".saga" / "slices" / "designer-local.slice.json"
SAGAPACK = REPO_ROOT / "Tools" / "SagaPackager" / "sagapack"
RUNTIME_BRIDGE = REPO_ROOT / "build" / "RelWithDebInfo-0.0.11" / "Managed" / "SagaScript.RuntimeBridge" / "SagaScript.RuntimeBridge.dll"
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
    assert CLI_ASSEMBLY.is_file(), f"missing SagaProjectKit assembly: {CLI_ASSEMBLY}"
    _cli_built = True

def run_cli(*args: str) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagaprojectkit-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagaprojectkit-nuget")
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
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagaprojectkit-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagaprojectkit-nuget")
    env.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
    ensure_cli_built(env)
    return subprocess.run(["dotnet", str(CLI_ASSEMBLY), *args], cwd=REPO_ROOT, env=env,
                          text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)

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
