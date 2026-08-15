# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

from .support.environment import *
from .support.evidence import *
from .support.templates import *

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
        assert report["summary"]["staleGeneratedEvidence"] == 0
        assert report["generatedArtifacts"][0]["status"] == "MissingGeneratedEvidence"
        assert any(d["code"] == "Project.SourceTruth.GeneratedArtifactMissing" for d in report["diagnostics"])
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
        assert report["freshnessDebt"][0]["status"] == "MissingGenerated"
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
        assert inspection_report["freshnessStatus"][0]["status"] == "MissingGenerated"
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

TESTS = (
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
)
