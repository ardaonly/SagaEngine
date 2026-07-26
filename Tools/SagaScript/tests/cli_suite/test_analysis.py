# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

from .support.environment import *
from .support.blocks import *
from .support.worker import *

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
        assert "Project.Manifest.SchemaVersionUnsupported" in result.stderr
        assert "Only .sagaproj schemaVersion 0 is supported." in result.stderr

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

TESTS = (
    test_valid_server_binding_emits_manifests,
    test_valid_server_binding_compiles_script_artifacts,
    test_compile_rejects_unknown_project_schema,
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
)
