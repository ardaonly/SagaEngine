# SPDX-License-Identifier: MPL-2.0
"""Linux Saga distribution layout staging and preflight.

This script stages the first Linux layout from real existing inputs, then
generates an archive and checksum from that layout. It fails honestly when
required binaries, docs, samples, archive, or checksum entries are missing.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

POLICY_ROOT = Path(__file__).resolve().parents[2] / "Developer" / "DistributionCheck"
if str(POLICY_ROOT) not in sys.path:
    sys.path.insert(0, str(POLICY_ROOT))

from saga_linux_distribution_policy import (
    EXCLUDED_DEV_TOOLS,
    EXCLUDED_RETIRED_TOOLS,
    EXCLUDED_SOURCE_SURFACES,
    LIMITATIONS,
    PACKAGE_NAME,
    PACKAGE_PLATFORM,
    PACKAGE_SCHEMA_VERSION,
    application_names,
    included_applications,
    included_public_tools,
    public_tool_names,
    staged_file_inventory,
    validate_layout,
    validate_metadata_sets,
)


ROOT = Path.cwd()
DEFAULT_OUTPUT = Path("build/dist/linux/Saga")
DEFAULT_REPORT = Path("build/reports/linux_package_preflight_report.json")
PACKAGED_DOTNET_TOOLS = [
    {
        "label": "sagaproject CLI",
        "name": "sagaproject",
        "project": Path("Tools/SagaProjectKit/SagaProjectKit.csproj"),
        "runtime_dir": "sagaproject",
        "dll": "sagaproject.dll",
    },
    {
        "label": "sagapack CLI",
        "name": "sagapack",
        "project": Path("Tools/SagaPackager/SagaPackager.csproj"),
        "runtime_dir": "sagapack",
        "dll": "sagapack.dll",
    },
    {
        "label": "sagascript CLI",
        "name": "sagascript",
        "project": Path("Tools/SagaScript/SagaScript.csproj"),
        "runtime_dir": "sagascript",
        "dll": "sagascript.dll",
    },
]


def read_version() -> str:
    version_file = Path("VERSION")
    if not version_file.exists():
        return "0.0.0"
    return version_file.read_text(encoding="utf-8").strip()


def default_build_dirs(config: str) -> list[Path]:
    version = read_version()
    candidates = [
        Path(f"build/RelWithDebInfo-{version}"),
        Path(f"build/{config}-{version}"),
        Path("build/RelWithDebInfo"),
        Path("build"),
    ]
    return candidates


def first_existing(paths: list[Path]) -> Path | None:
    for path in paths:
        if path.exists():
            return path
    return None


def is_executable(path: Path) -> bool:
    return path.is_file() and path.stat().st_size > 0 and os.access(path, os.X_OK)


def path_text(path: Path) -> str:
    return path.as_posix()


def record_check(
    *,
    label: str,
    kind: str,
    category: str,
    paths: list[Path],
    available: bool,
    required_inputs: list[dict[str, object]],
    available_inputs: list[dict[str, object]],
    missing_inputs: list[dict[str, object]],
    resolved_path: Path | None = None,
    message: str | None = None,
) -> None:
    record: dict[str, object] = {
        "label": label,
        "kind": kind,
        "category": category,
        "paths": [path_text(path) for path in paths],
        "status": "available" if available else "missing",
    }
    if resolved_path is not None:
        record["resolvedPath"] = path_text(resolved_path)
    if message is not None:
        record["message"] = message

    required_inputs.append(record)
    if available:
        available_inputs.append(record)
    else:
        missing_inputs.append(record)


def check_file(
    label: str,
    path: Path,
    missing: list[str],
    required_inputs: list[dict[str, object]],
    available_inputs: list[dict[str, object]],
    missing_inputs: list[dict[str, object]],
    *,
    category: str,
) -> None:
    available = path.is_file()
    message = None if available else f"{label}: missing file {path}"
    if message is not None:
        missing.append(message)
    record_check(
        label=label,
        kind="file",
        category=category,
        paths=[path],
        available=available,
        required_inputs=required_inputs,
        available_inputs=available_inputs,
        missing_inputs=missing_inputs,
        resolved_path=path if available else None,
        message=message,
    )


def check_dir(
    label: str,
    path: Path,
    missing: list[str],
    required_inputs: list[dict[str, object]],
    available_inputs: list[dict[str, object]],
    missing_inputs: list[dict[str, object]],
    *,
    category: str,
) -> None:
    available = path.is_dir()
    message = None if available else f"{label}: missing directory {path}"
    if message is not None:
        missing.append(message)
    record_check(
        label=label,
        kind="directory",
        category=category,
        paths=[path],
        available=available,
        required_inputs=required_inputs,
        available_inputs=available_inputs,
        missing_inputs=missing_inputs,
        resolved_path=path if available else None,
        message=message,
    )


def check_binary(
    label: str,
    candidates: list[Path],
    missing: list[str],
    required_inputs: list[dict[str, object]],
    available_inputs: list[dict[str, object]],
    missing_inputs: list[dict[str, object]],
    *,
    category: str,
) -> None:
    existing = first_existing(candidates)
    available = existing is not None and is_executable(existing)
    message = None
    if not available:
        joined = ", ".join(path.as_posix() for path in candidates)
        message = f"{label}: missing real executable; checked {joined}"
        missing.append(message)
    record_check(
        label=label,
        kind="executable",
        category=category,
        paths=candidates,
        available=available,
        required_inputs=required_inputs,
        available_inputs=available_inputs,
        missing_inputs=missing_inputs,
        resolved_path=existing if available else None,
        message=message,
    )


def status_for_label(required_inputs: list[dict[str, object]], label: str) -> str:
    for item in required_inputs:
        if item.get("label") == label:
            return str(item.get("status"))
    return "unknown"


def write_report(report_path: Path, report: dict[str, object]) -> None:
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def archive_path_for(output: Path) -> Path:
    return output.parent / f"{output.name}.tar.zst"


def checksum_path_for(output: Path) -> Path:
    return output.parent / f"{output.name}.sha256"


def resolved_input(available_inputs: list[dict[str, object]], label: str) -> Path:
    for item in available_inputs:
        if item.get("label") == label and item.get("resolvedPath"):
            return Path(str(item["resolvedPath"]))
    raise RuntimeError(f"internal error: missing resolved input for {label}")


def git_commit() -> str | None:
    result = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )
    if result.returncode != 0:
        return None
    commit = result.stdout.strip()
    return commit or None


def copy_file(source: Path, destination: Path, staged_inputs: list[dict[str, object]], label: str) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    staged_inputs.append(
        {
            "label": label,
            "kind": "file",
            "source": path_text(source),
            "destination": path_text(destination),
        }
    )


def copy_tree(source: Path, destination: Path, staged_inputs: list[dict[str, object]], label: str) -> None:
    if destination.exists():
        shutil.rmtree(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source, destination)
    staged_inputs.append(
        {
            "label": label,
            "kind": "directory",
            "source": path_text(source),
            "destination": path_text(destination),
        }
    )
