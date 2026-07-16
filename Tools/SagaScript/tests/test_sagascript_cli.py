#!/usr/bin/env python3
"""Focused CLI tests for the SagaScript toolchain."""

from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2].parent
PROJECT = REPO_ROOT / "Tools" / "SagaScript" / "SagaScript.csproj"
CLI_ASSEMBLY = PROJECT.parent / "bin" / "Release" / "net10.0" / "sagascript.dll"
SHARED_SCRIPTING_ROOT = REPO_ROOT / "Shared" / "include" / "SagaShared" / "Scripting"
BUILTIN_GAMEPLAY_NODES = (
    REPO_ROOT
    / "Tools"
    / "SagaScript"
    / "tests"
    / "fixtures"
    / "BuiltinGameplayNodes"
    / "BuiltinGameplayNodes.cs"
)
CSHARP_BLOCKS_FIXTURES = REPO_ROOT / "Tools" / "SagaScript" / "tests" / "fixtures" / "csharp_blocks"
TWO_WAY_AUTHORING_FIXTURES = REPO_ROOT / "Tools" / "SagaScript" / "tests" / "fixtures" / "two_way_authoring"
SHARED_BOUNDARY_FORBIDDEN = (
    "Roslyn",
    "CoreCLR",
    "Qt",
    "SagaEditor",
    "SagaEngine",
    "SagaServer",
    "Forge",
    "Pr" + "ism",
)
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
    assert CLI_ASSEMBLY.is_file(), f"missing SagaScript assembly: {CLI_ASSEMBLY}"
    _cli_built = True


def run_cli(*args: str) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagascript-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagascript-nuget-blockf")
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


def write_source(root: Path, name: str, body: str) -> Path:
    root.mkdir(parents=True, exist_ok=True)
    path = root / name
    path.write_text(body, encoding="utf-8")
    return path


def write_project_manifest(root: Path, schema_version: int = 0) -> Path:
    root.mkdir(parents=True, exist_ok=True)
    path = root / "Project.sagaproj"
    path.write_text(
        json.dumps(
            {
                "schemaVersion": schema_version,
                "projectId": "sagascript-test",
                "displayName": "SagaScript Test",
            }
        ),
        encoding="utf-8",
    )
    return path


def valid_server_source() -> str:
    return """
namespace Game;

using SagaEngine.Scripting;

[SagaScriptId("script://gameplay/inventory")]
public sealed class InventoryScripts : SagaScript
{
    [BlockCallable]
    [BlockCategory("Inventory")]
    [BlockName("Give Item")]
    [ServerOnly]
    [WritesPersistentState]
    [Replicates]
    [RequiresCapability("Gameplay.Inventory.Write")]
    public void GiveItem(string player, int count)
    {
    }
}
"""


def high_level_gameplay_source() -> str:
    return """
namespace Game;

using Saga;
using Saga.Gameplay.HighLevel;

[SagaBehavior(Id = "behavior://sandbox/door-interact", Level = SagaApiLevel.High, Domain = SagaApiDomain.Gameplay)]
public sealed class DoorLogic
{
    public void OnInteract(Player player, Door door)
    {
        if (Inventory.Has(player, "key"))
        {
            Door.Open(door);
            Quest.Complete(player, "open_first_door");
        }
    }
}
"""


def low_level_gameplay_source() -> str:
    return """
namespace Game;

using Saga;
using Saga.Gameplay.LowLevel;

[SagaBehavior(Id = "behavior://sandbox/door-state", Level = SagaApiLevel.Low, Domain = SagaApiDomain.Gameplay)]
public sealed class DoorStateLogic
{
    public void ApplyDoorUnlock(EntityId door, InventorySlot slot)
    {
        if (GameplayOps.QueryInventorySlot(slot, "key"))
        {
            GameplayOps.SetDoorState(door, "open");
            GameplayOps.ApplyItemDelta(slot, "key", -1);
            GameplayOps.EmitGameplayEvent("door.opened");
        }
    }
}
"""


def deferred_low_level_gameplay_source() -> str:
    return """
namespace Game;

using Saga;
using Saga.Gameplay.LowLevel;

[SagaBehavior(Id = "behavior://sandbox/deferred-low", Level = SagaApiLevel.Low, Domain = SagaApiDomain.Gameplay)]
public sealed class DeferredLowLogic
{
    public void Run(EntityId entity)
    {
        Trigger.OnEnter(entity);
        Spawn.Entity("crate");
        Timer.Delay("short");
        Entity.DestroyOrDeactivate(entity);
    }
}
"""


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


