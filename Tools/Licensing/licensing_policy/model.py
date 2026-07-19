# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

from dataclasses import asdict, dataclass, field
from pathlib import Path, PurePosixPath
import argparse
import json
import re
import subprocess
import sys
import tomllib
from typing import Any

from licensing_common import (
    ACTION_TO_SEVERITY,
    action_severity,
    gitwildmatch,
    repository_root,
    safe_relative_path,
    validate_spdx_expression,
)



@dataclass
class Finding:
    severity: str
    code: str
    message: str

    def render(self) -> str:
        return f"{self.severity:<7} {self.code:<36} {self.message}"


@dataclass
class Domain:
    raw: dict[str, Any]
    id: str
    priority: int
    paths: list[str]
    required: bool


@dataclass
class Resolution:
    domain_id: str
    matched: bool
    resolved: bool
    expression: str = ""
    source: str = ""
    evidence: list[str] = field(default_factory=list)


@dataclass(frozen=True)
class LoadedPolicyContext:
    root: Path
    policy_path: Path
    policy: dict[str, Any]
    rights_holders: set[str]
    domains: list[Domain]
    case_sensitive: bool


@dataclass
class FilePolicyDecision:
    path: str
    domain: Domain | None
    resolution: Resolution
    metadata_sources: list[str] = field(default_factory=list)
    diagnostics: list[Finding] = field(default_factory=list)
    modification_allowed: bool = False


@dataclass
class Report:
    findings: list[Finding] = field(default_factory=list)
    classified: dict[str, str] = field(default_factory=dict)
    resolutions: dict[str, Resolution] = field(default_factory=dict)

    def add(self, severity: str | None, code: str, message: str) -> None:
        if severity is not None:
            self.findings.append(Finding(severity, code, message))

    @property
    def errors(self) -> list[Finding]:
        return [item for item in self.findings if item.severity == "ERROR"]


def run(
    command: list[str],
    cwd: Path,
    *,
    check: bool = True,
    input_text: str | None = None,
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        command,
        cwd=cwd,
        input=input_text,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if check and result.returncode != 0:
        raise RuntimeError(
            f"{' '.join(command)} failed ({result.returncode}): "
            f"{result.stderr.strip()}"
        )
    return result


def run_git(root: Path, *args: str, check: bool = True) -> str:
    return run(["git", *args], root, check=check).stdout


def run_bytes(
    command: list[str],
    cwd: Path,
) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        command,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def emit_action(
    report: Report,
    validation: dict[str, Any],
    action_key: str,
    code: str,
    message: str,
    *,
    default: str = "deny",
) -> None:
    action = validation.get(action_key, default)
    try:
        severity = action_severity(action)
    except ValueError:
        report.add("ERROR", "INVALID_VALIDATION_ACTION", f"{action_key}={action!r}")
        return
    report.add(severity, code, message)
