# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

from .support.environment import *
from .support.blocks import *
from .support.worker import *

def test_runtime_bindings_projection_and_source_map_preserve_axes() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        out_dir = root / "Build" / "SagaScript"

        bindings_result = run_cli("emit-bindings", "--source", str(root), "--out", str(out_dir), "--json")
        projection_result = run_cli("project-blocks", "--source", str(root), "--out", str(out_dir), "--json")

        assert bindings_result.returncode == 0, bindings_result.stderr + bindings_result.stdout
        assert projection_result.returncode == 0, projection_result.stderr + projection_result.stdout
        bindings = json.loads((out_dir / "runtime_bindings.json").read_text(encoding="utf-8"))
        projection = json.loads((out_dir / "projection_report.json").read_text(encoding="utf-8"))
        source_map = json.loads((out_dir / "source_map.json").read_text(encoding="utf-8"))
        node_metadata = json.loads((out_dir / "node_metadata.json").read_text(encoding="utf-8"))
        assert bindings["bindings"][0]["apiLevel"] == "High"
        assert bindings["bindings"][0]["apiDomain"] == "Gameplay"
        assert projection["apiLevel"] == "High"
        assert projection["apiDomain"] == "Gameplay"
        assert source_map["behaviors"][0]["apiLevel"] == "High"
        assert any(node["level"] == "HighLevel" and node["domain"] == "Gameplay" for node in node_metadata["nodes"])
        call = next(node for behavior in projection["behaviors"] for node in behavior["nodes"] if node["displayName"] == "Inventory.Has")
        assert call["capability"] == "ProjectionOnly"
        assert call["projectionCompatibility"] == "ReadOnly"
        metadata = next(node for node in node_metadata["nodes"] if node["displayName"] == "Inventory.Has")
        assert metadata["capability"] == "ProjectionOnly"
        assert metadata["projectionCompatibility"] == "ReadOnly"
        assert "RuntimeBacked" not in metadata["capabilities"]
        binding_node = next(node for node in bindings["bindings"][0]["nodes"] if node["kind"] == "Call")
        assert binding_node["capability"] == "ProjectionOnly"
        assert binding_node["projectionCompatibility"] == "ReadOnly"
        assert binding_node["compatibilityClassification"] == "ReadOnlyProjectable"
        assert binding_node["runtimeProof"] == "NotRuntimeBacked"
        assert bindings["bindings"][0]["libraryIds"] == ["library://saga/gameplay/high"]

def test_compatibility_profile_classifies_readonly_editable_and_opaque_constructs() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "Profile.cs", """
using System.Linq;
using Saga;
using Saga.Gameplay.HighLevel;

[SagaBehavior(Id = "behavior://sandbox/profile", Level = SagaApiLevel.High, Domain = SagaApiDomain.Gameplay)]
public sealed class ProfileBehavior
{
    public void Run(Player player, Door door)
    {
        if (Inventory.Has(player, "key") && true)
        {
            Door.Open(door);
            var values = new[] { 1, 2 }.Where(value => value > 0);
        }
    }
}
""")
        before = source.read_bytes()
        out_dir = root / "out"

        result = run_cli("compatibility-profile", "--source", str(source), "--out", str(out_dir), "--json")

        assert result.returncode == 0, result.stderr + result.stdout
        assert source.read_bytes() == before
        report = json.loads((out_dir / "csharp_compatibility_profile_v2.json").read_text(encoding="utf-8"))
        assert report["schemaVersion"] == 2
        assert report["status"] == "Passed"
        assert any(c["kind"] == "SagaBehavior" and c["classification"] == "ReadOnlyProjectable" for c in report["constructs"])
        assert any(
            c["kind"] == "StringLiteral" and
            c["classification"] == "EditableByPatch" and
            c["patchOperation"] == "ReplaceStringLiteral"
            for c in report["constructs"]
        )
        assert any(c["kind"] == "Literal" and c["classification"] != "EditableByPatch" for c in report["constructs"])
        assert any(c["kind"] == "OpaqueRegion" and c["classification"] == "Opaque" for c in report["constructs"])