def run_compile(source: Path, manifest_dir: Path, artifact_dir: Path, project_root: Path) -> subprocess.CompletedProcess[str]:
    write_project_manifest(project_root)
    return run_cli(
        "compile",
        "--source",
        str(source),
        "--out",
        str(manifest_dir),
        "--artifacts-out",
        str(artifact_dir),
        "--project-root",
        str(project_root),
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


def test_valid_server_binding_emits_manifests() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "Inventory.cs", valid_server_source())
        out_dir = root / "manifests"

        result = run_cli("emit-manifests", "--source", str(source), "--out", str(out_dir), "--json")

        assert result.returncode == 0, result.stderr + result.stdout
        bindings = json.loads((out_dir / "script_bindings.json").read_text(encoding="utf-8"))
        capabilities = json.loads((out_dir / "script_capabilities.json").read_text(encoding="utf-8"))
        diagnostics = json.loads((out_dir / "sagascript_diagnostics.json").read_text(encoding="utf-8"))
        assert bindings["schemaVersion"] == 1
        assert bindings["bindings"][0]["scriptId"] == "script://gameplay/inventory"
        assert bindings["bindings"][0]["authority"] == "ServerOnly"
        assert capabilities["scripts"][0]["requestedCapabilities"] == ["Gameplay.Inventory.Write"]
        assert diagnostics["summary"]["hasBlockingDiagnostics"] is False


def test_valid_server_binding_compiles_script_artifacts() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root / "Scripts", "Inventory.cs", valid_server_source())
        write_project_manifest(root)
        manifest_dir = root / "Build" / "Manifests"
        artifact_dir = root / "Build" / "Artifacts" / "Scripts"
        diagnostics_path = root / "Build" / "Reports" / "sagascript_diagnostics.json"

        result = run_cli(
            "compile",
            "--source",
            str(source),
            "--out",
            str(manifest_dir),
            "--artifacts-out",
            str(artifact_dir),
            "--project-root",
            str(root),
            "--diagnostics",
            str(diagnostics_path),
            "--json",
        )

        assert result.returncode == 0, result.stderr + result.stdout
        artifacts = json.loads((manifest_dir / "script_artifacts.json").read_text(encoding="utf-8"))
        generic = json.loads((manifest_dir / "artifact_manifest.scripts.json").read_text(encoding="utf-8"))
        diagnostics = json.loads(diagnostics_path.read_text(encoding="utf-8"))
        assembly = artifact_dir / "SagaProjectScripts.scripts.dll"
        runtime_config = artifact_dir / "SagaProjectScripts.scripts.runtimeconfig.json"
        assert assembly.exists()
        assert runtime_config.exists()
        assert artifacts["targetFramework"] == "net10.0"
        assert artifacts["artifacts"][0]["artifactId"] == "artifact://scripts/gameplay/inventory"
        assert artifacts["artifacts"][0]["packageDestinationIntent"] == "server"
        assert artifacts["artifacts"][0]["assemblyPath"] == "Build/Artifacts/Scripts/SagaProjectScripts.scripts.dll"
        assert generic["artifacts"][0]["kind"] == "script"
        assert diagnostics["summary"]["hasBlockingDiagnostics"] is False


def test_compile_rejects_unknown_project_schema() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root / "Scripts", "Inventory.cs", valid_server_source())
        write_project_manifest(root, schema_version=1)

        result = run_cli(
            "compile",
            "--source",
            str(source),
            "--project-root",
            str(root),
            "--json",
        )

        assert result.returncode == 2
        assert ".sagaproj requires schemaVersion 0" in result.stderr


def test_missing_authority_fails_validation() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = write_source(Path(tmp), "MissingAuthority.cs", """
[SagaScriptId("script://gameplay/missing-authority")]
public static class Scripts
{
    [BlockCallable]
    [BlockName("Do Thing")]
    [BlockCategory("Gameplay")]
    public static void DoThing() {}
}
""")

        result = run_cli("validate", "--source", str(source), "--json")

        assert result.returncode == 1
        payload = json.loads(result.stdout)
        assert any(d["code"] == "Script.Binding.MissingAuthority" for d in payload["diagnostics"])


