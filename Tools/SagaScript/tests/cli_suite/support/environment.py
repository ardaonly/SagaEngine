# SPDX-License-Identifier: MPL-2.0
"""Shared process, path, and source fixtures for SagaScript CLI tests."""

from __future__ import annotations

import json
import hashlib
import os
import shutil
import subprocess
import tempfile
import time
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[5]
PROJECT = REPO_ROOT / "Tools" / "SagaScript" / "SagaScript.csproj"
CLI_ASSEMBLY = PROJECT.parent / "bin" / "Release" / "net10.0" / "sagascript.dll"
RUNTIME_BRIDGE_PROJECT = (
    REPO_ROOT
    / "Engine"
    / "Managed"
    / "SagaScript.RuntimeBridge"
    / "SagaScript.RuntimeBridge.csproj"
)
RUNTIME_BRIDGE_ASSEMBLY = (
    RUNTIME_BRIDGE_PROJECT.parent
    / "bin"
    / "Release"
    / "net10.0"
    / "SagaScript.RuntimeBridge.dll"
)
PACKAGER_PROJECT = REPO_ROOT / "Tools" / "SagaPackager" / "SagaPackager.csproj"
PACKAGER_ASSEMBLY = PACKAGER_PROJECT.parent / "bin" / "Release" / "net10.0" / "sagapack.dll"
STARTER_ROOT = REPO_ROOT / "Samples" / "StarterArena"
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
_packager_built = False



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

    bridge_result = subprocess.run(
        [
            "dotnet",
            "build",
            str(RUNTIME_BRIDGE_PROJECT),
            "--configuration",
            "Release",
            "--nologo",
            "--verbosity",
            "quiet",
        ],
        cwd=REPO_ROOT,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    assert bridge_result.returncode == 0, bridge_result.stderr + bridge_result.stdout
    assert RUNTIME_BRIDGE_ASSEMBLY.is_file(), (
        f"missing SagaScript runtime bridge assembly: {RUNTIME_BRIDGE_ASSEMBLY}"
    )
    env["SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY"] = str(RUNTIME_BRIDGE_ASSEMBLY)
    _cli_built = True

def run_cli(*args: str) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagascript-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagascript-nuget-blockf")
    env.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
    ensure_cli_built(env)
    env["SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY"] = str(RUNTIME_BRIDGE_ASSEMBLY)
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
