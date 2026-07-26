# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

from .environment import *

def build_project_blocks(source: Path, out_dir: Path) -> dict:
    result = run_cli("project-blocks", "--source", str(source), "--out", str(out_dir), "--json")
    assert result.returncode == 0, result.stderr + result.stdout
    return json.loads((out_dir / "source_map.json").read_text(encoding="utf-8"))

def run_compatibility_profile(source: Path, out_dir: Path) -> tuple[subprocess.CompletedProcess[str], dict]:
    result = run_cli("compatibility-profile", "--source", str(source), "--out", str(out_dir), "--json")
    report_path = out_dir / "csharp_compatibility_profile_v2.json"
    report = json.loads(report_path.read_text(encoding="utf-8")) if report_path.exists() else {}
    return result, report

def run_project_blocks(source: Path, out_dir: Path) -> tuple[subprocess.CompletedProcess[str], dict]:
    result = run_cli("project-blocks", "--source", str(source), "--out", str(out_dir), "--json")
    report_path = out_dir / "visual_blocks_projection_v1.json"
    report = json.loads(report_path.read_text(encoding="utf-8")) if report_path.exists() else {}
    return result, report

def run_plan_block_edit(projection: Path, operation: Path, report: Path) -> subprocess.CompletedProcess[str]:
    return run_cli(
        "plan-block-edit",
        "--projection",
        str(projection),
        "--operation",
        str(operation),
        "--out",
        str(report),
        "--json",
    )

def run_apply_block_edit(evaluation: Path, source_root: Path, out_dir: Path) -> subprocess.CompletedProcess[str]:
    return run_cli(
        "apply-block-edit",
        "--evaluation",
        str(evaluation),
        "--source-root",
        str(source_root),
        "--out",
        str(out_dir),
        "--json",
    )

def assert_visual_blocks_are_read_only(report: dict) -> None:
    assert report["blocks"]
    assert all(block["editable"] is False for block in report["blocks"])
    assert all(region["editable"] is False for region in report["opaqueRegions"])

def write_block_operation(root: Path, *, operation_kind: str, target_block_id: str, requested_value: str) -> Path:
    operation = root / "block_operation.json"
    operation.write_text(json.dumps({
        "operationKind": operation_kind,
        "targetBlockId": target_block_id,
        "requestedValue": requested_value,
    }), encoding="utf-8")
    return operation

def source_map_node(source_map: dict, *, kind: str, literal_value: str | None = None, display_name: str | None = None) -> dict:
    for behavior in source_map["behaviors"]:
        for node in behavior["nodes"]:
            if node["kind"] != kind:
                continue
            if literal_value is not None and node.get("literalValue") != literal_value:
                continue
            if display_name is not None and node.get("displayName") != display_name:
                continue
            return node
    raise AssertionError(f"source-map node not found: kind={kind} literal={literal_value} display={display_name}")

def span_text(source: Path, node: dict) -> str:
    data = source.read_bytes()
    span = node["sourceSpan"]
    return data[span["startByte"] : span["endByte"]].decode("utf-8")

def write_patch_request(root: Path, source_map: dict, node: dict, replacement: str, **extra: object) -> Path:
    request = root / "patch_request.json"
    payload = {
        "operation": "ReplaceStringLiteral",
        "nodeId": node["nodeId"],
        "baseSourceHash": source_map["files"][0]["sourceHash"],
        "expectedOldText": extra.pop("expectedOldText", None),
        "replacement": replacement,
    }
    if payload["expectedOldText"] is None:
        payload["expectedOldText"] = extra.pop("oldText")
    payload.update(extra)
    request.write_text(json.dumps(payload), encoding="utf-8")
    return request

def run_patch_apply(source_input: Path, source_map_path: Path, request: Path, report: Path) -> subprocess.CompletedProcess[str]:
    return run_cli(
        "patch-apply",
        "--source",
        str(source_input),
        "--source-map",
        str(source_map_path),
        "--request",
        str(request),
        "--out",
        str(report),
        "--json",
    )

def run_patch_diff(source_input: Path, source_map_path: Path, request: Path, report: Path) -> subprocess.CompletedProcess[str]:
    return run_cli(
        "patch-diff",
        "--source",
        str(source_input),
        "--source-map",
        str(source_map_path),
        "--request",
        str(request),
        "--out",
        str(report),
        "--json",
    )

def run_patch_review(diff_report: Path, decision: str, reviewer: str, report: Path) -> subprocess.CompletedProcess[str]:
    return run_cli(
        "patch-review",
        "--diff",
        str(diff_report),
        "--decision",
        decision,
        "--reviewer",
        reviewer,
        "--out",
        str(report),
        "--json",
    )

def run_patch_rollback(apply_report: Path, report: Path) -> subprocess.CompletedProcess[str]:
    return run_cli(
        "patch-rollback",
        "--apply-report",
        str(apply_report),
        "--out",
        str(report),
        "--json",
    )
