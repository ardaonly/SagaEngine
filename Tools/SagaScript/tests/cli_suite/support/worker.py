# SPDX-License-Identifier: MPL-2.0
from __future__ import annotations

from .environment import *

_packager_built = False

def sha256_file(path: Path) -> str:
    return "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest()

def ensure_packager_built(env: dict[str, str]) -> None:
    global _packager_built
    if _packager_built:
        return
    result = subprocess.run(
        ["dotnet", "build", str(PACKAGER_PROJECT), "--configuration", "Release", "--nologo", "--verbosity", "quiet"],
        cwd=REPO_ROOT,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    assert result.returncode == 0, result.stderr + result.stdout
    _packager_built = True

def prepare_worker_request(
    root: Path,
    project_root: Path,
    output_root: Path,
    operation_id: str,
    revision: str = "revision-1",
) -> tuple[Path, Path, Path, Path]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagascript-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagascript-nuget-blockf")
    ensure_packager_built(env)
    package_root = root / f"packages-{operation_id}"
    lock_path = package_root / "SagaPackage.lock"
    generated = package_root / "generated"
    resolve_report = package_root / "resolve.json"
    package_result = subprocess.run(
        [
            "dotnet",
            str(PACKAGER_ASSEMBLY),
            "resolve",
            "--project",
            str(project_root / "StarterArena.sagaproj"),
            "--engine-root",
            str(REPO_ROOT),
            "--lock",
            str(lock_path),
            "--generated-out",
            str(generated),
            "--runtime-bridge",
            str(RUNTIME_BRIDGE_ASSEMBLY),
            "--out",
            str(resolve_report),
        ],
        cwd=REPO_ROOT,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    assert package_result.returncode == 0, package_result.stderr + package_result.stdout
    package_current = json.loads((generated / "manifests" / "current.json").read_text(encoding="utf-8"))
    generation = generated / "generations" / package_current["generationId"]
    references_path = generation / "csharp_references_v1.json"
    api_surfaces_path = generation / "api_surfaces_v1.json"
    references = json.loads(references_path.read_text(encoding="utf-8"))
    bridge_reference = references["references"][0]
    bridge_assembly = generation / bridge_reference["resolvedAssemblyLogicalPath"]
    descriptor = REPO_ROOT / "Engine" / "Distribution" / bridge_reference["distributionPath"]
    project_path = project_root / "StarterArena.sagaproj"
    source_path = project_root / "Scripts" / "DoorController.cs"
    cancellation = root / f"cancel-{operation_id}.json"
    request_path = root / f"request-{operation_id}.json"
    result_path = root / f"result-{operation_id}.json"
    progress_path = root / f"progress-{operation_id}.ndjson"
    request = {
        "schemaVersion": 1,
        "operationId": operation_id,
        "project": {
            "projectId": json.loads(project_path.read_text(encoding="utf-8"))["projectId"],
            "path": str(project_path),
            "hash": sha256_file(project_path),
        },
        "sourceRevision": revision,
        "sources": [
            {
                "path": "Scripts/DoorController.cs",
                "hash": sha256_file(source_path),
            }
        ],
        "packageLock": {"path": str(lock_path), "hash": sha256_file(lock_path)},
        "packageGenerationId": package_current["generationId"],
        "packageGenerationHash": package_current["generationHash"],
        "packageGenerationManifestHash": package_current["generationManifestHash"],
        "packageGraphHash": json.loads(lock_path.read_text(encoding="utf-8"))["graphHash"],
        "referenceManifest": {
            "path": str(references_path),
            "hash": sha256_file(references_path),
        },
        "apiSurfaceManifest": {
            "path": str(api_surfaces_path),
            "hash": sha256_file(api_surfaces_path),
        },
        "runtimeBridge": {
            "logicalReferenceId": bridge_reference["artifactId"],
            "descriptorPath": str(descriptor),
            "descriptorHash": bridge_reference["artifactHash"],
            "assemblyPath": str(bridge_assembly),
            "assemblyHash": bridge_reference["resolvedAssemblyHash"],
            "targetFramework": bridge_reference["targetFramework"],
            "runtimeCompatibility": bridge_reference["runtimeCompatibility"],
            "trust": bridge_reference["trust"],
        },
        "operations": ["analyze", "compile", "project-blocks"],
        "outputRoot": str(output_root),
        "cancellation": {"operationId": operation_id, "path": str(cancellation)},
    }
    request_path.write_text(json.dumps(request, indent=2) + "\n", encoding="utf-8")
    return request_path, result_path, progress_path, cancellation

def worker_process(
    request: Path, result: Path, progress: Path, stage_delay_ms: int = 0,
    publication_barrier: Path | None = None,
) -> subprocess.Popen[str]:
    env = os.environ.copy()
    env.setdefault("DOTNET_CLI_HOME", "/tmp/sagascript-dotnet-home")
    env.setdefault("NUGET_PACKAGES", "/tmp/sagascript-nuget-blockf")
    env.setdefault("DOTNET_CLI_TELEMETRY_OPTOUT", "1")
    ensure_cli_built(env)
    env["SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY"] = str(RUNTIME_BRIDGE_ASSEMBLY)
    if stage_delay_ms:
        env["SAGASCRIPT_WORKER_STAGE_DELAY_MS"] = str(stage_delay_ms)
    if publication_barrier is not None:
        env["SAGASCRIPT_PUBLICATION_BARRIER"] = str(publication_barrier)
    return subprocess.Popen(
        [
            "dotnet",
            str(CLI_ASSEMBLY),
            "worker",
            "--request",
            str(request),
            "--result",
            str(result),
            "--progress",
            str(progress),
        ],
        cwd=REPO_ROOT,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

def wait_for_stage(progress: Path, stage: str) -> None:
    deadline = time.monotonic() + 10.0
    while time.monotonic() < deadline:
        if progress.exists() and f'"stage":"{stage}"' in progress.read_text(encoding="utf-8"):
            return
        time.sleep(0.01)
    raise AssertionError(f"worker did not reach {stage}: {progress.read_text(encoding='utf-8') if progress.exists() else ''}")

def read_current_artifacts(output_root: Path) -> tuple[dict[str, object], dict[str, bytes]]:
    manifest = json.loads((output_root / "manifests" / "current.json").read_text(encoding="utf-8"))
    artifacts = {
        item["artifactId"]: (output_root / item["objectPath"]).read_bytes()
        for item in manifest["artifacts"]
    }
    return manifest, artifacts
