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

from .model import *
from .schema import *
from .inventory import *


def policy_submodule_map(
    policy: dict[str, Any],
    report: Report,
) -> dict[str, str]:
    result: dict[str, str] = {}
    for index, record in enumerate(policy.get("submodules", []), 1):
        if not isinstance(record, dict):
            continue
        path = record.get("path")
        commit = record.get("commit")
        if not isinstance(path, str) or not safe_relative_path(path):
            report.add("ERROR", "SUBMODULE_PATH", f"submodules[{index}]")
            continue
        if not isinstance(commit, str) or not FULL_COMMIT_RE.fullmatch(commit):
            report.add("ERROR", "SUBMODULE_COMMIT", f"{path}: {commit!r}")
            continue
        if path in result:
            report.add("ERROR", "SUBMODULE_DUPLICATE", path)
        result[path] = commit
        if record.get("license_mode") != "upstream-file-specific":
            report.add("ERROR", "SUBMODULE_LICENSE_MODE", path)
        if not isinstance(record.get("review"), str) or not record.get("review"):
            report.add("ERROR", "SUBMODULE_REVIEW", path)
    return result


def expected_legacy_payload(policy: dict[str, Any]) -> dict[str, Any]:
    rules: list[dict[str, Any]] = []
    for domain in policy.get("domains", []):
        mode = domain.get("current_license_mode")
        expression = domain.get("current_license_expression", "")
        if mode == "fixed-spdx":
            license_value = expression
        elif mode == "upstream-file-specific":
            license_value = "UPSTREAM"
        elif mode == "file-specific":
            license_value = "FILE-SPECIFIC"
        elif mode == "user-selected":
            license_value = "USER-SELECTED"
        else:
            license_value = "NOT-APPLICABLE"
        for pattern in domain.get("paths", []):
            rules.append(
                {
                    "pattern": pattern,
                    "license": license_value,
                    "domain_id": domain["id"],
                    "priority": domain["priority"],
                }
            )
    return {
        "schema": "sagaengine.path_rules.v1",
        "version": "1.0",
        "mode": "deprecated-compatibility",
        "authoritative": False,
        "source_of_truth": "LICENSE_POLICY.toml",
        "generated_by": "Tools/Licensing/generate_legacy_path_rules.py",
        "description": (
            "Compatibility projection only. LICENSE_POLICY.toml is authoritative."
        ),
        "rules": sorted(
            rules,
            key=lambda item: (
                -item["priority"],
                item["pattern"],
                item["domain_id"],
            ),
        ),
    }


def validate_legacy(root: Path, policy: dict[str, Any], report: Report) -> None:
    validation = policy["validation"]
    legacy = policy["legacy"]
    path = root / legacy["path_rules_path"]
    if not path.is_file():
        emit_action(
            report,
            validation,
            "legacy_policy_conflict_action",
            "LEGACY_PATH_RULES_MISSING",
            legacy["path_rules_path"],
        )
        return
    try:
        actual = json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:
        emit_action(
            report,
            validation,
            "legacy_policy_conflict_action",
            "LEGACY_PATH_RULES_PARSE",
            str(exc),
        )
        return
    expected = expected_legacy_payload(policy)
    if actual != expected:
        emit_action(
            report,
            validation,
            "legacy_policy_conflict_action",
            "LEGACY_PATH_RULES_DRIFT",
            f"{legacy['path_rules_path']} must be regenerated",
        )

    cleanup = legacy["cleanup_status"]
    duplicates = legacy["duplicate_license_paths"]
    existing = [path for path in duplicates if (root / path).exists()]
    if cleanup == "complete" and existing:
        report.add("ERROR", "LEGACY_DUPLICATES_REMAIN", repr(existing))
    if cleanup == "pending" and not legacy["files_are_normative_until_cleanup"]:
        report.add(
            "ERROR",
            "LEGACY_NORMATIVE_STATE_INVALID",
            "pending cleanup must preserve current normative status",
        )


def validate_governance(root: Path, policy: dict[str, Any], report: Report) -> None:
    validation = policy["validation"]
    governance = policy["governance"]
    for relative in governance["required_documents"]:
        if not safe_relative_path(relative) or not (root / relative).is_file():
            emit_action(
                report,
                validation,
                "governance_missing_action",
                "GOVERNANCE_DOCUMENT_MISSING",
                relative,
            )

    source = root / governance["contributing_section_source"]
    contributing = root / governance["contributing_document"]
    if not source.is_file():
        emit_action(
            report,
            validation,
            "governance_missing_action",
            "CONTRIBUTING_SECTION_SOURCE_MISSING",
            str(source.relative_to(root)),
        )
    if not contributing.is_file():
        emit_action(
            report,
            validation,
            "governance_missing_action",
            "CONTRIBUTING_DOCUMENT_MISSING",
            str(contributing.relative_to(root)),
        )
        return

    if governance["contributing_update_status"] == "required-before-foundation-commit":
        text = contributing.read_text(encoding="utf-8", errors="replace")
        begin = governance["contributing_marker_begin"]
        end = governance["contributing_marker_end"]
        if begin not in text or end not in text:
            emit_action(
                report,
                validation,
                "governance_missing_action",
                "CONTRIBUTING_DCO_SECTION_NOT_APPLIED",
                governance["contributing_document"],
            )


def recursive_submodule_status(root: Path) -> dict[str, str]:
    output = run_git(root, "submodule", "status", "--recursive", check=False)
    result: dict[str, str] = {}
    for line in output.splitlines():
        match = re.match(r"^[ +-U]?([0-9a-f]{40})\s+(\S+)", line)
        if match:
            commit, path = match.groups()
            result[path] = commit
    return result


def validate_submodules(
    root: Path,
    policy_map: dict[str, str],
    policy: dict[str, Any],
    report: Report,
) -> None:
    validation = policy["validation"]
    actual = recursive_submodule_status(root)
    for path, commit in sorted(actual.items()):
        expected = policy_map.get(path)
        if expected is None:
            report.add("ERROR", "SUBMODULE_UNRECORDED", f"{path}@{commit}")
        elif expected != commit:
            report.add(
                "ERROR",
                "SUBMODULE_DRIFT",
                f"{path}: policy={expected}, actual={commit}",
            )
    for path in sorted(set(policy_map) - set(actual)):
        emit_action(
            report,
            validation,
            "submodule_missing_action",
            "SUBMODULE_NOT_PRESENT",
            path,
        )
