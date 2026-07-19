# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

from .support.environment import *
from .support.blocks import *
from .support.worker import *

def test_block_operation_contract_evaluations_string_literal_without_mutating_source() -> None:
    fixture = CSHARP_BLOCKS_FIXTURES / "projectable" / "GameRulesProjectable.cs"
    before = fixture.read_bytes()
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        out_dir = root / "out"
        project_result, projection = run_project_blocks(fixture, out_dir)
        assert project_result.returncode == 0, project_result.stderr + project_result.stdout
        target = next(block for block in projection["blocks"] if block["classification"] == "EditableByPatch")
        operation = write_block_operation(
            root,
            operation_kind="StringLiteralEdit",
            target_block_id=target["blockId"],
            requested_value="gold_key",
        )
        report_path = root / "block_patch_evaluation_v1.json"

        result = run_plan_block_edit(out_dir / "visual_blocks_projection_v1.json", operation, report_path)

        assert result.returncode == 0, result.stderr + result.stdout
        assert fixture.read_bytes() == before
        report = json.loads(report_path.read_text(encoding="utf-8"))
        assert report["command"] == "plan-block-edit"
        assert report["status"] == "Passed"
        assert report["mutatesSource"] is False
        assert report["sourcePreservation"] == "SourceNotMutated"
        assert "NoPatchApply" in report["nonClaims"]
        assert "NoSourceMutation" in report["nonClaims"]
        assert report["targetSourceHash"] == target["sourceHash"]
        assert report["targetSourceSpan"] == target["sourceSpan"]
        assert report["patchEvaluation"]["mutatesSource"] is False
        assert report["patchEvaluation"]["replacementKind"] == "MinimalSpanReplacement"
        assert report["patchEvaluation"]["replacementText"] == '"gold_key"'
        assert report["patchEvaluation"]["startByte"] == target["sourceSpan"]["startByte"]
        assert report["patchEvaluation"]["endByte"] == target["sourceSpan"]["endByte"]

def test_block_operation_contract_rejects_forbidden_operation_without_mutating_source() -> None:
    fixture = CSHARP_BLOCKS_FIXTURES / "projectable" / "GameRulesProjectable.cs"
    before = fixture.read_bytes()
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        out_dir = root / "out"
        project_result, projection = run_project_blocks(fixture, out_dir)
        assert project_result.returncode == 0, project_result.stderr + project_result.stdout
        target = next(block for block in projection["blocks"] if block["classification"] == "EditableByPatch")
        operation = write_block_operation(
            root,
            operation_kind="ArbitrarySourceRewrite",
            target_block_id=target["blockId"],
            requested_value="rewrite everything",
        )
        report_path = root / "forbidden_block_patch_evaluation_v1.json"

        result = run_plan_block_edit(out_dir / "visual_blocks_projection_v1.json", operation, report_path)

        assert result.returncode == 1
        assert fixture.read_bytes() == before
        report = json.loads(report_path.read_text(encoding="utf-8"))
        assert report["status"] == "Failed"
        assert report["patchEvaluation"] is None
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.BlockOperation.ForbiddenOperation" in codes

def test_block_operation_contract_rejects_unknown_operation_without_mutating_source() -> None:
    fixture = CSHARP_BLOCKS_FIXTURES / "projectable" / "GameRulesProjectable.cs"
    before = fixture.read_bytes()
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        out_dir = root / "out"
        project_result, projection = run_project_blocks(fixture, out_dir)
        assert project_result.returncode == 0, project_result.stderr + project_result.stdout
        target = next(block for block in projection["blocks"] if block["classification"] == "EditableByPatch")
        operation = write_block_operation(
            root,
            operation_kind="TeleportBlockEdit",
            target_block_id=target["blockId"],
            requested_value="gold_key",
        )
        report_path = root / "unknown_block_patch_evaluation_v1.json"

        result = run_plan_block_edit(out_dir / "visual_blocks_projection_v1.json", operation, report_path)

        assert result.returncode == 1
        assert fixture.read_bytes() == before
        report = json.loads(report_path.read_text(encoding="utf-8"))
        assert report["status"] == "Failed"
        assert report["patchEvaluation"] is None
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.BlockOperation.UnsupportedOperation" in codes

