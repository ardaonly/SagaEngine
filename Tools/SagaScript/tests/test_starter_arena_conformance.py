# SPDX-License-Identifier: MPL-2.0
"""Editorless StarterArena source/tooling conformance across two clean roots."""

from __future__ import annotations

import json
import os
import re
import shutil
import subprocess
import tempfile
import os
from pathlib import Path

import test_sagascript_cli as script_suite


REPO_ROOT = script_suite.REPO_ROOT
STARTER_ROOT = REPO_ROOT / "Samples" / "StarterArena"
PROJECT_KIT = REPO_ROOT / "Tools" / "SagaProjectKit" / "SagaProjectKit.csproj"
PROJECT_KIT_ASSEMBLY = PROJECT_KIT.parent / "bin" / "Release" / "net10.0" / "sagaproject.dll"


def run_dotnet(assembly: Path, *args: str) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/starter-conformance-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/starter-conformance-nuget")
    env.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
    return subprocess.run(
        ["dotnet", str(assembly), *args],
        cwd=REPO_ROOT,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def ensure_tools() -> None:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/starter-conformance-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/starter-conformance-nuget")
    for project in (PROJECT_KIT, script_suite.PACKAGER_PROJECT):
        result = subprocess.run(
            ["dotnet", "build", str(project), "--configuration", "Release", "--nologo", "--verbosity", "quiet"],
            cwd=REPO_ROOT,
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        assert result.returncode == 0, result.stderr + result.stdout
    script_suite.ensure_cli_built(env)


def canonical_objects(root: Path, manifest_path: Path) -> dict[str, bytes]:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    return {
        item["artifactId"]: (root / (item.get("objectPath") or item["path"])).read_bytes()
        for item in manifest["artifacts"]
    }


def strings(value: object) -> list[str]:
    if isinstance(value, dict):
        return [item for child in value.values() for item in strings(child)]
    if isinstance(value, list):
        return [item for child in value for item in strings(child)]
    return [value] if isinstance(value, str) else []


def run_conformance(root: Path) -> dict[str, object]:
    project = root / "StarterArena"
    shutil.copytree(STARTER_ROOT, project)
    manifest = project / "StarterArena.sagaproj"
    project_report = root / "project.json"
    project_result = run_dotnet(
        PROJECT_KIT_ASSEMBLY,
        "validate",
        "--project",
        str(manifest),
        "--out",
        str(project_report),
    )
    assert project_result.returncode == 0, project_result.stderr + project_result.stdout

    package_generation = root / "packages"
    resolve_report = root / "resolve.json"
    resolve_result = run_dotnet(
        script_suite.PACKAGER_ASSEMBLY,
        "resolve",
        "--project", str(manifest),
        "--engine-root", str(REPO_ROOT),
        "--lock", str(project / "SagaPackage.lock"),
        "--generated-out", str(package_generation),
        "--runtime-bridge", str(script_suite.RUNTIME_BRIDGE_ASSEMBLY),
        "--out", str(resolve_report),
    )
    assert resolve_result.returncode == 0, resolve_result.stderr + resolve_result.stdout

    lock_report = root / "lock.json"
    lock_result = run_dotnet(
        script_suite.PACKAGER_ASSEMBLY,
        "lock-verify",
        "--project",
        str(manifest),
        "--engine-root",
        str(REPO_ROOT),
        "--lock",
        str(project / "SagaPackage.lock"),
        "--generated-out",
        str(package_generation),
        "--runtime-bridge",
        str(script_suite.RUNTIME_BRIDGE_ASSEMBLY),
        "--out",
        str(lock_report),
    )
    assert lock_result.returncode == 0, lock_result.stderr + lock_result.stdout

    worker_output = root / "scripts"
    request, result, progress, _ = script_suite.prepare_worker_request(
        root / "worker",
        project,
        worker_output,
        "starter-arena-conformance",
        "source-revision-1",
    )
    worker_result = script_suite.run_cli(
        "worker",
        "--request",
        str(request),
        "--result",
        str(result),
        "--progress",
        str(progress),
    )
    assert worker_result.returncode == 0, worker_result.stderr + worker_result.stdout

    content_root = root / "content"
    script_manifest = worker_output / "manifests" / "current.json"
    content_result = run_dotnet(
        script_suite.PACKAGER_ASSEMBLY,
        "compile-content",
        "--project",
        str(manifest),
        "--script-manifest",
        str(script_manifest),
        "--out",
        str(content_root),
    )
    assert content_result.returncode == 0, content_result.stderr + content_result.stdout

    content_manifest = content_root / "manifests" / "current.json"
    content = json.loads(content_manifest.read_text(encoding="utf-8"))
    scene_record = next(item for item in content["artifacts"] if item["artifactType"] == "runtime.scene")
    runtime_scene = json.loads((content_root / scene_record["path"]).read_text(encoding="utf-8"))
    for value in strings(runtime_scene):
        assert not value.endswith(".cs")
        assert str(project) not in value
        assert not os.path.isabs(value)
        assert not re.match(r"^[A-Za-z]:[\\/]", value)
        assert not value.startswith("\\\\")
        assert "../" not in value and "..\\" not in value
    package_request = json.loads(request.read_text(encoding="utf-8"))
    return {
        "projectHash": script_suite.sha256_file(manifest),
        "lockGraphHash": json.loads(lock_report.read_text(encoding="utf-8"))["graphHash"],
        "resolvedLock": Path(package_request["packageLock"]["path"]).read_bytes(),
        "referenceManifest": Path(package_request["referenceManifest"]["path"]).read_bytes(),
        "apiSurfaceManifest": Path(package_request["apiSurfaceManifest"]["path"]).read_bytes(),
        "ownershipCatalog": Path(package_request["ownershipCatalog"]["path"]).read_bytes(),
        "contentManifest": content_manifest.read_bytes(),
        "contentObjects": canonical_objects(content_root, content_manifest),
        "scriptManifest": script_manifest.read_bytes(),
        "scriptObjects": canonical_objects(worker_output, script_manifest),
    }


def test_starter_arena_source_tooling_conformance() -> None:
    ensure_tools()
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        first = run_conformance(root / "first")
        second = run_conformance(root / "second")
    assert first == second


def test_scene_and_input_structural_failures_are_deterministic() -> None:
    ensure_tools()
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        project = root / "StarterArena"
        shutil.copytree(STARTER_ROOT, project)
        worker_output = root / "scripts"
        request, result, progress, _ = script_suite.prepare_worker_request(
            root / "worker", project, worker_output, "structural-evidence")
        worker = script_suite.run_cli("worker", "--request", str(request), "--result", str(result), "--progress", str(progress))
        assert worker.returncode == 0, worker.stderr + worker.stdout
        semantic = worker_output / "manifests" / "current.json"
        scene_path = project / "Scenes" / "StarterArena.sagascene"
        original = json.loads(scene_path.read_text(encoding="utf-8"))

        cases: list[tuple[str, object, str]] = []
        duplicate = json.loads(json.dumps(original)); duplicate["entities"].append(duplicate["entities"][0])
        cases.append(("duplicate", duplicate, "Scene.Entity.DuplicateId"))
        missing = json.loads(json.dumps(original)); missing["entities"][1]["parentEntityId"] = "missing"
        cases.append(("missing-parent", missing, "Scene.Entity.ParentMissing"))
        cycle = json.loads(json.dumps(original)); cycle["entities"][0]["parentEntityId"] = "ground"
        cases.append(("cycle", cycle, "Scene.Entity.ParentCycle"))
        component = json.loads(json.dumps(original)); component["entities"][0]["components"].append(component["entities"][0]["components"][0])
        cases.append(("component", component, "Scene.Component.DuplicateType"))
        vector = json.loads(json.dumps(original)); vector["entities"][0]["components"][0]["position"] = [0, 0]
        cases.append(("vector", vector, "Scene.Component.TransformInvalid"))
        for name, payload, code in cases:
            scene_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
            output = root / ("scene-" + name)
            compiled = run_dotnet(script_suite.PACKAGER_ASSEMBLY, "compile-scenes", "--project",
                                  str(project / "StarterArena.sagaproj"), "--script-manifest", str(semantic),
                                  "--out", str(output))
            assert compiled.returncode == 1
            diagnostics = json.loads((output / "compile-result.json").read_text(encoding="utf-8"))["diagnostics"]
            assert code in [item["code"] for item in diagnostics]
        scene_path.write_text(json.dumps(original, indent=2) + "\n", encoding="utf-8")

        input_path = project / "Input/Input.saga.json"
        input_source = json.loads(input_path.read_text(encoding="utf-8"))
        input_source["actions"].append(json.loads(json.dumps(input_source["actions"][0])))
        input_path.write_text(json.dumps(input_source, indent=2) + "\n", encoding="utf-8")
        output = root / "input-duplicate"
        compiled = run_dotnet(script_suite.PACKAGER_ASSEMBLY, "compile-inputs", "--project",
                              str(project / "StarterArena.sagaproj"), "--out", str(output))
        assert compiled.returncode == 1
        codes = [item["code"] for item in json.loads((output / "compile-result.json").read_text(encoding="utf-8"))["diagnostics"]]
        assert "Input.Action.DuplicateId" in codes
        assert "Input.Binding.DuplicatePhysical" in codes


if __name__ == "__main__":
    test_starter_arena_source_tooling_conformance()
    test_scene_and_input_structural_failures_are_deterministic()
    print("StarterArena source/tooling conformance and structural failures passed")
