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
from .resolver import *
from .repository_validation import *
from .governance_validation import *
from .cmake_validation import *


def load_policy_context(
    root: Path,
    policy_path: Path | None = None,
) -> LoadedPolicyContext:
    root = root.resolve()
    selected = (policy_path or root / "LICENSE_POLICY.toml").resolve()
    try:
        policy = tomllib.loads(selected.read_text(encoding="utf-8"))
    except (OSError, tomllib.TOMLDecodeError) as exc:
        raise ValueError(f"invalid canonical licensing policy: {exc}") from exc
    report = Report()
    if policy.get("schema_version") != SUPPORTED_SCHEMA_VERSION:
        report.add("ERROR", "SCHEMA_VERSION",
            f"expected={SUPPORTED_SCHEMA_VERSION}, actual={policy.get('schema_version')!r}")
    validate_schema(policy, report)
    if not report.errors:
        validate_policy_tables(policy, report)
    rights_holders = parse_rights_holders(policy, report) if not report.errors else set()
    domains = parse_domains(root, policy, rights_holders, report) if not report.errors else []
    if report.errors:
        raise ValueError(
            "invalid canonical licensing policy: "
            + "; ".join(item.render() for item in report.errors)
        )
    return LoadedPolicyContext(
        root=root,
        policy_path=selected,
        policy=policy,
        rights_holders=rights_holders,
        domains=domains,
        case_sensitive=bool(policy["validation"]["case_sensitive"]),
    )


def validate_policy(
    root: Path,
    policy_path: Path,
    *,
    build_dir: Path | None,
    cmake_config: str | None,
    base_ref: str | None,
    strict: bool,
) -> Report:
    report = Report()
    try:
        policy = tomllib.loads(policy_path.read_text(encoding="utf-8"))
    except Exception as exc:
        report.add("ERROR", "POLICY_PARSE", str(exc))
        return report

    if policy.get("schema_version") != SUPPORTED_SCHEMA_VERSION:
        report.add(
            "ERROR",
            "SCHEMA_VERSION",
            f"expected={SUPPORTED_SCHEMA_VERSION}, "
            f"actual={policy.get('schema_version')!r}",
        )

    validate_schema(policy, report)
    if report.errors:
        return report
    validate_policy_tables(policy, report)
    if report.errors:
        return report

    rights_holders = parse_rights_holders(policy, report)
    domains = parse_domains(root, policy, rights_holders, report)
    submodule_map = policy_submodule_map(policy, report)
    target_records = parse_targets(
        root,
        policy,
        {domain.id for domain in domains},
        report,
    )

    include_untracked = bool(policy["validation"]["include_untracked_nonignored"])
    try:
        files = list_repository_files(
            root,
            include_untracked,
            policy["validation"]["untracked_exclude_patterns"],
            case_sensitive=bool(policy["validation"]["case_sensitive"]),
        )
    except Exception as exc:
        report.add("ERROR", "GIT_FILE_LIST", str(exc))
        return report

    validate_files(
        root,
        files,
        domains,
        set(submodule_map),
        policy,
        report,
    )
    reviewed_commit = validate_commits(root, policy, report)
    validate_legacy(root, policy, report)
    validate_governance(root, policy, report)
    validate_submodules(root, submodule_map, policy, report)
    validate_symlinks(root, policy, report)
    validate_lfs(root, files, policy, report)
    validate_renames(root, reviewed_commit, domains, policy, report)
    validate_cmake_targets(
        root,
        build_dir,
        cmake_config,
        policy,
        domains,
        target_records,
        report,
        strict,
    )
    return report