def test_persistent_write_requires_server() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = write_source(Path(tmp), "ClientPersistent.cs", """
[SagaScriptId("script://client/persistent")]
public static class Scripts
{
    [BlockCallable]
    [BlockName("Save")]
    [BlockCategory("Client")]
    [ClientOnly]
    [WritesPersistentState]
    public static void Save() {}
}
""")

        result = run_cli("validate", "--source", str(source), "--json")

        assert result.returncode == 1
        payload = json.loads(result.stdout)
        assert any(d["code"] == "Script.Authority.PersistentWriteRequiresServer" for d in payload["diagnostics"])


def test_unsupported_parameter_type_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = write_source(Path(tmp), "UnsupportedParameter.cs", """
[SagaScriptId("script://gameplay/unsupported")]
public static class Scripts
{
    [BlockCallable]
    [BlockName("Spawn")]
    [BlockCategory("Gameplay")]
    [ServerOnly]
    public static void Spawn(object payload) {}
}
""")

        result = run_cli("validate", "--source", str(source), "--json")

        assert result.returncode == 1
        payload = json.loads(result.stdout)
        assert any(d["code"] == "Script.Type.UnsupportedParameter" for d in payload["diagnostics"])


def test_engine_internal_capability_denied_for_project_script() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = write_source(Path(tmp), "EngineInternal.cs", """
[SagaScriptId("script://gameplay/internal")]
public static class Scripts
{
    [BlockCallable]
    [BlockName("Unsafe")]
    [BlockCategory("Gameplay")]
    [ServerOnly]
    [RequiresCapability("EngineInternal.WorldMutation")]
    public static void Unsafe() {}
}
""")

        result = run_cli("validate", "--source", str(source), "--json")

        assert result.returncode == 1
        payload = json.loads(result.stdout)
        assert any(d["code"] == "Script.Capability.EngineInternalDenied" for d in payload["diagnostics"])


def test_duplicate_script_id_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_source(root, "A.cs", valid_server_source())
        write_source(root, "B.cs", valid_server_source().replace("GiveItem", "GrantItem"))

        result = run_cli("validate", "--source", str(root), "--json")

        assert result.returncode == 1
        payload = json.loads(result.stdout)
        assert any(d["code"] == "Script.Identity.Duplicate" for d in payload["diagnostics"])


def test_missing_block_name_and_category_warns_only() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = write_source(Path(tmp), "Warnings.cs", """
[SagaScriptId("script://gameplay/warnings")]
public static class Scripts
{
    [BlockCallable]
    [ServerOnly]
    public static void DoThing() {}
}
""")

        result = run_cli("validate", "--source", str(source), "--json")

        assert result.returncode == 0, result.stdout
        payload = json.loads(result.stdout)
        codes = {d["code"] for d in payload["diagnostics"]}
        assert "Script.Binding.MissingBlockName" in codes
        assert "Script.Binding.MissingBlockCategory" in codes


def test_freeform_round_trip_warning() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        source = write_source(Path(tmp), "Freeform.cs", """
public static class Freeform
{
    public static void Helper() {}
}
""")

        result = run_cli("validate", "--source", str(source), "--json")

        assert result.returncode == 0
        payload = json.loads(result.stdout)
        codes = {d["code"] for d in payload["diagnostics"]}
        assert "Script.Source.NoBlockCallableBindings" in codes
        assert "Script.Source.FreeformNotRoundTrippable" in codes


def test_analyze_classifies_high_and_low_gameplay_axes() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_source(root, "DoorLogic.High.cs", high_level_gameplay_source())
        write_source(root, "DoorState.Low.cs", low_level_gameplay_source())
        out_dir = root / "Build" / "SagaScript"

        result = run_cli("analyze", "--source", str(root), "--out", str(out_dir), "--json")

        assert result.returncode == 0, result.stderr + result.stdout
        analysis = json.loads((out_dir / "analysis_report.json").read_text(encoding="utf-8"))
        diagnostics = json.loads((out_dir / "sagascript_diagnostics.json").read_text(encoding="utf-8"))
        behaviors = {behavior["behaviorId"]: behavior for behavior in analysis["behaviors"]}
        assert behaviors["behavior://sandbox/door-interact"]["apiLevel"] == "High"
        assert behaviors["behavior://sandbox/door-interact"]["apiDomain"] == "Gameplay"
        assert behaviors["behavior://sandbox/door-state"]["apiLevel"] == "Low"
        assert behaviors["behavior://sandbox/door-state"]["apiDomain"] == "Gameplay"
        assert diagnostics["summary"]["hasBlockingDiagnostics"] is False


