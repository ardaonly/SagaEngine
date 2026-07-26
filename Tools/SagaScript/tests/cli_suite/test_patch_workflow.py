# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

from .support.environment import *
from .support.blocks import *
from .support.worker import *

def test_patch_evaluation_replaces_string_literal_without_mutating_source() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        before = source.read_bytes()
        out_dir = root / "Build" / "SagaScript"
        assert run_cli("project-blocks", "--source", str(source), "--out", str(out_dir), "--json").returncode == 0
        source_map = json.loads((out_dir / "source_map.json").read_text(encoding="utf-8"))
        literal = next(
            node
            for behavior in source_map["behaviors"]
            for node in behavior["nodes"]
            if node["kind"] == "StringLiteral" and node["literalValue"] == "key"
        )
        request = root / "patch_request.json"
        request.write_text(json.dumps({
            "operation": "ReplaceStringLiteral",
            "nodeId": literal["nodeId"],
            "baseSourceHash": source_map["files"][0]["sourceHash"],
            "replacement": "gold_key",
        }), encoding="utf-8")
        evaluation = root / "patch_evaluation.json"

        result = run_cli(
            "patch-evaluation",
            "--source",
            str(source),
            "--source-map",
            str(out_dir / "source_map.json"),
            "--request",
            str(request),
            "--out",
            str(evaluation),
            "--json",
        )

        assert result.returncode == 0, result.stderr + result.stdout
        assert source.read_bytes() == before
        payload = json.loads(evaluation.read_text(encoding="utf-8"))
        assert payload["mutatesSource"] is False
        assert payload["evaluation"]["oldText"] == '"key"'
        assert payload["evaluation"]["newText"] == '"gold_key"'

