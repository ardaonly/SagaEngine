#!/usr/bin/env python3
"""Focused CLI tests for SagaViewKit."""

from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2].parent
SAGAVIEWKIT = REPO_ROOT / "Tools" / "SagaViewKit" / "sagaviewkit"


def run_cli(*args: str) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagaviewkit-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagaviewkit-nuget")
    env.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
    return subprocess.run(
        [str(SAGAVIEWKIT), *args],
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


def simple_profile(**overrides: object) -> dict:
    profile = {
        "schemaVersion": 1,
        "viewId": "simple-view",
        "viewKind": "Simple",
        "allowedApiDomains": ["Gameplay"],
        "allowedApiLevels": ["High"],
        "canShowOpaqueNodes": True,
        "mustDiscloseOpaqueNodes": True,
        "canEditSupportedNodes": False,
        "canEditUnsupportedNodes": False,
        "canApplyPatch": False,
        "canMutateSource": False,
        "canRegenerateSource": False,
        "canShowSourceLinks": False,
        "canShowDiagnostics": True,
        "canShowCollaborationMetadata": False,
        "requiredEvidenceArtifacts": [],
    }
    profile.update(overrides)
    return profile


def projection(nodes: list[dict], compatibility: str = "Supported") -> dict:
    return {
        "schemaVersion": 1,
        "tool": "sagascript",
        "command": "project-blocks",
        "status": "Passed",
        "apiLevel": "Mixed",
        "apiDomain": "Gameplay",
        "sourceHash": "hash",
        "behaviors": [
            {
                "behaviorId": "behavior://test",
                "apiLevel": "High",
                "apiDomain": "Gameplay",
                "compatibility": compatibility,
                "sourceFile": "Scripts/Test.cs",
                "sourceHash": "hash",
                "nodes": nodes,
            }
        ],
        "summary": {"hasBlockingDiagnostics": False},
        "diagnostics": [],
    }


def slice_resolution(resources: list[dict]) -> dict:
    return {
        "schemaVersion": 1,
        "tool": "sagaproject",
        "command": "restricted-resolve",
        "status": "Passed",
        "project": {"projectId": "fixture-project"},
        "slice": {"sliceId": "designer-local"},
        "resolvedResources": [],
        "restrictedResources": [],
        "resourceVisibility": resources,
        "mutatesSource": False,
        "enforcement": "ReportOnly",
        "diagnostics": [],
    }


def test_emit_profiles_and_validate_simple_profile() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "profiles"
        result = run_cli("emit-profiles", "--out", str(out))
        assert result.returncode == 0, result.stderr + result.stdout
        assert (out / "simple-view.json").exists()

        report = Path(tmp) / "report.json"
        result = run_cli("validate", "--manifest", str(out / "simple-view.json"), "--out", str(report))
        assert result.returncode == 0, result.stderr + result.stdout
        payload = read_json(report)
        assert payload["status"] == "Passed"
        assert payload["profileSummaries"][0]["viewId"] == "simple-view"


def test_invalid_manifest_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest = Path(tmp) / "invalid.json"
        report = Path(tmp) / "report.json"
        write_json(manifest, simple_profile(viewKind="Impossible"))

        result = run_cli("validate", "--manifest", str(manifest), "--out", str(report))
        assert result.returncode == 1
        payload = read_json(report)
        assert payload["status"] == "Failed"
        assert any(v["code"] == "View.Manifest.UnsupportedViewKind" for v in payload["violations"])


def test_simple_view_builtin_profile_accepts_underscore_alias() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        projection_path = root / "projection_report.json"
        report = root / "simple_view_honesty_report.json"
        write_json(projection_path, projection([]))

        result = run_cli(
            "check-simple",
            "--projection",
            str(projection_path),
            "--profile",
            "simple_view",
            "--evidence-root",
            str(root),
            "--out",
            str(report),
        )

        assert result.returncode == 0, result.stderr + result.stdout
        payload = read_json(report)
        assert payload["status"] == "Passed"
        assert payload["viewProfile"]["viewId"] == "simple-view"


def test_simple_view_cannot_claim_advanced_edit_or_source_mutation() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest = Path(tmp) / "simple.json"
        report = Path(tmp) / "report.json"
        write_json(
            manifest,
            simple_profile(
                canEditUnsupportedNodes=True,
                canApplyPatch=True,
                canMutateSource=True,
            ),
        )

        result = run_cli("validate", "--manifest", str(manifest), "--out", str(report))
        assert result.returncode == 1
        codes = {v["code"] for v in read_json(report)["violations"]}
        assert "View.Simple.UnsupportedNodeMarkedEditable" in codes
        assert "View.Simple.SourceMutationCapabilityForbidden" in codes
        assert "View.Simple.PatchApplyCapabilityForbidden" in codes


def test_pro_view_source_link_capability_accepted() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        manifest = Path(tmp) / "pro.json"
        report = Path(tmp) / "report.json"
        write_json(
            manifest,
            {
                "schemaVersion": 1,
                "viewId": "pro-view",
                "viewKind": "Pro",
                "allowedApiDomains": ["Gameplay", "Diagnostics"],
                "allowedApiLevels": ["High", "Low"],
                "canShowOpaqueNodes": True,
                "mustDiscloseOpaqueNodes": True,
                "canEditSupportedNodes": False,
                "canEditUnsupportedNodes": False,
                "canApplyPatch": False,
                "canMutateSource": False,
                "canRegenerateSource": False,
                "canShowSourceLinks": True,
                "canShowDiagnostics": True,
                "canShowCollaborationMetadata": False,
                "requiredEvidenceArtifacts": [],
            },
        )

        result = run_cli("validate", "--manifest", str(manifest), "--out", str(report))
        assert result.returncode == 0, result.stderr + result.stdout
        assert read_json(report)["profileSummaries"][0]["viewKind"] == "Pro"


def test_simple_view_honesty_rejects_editable_unsupported_node() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        profile = root / "simple.json"
        projection_path = root / "projection_report.json"
        report = root / "simple_view_honesty_report.json"
        write_json(profile, simple_profile())
        write_json(
            projection_path,
            projection(
                [
                    {
                        "nodeId": "node.low.editable",
                        "kind": "Opaque",
                        "displayName": "Unsupported",
                        "apiLevel": "Low",
                        "apiDomain": "Gameplay",
                        "readOnly": False,
                        "opaqueReason": "Unsupported C# region is projected as read-only.",
                    }
                ]
            ),
        )

        result = run_cli(
            "check-simple",
            "--projection",
            str(projection_path),
            "--profile",
            str(profile),
            "--out",
            str(report),
        )
        assert result.returncode == 1
        payload = read_json(report)
        assert payload["opaqueNodeCount"] == 1
        assert any(v["code"] == "View.Simple.UnsupportedNodeMarkedEditable" for v in payload["violations"])


def test_simple_view_honesty_requires_opaque_disclosure() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        profile = root / "simple.json"
        projection_path = root / "projection_report.json"
        report = root / "report.json"
        write_json(profile, simple_profile())
        write_json(
            projection_path,
            projection(
                [
                    {
                        "nodeId": "node.low.hidden",
                        "kind": "Operation",
                        "apiLevel": "Low",
                        "apiDomain": "Gameplay",
                        "readOnly": True,
                    }
                ]
            ),
        )

        result = run_cli("check-simple", "--projection", str(projection_path), "--profile", str(profile), "--out", str(report))
        assert result.returncode == 1
        assert any(v["code"] == "View.Simple.OpaqueDisclosureMissing" for v in read_json(report)["violations"])


def test_simple_view_honesty_rejects_hidden_advanced_region_from_node_metadata() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        profile = root / "simple.json"
        projection_path = root / "projection_report.json"
        metadata = root / "node_metadata.json"
        report = root / "report.json"
        write_json(profile, simple_profile())
        write_json(
            projection_path,
            projection(
                [
                    {
                        "nodeId": "node.high",
                        "kind": "Call",
                        "apiLevel": "High",
                        "apiDomain": "Gameplay",
                        "readOnly": True,
                    }
                ]
            ),
        )
        write_json(
            metadata,
            {
                "schemaVersion": 1,
                "tool": "sagascript",
                "command": "project-blocks",
                "nodes": [
                    {
                        "nodeId": "node.low.missing",
                        "displayName": "Low operation",
                        "level": "LowLevel",
                        "domain": "Gameplay",
                        "readOnly": True,
                    }
                ],
            },
        )

        result = run_cli(
            "check-simple",
            "--projection",
            str(projection_path),
            "--profile",
            str(profile),
            "--node-metadata",
            str(metadata),
            "--out",
            str(report),
        )
        assert result.returncode == 1
        assert any(v["code"] == "View.Simple.AdvancedRegionHidden" for v in read_json(report)["violations"])


def test_simple_view_rejects_editable_deferred_node() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        profile = root / "simple.json"
        projection_path = root / "projection_report.json"
        report = root / "report.json"
        write_json(profile, simple_profile())
        write_json(
            projection_path,
            projection(
                [
                    {
                        "nodeId": "Gameplay.Low.Timer.Delay",
                        "kind": "Operation",
                        "apiLevel": "Low",
                        "apiDomain": "Gameplay",
                        "capability": "Deferred",
                        "projectionCompatibility": "Deferred",
                        "readOnly": False,
                        "opaqueReason": "Deferred gameplay node is disclosed as read-only.",
                    }
                ]
            ),
        )

        result = run_cli("check-simple", "--projection", str(projection_path), "--profile", str(profile), "--out", str(report))

        assert result.returncode == 1
        assert any(v["code"] == "View.Simple.UnsupportedNodeMarkedEditable" for v in read_json(report)["violations"])


def test_simple_view_rejects_hidden_deferred_node_metadata() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        profile = root / "simple.json"
        projection_path = root / "projection_report.json"
        metadata = root / "node_metadata.json"
        report = root / "report.json"
        write_json(profile, simple_profile())
        write_json(projection_path, projection([]))
        write_json(
            metadata,
            {
                "schemaVersion": 1,
                "tool": "sagascript",
                "command": "project-blocks",
                "status": "Passed",
                "nodes": [
                    {
                        "nodeId": "Gameplay.Low.Spawn.Entity",
                        "displayName": "Spawn Entity",
                        "apiLevel": "Low",
                        "apiDomain": "Gameplay",
                        "level": "LowLevel",
                        "domain": "Gameplay",
                        "capability": "Deferred",
                        "projectionCompatibility": "Deferred",
                        "readOnly": True,
                    }
                ],
            },
        )

        result = run_cli(
            "check-simple",
            "--projection",
            str(projection_path),
            "--profile",
            str(profile),
            "--node-metadata",
            str(metadata),
            "--out",
            str(report),
        )

        assert result.returncode == 1
        assert any(v["code"] == "View.Simple.AdvancedRegionHidden" for v in read_json(report)["violations"])


def test_report_is_deterministic() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        manifest = root / "simple.json"
        first = root / "first.json"
        second = root / "second.json"
        write_json(manifest, simple_profile())

        assert run_cli("validate", "--manifest", str(manifest), "--out", str(first)).returncode == 0
        assert run_cli("validate", "--manifest", str(manifest), "--out", str(second)).returncode == 0
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")


def test_slice_compatibility_simple_discloses_restricted_counts() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        resolution = root / "restricted_project_resolution_report.json"
        report = root / "view_slice_compatibility_report.json"
        write_json(
            resolution,
            slice_resolution(
                [
                    {
                        "kind": "ScriptFile",
                        "id": "visible",
                        "relativePath": "Scripts/DoorLogic.High.cs",
                        "status": "Included",
                        "visibility": "FullSource",
                    },
                    {
                        "kind": "ScriptFile",
                        "id": "hidden",
                        "relativePath": "",
                        "status": "Restricted",
                        "visibility": "Hidden",
                    },
                ]
            ),
        )

        result = run_cli("slice-compatibility", "--view", "Simple", "--slice-resolution", str(resolution), "--out", str(report))

        assert result.returncode == 0, result.stderr + result.stdout
        payload = read_json(report)
        assert payload["status"] == "Passed"
        assert payload["restrictedCount"] == 1
        assert payload["hiddenCount"] == 1
        assert any(d["code"] == "View.Slice.RestrictionsDisclosed" for d in payload["diagnostics"])


def test_slice_compatibility_csharp_source_blocks_summary_only() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        resolution = root / "restricted_project_resolution_report.json"
        report = root / "view_slice_compatibility_report.json"
        write_json(
            resolution,
            slice_resolution(
                [
                    {
                        "kind": "ScriptFile",
                        "id": "summary",
                        "relativePath": "Scripts/AdvancedUnsupported.cs",
                        "status": "Included",
                        "visibility": "SummaryOnly",
                    }
                ]
            ),
        )

        result = run_cli(
            "slice-compatibility",
            "--view",
            "CSharpSource",
            "--slice-resolution",
            str(resolution),
            "--out",
            str(report),
        )

        assert result.returncode == 1
        payload = read_json(report)
        assert payload["status"] == "Blocked"
        assert payload["decision"] == "Blocked"
        assert payload["mutatesSource"] is False
        assert payload["enforcement"] == "ReportOnly"
        assert any(d["code"] == "View.Slice.CSharpSourceBlocked" for d in payload["diagnostics"])


def test_slice_compatibility_reports_are_deterministic() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        resolution = root / "restricted_project_resolution_report.json"
        first = root / "first.json"
        second = root / "second.json"
        write_json(
            resolution,
            slice_resolution(
                [
                    {
                        "kind": "ScriptFile",
                        "id": "restricted",
                        "relativePath": "",
                        "status": "Restricted",
                        "visibility": "OpaqueRestricted",
                    }
                ]
            ),
        )

        first_result = run_cli("slice-compatibility", "--view", "Pro", "--slice-resolution", str(resolution), "--out", str(first))
        second_result = run_cli("slice-compatibility", "--view", "Pro", "--slice-resolution", str(resolution), "--out", str(second))

        assert first_result.returncode == 0, first_result.stderr + first_result.stdout
        assert second_result.returncode == 0, second_result.stderr + second_result.stdout
        assert first.read_text(encoding="utf-8") == second.read_text(encoding="utf-8")


def main() -> None:
    tests = [
        test_emit_profiles_and_validate_simple_profile,
        test_invalid_manifest_fails,
        test_simple_view_builtin_profile_accepts_underscore_alias,
        test_simple_view_cannot_claim_advanced_edit_or_source_mutation,
        test_pro_view_source_link_capability_accepted,
        test_simple_view_honesty_rejects_editable_unsupported_node,
        test_simple_view_honesty_requires_opaque_disclosure,
        test_simple_view_honesty_rejects_hidden_advanced_region_from_node_metadata,
        test_simple_view_rejects_editable_deferred_node,
        test_simple_view_rejects_hidden_deferred_node_metadata,
        test_report_is_deterministic,
        test_slice_compatibility_simple_discloses_restricted_counts,
        test_slice_compatibility_csharp_source_blocks_summary_only,
        test_slice_compatibility_reports_are_deterministic,
    ]
    for test in tests:
        test()
    print(f"{len(tests)} SagaViewKit CLI tests passed")


if __name__ == "__main__":
    main()
