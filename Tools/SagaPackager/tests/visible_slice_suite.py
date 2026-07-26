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
        assert payload["prompts"] == [{"actionId": "Interact", "behaviorId": "StarterArena.DoorController",
            "elementId": "interaction-prompt", "entityId": "door-root", "falseLabel": "Open Door",
            "stateKey": "door.target-open", "trueLabel": "Close Door", "visibility": "Always"}]
        for collection in ("documents", "stylesheets", "fonts"):
            for resource in payload[collection]:
                assert resource["path"].startswith("ui/generations/")
                assert (first / resource["path"]).read_bytes() == (second / resource["path"]).read_bytes()
                assert resource["hash"] == "sha256:" + hashlib.sha256(
                    (first / resource["path"]).read_bytes()).hexdigest()
        font = payload["fonts"][0]
        assert font["hash"] == "sha256:7da195a74c55bef988d0d48f9508bd5d849425c1770dba5d7bfc6ce9ed848954"


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


TESTS = (test_input_rebind_changes_only_input_artifact,
         test_runtime_ui_set_is_deterministic_and_strict,
         test_runtime_ui_failure_preserves_valid_selectors)