def test_compatibility_profile_reports_invalid_syntax_and_metadata() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_source(root, "Invalid.cs", """
using Saga;

[SagaBehavior(Id = "behavior://sandbox/invalid", Level = SagaApiLevel.Unsupported, Domain = SagaApiDomain.Gameplay)]
public sealed class InvalidBehavior
{
    public void Run(
}
""")
        out_dir = root / "out"

        result = run_cli("compatibility-profile", "--source", str(root), "--out", str(out_dir), "--json")

        assert result.returncode == 1
        report = json.loads((out_dir / "csharp_compatibility_profile_v2.json").read_text(encoding="utf-8"))
        assert report["status"] == "Failed"
        assert any(c["classification"] == "Invalid" for c in report["constructs"])

def test_csharp_blocks_projectable_fixture_reports_profile_contract() -> None:
    fixture = CSHARP_BLOCKS_FIXTURES / "projectable" / "GameRulesProjectable.cs"
    before = fixture.read_bytes()
    with tempfile.TemporaryDirectory() as tmp:
        result, report = run_compatibility_profile(fixture, Path(tmp) / "out")

        assert result.returncode == 0, result.stderr + result.stdout
        assert fixture.read_bytes() == before
        assert report["status"] == "Passed"
        classifications = {construct["classification"] for construct in report["constructs"]}
        assert "ReadOnlyProjectable" in classifications
        assert classifications <= {"ReadOnlyProjectable", "EditableByPatch"}
        assert any(construct["kind"] == "SagaBehavior" for construct in report["constructs"])
        assert any(construct["kind"] == "Invocation" for construct in report["constructs"])
        assert all(construct.get("sourceSpan") is not None for construct in report["constructs"])

def test_csharp_blocks_partially_projectable_fixture_reports_opaque_regions() -> None:
    fixture = CSHARP_BLOCKS_FIXTURES / "partially_projectable" / "GameRulesPartial.cs"
    before = fixture.read_bytes()
    with tempfile.TemporaryDirectory() as tmp:
        result, report = run_compatibility_profile(fixture, Path(tmp) / "out")

        assert result.returncode == 0, result.stderr + result.stdout
        assert fixture.read_bytes() == before
        assert report["status"] == "Passed"
        classifications = {construct["classification"] for construct in report["constructs"]}
        assert "ReadOnlyProjectable" in classifications
        assert "Opaque" in classifications
        assert "Unsupported" not in classifications
        assert "Invalid" not in classifications
        assert any(
            construct["kind"] == "OpaqueRegion" and construct.get("sourceSpan") is not None
            for construct in report["constructs"]
        )

def test_csharp_blocks_advanced_opaque_fixture_remains_read_only() -> None:
    fixture = CSHARP_BLOCKS_FIXTURES / "advanced_opaque" / "GameRulesAdvancedOpaque.cs"
    before = fixture.read_bytes()
    with tempfile.TemporaryDirectory() as tmp:
        result, report = run_compatibility_profile(fixture, Path(tmp) / "out")

        assert result.returncode == 0, result.stderr + result.stdout
        assert fixture.read_bytes() == before
        assert report["status"] == "Passed"
        opaque_constructs = [
            construct for construct in report["constructs"] if construct["classification"] == "Opaque"
        ]
        assert opaque_constructs
        assert all(construct["editable"] is False for construct in opaque_constructs)
        assert all(construct.get("sourceSpan") is not None for construct in opaque_constructs)
        assert not any(
            construct["classification"] in {"Unsupported", "Invalid"}
            for construct in report["constructs"]
        )