def test_block_operation_contract_rejects_opaque_block_without_mutating_source() -> None:
    fixture = CSHARP_BLOCKS_FIXTURES / "advanced_opaque" / "GameRulesAdvancedOpaque.cs"
    before = fixture.read_bytes()
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        out_dir = root / "out"
        project_result, projection = run_project_blocks(fixture, out_dir)
        assert project_result.returncode == 0, project_result.stderr + project_result.stdout
        target = next(block for block in projection["blocks"] if block["classification"] == "Opaque")
        operation = write_block_operation(
            root,
            operation_kind="StringLiteralEdit",
            target_block_id=target["blockId"],
            requested_value="gold_key",
        )
        report_path = root / "opaque_block_patch_evaluation_v1.json"

        result = run_plan_block_edit(out_dir / "visual_blocks_projection_v1.json", operation, report_path)

        assert result.returncode == 1
        assert fixture.read_bytes() == before
        report = json.loads(report_path.read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.BlockOperation.OpaqueRegionReadOnly" in codes
        assert "Script.BlockOperation.NotPatchCapable" in codes

def test_block_operation_contract_rejects_unsupported_block_without_mutating_source() -> None:
    fixture = CSHARP_BLOCKS_FIXTURES / "unsupported" / "GameRulesUnsupported.cs"
    before = fixture.read_bytes()
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        out_dir = root / "out"
        project_result, projection = run_project_blocks(fixture, out_dir)
        assert project_result.returncode == 1
        target = next(block for block in projection["blocks"] if block["blockKind"] == "UnsupportedDiagnosticBlock")
        operation = write_block_operation(
            root,
            operation_kind="StringLiteralEdit",
            target_block_id=target["blockId"],
            requested_value="gold_key",
        )
        report_path = root / "unsupported_block_patch_evaluation_v1.json"

        result = run_plan_block_edit(out_dir / "visual_blocks_projection_v1.json", operation, report_path)

        assert result.returncode == 1
        assert fixture.read_bytes() == before
        report = json.loads(report_path.read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.BlockOperation.UnsupportedDiagnosticReadOnly" in codes
        assert "Script.BlockOperation.NotPatchCapable" in codes

def test_apply_block_edit_writes_patched_copy_without_mutating_source() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", """
namespace Game;

using Saga;
using Saga.Gameplay.HighLevel;

[SagaBehavior(Id = "behavior://sandbox/comments", Level = SagaApiLevel.High, Domain = SagaApiDomain.Gameplay)]
public sealed class CommentedLogic
{
    public void OnInteract(Player player, Door door)
    {
        // keep this comment
        if (Inventory.Has(player, "key"))
        {
            Door.Open(door); // keep inline comment
        }
    }
}
""")
        before = source.read_bytes()
        out_dir = root / "Build" / "SagaScript"
        project_result, projection = run_project_blocks(source, out_dir)
        assert project_result.returncode == 0, project_result.stderr + project_result.stdout
        target = next(
            block for block in projection["blocks"]
            if block["classification"] == "EditableByPatch" and span_text(source, block) == '"key"'
        )
        operation = write_block_operation(
            root,
            operation_kind="StringLiteralEdit",
            target_block_id=target["blockId"],
            requested_value="gold_key",
        )
        evaluation_path = root / "block_patch_evaluation_v1.json"
        evaluation_result = run_plan_block_edit(out_dir / "visual_blocks_projection_v1.json", operation, evaluation_path)
        assert evaluation_result.returncode == 0, evaluation_result.stderr + evaluation_result.stdout
        evaluation = json.loads(evaluation_path.read_text(encoding="utf-8"))

        apply_out = root / "apply-out"
        result = run_apply_block_edit(evaluation_path, root, apply_out)

        assert result.returncode == 0, result.stderr + result.stdout
        assert source.read_bytes() == before
        report_path = apply_out / "block_patch_apply_v1.json"
        report = json.loads(report_path.read_text(encoding="utf-8"))
        patched_source = Path(report["patchedSourceFile"])
        assert patched_source.exists()
        patched = patched_source.read_bytes()
        replacement = b'"gold_key"'
        start = report["targetSourceSpan"]["startByte"]
        end = report["targetSourceSpan"]["endByte"]
        assert report["command"] == "apply-block-edit"
        assert report["status"] == "Passed"
        assert report["mutatesSource"] is False
        assert report["changedSpanOnly"] is True
        assert report["sourcePreservation"] == "CopiedSourceWithSingleSpanReplacement"
        assert report["originalHash"] == evaluation["targetSourceHash"]
        assert report["patchedHash"] != report["originalHash"]
        assert "NoInPlaceMutationByDefault" in report["nonClaims"]
        assert before[:start] == patched[:start]
        assert before[end:] == patched[start + len(replacement):]
        patched_text = patched.decode("utf-8")
        assert '"gold_key"' in patched_text
        assert "using Saga;\nusing Saga.Gameplay.HighLevel;" in patched_text
        assert "// keep this comment" in patched_text
        assert "Door.Open(door); // keep inline comment" in patched_text

def test_apply_block_edit_rejects_failed_evaluation_without_mutating_source() -> None:
    fixture = CSHARP_BLOCKS_FIXTURES / "advanced_opaque" / "GameRulesAdvancedOpaque.cs"
    before = fixture.read_bytes()
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        out_dir = root / "out"
        project_result, projection = run_project_blocks(fixture, out_dir)
        assert project_result.returncode == 0, project_result.stderr + project_result.stdout
        target = next(block for block in projection["blocks"] if block["classification"] == "Opaque")
        operation = write_block_operation(
            root,
            operation_kind="StringLiteralEdit",
            target_block_id=target["blockId"],
            requested_value="gold_key",
        )
        evaluation_path = root / "opaque_block_patch_evaluation_v1.json"
        evaluation_result = run_plan_block_edit(out_dir / "visual_blocks_projection_v1.json", operation, evaluation_path)
        assert evaluation_result.returncode == 1

        apply_out = root / "apply-out"
        result = run_apply_block_edit(evaluation_path, fixture.parent, apply_out)

        assert result.returncode == 1
        assert fixture.read_bytes() == before
        report = json.loads((apply_out / "block_patch_apply_v1.json").read_text(encoding="utf-8"))
        assert report["status"] == "Failed"
        assert report["patchedSourceFile"] == ""
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.BlockApply.EvaluationFailed" in codes

def test_apply_block_edit_rejects_stale_source_hash_without_mutating_source() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        out_dir = root / "Build" / "SagaScript"
        project_result, projection = run_project_blocks(source, out_dir)
        assert project_result.returncode == 0, project_result.stderr + project_result.stdout
        target = next(
            block for block in projection["blocks"]
            if block["classification"] == "EditableByPatch" and span_text(source, block) == '"key"'
        )
        operation = write_block_operation(
            root,
            operation_kind="StringLiteralEdit",
            target_block_id=target["blockId"],
            requested_value="gold_key",
        )
        evaluation_path = root / "block_patch_evaluation_v1.json"
        evaluation_result = run_plan_block_edit(out_dir / "visual_blocks_projection_v1.json", operation, evaluation_path)
        assert evaluation_result.returncode == 0, evaluation_result.stderr + evaluation_result.stdout
        source.write_text(source.read_text(encoding="utf-8").replace('"key"', '"stale_key"', 1), encoding="utf-8")
        before_apply = source.read_bytes()

        apply_out = root / "apply-out"
        result = run_apply_block_edit(evaluation_path, root, apply_out)

        assert result.returncode == 1
        assert source.read_bytes() == before_apply
        report = json.loads((apply_out / "block_patch_apply_v1.json").read_text(encoding="utf-8"))
        assert report["status"] == "Failed"
        assert report["patchedSourceFile"] == ""
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.BlockApply.SourceHashMismatch" in codes

def test_apply_block_edit_rejects_malformed_evaluation_inputs_without_mutating_source() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        before = source.read_bytes()
        out_dir = root / "Build" / "SagaScript"
        project_result, projection = run_project_blocks(source, out_dir)
        assert project_result.returncode == 0, project_result.stderr + project_result.stdout
        target = next(
            block for block in projection["blocks"]
            if block["classification"] == "EditableByPatch" and span_text(source, block) == '"key"'
        )
        operation = write_block_operation(
            root,
            operation_kind="StringLiteralEdit",
            target_block_id=target["blockId"],
            requested_value="gold_key",
        )
        evaluation_path = root / "block_patch_evaluation_v1.json"
        evaluation_result = run_plan_block_edit(out_dir / "visual_blocks_projection_v1.json", operation, evaluation_path)
        assert evaluation_result.returncode == 0, evaluation_result.stderr + evaluation_result.stdout
        valid_evaluation = json.loads(evaluation_path.read_text(encoding="utf-8"))
        span_evaluation = json.loads(json.dumps(valid_evaluation))
        span_evaluation["patchEvaluation"]["startByte"] = span_evaluation["patchEvaluation"]["startByte"] + 1
        evaluation_path.write_text(json.dumps(span_evaluation), encoding="utf-8")

        apply_out = root / "apply-out"
        result = run_apply_block_edit(evaluation_path, root, apply_out)

        assert result.returncode == 1
        assert source.read_bytes() == before
        report = json.loads((apply_out / "block_patch_apply_v1.json").read_text(encoding="utf-8"))
        assert report["status"] == "Failed"
        assert report["patchedSourceFile"] == ""
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.BlockApply.SourceSpanMismatch" in codes

        invalid_replacement = json.loads(json.dumps(valid_evaluation))
        invalid_replacement["patchEvaluation"]["replacementText"] = "not quoted"
        evaluation_path.write_text(json.dumps(invalid_replacement), encoding="utf-8")
        apply_out = root / "apply-out-invalid-replacement"
        result = run_apply_block_edit(evaluation_path, root, apply_out)

        assert result.returncode == 1
        assert source.read_bytes() == before
        report = json.loads((apply_out / "block_patch_apply_v1.json").read_text(encoding="utf-8"))
        assert report["status"] == "Failed"
        assert report["patchedSourceFile"] == ""
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.BlockApply.InvalidReplacement" in codes

def test_two_way_authoring_cli_loop_analyzes_and_compiles_patched_copy() -> None:
    fixture = TWO_WAY_AUTHORING_FIXTURES / "string_literal_edit" / "TwoWayRules.cs"
    before = fixture.read_bytes()
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        profile_out = root / "profile"
        profile_result, profile = run_compatibility_profile(fixture, profile_out)
        assert profile_result.returncode == 0, profile_result.stderr + profile_result.stdout
        assert profile["status"] == "Passed"

        blocks_out = root / "blocks"
        project_result, projection = run_project_blocks(fixture, blocks_out)
        assert project_result.returncode == 0, project_result.stderr + project_result.stdout
        target = next(
            block for block in projection["blocks"]
            if block["classification"] == "EditableByPatch" and span_text(fixture, block) == '"starter_key"'
        )
        operation = write_block_operation(
            root,
            operation_kind="StringLiteralEdit",
            target_block_id=target["blockId"],
            requested_value="case17_key",
        )
        evaluation_path = root / "block_patch_evaluation_v1.json"
        evaluation_result = run_plan_block_edit(blocks_out / "visual_blocks_projection_v1.json", operation, evaluation_path)
        assert evaluation_result.returncode == 0, evaluation_result.stderr + evaluation_result.stdout
        evaluation = json.loads(evaluation_path.read_text(encoding="utf-8"))

        apply_out = root / "apply"
        apply_result = run_apply_block_edit(evaluation_path, fixture, apply_out)
        assert apply_result.returncode == 0, apply_result.stderr + apply_result.stdout
        apply_report = json.loads((apply_out / "block_patch_apply_v1.json").read_text(encoding="utf-8"))
        patched_source = Path(apply_report["patchedSourceFile"])
        assert patched_source.exists()
        assert fixture.read_bytes() == before

        patched = patched_source.read_bytes()
        replacement = b'"case17_key"'
        start = apply_report["targetSourceSpan"]["startByte"]
        end = apply_report["targetSourceSpan"]["endByte"]
        assert apply_report["status"] == "Passed"
        assert apply_report["changedSpanOnly"] is True
        assert apply_report["mutatesSource"] is False
        assert before[:start] == patched[:start]
        assert before[end:] == patched[start + len(replacement):]
        patched_text = patched.decode("utf-8")
        assert '"case17_key"' in patched_text
        assert "using Saga;\nusing SagaEngine.Scripting;" in patched_text
        assert "// preserve this comment" in patched_text

        analyze_out = root / "patched-analyze"
        analyze_result = run_cli("analyze", "--source", str(patched_source), "--out", str(analyze_out), "--json")
        assert analyze_result.returncode == 0, analyze_result.stderr + analyze_result.stdout
        analyze_report = json.loads((analyze_out / "sagascript_diagnostics.json").read_text(encoding="utf-8"))
        assert analyze_report["summary"]["hasBlockingDiagnostics"] is False

        manifest_dir = root / "patched-manifests"
        artifact_dir = root / "patched-artifacts"
        compile_result = run_compile(patched_source, manifest_dir, artifact_dir, root)
        assert compile_result.returncode == 0, compile_result.stderr + compile_result.stdout
        compile_report = json.loads((manifest_dir / "sagascript_diagnostics.json").read_text(encoding="utf-8"))
        artifacts = json.loads((manifest_dir / "script_artifacts.json").read_text(encoding="utf-8"))
        assert compile_report["summary"]["hasBlockingDiagnostics"] is False
        assert artifacts["artifacts"]
        assert (artifact_dir / "SagaProjectScripts.scripts.dll").exists()

        opaque_fixture = CSHARP_BLOCKS_FIXTURES / "advanced_opaque" / "GameRulesAdvancedOpaque.cs"
        opaque_out = root / "opaque-blocks"
        opaque_project, opaque_projection = run_project_blocks(opaque_fixture, opaque_out)
        assert opaque_project.returncode == 0, opaque_project.stderr + opaque_project.stdout
        opaque_target = next(block for block in opaque_projection["blocks"] if block["classification"] == "Opaque")
        (root / "opaque").mkdir()
        opaque_operation = write_block_operation(
            root / "opaque",
            operation_kind="StringLiteralEdit",
            target_block_id=opaque_target["blockId"],
            requested_value="case17_key",
        )
        opaque_evaluation = root / "opaque_block_patch_evaluation_v1.json"
        opaque_evaluation_result = run_plan_block_edit(
            opaque_out / "visual_blocks_projection_v1.json",
            opaque_operation,
            opaque_evaluation,
        )
        assert opaque_evaluation_result.returncode == 1

        authoring_report = {
            "schemaVersion": 1,
            "tool": "sagascript-test",
            "status": "Passed",
            "sourceFile": str(fixture),
            "patchedSourceFile": str(patched_source),
            "operations": [json.loads(operation.read_text(encoding="utf-8"))],
            "profileReport": str(profile_out / "csharp_compatibility_profile_v2.json"),
            "projectionReport": str(blocks_out / "visual_blocks_projection_v1.json"),
            "evaluationReport": str(evaluation_path),
            "applyReport": str(apply_out / "block_patch_apply_v1.json"),
            "patchedAnalyzeResult": str(analyze_out / "sagascript_diagnostics.json"),
            "patchedCompileResult": str(manifest_dir / "script_artifacts.json"),
            "sourcePreservation": "OriginalSourceUnchangedCopiedPatchCompiled",
            "nonClaims": [
                "NoEditorWorkflow",
                "NoFullVisualScripting",
                "NoInPlaceEditing",
                "NoArbitraryCSharpToBlocks",
            ],
            "diagnostics": [],
        }
        report_path = root / "two_way_authoring_report_v1.json"
        report_path.write_text(json.dumps(authoring_report, indent=2) + "\n", encoding="utf-8")
        report = json.loads(report_path.read_text(encoding="utf-8"))
        assert report["status"] == "Passed"
        assert fixture.read_bytes() == before

TESTS = (
    test_block_operation_contract_evaluations_string_literal_without_mutating_source,
    test_block_operation_contract_rejects_forbidden_operation_without_mutating_source,
    test_block_operation_contract_rejects_unknown_operation_without_mutating_source,
    test_block_operation_contract_rejects_opaque_block_without_mutating_source,
    test_block_operation_contract_rejects_unsupported_block_without_mutating_source,
    test_apply_block_edit_writes_patched_copy_without_mutating_source,
    test_apply_block_edit_rejects_failed_evaluation_without_mutating_source,
    test_apply_block_edit_rejects_stale_source_hash_without_mutating_source,
    test_apply_block_edit_rejects_malformed_evaluation_inputs_without_mutating_source,
    test_two_way_authoring_cli_loop_analyzes_and_compiles_patched_copy,
)
