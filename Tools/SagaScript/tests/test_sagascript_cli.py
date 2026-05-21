#!/usr/bin/env python3
"""Focused CLI tests for the SagaScript toolchain."""

from __future__ import annotations

import json
import os
import subprocess
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2].parent
SAGASCRIPT = REPO_ROOT / "Tools" / "SagaScript" / "sagascript"


def run_cli(*args: str) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagascript-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagascript-nuget")
    env.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
    return subprocess.run(
        [str(SAGASCRIPT), *args],
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


def test_shared_boundary_scan_remains_clean() -> None:
    result = subprocess.run(
        [
            "rg",
            "-n",
            "Roslyn|CoreCLR|Qt|SagaEditor|SagaEngine|SagaServer|Forge|Prism|SDE",
            "Shared/include/SagaShared/Scripting",
        ],
        cwd=REPO_ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )

    assert result.returncode == 1, result.stdout


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
        test_shared_boundary_scan_remains_clean,
    ]
    for test in tests:
        test()
    print(f"{len(tests)} SagaScript CLI tests passed")