def test_csharp_blocks_unsupported_fixture_fails_with_diagnostics() -> None:
    fixture = CSHARP_BLOCKS_FIXTURES / "unsupported" / "GameRulesUnsupported.cs"
    before = fixture.read_bytes()
    with tempfile.TemporaryDirectory() as tmp:
        result, report = run_compatibility_profile(fixture, Path(tmp) / "out")

        assert result.returncode == 1
        assert fixture.read_bytes() == before
        assert report["status"] == "Failed"
        assert any(construct["classification"] == "Invalid" for construct in report["constructs"])
        assert any(construct["kind"] == "UnsupportedConstruct" for construct in report["constructs"])
        assert any(diagnostic["severity"] == "Error" for diagnostic in report["diagnostics"])

def test_readonly_visual_blocks_projection_projectable_fixture() -> None:
    fixture = CSHARP_BLOCKS_FIXTURES / "projectable" / "GameRulesProjectable.cs"
    before = fixture.read_bytes()
    with tempfile.TemporaryDirectory() as tmp:
        result, report = run_project_blocks(fixture, Path(tmp) / "out")

        assert result.returncode == 0, result.stderr + result.stdout
        assert fixture.read_bytes() == before
        assert report["schemaVersion"] == 1
        assert report["projectionStatus"] == "Passed"
        assert report["sourcePreservation"] == "SourceNotMutated"
        assert "NoVisualBlocksEditor" in report["nonClaims"]
        assert "NoSourceMutation" in report["nonClaims"]
        assert_visual_blocks_are_read_only(report)
        assert any(block["blockKind"] == "ScriptClassBlock" for block in report["blocks"])
        assert any(block["blockKind"] == "CallableMethodBlock" for block in report["blocks"])
        assert all(block["sourceSpan"] is not None for block in report["blocks"])

def test_readonly_visual_blocks_projection_partially_projectable_fixture() -> None:
    fixture = CSHARP_BLOCKS_FIXTURES / "partially_projectable" / "GameRulesPartial.cs"
    before = fixture.read_bytes()
    with tempfile.TemporaryDirectory() as tmp:
        result, report = run_project_blocks(fixture, Path(tmp) / "out")

        assert result.returncode == 0, result.stderr + result.stdout
        assert fixture.read_bytes() == before
        assert report["projectionStatus"] == "Passed"
        assert_visual_blocks_are_read_only(report)
        assert any(block["classification"] == "ReadOnlyProjectable" for block in report["blocks"])
        assert any(block["classification"] == "Opaque" for block in report["blocks"])
        assert any(region["blockKind"] == "OpaqueSourceRegionBlock" for region in report["opaqueRegions"])

def test_readonly_visual_blocks_projection_advanced_opaque_fixture() -> None:
    fixture = CSHARP_BLOCKS_FIXTURES / "advanced_opaque" / "GameRulesAdvancedOpaque.cs"
    before = fixture.read_bytes()
    with tempfile.TemporaryDirectory() as tmp:
        result, report = run_project_blocks(fixture, Path(tmp) / "out")

        assert result.returncode == 0, result.stderr + result.stdout
        assert fixture.read_bytes() == before
        assert report["projectionStatus"] == "Passed"
        assert_visual_blocks_are_read_only(report)
        opaque_blocks = [block for block in report["blocks"] if block["classification"] == "Opaque"]
        assert opaque_blocks
        assert any(region["classification"] == "Opaque" for region in report["opaqueRegions"])

def test_readonly_visual_blocks_projection_unsupported_fixture_reports_diagnostics() -> None:
    fixture = CSHARP_BLOCKS_FIXTURES / "unsupported" / "GameRulesUnsupported.cs"
    before = fixture.read_bytes()
    with tempfile.TemporaryDirectory() as tmp:
        result, report = run_project_blocks(fixture, Path(tmp) / "out")

        assert result.returncode == 1
        assert fixture.read_bytes() == before
        assert report["projectionStatus"] == "Failed"
        assert_visual_blocks_are_read_only(report)
        assert any(block["blockKind"] == "UnsupportedDiagnosticBlock" for block in report["blocks"])
        assert any(region["blockKind"] == "UnsupportedDiagnosticBlock" for region in report["opaqueRegions"])
        assert any(diagnostic["severity"] == "Error" for diagnostic in report["diagnostics"])

