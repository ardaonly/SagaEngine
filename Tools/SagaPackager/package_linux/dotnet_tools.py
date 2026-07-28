# SPDX-License-Identifier: MPL-2.0
"""Copy the CMake-owned managed-tool stage into the Python package layout."""

from .common import *  # noqa: F403


def distribution_wrapper_text(tool_directory: str, assembly: str) -> str:
    return f"""#!/usr/bin/env bash
set -euo pipefail

wrapper_dir="$(cd "$(dirname "${{BASH_SOURCE[0]}}")" && pwd)"
product_root="$(cd "${{wrapper_dir}}/.." && pwd)"
dotnet="${{product_root}}/runtime/dotnet/dotnet"
tool="${{product_root}}/tools/{tool_directory}/{assembly}.dll"

if [[ ! -x "${{dotnet}}" ]]; then
    echo "{assembly}: Saga private runtime is missing: ${{dotnet}}" >&2
    exit 127
fi
if [[ ! -f "${{tool}}" ]]; then
    echo "{assembly}: packaged tool is missing: ${{tool}}" >&2
    exit 127
fi

export DOTNET_ROOT="${{product_root}}/runtime/dotnet"
export DOTNET_ROOT_X64="${{DOTNET_ROOT}}"
export DOTNET_HOST_PATH="${{dotnet}}"
export DOTNET_MULTILEVEL_LOOKUP=0
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
if [[ "{assembly}" == "sagascript" ]]; then
    export SAGASCRIPT_RUNTIME_BRIDGE_ASSEMBLY="${{product_root}}/Engine/Managed/SagaScript.RuntimeBridge/SagaScript.RuntimeBridge.dll"
fi
exec "${{dotnet}}" "${{tool}}" "$@"
"""


def write_executable_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    path.chmod(path.stat().st_mode | 0o755)


def stage_packaged_dotnet_tools(
    *,
    output: Path,
    build_dir: Path,
    staged_inputs: list[dict[str, object]],
    missing: list[str],
    required_inputs: list[dict[str, object]],
    available_inputs: list[dict[str, object]],
    missing_inputs: list[dict[str, object]],
) -> list[dict[str, object]]:
    stage = build_dir / "ManagedToolRuntime"
    manifest = stage / "managed-tool-runtime.json"
    host = stage / "runtime" / "dotnet" / "dotnet"
    identities: list[tuple[str, str]] = []
    try:
        manifest_data = json.loads(manifest.read_text(encoding="utf-8"))
        expected_runtime = managed_runtime_metadata()
        if (
            manifest_data.get("framework") != expected_runtime["framework"]
            or manifest_data.get("targetFramework") != expected_runtime["targetFramework"]
            or manifest_data.get("minimumRuntimeVersion")
            != expected_runtime["minimumRuntimeVersion"]
            or manifest_data.get("runtimeVersion") != expected_runtime["runtimeVersion"]
            or manifest_data.get("rid") != expected_runtime["rid"]
        ):
            raise ValueError("runtime identity does not match distribution policy")
        identities = [
            (str(tool["directory"]), str(tool["assembly"]))
            for tool in manifest_data["tools"]
        ]
        if identities != [
            ("SagaProjectKit", "sagaproject"),
            ("SagaScript", "sagascript"),
            ("SagaPackager", "sagapack"),
        ]:
            raise ValueError("managed tool identities do not match distribution policy")
    except (OSError, json.JSONDecodeError, KeyError, TypeError, ValueError) as error:
        message = f"managed runtime manifest is invalid: {error}"
        missing.append(message)
        return [{"name": name, "status": "blocked", "message": message}
                for name in ("sagaproject", "sagascript", "sagapack")]
    required = [manifest, host]
    for directory, assembly in identities:
        base = stage / "tools" / directory / assembly
        required.extend(
            Path(f"{base}.{suffix}")
            for suffix in ("dll", "runtimeconfig.json", "deps.json")
        )
    ready = all(path.is_file() and path.stat().st_size > 0 for path in required)
    message = None if ready else (
        "CMake managed-tool stage is missing or incomplete; build "
        "SagaManagedToolRuntimeStage before packaging"
    )
    record_check(
        label="CMake managed-tool runtime stage",
        kind="directory",
        category="tool-packaging",
        paths=[stage],
        available=ready,
        required_inputs=required_inputs,
        available_inputs=available_inputs,
        missing_inputs=missing_inputs,
        resolved_path=stage if ready else None,
        message=message,
    )
    if not ready:
        missing.append(message)
        return [{"name": assembly, "status": "blocked", "message": message}
                for _, assembly in identities]

    copy_tree(stage / "runtime", output / "runtime", staged_inputs,
              "CMake managed runtime")
    copy_tree(stage / "tools", output / "tools", staged_inputs,
              "CMake managed tools")
    copy_file(manifest, output / "managed-tool-runtime.json", staged_inputs,
              "managed tool runtime manifest")
    copy_tree(stage / "licenses" / "ThirdParty" / "dotnet",
              output / "licenses" / "ThirdParty" / "dotnet",
              staged_inputs, "Microsoft .NET runtime notices")

    statuses: list[dict[str, object]] = []
    for directory, assembly in identities:
        wrapper = output / "bin" / assembly
        write_executable_text(
            wrapper, distribution_wrapper_text(directory, assembly))
        staged_inputs.append({
            "label": f"{assembly} private-runtime wrapper",
            "kind": "generated-wrapper",
            "source": "scripts/package",
            "destination": path_text(wrapper),
        })
        statuses.append({
            "name": assembly,
            "status": "available",
            "host": path_text(output / "runtime/dotnet/dotnet"),
            "artifact": path_text(
                output / "tools" / directory / f"{assembly}.dll"),
        })
    return statuses