def test_patch_evaluation_rejects_stale_hash() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        out_dir = root / "Build" / "SagaScript"
        assert run_cli("project-blocks", "--source", str(source), "--out", str(out_dir), "--json").returncode == 0
        source_map = json.loads((out_dir / "source_map.json").read_text(encoding="utf-8"))
        literal = next(
            node
            for behavior in source_map["behaviors"]
            for node in behavior["nodes"]
            if node["kind"] == "StringLiteral"
        )
        request = root / "patch_request.json"
        request.write_text(json.dumps({
            "operation": "ReplaceStringLiteral",
            "nodeId": literal["nodeId"],
            "baseSourceHash": "stale",
            "replacement": "gold_key",
        }), encoding="utf-8")

        result = run_cli(
            "patch-evaluation",
            "--source",
            str(source),
            "--source-map",
            str(out_dir / "source_map.json"),
            "--request",
            str(request),
            "--out",
            str(root / "patch_evaluation.json"),
            "--json",
        )

        assert result.returncode == 1
        payload = json.loads((root / "patch_evaluation.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in payload["diagnostics"]}
        assert "Script.Patch.SourceHashMismatch" in codes

def test_patch_apply_replaces_string_literal_exact_span_only() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        before = source.read_bytes()
        out_dir = root / "Build" / "SagaScript"
        source_map = build_project_blocks(source, out_dir)
        literal = source_map_node(source_map, kind="StringLiteral", literal_value="key")
        old_text = span_text(source, literal)
        request = write_patch_request(root, source_map, literal, "gold_key", oldText=old_text)
        report_path = root / "patch_apply_report.json"

        result = run_patch_apply(source, out_dir / "source_map.json", request, report_path)

        assert result.returncode == 0, result.stderr + result.stdout
        payload = json.loads(report_path.read_text(encoding="utf-8"))
        after = source.read_bytes()
        start = literal["sourceSpan"]["startByte"]
        end = literal["sourceSpan"]["endByte"]
        assert payload["mutatesSource"] is True
        assert payload["oldText"] == '"key"'
        assert payload["newText"] == '"gold_key"'
        assert payload["rollbackStatus"] == "NotNeeded"
        assert Path(payload["backupPath"]).exists()
        assert before[:start] == after[:start]
        assert before[end:] == after[start + len(b'"gold_key"') :]
        assert b'"gold_key"' in after

def test_patch_apply_preserves_comments_whitespace_and_using_order() -> None:
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
        before_text = source.read_text(encoding="utf-8")
        out_dir = root / "Build" / "SagaScript"
        source_map = build_project_blocks(source, out_dir)
        literal = source_map_node(source_map, kind="StringLiteral", literal_value="key")
        request = write_patch_request(root, source_map, literal, "silver_key", oldText=span_text(source, literal))

        result = run_patch_apply(source, out_dir / "source_map.json", request, root / "patch_apply_report.json")

        assert result.returncode == 0, result.stderr + result.stdout
        after_text = source.read_text(encoding="utf-8")
        assert "using Saga;\nusing Saga.Gameplay.HighLevel;" in after_text
        assert "// keep this comment" in after_text
        assert "Door.Open(door); // keep inline comment" in after_text
        assert after_text == before_text.replace('"key"', '"silver_key"', 1)

def test_patch_apply_rejects_stale_hash_and_preserves_source() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        before = source.read_bytes()
        out_dir = root / "Build" / "SagaScript"
        source_map = build_project_blocks(source, out_dir)
        literal = source_map_node(source_map, kind="StringLiteral", literal_value="key")
        request = write_patch_request(root, source_map, literal, "gold_key", oldText=span_text(source, literal))
        payload = json.loads(request.read_text(encoding="utf-8"))
        payload["baseSourceHash"] = "stale"
        request.write_text(json.dumps(payload), encoding="utf-8")

        result = run_patch_apply(source, out_dir / "source_map.json", request, root / "patch_apply_report.json")

        assert result.returncode == 1
        assert source.read_bytes() == before
        report = json.loads((root / "patch_apply_report.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.Patch.SourceHashMismatch" in codes

def test_patch_apply_rejects_missing_source_file_deterministically() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        out_dir = root / "Build" / "SagaScript"
        source_map = build_project_blocks(source, out_dir)
        literal = source_map_node(source_map, kind="StringLiteral", literal_value="key")
        request = write_patch_request(root, source_map, literal, "gold_key", oldText=span_text(source, literal))
        source.unlink()

        result = run_patch_apply(root, out_dir / "source_map.json", request, root / "patch_apply_report.json")

        assert result.returncode == 1
        report = json.loads((root / "patch_apply_report.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.Patch.SourceFileMissing" in codes
        assert report["status"] == "Failed"

def test_patch_apply_rejects_wrong_expected_old_text_and_preserves_source() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        before = source.read_bytes()
        out_dir = root / "Build" / "SagaScript"
        source_map = build_project_blocks(source, out_dir)
        literal = source_map_node(source_map, kind="StringLiteral", literal_value="key")
        request = write_patch_request(root, source_map, literal, "gold_key", expectedOldText='"wrong"')

        result = run_patch_apply(source, out_dir / "source_map.json", request, root / "patch_apply_report.json")

        assert result.returncode == 1
        assert source.read_bytes() == before
        report = json.loads((root / "patch_apply_report.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.Patch.ExpectedOldTextMismatch" in codes

def test_patch_apply_rejects_readonly_opaque_string_target() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        before = source.read_bytes()
        out_dir = root / "Build" / "SagaScript"
        source_map = build_project_blocks(source, out_dir)
        literal = source_map_node(source_map, kind="StringLiteral", literal_value="key")
        literal["readOnly"] = True
        literal["projectionCompatibility"] = "Opaque"
        literal["opaqueReason"] = "test opaque source-map target"
        (out_dir / "source_map.json").write_text(json.dumps(source_map), encoding="utf-8")
        request = write_patch_request(root, source_map, literal, "gold_key", oldText=span_text(source, literal))

        result = run_patch_apply(source, out_dir / "source_map.json", request, root / "patch_apply_report.json")

        assert result.returncode == 1
        assert source.read_bytes() == before
        report = json.loads((root / "patch_apply_report.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.Patch.ReadOnlyNode" in codes
        assert "Script.Patch.UnsupportedProjectionCompatibility" in codes
        assert "Script.Patch.OpaqueNode" in codes

def test_patch_apply_rejects_deferred_and_non_string_target() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DeferredLow.cs", deferred_low_level_gameplay_source())
        before = source.read_bytes()
        out_dir = root / "Build" / "SagaScript"
        source_map = build_project_blocks(source, out_dir)
        deferred = source_map_node(source_map, kind="Operation", display_name="Spawn.Entity")
        request = write_patch_request(root, source_map, deferred, "box", expectedOldText='Spawn.Entity("crate")')

        result = run_patch_apply(source, out_dir / "source_map.json", request, root / "patch_apply_report.json")

        assert result.returncode == 1
        assert source.read_bytes() == before
        report = json.loads((root / "patch_apply_report.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.Patch.UnsupportedNode" in codes
        assert "Script.Patch.DeferredNode" in codes
        assert "Script.Patch.UnsupportedProjectionCompatibility" in codes

def test_patch_apply_reports_stale_artifacts_without_regenerating() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        out_dir = root / "Build" / "SagaScript"
        source_map = build_project_blocks(source, out_dir)
        literal = source_map_node(source_map, kind="StringLiteral", literal_value="key")
        artifacts = [
            out_dir / "source_map.json",
            out_dir / "projection_report.json",
            out_dir / "node_metadata.json",
        ]
        mtimes = {path: path.stat().st_mtime_ns for path in artifacts}
        request = write_patch_request(root, source_map, literal, "gold_key", oldText=span_text(source, literal))

        result = run_patch_apply(source, out_dir / "source_map.json", request, root / "patch_apply_report.json")

        assert result.returncode == 0, result.stderr + result.stdout
        report = json.loads((root / "patch_apply_report.json").read_text(encoding="utf-8"))
        assert report["artifactRegeneration"] == "NotPerformed"
        assert report["staleArtifacts"] == [
            "source_map.json",
            "projection_report.json",
            "node_metadata.json",
            "runtime_bindings.json",
        ]
        assert {path: path.stat().st_mtime_ns for path in artifacts} == mtimes
        assert not (out_dir / "runtime_bindings.json").exists()

def test_patch_apply_post_check_failure_rolls_back_original_bytes() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        before = source.read_bytes()
        out_dir = root / "Build" / "SagaScript"
        source_map = build_project_blocks(source, out_dir)
        literal = source_map_node(source_map, kind="StringLiteral", literal_value="key")
        request = write_patch_request(
            root,
            source_map,
            literal,
            "gold_key",
            oldText=span_text(source, literal),
            testOnlyForcePostApplyVerificationFailure=True,
        )

        result = run_patch_apply(source, out_dir / "source_map.json", request, root / "patch_apply_report.json")

        assert result.returncode == 1
        assert source.read_bytes() == before
        report = json.loads((root / "patch_apply_report.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.Patch.PostApplyVerificationFailed" in codes
        assert report["rollbackStatus"] == "Restored"
        assert Path(report["backupPath"]).exists()

def test_patch_diff_emits_deterministic_evaluation_without_mutating_source() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        before = source.read_bytes()
        out_dir = root / "Build" / "SagaScript"
        source_map = build_project_blocks(source, out_dir)
        literal = source_map_node(source_map, kind="StringLiteral", literal_value="key")
        request = write_patch_request(root, source_map, literal, "gold_key", oldText=span_text(source, literal))
        report_path = root / "patch_diff_report.json"

        result = run_patch_diff(source, out_dir / "source_map.json", request, report_path)

        assert result.returncode == 0, result.stderr + result.stdout
        assert source.read_bytes() == before
        report = json.loads(report_path.read_text(encoding="utf-8"))
        assert report["command"] == "patch-diff"
        assert report["mutatesSource"] is False
        assert report["oldText"] == '"key"'
        assert report["newText"] == '"gold_key"'
        assert report["byteDiff"]["startByte"] == literal["sourceSpan"]["startByte"]
        lines = report["textDiff"]["hunks"][0]["lines"]
        assert any(line == '-        if (Inventory.Has(player, "key"))' for line in lines)
        assert any(line == '+        if (Inventory.Has(player, "gold_key"))' for line in lines)

def test_patch_diff_rejects_stale_hash_and_preserves_source() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        before = source.read_bytes()
        out_dir = root / "Build" / "SagaScript"
        source_map = build_project_blocks(source, out_dir)
        literal = source_map_node(source_map, kind="StringLiteral", literal_value="key")
        request = write_patch_request(root, source_map, literal, "gold_key", oldText=span_text(source, literal))
        payload = json.loads(request.read_text(encoding="utf-8"))
        payload["baseSourceHash"] = "stale"
        request.write_text(json.dumps(payload), encoding="utf-8")

        result = run_patch_diff(source, out_dir / "source_map.json", request, root / "patch_diff_report.json")

        assert result.returncode == 1
        assert source.read_bytes() == before
        report = json.loads((root / "patch_diff_report.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.Patch.SourceHashMismatch" in codes

def test_patch_diff_rejects_missing_or_wrong_expected_old_text() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        before = source.read_bytes()
        out_dir = root / "Build" / "SagaScript"
        source_map = build_project_blocks(source, out_dir)
        literal = source_map_node(source_map, kind="StringLiteral", literal_value="key")
        request = write_patch_request(root, source_map, literal, "gold_key", oldText=span_text(source, literal))
        payload = json.loads(request.read_text(encoding="utf-8"))
        payload.pop("expectedOldText")
        request.write_text(json.dumps(payload), encoding="utf-8")

        result = run_patch_diff(source, out_dir / "source_map.json", request, root / "missing_expected_diff.json")

        assert result.returncode == 1
        assert source.read_bytes() == before
        report = json.loads((root / "missing_expected_diff.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.Patch.MissingField" in codes

        payload["expectedOldText"] = '"wrong"'
        request.write_text(json.dumps(payload), encoding="utf-8")
        result = run_patch_diff(source, out_dir / "source_map.json", request, root / "wrong_expected_diff.json")

        assert result.returncode == 1
        assert source.read_bytes() == before
        report = json.loads((root / "wrong_expected_diff.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.Patch.ExpectedOldTextMismatch" in codes

def test_patch_diff_rejects_readonly_opaque_deferred_and_non_string_targets() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        before = source.read_bytes()
        out_dir = root / "Build" / "SagaScript"
        source_map = build_project_blocks(source, out_dir)
        literal = source_map_node(source_map, kind="StringLiteral", literal_value="key")
        literal["readOnly"] = True
        literal["projectionCompatibility"] = "Opaque"
        literal["opaqueReason"] = "test opaque source-map target"
        (out_dir / "source_map.json").write_text(json.dumps(source_map), encoding="utf-8")
        request = write_patch_request(root, source_map, literal, "gold_key", oldText=span_text(source, literal))

        result = run_patch_diff(source, out_dir / "source_map.json", request, root / "opaque_diff.json")

        assert result.returncode == 1
        assert source.read_bytes() == before
        report = json.loads((root / "opaque_diff.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.Patch.ReadOnlyNode" in codes
        assert "Script.Patch.OpaqueNode" in codes

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DeferredLow.cs", deferred_low_level_gameplay_source())
        before = source.read_bytes()
        out_dir = root / "Build" / "SagaScript"
        source_map = build_project_blocks(source, out_dir)
        deferred = source_map_node(source_map, kind="Operation", display_name="Spawn.Entity")
        request = write_patch_request(root, source_map, deferred, "box", expectedOldText='Spawn.Entity("crate")')

        result = run_patch_diff(source, out_dir / "source_map.json", request, root / "deferred_diff.json")

        assert result.returncode == 1
        assert source.read_bytes() == before
        report = json.loads((root / "deferred_diff.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.Patch.UnsupportedNode" in codes
        assert "Script.Patch.DeferredNode" in codes

def test_patch_review_approval_and_rejection_are_report_only() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        before = source.read_bytes()
        out_dir = root / "Build" / "SagaScript"
        source_map = build_project_blocks(source, out_dir)
        literal = source_map_node(source_map, kind="StringLiteral", literal_value="key")
        request = write_patch_request(root, source_map, literal, "gold_key", oldText=span_text(source, literal))
        diff_report = root / "patch_diff_report.json"
        assert run_patch_diff(source, out_dir / "source_map.json", request, diff_report).returncode == 0

        approved = root / "patch_review_approved.json"
        rejected = root / "patch_review_rejected.json"
        approved_result = run_patch_review(diff_report, "Approved", "reviewer://case73", approved)
        rejected_result = run_patch_review(diff_report, "Rejected", "reviewer://case73", rejected)

        assert approved_result.returncode == 0, approved_result.stderr + approved_result.stdout
        assert rejected_result.returncode == 0, rejected_result.stderr + rejected_result.stdout
        assert source.read_bytes() == before
        approved_report = json.loads(approved.read_text(encoding="utf-8"))
        rejected_report = json.loads(rejected.read_text(encoding="utf-8"))
        assert approved_report["decision"] == "Approved"
        assert approved_report["mutatesSource"] is False
        assert approved_report["appliesPatch"] is False
        assert rejected_report["decision"] == "Rejected"
        assert rejected_report["mutatesSource"] is False
        assert rejected_report["appliesPatch"] is False

def test_patch_review_rejects_failed_or_malformed_diff_report_and_invalid_decision() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        failed_diff = root / "failed_diff.json"
        failed_diff.write_text(json.dumps({
            "schemaVersion": 1,
            "tool": "sagascript",
            "command": "patch-diff",
            "status": "Failed",
            "mutatesSource": False,
        }), encoding="utf-8")
        result = run_patch_review(failed_diff, "Approved", "reviewer://case73", root / "failed_review.json")

        assert result.returncode == 1
        report = json.loads((root / "failed_review.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.PatchReview.FailedDiffReport" in codes

        malformed = root / "malformed_diff.json"
        malformed.write_text(json.dumps({
            "schemaVersion": 1,
            "tool": "sagascript",
            "command": "patch-apply",
            "status": "Passed",
            "mutatesSource": True,
        }), encoding="utf-8")
        result = run_patch_review(malformed, "Maybe", "reviewer://case73", root / "malformed_review.json")

        assert result.returncode == 1
        report = json.loads((root / "malformed_review.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.PatchReview.InvalidDiffReport" in codes
        assert "Script.PatchReview.MutatingDiffReport" in codes
        assert "Script.PatchReview.InvalidDecision" in codes

def test_patch_rollback_restores_exact_backup_bytes_after_apply() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        before = source.read_bytes()
        out_dir = root / "Build" / "SagaScript"
        source_map = build_project_blocks(source, out_dir)
        literal = source_map_node(source_map, kind="StringLiteral", literal_value="key")
        request = write_patch_request(root, source_map, literal, "gold_key", oldText=span_text(source, literal))
        apply_report = root / "patch_apply_report.json"
        assert run_patch_apply(source, out_dir / "source_map.json", request, apply_report).returncode == 0
        applied = source.read_bytes()
        assert applied != before

        result = run_patch_rollback(apply_report, root / "patch_rollback_report.json")

        assert result.returncode == 0, result.stderr + result.stdout
        assert source.read_bytes() == before
        report = json.loads((root / "patch_rollback_report.json").read_text(encoding="utf-8"))
        assert report["rollbackStatus"] == "Restored"
        assert report["restoresExactPreviousBytes"] is True
        assert report["postRollbackSourceHash"] == report["baseSourceHash"]
        assert report["mutatesSource"] is True

def test_patch_rollback_rejects_current_source_hash_mismatch_and_preserves_source() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        out_dir = root / "Build" / "SagaScript"
        source_map = build_project_blocks(source, out_dir)
        literal = source_map_node(source_map, kind="StringLiteral", literal_value="key")
        request = write_patch_request(root, source_map, literal, "gold_key", oldText=span_text(source, literal))
        apply_report = root / "patch_apply_report.json"
        assert run_patch_apply(source, out_dir / "source_map.json", request, apply_report).returncode == 0
        source.write_text(source.read_text(encoding="utf-8").replace("gold_key", "silver_key"), encoding="utf-8")
        current = source.read_bytes()

        result = run_patch_rollback(apply_report, root / "patch_rollback_report.json")

        assert result.returncode == 1
        assert source.read_bytes() == current
        report = json.loads((root / "patch_rollback_report.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.PatchRollback.SourceHashMismatch" in codes
        assert report["rollbackStatus"] == "NotPerformed"

def test_patch_rollback_rejects_backup_hash_mismatch_and_preserves_source() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        out_dir = root / "Build" / "SagaScript"
        source_map = build_project_blocks(source, out_dir)
        literal = source_map_node(source_map, kind="StringLiteral", literal_value="key")
        request = write_patch_request(root, source_map, literal, "gold_key", oldText=span_text(source, literal))
        apply_report = root / "patch_apply_report.json"
        assert run_patch_apply(source, out_dir / "source_map.json", request, apply_report).returncode == 0
        current = source.read_bytes()
        apply_payload = json.loads(apply_report.read_text(encoding="utf-8"))
        Path(apply_payload["backupPath"]).write_text("not the original source", encoding="utf-8")

        result = run_patch_rollback(apply_report, root / "patch_rollback_report.json")

        assert result.returncode == 1
        assert source.read_bytes() == current
        report = json.loads((root / "patch_rollback_report.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.PatchRollback.BackupHashMismatch" in codes
        assert report["rollbackStatus"] == "NotPerformed"

def test_patch_rollback_rejects_missing_or_failed_apply_inputs() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        missing_result = run_patch_rollback(root / "missing_apply.json", root / "missing_rollback.json")
        assert missing_result.returncode == 1
        missing_report = json.loads((root / "missing_rollback.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in missing_report["diagnostics"]}
        assert "Script.Patch.InputMissing" in codes

        failed_apply = root / "failed_apply.json"
        failed_apply.write_text(json.dumps({
            "schemaVersion": 1,
            "tool": "sagascript",
            "command": "patch-apply",
            "status": "Failed",
            "mutatesSource": True,
            "sourceFile": str(root / "Source.cs"),
            "backupPath": str(root / "Source.cs.saga-backup"),
            "baseSourceHash": "base",
            "newSourceHash": "new",
        }), encoding="utf-8")
        result = run_patch_rollback(failed_apply, root / "failed_apply_rollback.json")
        assert result.returncode == 1
        report = json.loads((root / "failed_apply_rollback.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.PatchRollback.FailedApplyReport" in codes
        assert "Script.PatchRollback.SourceFileMissing" in codes
        assert "Script.PatchRollback.BackupMissing" in codes

def test_patch_rollback_post_check_failure_restores_current_bytes() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        before = source.read_bytes()
        out_dir = root / "Build" / "SagaScript"
        source_map = build_project_blocks(source, out_dir)
        literal = source_map_node(source_map, kind="StringLiteral", literal_value="key")
        request = write_patch_request(root, source_map, literal, "gold_key", oldText=span_text(source, literal))
        apply_report = root / "patch_apply_report.json"
        assert run_patch_apply(source, out_dir / "source_map.json", request, apply_report).returncode == 0
        applied = source.read_bytes()
        assert applied != before
        payload = json.loads(apply_report.read_text(encoding="utf-8"))
        payload["testOnlyForceRollbackVerificationFailure"] = True
        apply_report.write_text(json.dumps(payload), encoding="utf-8")

        result = run_patch_rollback(apply_report, root / "patch_rollback_report.json")

        assert result.returncode == 1
        assert source.read_bytes() == applied
        report = json.loads((root / "patch_rollback_report.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.PatchRollback.PostRollbackVerificationFailed" in codes
        assert report["rollbackStatus"] == "Failed"

def test_patch_rollback_reports_stale_artifacts_without_regenerating() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        out_dir = root / "Build" / "SagaScript"
        source_map = build_project_blocks(source, out_dir)
        artifacts = [
            out_dir / "source_map.json",
            out_dir / "projection_report.json",
            out_dir / "node_metadata.json",
        ]
        literal = source_map_node(source_map, kind="StringLiteral", literal_value="key")
        request = write_patch_request(root, source_map, literal, "gold_key", oldText=span_text(source, literal))
        apply_report = root / "patch_apply_report.json"
        assert run_patch_apply(source, out_dir / "source_map.json", request, apply_report).returncode == 0
        mtimes = {path: path.stat().st_mtime_ns for path in artifacts}

        result = run_patch_rollback(apply_report, root / "patch_rollback_report.json")

        assert result.returncode == 0, result.stderr + result.stdout
        report = json.loads((root / "patch_rollback_report.json").read_text(encoding="utf-8"))
        assert report["artifactRegeneration"] == "NotPerformed"
        assert report["staleArtifacts"] == [
            "source_map.json",
            "projection_report.json",
            "node_metadata.json",
            "runtime_bindings.json",
        ]
        assert {path: path.stat().st_mtime_ns for path in artifacts} == mtimes

TESTS = (
    test_patch_evaluation_replaces_string_literal_without_mutating_source,
    test_patch_evaluation_rejects_stale_hash,
    test_patch_apply_replaces_string_literal_exact_span_only,
    test_patch_apply_preserves_comments_whitespace_and_using_order,
    test_patch_apply_rejects_stale_hash_and_preserves_source,
    test_patch_apply_rejects_missing_source_file_deterministically,
    test_patch_apply_rejects_wrong_expected_old_text_and_preserves_source,
    test_patch_apply_rejects_readonly_opaque_string_target,
    test_patch_apply_rejects_deferred_and_non_string_target,
    test_patch_apply_reports_stale_artifacts_without_regenerating,
    test_patch_apply_post_check_failure_rolls_back_original_bytes,
    test_patch_diff_emits_deterministic_evaluation_without_mutating_source,
    test_patch_diff_rejects_stale_hash_and_preserves_source,
    test_patch_diff_rejects_missing_or_wrong_expected_old_text,
    test_patch_diff_rejects_readonly_opaque_deferred_and_non_string_targets,
    test_patch_review_approval_and_rejection_are_report_only,
    test_patch_review_rejects_failed_or_malformed_diff_report_and_invalid_decision,
    test_patch_rollback_restores_exact_backup_bytes_after_apply,
    test_patch_rollback_rejects_current_source_hash_mismatch_and_preserves_source,
    test_patch_rollback_rejects_backup_hash_mismatch_and_preserves_source,
    test_patch_rollback_rejects_missing_or_failed_apply_inputs,
    test_patch_rollback_post_check_failure_restores_current_bytes,
    test_patch_rollback_reports_stale_artifacts_without_regenerating,
)