def test_validate_artifacts_accepts_consistent_projection_only_evidence() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root / "src", "DoorLogic.High.cs", high_level_gameplay_source())
        artifact_root = root / "artifacts"
        for args in [
            ("extract-nodes", "--source", str(BUILTIN_GAMEPLAY_NODES), "--out", str(artifact_root), "--json"),
            ("project-blocks", "--source", str(source), "--out", str(artifact_root), "--json"),
            ("emit-bindings", "--source", str(source), "--out", str(artifact_root), "--json"),
            ("compatibility-profile", "--source", str(source), "--out", str(artifact_root), "--json"),
        ]:
            result = run_cli(*args)
            assert result.returncode == 0, result.stderr + result.stdout
        validation = root / "script_artifact_validation_report.json"

        result = run_cli("validate-artifacts", "--artifact-root", str(artifact_root), "--out", str(validation), "--json")

        assert result.returncode == 0, result.stderr + result.stdout
        report = json.loads(validation.read_text(encoding="utf-8"))
        assert report["status"] == "Passed"
        assert report["runtimeProofSummary"]["projectionOnly"] > 0
        assert report["runtimeProofSummary"]["runtimeBackedMissingEvidence"] == 0
        assert any(a["kind"] == "compatibilityProfile" and a["status"] == "Passed" for a in report["checkedArtifacts"])

