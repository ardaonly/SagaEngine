# SPDX-License-Identifier: MPL-2.0
"""Runtime UI/input artifact tests kept out of the legacy CLI suite."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[3]
PROJECT = REPO_ROOT / "Tools/SagaPackager/SagaPackager.csproj"
CLI = PROJECT.parent / "bin/Release/net10.0/sagapack.dll"
STARTER = REPO_ROOT / "Samples/StarterArena"
ARENA_RUSH = REPO_ROOT / "Samples/ArenaRush"
BUILD_DIR = Path(os.environ.get(
    "SAGA_BUILD_DIR", REPO_ROOT / "build" / "RelWithDebInfo-0.0.11"))
RUNTIME_BRIDGE = (
    BUILD_DIR / "Managed" / "SagaScript.RuntimeBridge" / "SagaScript.RuntimeBridge.dll")
_built = False


def load(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def run_cli(*arguments: str) -> subprocess.CompletedProcess[str]:
    global _built
    environment = os.environ.copy()
    environment.setdefault("DOTNET_CLI_HOME", "/tmp/sagapack-dotnet-home")
    environment.setdefault("NUGET_PACKAGES", "/tmp/sagapack-nuget")
    environment.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
    if not _built:
        built = subprocess.run(["dotnet", "build", str(PROJECT), "--configuration", "Release",
            "--nologo", "--verbosity", "quiet"], cwd=REPO_ROOT, env=environment,
            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
        assert built.returncode == 0, built.stderr + built.stdout
        _built = True
    return subprocess.run(["dotnet", str(CLI), *arguments], cwd=REPO_ROOT,
        env=environment, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)


def copy_starter(root: Path) -> Path:
    project = root / "StarterArena"
    shutil.copytree(STARTER, project)
    return project


def compile_starter(project: Path, output: Path, command: str) -> subprocess.CompletedProcess[str]:
    return run_cli(command, "--project", str(project / "StarterArena.sagaproj"),
                   "--out", str(output))


def artifact(output: Path, identity: str) -> dict[str, object]:
    entry = next(item for item in load(output / "manifests/current.json")["artifacts"]
                 if item["artifactId"] == identity)
    return load(output / entry["path"])


def hashes(output: Path) -> dict[str, str]:
    return {item["artifactId"]: item["contentHash"]
            for item in load(output / "manifests/current.json")["artifacts"]}


def test_input_rebind_changes_only_input_artifact() -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        project = copy_starter(root)
        before, after = root / "before", root / "after"
        assert compile_starter(project, before, "compile-inputs").returncode == 0
        source_path = project / "Input/Input.saga.json"
        source = load(source_path)
        binding = source["actions"][0]["bindings"][0]
        binding["control"], binding["displayName"] = "F", "Keyboard F"
        write_json(source_path, source)
        assert compile_starter(project, after, "compile-inputs").returncode == 0
        assert artifact(before, "starter-arena.input")["payload"]["actions"][0]["bindings"][0]["displayLabel"] == "E"
        assert artifact(after, "starter-arena.input")["payload"]["actions"][0]["bindings"][0]["displayLabel"] == "F"
        assert sorted(key for key in hashes(before) if hashes(before)[key] != hashes(after)[key]) == ["starter-arena.input"]


def test_runtime_ui_set_is_deterministic_and_strict() -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        first_project, second_project = copy_starter(root / "first"), copy_starter(root / "second")
        first, second = root / "first-output", root / "second-output"
        assert compile_starter(first_project, first, "compile-ui").returncode == 0
        assert compile_starter(second_project, second, "compile-ui").returncode == 0
        assert (first / "manifests/current.json").read_bytes() == (second / "manifests/current.json").read_bytes()
        assert (first / "ui/current.json").read_bytes() == (second / "ui/current.json").read_bytes()
        ui = artifact(first, "starter-arena.hud")
        payload = ui["payload"]
        assert ui["artifactType"] == "runtime.ui-set" and ui["schemaVersion"] == 1
        assert payload["prompts"] == [{"actionId": "Interact", "behaviorId": "behavior://starter-arena/door-controller",
            "elementId": "interaction-prompt", "entityId": "door-root", "falseLabel": "Open Door",
            "stateKey": "door.target-open", "trueLabel": "Close Door", "visibility": "Always"}]
        for collection in ("documents", "stylesheets", "fonts"):
            for resource in payload[collection]:
                assert resource["path"].startswith("ui/generations/")
                assert "/resources/Content/UI/" in resource["path"]
                assert (first / resource["path"]).read_bytes() == (second / resource["path"]).read_bytes()
                assert resource["hash"] == "sha256:" + hashlib.sha256(
                    (first / resource["path"]).read_bytes()).hexdigest()
        font = payload["fonts"][0]
        assert font["hash"] == "sha256:7da195a74c55bef988d0d48f9508bd5d849425c1770dba5d7bfc6ce9ed848954"

def test_runtime_ui_stylesheet_is_external_authority() -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        project = copy_starter(root)
        before, after = root / "before", root / "after"
        assert compile_starter(project, before, "compile-ui").returncode == 0
        style = project / "Content/UI/HUD/interaction-prompt.rcss"
        style.write_text(style.read_text(encoding="utf-8").replace(
            "rgba(12, 18, 28, 210)", "rgba(220, 20, 20, 255)"), encoding="utf-8")
        assert compile_starter(project, after, "compile-ui").returncode == 0
        before_ui = artifact(before, "starter-arena.hud")
        after_ui = artifact(after, "starter-arena.hud")
        assert before_ui["payload"]["documents"][0]["hash"] == \
            after_ui["payload"]["documents"][0]["hash"]
        assert before_ui["payload"]["stylesheets"][0]["hash"] != \
            after_ui["payload"]["stylesheets"][0]["hash"]
        assert before_ui["contentHash"] != after_ui["contentHash"]

        inline = project / "Content/UI/HUD/interaction-prompt.rml"
        inline.write_text(inline.read_text(encoding="utf-8").replace(
            "</head>", "<style>body { color: red; }</style></head>"), encoding="utf-8")
        rejected = compile_starter(project, root / "inline", "compile-ui")
        assert rejected.returncode == 1
        assert "UI.Source.InlineStyleForbidden" in [
            item["code"] for item in load(root / "inline/compile-result.json")["diagnostics"]]

        inline.write_text("""<rml><head><title>HUD</title></head><body/></rml>\n""",
                          encoding="utf-8")
        unreferenced = compile_starter(project, root / "unreferenced", "compile-ui")
        assert unreferenced.returncode == 1
        assert "UI.Source.StylesheetUnreferenced" in [
            item["code"] for item in
            load(root / "unreferenced/compile-result.json")["diagnostics"]]


def test_runtime_ui_failure_preserves_valid_selectors() -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        project, output = copy_starter(root), root / "output"
        assert compile_starter(project, output, "compile-ui").returncode == 0
        manifest, ui = output / "manifests/current.json", output / "ui/current.json"
        before_manifest, before_ui = manifest.read_bytes(), ui.read_bytes()
        (project / "Content/UI/HUD/interaction-prompt.rml").unlink()
        assert compile_starter(project, output, "compile-ui").returncode == 1
        assert manifest.read_bytes() == before_manifest and ui.read_bytes() == before_ui
        assert "Project.Reference.Missing" in [item["code"] for item in
            load(output / "compile-result.json")["diagnostics"]]

def test_arena_rush_assets_inputs_and_composite_ui_compile() -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        project = root / "ArenaRush"
        shutil.copytree(ARENA_RUSH, project, ignore=shutil.ignore_patterns("Build"))
        manifest = project / "ArenaRush.sagaproj"
        for command in ("compile-assets", "compile-inputs", "compile-ui"):
            first = root / f"{command}-first"
            second = root / f"{command}-second"
            assert run_cli(command, "--project", str(manifest), "--out", str(first)).returncode == 0
            assert run_cli(command, "--project", str(manifest), "--out", str(second)).returncode == 0
            assert (first / "manifests/current.json").read_bytes() == (
                second / "manifests/current.json"
            ).read_bytes()
        ui = artifact(root / "compile-ui-first", "arena-rush.ui")
        assert len(ui["payload"]["documents"]) == 3
        assert len(ui["payload"]["stylesheets"]) == 3
        assert len(ui["payload"]["fonts"]) == 1
        assert ui["schemaVersion"] == 2
        assert len(ui["payload"]["textBindings"]) == 9
        assert len(ui["payload"]["inputLabelBindings"]) == 6
        input_artifact = artifact(root / "compile-inputs-first", "arena-rush.input")
        assert [item["actionId"] for item in input_artifact["payload"]["actions"]] == [
            "Exit", "MoveBack", "MoveForward", "MoveLeft", "MoveRight",
            "Restart", "Resume", "StartChallenge",
        ]

def test_arena_rush_multi_surface_package_generation_is_exact() -> None:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        project = root / "ArenaRush"
        shutil.copytree(ARENA_RUSH, project, ignore=shutil.ignore_patterns("Build"))
        output = root / "output"
        result = run_cli(
            "resolve",
            "--project", str(project / "ArenaRush.sagaproj"),
            "--engine-root", str(REPO_ROOT),
            "--lock", str(output / "SagaPackage.lock"),
            "--generated-out", str(output / "generated"),
            "--runtime-bridge", str(RUNTIME_BRIDGE),
            "--out", str(output / "resolve-report.json"),
        )
        assert result.returncode == 0, result.stderr + result.stdout
        lock = load(output / "SagaPackage.lock")
        assert [item["packageId"] for item in lock["packages"]] == [
            "com.saga.gameplay.challenge",
            "com.saga.gameplay.foundation",
        ]
        current = load(output / "generated/manifests/current.json")
        generation = output / "generated/generations" / current["generationId"]
        surfaces = load(generation / "api_surfaces_v1.json")["surfaces"]
        assert [item["artifactId"] for item in surfaces] == [
            "com.saga.gameplay.challenge.challenge-core-api-surface",
            "com.saga.gameplay.challenge.collectibles-api-surface",
            "com.saga.gameplay.foundation.api-surface",
        ]
        payload = json.dumps(lock, sort_keys=True)
        for forbidden in (
            "collectibles.lowlevel", "instrumentation", "ArenaRush", "EnergyCore",
        ):
            assert forbidden not in payload
        descriptors = [
            load(REPO_ROOT / "Engine/Distribution/Packages/"
                 "com.saga.gameplay.challenge/0.0.11/api/"
                 "challenge.core.highlevel.surface.json"),
            load(REPO_ROOT / "Engine/Distribution/Packages/"
                 "com.saga.gameplay.challenge/0.0.11/api/"
                 "collectibles.highlevel.surface.json"),
        ]
        assert [
            api["apiId"] for descriptor in descriptors for api in descriptor["apis"]
        ] == [
            "ChallengeDefinition",
            "ChallengePhase",
            "ChallengeResult",
            "ChallengeState",
            "ChallengeHighLevel.AddProgress",
            "ChallengeHighLevel.AdvanceTime",
            "ChallengeHighLevel.Arm",
            "ChallengeHighLevel.Complete",
            "ChallengeHighLevel.Create",
            "ChallengeHighLevel.CreateResult",
            "ChallengeHighLevel.Reset",
            "ChallengeHighLevel.Restart",
            "ChallengeHighLevel.Resume",
            "ChallengeHighLevel.SetRemainingTime",
            "ChallengeHighLevel.StartOnce",
            "ChallengeHighLevel.Suspend",
            "ChallengeHighLevel.Timeout",
            "CollectibleDefinition",
            "CollectiblePhase",
            "CollectibleState",
            "CollectiblesHighLevel.AdvanceRespawnTime",
            "CollectiblesHighLevel.Collect",
            "CollectiblesHighLevel.CompleteRespawn",
            "CollectiblesHighLevel.Create",
            "CollectiblesHighLevel.Deactivate",
            "CollectiblesHighLevel.Reset",
            "CollectiblesHighLevel.ScheduleRespawn",
            "CollectiblesHighLevel.SelectSpawnIndex",
            "CollectiblesHighLevel.SetRespawnRemaining",
        ]
        assert all(
            descriptor["runtimeReadiness"] == "RuntimeBacked"
            and all(api["blockProjection"] == "Supported" for api in descriptor["apis"])
            for descriptor in descriptors
        )

        engine_root = root / "engine"
        distribution = engine_root / "Engine/Distribution"
        shutil.copytree(REPO_ROOT / "Engine/Distribution", distribution)
        surface_path = (
            distribution / "Packages/com.saga.gameplay.challenge/0.0.11/"
            "api/challenge.core.highlevel.surface.json"
        )
        surface = load(surface_path)
        surface["runtimeReadiness"] = "Imaginary"
        write_json(surface_path, surface)
        rejected = run_cli(
            "resolve",
            "--project", str(project / "ArenaRush.sagaproj"),
            "--engine-root", str(engine_root),
            "--lock", str(root / "bad/SagaPackage.lock"),
            "--generated-out", str(root / "bad/generated"),
            "--runtime-bridge", str(RUNTIME_BRIDGE),
            "--out", str(root / "bad/report.json"),
        )
        assert rejected.returncode == 1
        assert "Package.Resolve.ApiSurfaceInvalid" in [
            item["code"] for item in load(root / "bad/report.json")["diagnostics"]
        ]


TESTS = (test_input_rebind_changes_only_input_artifact,
         test_runtime_ui_set_is_deterministic_and_strict,
         test_runtime_ui_stylesheet_is_external_authority,
         test_runtime_ui_failure_preserves_valid_selectors,
         test_arena_rush_assets_inputs_and_composite_ui_compile,
         test_arena_rush_multi_surface_package_generation_is_exact)
