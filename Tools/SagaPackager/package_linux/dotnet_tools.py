# SPDX-License-Identifier: MPL-2.0

from .common import *  # noqa: F403

def distribution_wrapper_text(
    tool_name: str,
    runtime_dir: str,
    dll: str,
    dotnet_command: str,
    *,
    tools_payload_relative: str = ".saga-tools",
) -> str:
    target = f"{tools_payload_relative}/{runtime_dir}/{dll}"
    if tool_name != "sagascript":
        return f"""#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${{BASH_SOURCE[0]}}")" && pwd)"
TOOL="$SCRIPT_DIR/{target}"
DOTNET="${{SAGA_DOTNET:-{dotnet_command}}}"

if [[ ! -x "$DOTNET" ]]; then
    echo "{tool_name}: missing .NET runtime command: $DOTNET" >&2
    exit 127
fi

if [[ ! -f "$TOOL" ]]; then
    echo "{tool_name}: missing packaged tool artifact: $TOOL" >&2
    exit 127
fi

exec "$DOTNET" "$TOOL" "$@"
"""

    return f"""#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${{BASH_SOURCE[0]}}")" && pwd)"
TOOL="$SCRIPT_DIR/{target}"
DOTNET="${{SAGA_DOTNET:-{dotnet_command}}}"

required_sdk_major="10"
required_target_framework="net10.0"
diagnostic_code="Script.Toolchain.DotNetSdkMissing"
diagnostic_message="SagaScript compile requires a .NET 10 SDK for target framework net10.0. Install a .NET 10 SDK and rerun; SagaScript will not downgrade target frameworks automatically."

write_missing_sdk_diagnostics() {{
    local diagnostics_path=""
    local json_output="false"
    local args=("$@")
    local index=0

    while [[ "$index" -lt "${{#args[@]}}" ]]; do
        case "${{args[$index]}}" in
            --diagnostics)
                if [[ $((index + 1)) -lt "${{#args[@]}}" ]]; then
                    diagnostics_path="${{args[$((index + 1))]}}"
                fi
                index=$((index + 2))
                ;;
            --json)
                json_output="true"
                index=$((index + 1))
                ;;
            *)
                index=$((index + 1))
                ;;
        esac
    done

    local payload
    payload=$(cat <<JSON
{{
  "schemaVersion": 1,
  "toolName": "sagascript",
  "toolchainVersion": "0.0.8-dev",
  "inputs": [],
  "summary": {{
    "infoCount": 0,
    "warningCount": 0,
    "errorCount": 1,
    "securityCount": 0,
    "hasBlockingDiagnostics": true
  }},
  "diagnostics": [
    {{
      "severity": "Error",
      "code": "$diagnostic_code",
      "message": "$diagnostic_message"
    }}
  ]
}}
JSON
)

    if [[ -n "$diagnostics_path" ]]; then
        mkdir -p "$(dirname "$diagnostics_path")"
        printf '%s\\n' "$payload" > "$diagnostics_path"
    fi

    if [[ "$json_output" == "true" ]]; then
        printf '%s\\n' "$payload"
    else
        printf 'Error %s: %s\\n' "$diagnostic_code" "$diagnostic_message" >&2
    fi
}}

has_required_dotnet_sdk() {{
    [[ -x "$DOTNET" ]] &&
        "$DOTNET" --list-sdks 2>/dev/null |
            awk '{{print $1}}' |
            grep -Eq "^${{required_sdk_major}}\\\\."
}}

if [[ "${{1:-}}" == "compile" ]] && ! has_required_dotnet_sdk; then
    write_missing_sdk_diagnostics "$@"
    exit 1
fi

if [[ ! -x "$DOTNET" ]]; then
    echo "sagascript: missing .NET runtime command: $DOTNET" >&2
    exit 127
fi

if [[ ! -f "$TOOL" ]]; then
    echo "sagascript: missing packaged tool artifact: $TOOL" >&2
    exit 127
fi

exec "$DOTNET" "$TOOL" "$@"
"""


def write_executable_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    path.chmod(path.stat().st_mode | 0o755)


def has_sdk_major(output: str, major: int) -> bool:
    prefix = f"{major}."
    for line in output.splitlines():
        stripped = line.strip()
        if stripped.startswith(prefix):
            return True
    return False


