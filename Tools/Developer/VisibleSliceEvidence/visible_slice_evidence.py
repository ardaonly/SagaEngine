#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Serial visible-slice evidence orchestration; it complements, never replaces, CTest."""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
import pathlib
import platform
import subprocess
import sys
import time
from typing import Any


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return "sha256:" + digest.hexdigest()


def atomic_json(path: pathlib.Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + f".{os.getpid()}.tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def defaults(build_root: pathlib.Path) -> list[list[str]]:
    binary = build_root / "bin"
    return [
        [str(binary / "SagaRuntimeVisibleSliceTests"), "--gtest_brief=1"],
        [str(binary / "SagaUIModuleTests"),
         "--gtest_filter=RmlUiRenderInterfaceTests.*", "--gtest_brief=1"],
        [str(binary / "SagaVisualBlocksEditorModuleTests"),
         "--gtest_filter=ReadOnlyScriptProjectionTests.*", "--gtest_brief=1"],
        [str(binary / "SagaEditorAuthoringModuleTests"),
         "--gtest_filter=ScriptProjectionSessionTests.*", "--gtest_brief=1"],
        [str(binary / "SagaEditorAuthoringModuleTests"),
         "--gtest_filter=PlayPreparationPipelineTests.WorkspaceAndSavedSnapshotPublishIdenticalCanonicalArtifacts:PlayPreparationPipelineTests.InputDisplayLabelChangeLeavesUnrelatedArtifactsStable",
         "--gtest_brief=1"],
        [str(binary / "SagaEditorCoreModuleTests"),
         "--gtest_filter=DocumentRecoveryTests.*", "--gtest_brief=1"],
    ]


def parse_command(value: str) -> list[str]:
    command = json.loads(value)
    if not isinstance(command, list) or not command or not all(isinstance(item, str) for item in command):
        raise argparse.ArgumentTypeError("--command-json must be a non-empty JSON string array")
    return command


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=pathlib.Path, default=pathlib.Path.cwd())
    parser.add_argument("--build-root", type=pathlib.Path,
                        default=pathlib.Path("build/RelWithDebInfo-0.0.11"))
    parser.add_argument("--evidence-root", type=pathlib.Path,
                        default=pathlib.Path(".saga-private/evidence/visible-slice"))
    parser.add_argument("--command-json", action="append", type=parse_command, default=[])
    parser.add_argument("--artifact", action="append", type=pathlib.Path, default=[])
    parser.add_argument("--runtime-evidence", type=pathlib.Path)
    parser.add_argument("--graphical", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    build_root = args.build_root if args.build_root.is_absolute() else repo_root / args.build_root
    evidence_root = args.evidence_root if args.evidence_root.is_absolute() else repo_root / args.evidence_root
    commands = args.command_json or defaults(build_root)
    started = dt.datetime.now(dt.timezone.utc)
    records: list[dict[str, Any]] = []
    overall = 0
    for index, command in enumerate(commands):
        record: dict[str, Any] = {"command": command, "index": index}
        executable = pathlib.Path(command[0])
        if executable.is_file():
            record["executable"] = {"path": str(executable), "hash": sha256(executable)}
        if args.dry_run:
            record.update({"durationMs": 0, "exitCode": None, "status": "unexecuted"})
            records.append(record)
            continue
        command_start = time.monotonic()
        try:
            completed = subprocess.run(command, cwd=repo_root, text=True,
                                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                       check=False, timeout=180)
            record["durationMs"] = round((time.monotonic() - command_start) * 1000)
            record["exitCode"] = completed.returncode
            record["status"] = "passed" if completed.returncode == 0 else "failed"
            log_path = evidence_root / "logs" / f"{index:02d}.log"
            log_path.parent.mkdir(parents=True, exist_ok=True)
            log_path.write_text(completed.stdout, encoding="utf-8")
            record["logPath"] = str(log_path)
            record["logHash"] = sha256(log_path)
            if completed.returncode != 0: overall = 1
        except (OSError, subprocess.TimeoutExpired) as error:
            record.update({"durationMs": round((time.monotonic() - command_start) * 1000),
                           "exitCode": None, "status": "failed", "diagnostic": str(error)})
            overall = 1
        records.append(record)

    runtime: dict[str, Any] = {}
    if args.runtime_evidence:
        runtime_path = args.runtime_evidence if args.runtime_evidence.is_absolute() else repo_root / args.runtime_evidence
        if runtime_path.is_file():
            runtime = json.loads(runtime_path.read_text(encoding="utf-8"))
            runtime["sourcePath"] = str(runtime_path)
            runtime["sourceHash"] = sha256(runtime_path)
    artifacts = []
    for path in args.artifact:
        resolved = path if path.is_absolute() else repo_root / path
        artifacts.append({"path": str(resolved), "hash": sha256(resolved) if resolved.is_file() else None})
    observed = runtime.get("observedState", {}) if isinstance(runtime.get("observedState", {}), dict) else {}
    capture_paths = runtime.get("capturePaths", [])
    if not capture_paths:
        capture_paths = [item.get("subject") for item in runtime.get("runtimeDiagnostics", [])
                         if isinstance(item, dict) and item.get("code") == "Runtime.Render.FrameCaptureEvidence"
                         and item.get("subject")]
    captures = []
    for capture in capture_paths:
        capture_path = pathlib.Path(capture)
        if not capture_path.is_absolute(): capture_path = repo_root / capture_path
        captures.append({"path": str(capture_path),
                         "hash": sha256(capture_path) if capture_path.is_file() else None})
    pixel_statistics = runtime.get("pixelStatistics", {}) or {
        "meaningfulRendererPixels": observed.get("meaningfulRendererPixels"),
        "drawCount": observed.get("drawCount"),
    }
    prompt_sequence = runtime.get("promptStateSequence", []) or observed.get("promptText", [])
    session = runtime.get("session", {}) or {
        "playSessionId": runtime.get("playSessionId"),
        "canonicalLaunchManifestHash": runtime.get("canonicalLaunchManifestHash"),
    }
    result_summary = runtime.get("result", {}) or {
        "status": runtime.get("status"),
        "exitClassification": runtime.get("exitClassification"),
        "shutdownClassification": runtime.get("shutdownClassification"),
        "framesExecuted": runtime.get("framesExecuted"),
        "interactionActionCount": runtime.get("interactionActionCount"),
    }
    graphical_status = "executed" if args.graphical and runtime and captures and all(
        capture["hash"] for capture in captures) else "unexecuted"
    report = {
        "schemaVersion": 1,
        "producer": "visible-slice-evidence",
        "scope": "visible-slice",
        "startedUtc": started.isoformat(),
        "finishedUtc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "repository": {"root": str(repo_root)},
        "environment": {
            "platform": platform.platform(),
            "python": platform.python_version(),
            "backend": os.environ.get("SAGA_GRAPHICS_BACKEND", "unreported"),
            "gpu": os.environ.get("SAGA_GPU_IDENTITY", "unreported"),
            "display": os.environ.get("DISPLAY", ""),
        },
        "commands": records,
        "artifacts": artifacts,
        "runtime": runtime,
        "graphical": {
            "status": graphical_status,
            "captures": captures,
            "pixelStatistics": pixel_statistics,
            "promptStateSequence": prompt_sequence,
            "session": session,
            "result": result_summary,
        },
        "status": "passed" if overall == 0 and not args.dry_run else
                  "unexecuted" if args.dry_run else "failed",
        "nonClaims": [
            "This serial runner does not replace the normal CTest graph.",
            "Graphical proof is absent unless graphical.status is executed.",
            "A focused run is not a full CTest result.",
        ],
    }
    report_path = evidence_root / "visible-slice-evidence.json"
    atomic_json(report_path, report)
    print(report_path)
    return overall


if __name__ == "__main__":
    raise SystemExit(main())
