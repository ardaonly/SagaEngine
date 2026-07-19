# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

from .environment import *

def write_metadata_ownership_inputs(root: Path, manifest: Path) -> tuple[Path, Path]:
    source_truth = root / "source_truth_inventory_report.json"
    scene_validation = root / "scene_entity_validation_report.json"
    freshness = root / "generated_artifact_freshness_report.json"
    assert run_cli("source-truth-inventory", "--project", str(manifest), "--out", str(source_truth)).returncode == 0
    assert run_cli("scene-entity-validate", "--project", str(manifest), "--out", str(scene_validation)).returncode == 0
    assert run_cli("generated-freshness-gate", "--project", str(manifest), "--inventory", str(source_truth), "--out", str(freshness)).returncode == 0
    return scene_validation, freshness

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