def test_validate_artifacts_blocks_runtime_backed_without_evidence_and_bad_patch_report() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        artifact_root = root / "artifacts"
        source = write_source(root / "src", "DoorLogic.High.cs", high_level_gameplay_source())
        for args in [
            ("extract-nodes", "--source", str(BUILTIN_GAMEPLAY_NODES), "--out", str(artifact_root), "--json"),
            ("project-blocks", "--source", str(source), "--out", str(artifact_root), "--json"),
            ("emit-bindings", "--source", str(source), "--out", str(artifact_root), "--json"),
            ("compatibility-profile", "--source", str(source), "--out", str(artifact_root), "--json"),
        ]:
            assert run_cli(*args).returncode == 0
        bindings_path = artifact_root / "runtime_bindings.json"
        bindings = json.loads(bindings_path.read_text(encoding="utf-8"))
        bindings["bindings"][0]["nodes"][0]["capability"] = "RuntimeBacked"
        bindings["bindings"][0]["nodes"][0]["runtimeProof"] = "RuntimeBackedWithEvidenceMissing"
        bindings_path.write_text(json.dumps(bindings), encoding="utf-8")
        (artifact_root / "patch_evaluation.json").write_text(json.dumps({
            "schemaVersion": 1,
            "tool": "sagascript",
            "command": "patch-evaluation",
            "status": "Passed",
            "mutatesSource": True,
        }), encoding="utf-8")

        result = run_cli(
            "validate-artifacts",
            "--artifact-root",
            str(artifact_root),
            "--out",
            str(root / "script_artifact_validation_report.json"),
            "--json",
        )

        assert result.returncode == 1
        report = json.loads((root / "script_artifact_validation_report.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.Artifact.RuntimeProofMissing" in codes
        assert "Script.Artifact.PatchMutationMismatch" in codes
        assert report["runtimeProofSummary"]["runtimeBackedMissingEvidence"] == 1

def test_project_blocks_discloses_deferred_nodes_as_read_only() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DeferredLow.cs", deferred_low_level_gameplay_source())
        before = source.read_bytes()
        out_dir = root / "Build" / "SagaScript"

        result = run_cli("project-blocks", "--source", str(source), "--out", str(out_dir), "--json")

        assert result.returncode == 0, result.stderr + result.stdout
        assert source.read_bytes() == before
        projection = json.loads((out_dir / "projection_report.json").read_text(encoding="utf-8"))
        deferred_nodes = [
            node
            for behavior in projection["behaviors"]
            for node in behavior["nodes"]
            if node["capability"] == "Deferred"
        ]
        assert deferred_nodes
        assert all(node["readOnly"] is True for node in deferred_nodes)
        assert all(node["projectionCompatibility"] == "Deferred" for node in deferred_nodes)
        assert all(node["opaqueReason"] == "Deferred gameplay node is disclosed as read-only." for node in deferred_nodes)

def test_project_blocks_discloses_low_level_nodes_for_simple_view() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorState.Low.cs", low_level_gameplay_source())
        before = source.read_bytes()
        out_dir = root / "Build" / "SagaScript"

        result = run_cli("project-blocks", "--source", str(source), "--out", str(out_dir), "--json")

        assert result.returncode == 0, result.stderr + result.stdout
        assert source.read_bytes() == before
        projection = json.loads((out_dir / "projection_report.json").read_text(encoding="utf-8"))
        low_nodes = [
            node
            for behavior in projection["behaviors"]
            for node in behavior["nodes"]
            if node["apiLevel"] == "Low"
        ]
        assert low_nodes
        assert all(node["readOnly"] is True for node in low_nodes)
        assert all(node["opaqueReason"] == "Advanced gameplay node is disclosed as read-only for Simple View." for node in low_nodes)

def test_unsupported_code_projects_as_opaque_and_no_edit_preserves_bytes() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "AdvancedUnsupported.cs", """
using System.Linq;
using Saga;
using Saga.Gameplay.HighLevel;

// preserve this comment and spacing
[SagaBehavior(Id = "behavior://sandbox/advanced", Level = SagaApiLevel.High, Domain = SagaApiDomain.Gameplay)]
public sealed class AdvancedUnsupported
{
    public void Run(Player player)
    {
        var values = new[] { "a", "b" }.Where(value => value.Length > 0);
    }
}
""")
        before = source.read_bytes()
        out_dir = root / "Build" / "SagaScript"

        result = run_cli("project-blocks", "--source", str(source), "--out", str(out_dir), "--json")

        assert result.returncode == 0, result.stderr + result.stdout
        assert source.read_bytes() == before
        projection = json.loads((out_dir / "projection_report.json").read_text(encoding="utf-8"))
        nodes = projection["behaviors"][0]["nodes"]
        assert any(node["kind"] == "Opaque" and node["readOnly"] is True for node in nodes)

TESTS = (
    test_runtime_bindings_projection_and_source_map_preserve_axes,
    test_compatibility_profile_classifies_readonly_editable_and_opaque_constructs,
    test_compatibility_profile_reports_invalid_syntax_and_metadata,
    test_csharp_blocks_projectable_fixture_reports_profile_contract,
    test_csharp_blocks_partially_projectable_fixture_reports_opaque_regions,
    test_csharp_blocks_advanced_opaque_fixture_remains_read_only,
    test_csharp_blocks_unsupported_fixture_fails_with_diagnostics,
    test_readonly_visual_blocks_projection_projectable_fixture,
    test_readonly_visual_blocks_projection_partially_projectable_fixture,
    test_readonly_visual_blocks_projection_advanced_opaque_fixture,
    test_readonly_visual_blocks_projection_unsupported_fixture_reports_diagnostics,
    test_validate_artifacts_accepts_consistent_projection_only_evidence,
    test_validate_artifacts_blocks_runtime_backed_without_evidence_and_bad_patch_report,
    test_project_blocks_discloses_deferred_nodes_as_read_only,
    test_project_blocks_discloses_low_level_nodes_for_simple_view,
    test_unsupported_code_projects_as_opaque_and_no_edit_preserves_bytes,
)