def candidate_dotnet_executables() -> list[Path]:
    candidates: list[Path] = []
    current = shutil.which("dotnet")
    if current is not None:
        candidates.append(Path(current))

    nix_store = Path("/nix/store")
    if nix_store.is_dir():
        candidates.extend(sorted(nix_store.glob("*dotnet-sdk-10*/bin/dotnet")))

    unique: list[Path] = []
    seen: set[str] = set()
    for candidate in candidates:
        key = str(candidate)
        if key not in seen:
            unique.append(candidate)
            seen.add(key)
    return unique


def dotnet_publish_toolchain() -> dict[str, object]:
    for candidate in candidate_dotnet_executables():
        direct = subprocess.run(
            [str(candidate), "--list-sdks"],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        if direct.returncode == 0 and has_sdk_major(direct.stdout, 10):
            return {
                "available": True,
                "mode": "direct",
                "command": str(candidate),
                "path": str(candidate),
                "sdkListEvaluation": direct.stdout[:1200],
            }

    nix_shell_path = shutil.which("nix-shell")
    if nix_shell_path is not None:
        nix = subprocess.run(
            ["nix-shell", "--run", "dotnet --list-sdks"],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        if nix.returncode == 0 and has_sdk_major(nix.stdout, 10):
            return {
                "available": True,
                "mode": "nix-shell",
                "command": "dotnet",
                "path": nix_shell_path,
                "sdkListEvaluation": nix.stdout[:1200],
            }

    return {
        "available": False,
        "mode": "missing",
        "path": nix_shell_path or "dotnet",
        "message": "dotnet publish toolchain: missing .NET 10 SDK in direct environment, /nix/store, and nix-shell",
    }


def publish_dotnet_tool(
    *,
    project: Path,
    destination: Path,
    toolchain: dict[str, object],
) -> subprocess.CompletedProcess[str]:
    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True, exist_ok=True)
    command = [
        str(toolchain.get("command", "dotnet")),
        "publish",
        str(project),
        "--configuration",
        "Release",
        "--self-contained",
        "false",
        "--output",
        str(destination),
    ]
    if toolchain.get("mode") == "nix-shell":
        command = ["nix-shell", "--run", " ".join(shlex.quote(part) for part in command)]
    return subprocess.run(
        command,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )


def stage_packaged_dotnet_tools(
    *,
    output: Path,
    staged_inputs: list[dict[str, object]],
    missing: list[str],
    required_inputs: list[dict[str, object]],
    available_inputs: list[dict[str, object]],
    missing_inputs: list[dict[str, object]],
) -> list[dict[str, object]]:
    statuses: list[dict[str, object]] = []
    tools_root = output / "tools" / ".saga-tools"

    toolchain = dotnet_publish_toolchain()
    dotnet_ready = bool(toolchain.get("available"))
    dotnet_message = None if dotnet_ready else str(toolchain.get("message"))
    if dotnet_message is not None:
        missing.append(dotnet_message)
    record_check(
        label=".NET 10 publish toolchain",
        kind="tool",
        category="tool-packaging",
        paths=[Path(str(toolchain.get("path", "dotnet")))],
        available=dotnet_ready,
        required_inputs=required_inputs,
        available_inputs=available_inputs,
        missing_inputs=missing_inputs,
        resolved_path=Path(str(toolchain.get("path"))) if dotnet_ready else None,
        message=dotnet_message,
    )

    if not dotnet_ready:
        return [
            {
                "label": item["label"],
                "name": item["name"],
                "project": path_text(Path(item["project"])),
                "status": "blocked",
                "message": dotnet_message,
            }
            for item in PACKAGED_DOTNET_TOOLS
        ]

    for item in PACKAGED_DOTNET_TOOLS:
        label = str(item["label"])
        name = str(item["name"])
        project = Path(item["project"])
        runtime_dir = str(item["runtime_dir"])
        dll = str(item["dll"])
        publish_dir = tools_root / runtime_dir
        status: dict[str, object] = {
            "label": label,
            "name": name,
            "project": path_text(project),
            "publishDirectory": path_text(publish_dir),
        }

        project_ready = project.is_file()
        project_message = None if project_ready else f"{label}: missing project file {project}"
        if project_message is not None:
            missing.append(project_message)
        record_check(
            label=f"{label} project",
            kind="file",
            category="tool-packaging",
            paths=[project],
            available=project_ready,
            required_inputs=required_inputs,
            available_inputs=available_inputs,
            missing_inputs=missing_inputs,
            resolved_path=project if project_ready else None,
            message=project_message,
        )
        if not project_ready:
            status["status"] = "blocked"
            status["message"] = project_message
            statuses.append(status)
            continue

        result = publish_dotnet_tool(project=project, destination=publish_dir, toolchain=toolchain)
        dll_path = publish_dir / dll
        published_ready = result.returncode == 0 and dll_path.is_file() and dll_path.stat().st_size > 0
        publish_message = None
        if not published_ready:
            publish_error = (result.stderr.strip() or result.stdout.strip() or "dotnet publish failed")[:2000]
            publish_message = f"{label}: real dotnet publish failed or did not produce artifact {dll_path}: {publish_error}"
            missing.append(publish_message)
        record_check(
            label=f"{label} published artifact",
            kind="file",
            category="tool-packaging",
            paths=[dll_path],
            available=published_ready,
            required_inputs=required_inputs,
            available_inputs=available_inputs,
            missing_inputs=missing_inputs,
            resolved_path=dll_path if published_ready else None,
            message=publish_message,
        )
        status.update(
            {
                "status": "available" if published_ready else "blocked",
                "artifact": path_text(dll_path),
                "dotnetCommand": str(toolchain.get("command", "dotnet")),
                "stdoutEvaluation": result.stdout[:1200],
                "stderrEvaluation": result.stderr[:1200],
            }
        )
        if not published_ready:
            status["message"] = publish_message
            statuses.append(status)
            continue

        # Some SDK/Nix inputs carry executable mode bits on managed assemblies
        # and localized resources. Only the published apphost is an executable
        # package surface; normalize every other payload file before inventory.
        apphost = publish_dir / name
        for published_path in publish_dir.rglob("*"):
            if published_path.is_file() and not published_path.is_symlink():
                mode = published_path.stat().st_mode
                if published_path == apphost:
                    published_path.chmod(mode | 0o111)
                else:
                    published_path.chmod(mode & ~0o111)

        wrapper_paths = [
            (output / "tools" / name, ".saga-tools"),
            (output / "bin" / name, "../tools/.saga-tools"),
        ]
        wrapper_ready = True
        for wrapper_path, tools_payload_relative in wrapper_paths:
            write_executable_text(
                wrapper_path,
                distribution_wrapper_text(
                    name,
                    runtime_dir,
                    dll,
                    "dotnet",
                    tools_payload_relative=tools_payload_relative,
                ),
            )
            record_check(
                label=f"{label} distribution wrapper {path_text(wrapper_path)}",
                kind="executable",
                category="tool-packaging",
                paths=[wrapper_path],
                available=is_executable(wrapper_path),
                required_inputs=required_inputs,
                available_inputs=available_inputs,
                missing_inputs=missing_inputs,
                resolved_path=wrapper_path if is_executable(wrapper_path) else None,
                message=None if is_executable(wrapper_path) else f"{label}: distribution wrapper is not executable {wrapper_path}",
            )
            if not is_executable(wrapper_path):
                wrapper_ready = False
                missing.append(f"{label}: distribution wrapper is not executable {wrapper_path}")

        record_check(
            label=f"{label} public bin wrapper",
            kind="executable",
            category="tool-packaging",
            paths=[output / "bin" / name],
            available=is_executable(output / "bin" / name),
            required_inputs=required_inputs,
            available_inputs=available_inputs,
            missing_inputs=missing_inputs,
            resolved_path=output / "bin" / name if is_executable(output / "bin" / name) else None,
            message=None if is_executable(output / "bin" / name) else f"{label}: public bin wrapper is not executable {output / 'bin' / name}",
        )
        if not wrapper_ready:
            message = f"{label}: one or more distribution wrappers are not executable"
            missing.append(message)
            status["status"] = "blocked"
            status["message"] = message
        else:
            staged_inputs.append(
                {
                    "label": f"{label} published runtime",
                    "kind": "directory",
                    "source": path_text(project),
                    "destination": path_text(publish_dir),
                }
            )
            for wrapper_path, _tools_payload_relative in wrapper_paths:
                staged_inputs.append(
                    {
                        "label": f"{label} distribution wrapper",
                        "kind": "generated-wrapper",
                        "source": "scripts/package-linux-saga",
                        "destination": path_text(wrapper_path),
                    }
                )
        statuses.append(status)

    return statuses