def test_mixed_level_and_domain_are_diagnostics_not_guesses() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        source = write_source(root, "Mixed.cs", """
using Saga;
using Saga.Gameplay.HighLevel;
using Saga.Server.LowLevel;

[SagaBehavior(Id = "behavior://sandbox/mixed", Level = SagaApiLevel.Low, Domain = SagaApiDomain.Gameplay)]
public sealed class MixedBehavior
{
    public void Run(Player player) {}
}
""")

        result = run_cli("analyze", "--source", str(source), "--out", str(root / "out"), "--json")

        assert result.returncode == 1
        payload = json.loads((root / "out" / "analysis_report.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in payload["diagnostics"]}
        assert "Script.Metadata.MixedApiLevel" in codes
        assert "Script.Metadata.MixedApiDomain" in codes


def test_extract_nodes_detects_valid_sagalibrary() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        out_dir = root / "Build" / "SagaScript"

        result = run_cli("extract-nodes", "--source", str(BUILTIN_GAMEPLAY_NODES), "--out", str(out_dir), "--json")

        assert result.returncode == 0, result.stderr + result.stdout
        report = json.loads((out_dir / "node_library_report.json").read_text(encoding="utf-8"))
        assert report["command"] == "extract-nodes"
        assert report["status"] == "Passed"
        library_ids = {library["libraryId"] for library in report["libraries"]}
        assert "library://saga/gameplay/high" in library_ids
        assert "library://saga/gameplay/low" in library_ids


def test_extract_nodes_detects_gameplay_high_saganode_as_projection_only() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out_dir = Path(tmp) / "out"

        result = run_cli("extract-nodes", "--source", str(BUILTIN_GAMEPLAY_NODES), "--out", str(out_dir), "--json")

        assert result.returncode == 0, result.stderr + result.stdout
        report = json.loads((out_dir / "node_library_report.json").read_text(encoding="utf-8"))
        node = next(node for node in report["nodes"] if node["nodeId"] == "Gameplay.High.Inventory.Has")
        assert node["apiDomain"] == "Gameplay"
        assert node["apiLevel"] == "High"
        assert node["capability"] == "ProjectionOnly"
        assert node["compatibility"] == "MetadataOnly"


def test_extract_nodes_detects_gameplay_low_saganode_as_deferred() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out_dir = Path(tmp) / "out"

        result = run_cli("extract-nodes", "--source", str(BUILTIN_GAMEPLAY_NODES), "--out", str(out_dir), "--json")

        assert result.returncode == 0, result.stderr + result.stdout
        report = json.loads((out_dir / "node_library_report.json").read_text(encoding="utf-8"))
        node = next(node for node in report["nodes"] if node["nodeId"] == "Gameplay.Low.Timer.Delay")
        assert node["apiDomain"] == "Gameplay"
        assert node["apiLevel"] == "Low"
        assert node["capability"] == "Deferred"
        assert node["compatibility"] == "MetadataOnly"


def test_extract_nodes_duplicate_node_id_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_source(root, "Duplicate.cs", """
using Saga;

[SagaLibrary(Id = "library://duplicate", Level = SagaApiLevel.High, Domain = SagaApiDomain.Gameplay)]
public static class DuplicateNodes
{
    [SagaNode(Id = "Gameplay.High.Inventory.Has", Level = SagaApiLevel.High, Domain = SagaApiDomain.Gameplay)]
    public static bool A() => false;

    [SagaNode(Id = "Gameplay.High.Inventory.Has", Level = SagaApiLevel.High, Domain = SagaApiDomain.Gameplay)]
    public static bool B() => false;
}
""")

        result = run_cli("extract-nodes", "--source", str(root), "--out", str(root / "out"), "--json")

        assert result.returncode == 1
        report = json.loads((root / "out" / "node_library_report.json").read_text(encoding="utf-8"))
        assert any(diagnostic["code"] == "Script.Node.DuplicateId" for diagnostic in report["diagnostics"])


def test_extract_nodes_missing_node_id_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_source(root, "MissingNodeId.cs", """
using Saga;

[SagaLibrary(Id = "library://missing-node", Level = SagaApiLevel.High, Domain = SagaApiDomain.Gameplay)]
public static class MissingNodeId
{
    [SagaNode(Level = SagaApiLevel.High, Domain = SagaApiDomain.Gameplay)]
    public static bool Missing() => false;
}
""")

        result = run_cli("extract-nodes", "--source", str(root), "--out", str(root / "out"), "--json")

        assert result.returncode == 1
        report = json.loads((root / "out" / "node_library_report.json").read_text(encoding="utf-8"))
        assert any(diagnostic["code"] == "Script.Node.MissingNodeId" for diagnostic in report["diagnostics"])


def test_extract_nodes_mixed_domain_or_level_fails() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_source(root, "MixedNode.cs", """
using Saga;

[SagaLibrary(Id = "library://mixed-node", Level = SagaApiLevel.High, Domain = SagaApiDomain.Gameplay)]
public static class MixedNode
{
    [SagaNode(Id = "Gameplay.Low.Trigger.OnEnter", Level = SagaApiLevel.Low, Domain = SagaApiDomain.Server)]
    public static void Mixed() {}
}
""")

        result = run_cli("extract-nodes", "--source", str(root), "--out", str(root / "out"), "--json")

        assert result.returncode == 1
        report = json.loads((root / "out" / "node_library_report.json").read_text(encoding="utf-8"))
        codes = {diagnostic["code"] for diagnostic in report["diagnostics"]}
        assert "Script.Node.MixedApiLevel" in codes
        assert "Script.Node.MixedApiDomain" in codes


def test_node_library_report_is_deterministic() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        first = root / "first"
        second = root / "second"

        assert run_cli("extract-nodes", "--source", str(BUILTIN_GAMEPLAY_NODES), "--out", str(first), "--json").returncode == 0
        assert run_cli("extract-nodes", "--source", str(BUILTIN_GAMEPLAY_NODES), "--out", str(second), "--json").returncode == 0

        assert (first / "node_library_report.json").read_text(encoding="utf-8") == (
            second / "node_library_report.json"
        ).read_text(encoding="utf-8")


def test_extract_nodes_does_not_modify_source() -> None:
    before = BUILTIN_GAMEPLAY_NODES.read_bytes()
    with tempfile.TemporaryDirectory() as tmp:
        result = run_cli("extract-nodes", "--source", str(BUILTIN_GAMEPLAY_NODES), "--out", str(Path(tmp) / "out"), "--json")

        assert result.returncode == 0, result.stderr + result.stdout
        assert BUILTIN_GAMEPLAY_NODES.read_bytes() == before


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


def test_shared_boundary_scan_remains_clean() -> None:
    matches: list[str] = []
    for path in sorted(SHARED_SCRIPTING_ROOT.rglob("*")):
        if not path.is_file():
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            text = path.read_text(encoding="utf-8", errors="replace")
        for line_number, line in enumerate(text.splitlines(), start=1):
            if any(token in line for token in SHARED_BOUNDARY_FORBIDDEN):
                relative = path.relative_to(REPO_ROOT)
                matches.append(f"{relative}:{line_number}: {line.strip()}")

    assert not matches, "Forbidden shared-boundary references found:\n" + "\n".join(matches)


if __name__ == "__main__":
    tests = [
        test_valid_server_binding_emits_manifests,
        test_valid_server_binding_compiles_script_artifacts,
        test_missing_authority_fails_validation,
        test_persistent_write_requires_server,
        test_unsupported_parameter_type_fails,
        test_engine_internal_capability_denied_for_project_script,
        test_duplicate_script_id_fails,
        test_missing_block_name_and_category_warns_only,
        test_freeform_round_trip_warning,
        test_analyze_classifies_high_and_low_gameplay_axes,
        test_mixed_level_and_domain_are_diagnostics_not_guesses,
        test_extract_nodes_detects_valid_sagalibrary,
        test_extract_nodes_detects_gameplay_high_saganode_as_projection_only,
        test_extract_nodes_detects_gameplay_low_saganode_as_deferred,
        test_extract_nodes_duplicate_node_id_fails,
        test_extract_nodes_missing_node_id_fails,
        test_extract_nodes_mixed_domain_or_level_fails,
        test_node_library_report_is_deterministic,
        test_extract_nodes_does_not_modify_source,
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
        test_validate_artifacts_accepts_consistent_projection_only_evidence,
        test_validate_artifacts_blocks_runtime_backed_without_evidence_and_bad_patch_report,
        test_project_blocks_discloses_deferred_nodes_as_read_only,
        test_project_blocks_discloses_low_level_nodes_for_simple_view,
        test_unsupported_code_projects_as_opaque_and_no_edit_preserves_bytes,
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
        test_shared_boundary_scan_remains_clean,
    ]
    for test in tests:
        test()
    print(f"{len(tests)} SagaScript CLI tests passed")
