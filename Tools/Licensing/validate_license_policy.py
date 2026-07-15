#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Arda Koyuncu
# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

from dataclasses import dataclass, field, asdict
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

SUPPORTED_SCHEMA_VERSION = 3
FULL_COMMIT_RE = re.compile(r"^[0-9a-f]{40}$")
SPDX_LINE_RE = re.compile(
    r"SPDX-License-Identifier:\s*([A-Za-z0-9.+(): _-]+)"
)
TEXT_SCAN_LIMIT = 256 * 1024

ALLOWED_DOMAIN_MODES = {
    "fixed-spdx",
    "upstream-file-specific",
    "user-selected",
    "file-specific",
    "unclassified",
    "not-applicable",
}
ALLOWED_REQUIREMENTS = {
    "metadata-or-policy-default",
    "metadata-required",
    "canonical-manifest",
    "upstream-evidence-required",
    "upstream-document",
    "document-index",
    "user-selected",
    "not-applicable",
}
ALLOWED_MIGRATION_STATUS = {
    "unchanged",
    "approved-not-effective",
    "effective",
    "excluded",
    "removed",
}
ALLOWED_TARGET_CURRENT_MODES = {
    "fixed-spdx",
    "mixed-source-composition",
}
ALLOWED_TARGET_MODES = {"fixed-spdx"}

TOP_LEVEL_REQUIRED = {
    "schema_version",
    "policy_revision",
    "policy_status",
    "repository_state",
    "reviewed_branch",
    "reviewed_commit",
    "reviewed_project_version",
    "review_record_paths",
    "decision_record",
    "effective_from_commit",
    "approved_by",
    "validation",
    "metadata",
    "legacy",
    "governance",
    "target_validation",
    "rights_holders",
    "domains",
    "targets",
    "submodules",
}
TOP_LEVEL_KEYS = set(TOP_LEVEL_REQUIRED)
VALIDATION_KEYS = {
    "path_format",
    "case_sensitive",
    "include_untracked_nonignored",
    "unclassified_action",
    "overlap_action",
    "unknown_field_action",
    "metadata_conflict_action",
    "unresolved_license_action",
    "cmake_graph_action",
    "legacy_policy_conflict_action",
    "rename_domain_change_action",
    "lfs_pointer_action",
    "orphan_required_rule_action",
    "external_symlink_action",
    "submodule_inherits_parent_policy",
    "submodule_missing_action",
    "target_drift_action",
    "governance_missing_action",
    "untracked_exclude_patterns",
}
METADATA_KEYS = {"precedence", "conflict_action"}
LEGACY_KEYS = {
    "path_rules_path",
    "path_rules_status",
    "path_rules_source_of_truth",
    "duplicate_license_paths",
    "notice_path",
    "transition_status",
    "cleanup_status",
    "files_are_normative_until_cleanup",
}
GOVERNANCE_KEYS = {
    "required_documents",
    "contributing_document",
    "contributing_section_source",
    "contributing_marker_begin",
    "contributing_marker_end",
    "contributing_update_status",
}
TARGET_VALIDATION_KEYS = {
    "inventory_status",
    "configured_graph_required_in_report",
    "configured_graph_required_in_strict",
    "unrecorded_target_patterns",
    "source_domain_comparison",
    "dependency_comparison",
    "install_surface_comparison",
}
RIGHTS_HOLDER_KEYS = {
    "id",
    "legal_name",
    "status",
    "basis",
    "relicensing_authority",
}
DOMAIN_KEYS = {
    "id",
    "priority",
    "paths",
    "required",
    "domain_owner",
    "rights_holder_refs",
    "current_license_mode",
    "resolution_requirement",
    "current_license_expression",
    "target_license_mode",
    "target_license_expression",
    "provenance_review",
    "licensing_authority_review",
    "migration_status",
    "effective_commit",
    "notes",
    "lifecycle",
    "historical_paths",
}
TARGET_KEYS = {
    "name",
    "source_domains",
    "public_dependencies",
    "private_dependencies",
    "generated_source_patterns",
    "generated_source_action",
    "expected_installed_artifact",
    "expected_exported_target",
    "runtime_dependencies",
    "current_license_mode",
    "current_license_expressions",
    "target_license_mode",
    "target_license_expression",
    "distribution",
    "review",
    "blockers",
    "graph_required",
    "export_validation",
}
SUBMODULE_KEYS = {"path", "commit", "license_mode", "review"}
DOMAIN_REQUIRED_KEYS = DOMAIN_KEYS - {"notes", "lifecycle", "historical_paths"}


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


def unknown_keys(
    report: Report,
    validation: dict[str, Any],
    context: str,
    value: dict[str, Any],
    allowed: set[str],
) -> None:
    for key in sorted(set(value) - allowed):
        emit_action(
            report,
            validation,
            "unknown_field_action",
            "UNKNOWN_POLICY_FIELD",
            f"{context}.{key}",
        )


def require_keys(
    report: Report,
    context: str,
    value: dict[str, Any],
    required: set[str],
) -> None:
    for key in sorted(required - set(value)):
        report.add("ERROR", "MISSING_POLICY_FIELD", f"{context}.{key}")


def validate_string_array(
    report: Report,
    context: str,
    value: Any,
    *,
    allow_empty: bool = True,
) -> list[str]:
    if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
        report.add("ERROR", "STRING_ARRAY_REQUIRED", context)
        return []
    if not allow_empty and not value:
        report.add("ERROR", "NONEMPTY_ARRAY_REQUIRED", context)
    return value


def validate_action_table(report: Report, validation: dict[str, Any]) -> None:
    for key, value in validation.items():
        if key.endswith("_action") and value not in ACTION_TO_SEVERITY:
            report.add("ERROR", "INVALID_VALIDATION_ACTION", f"{key}={value!r}")
    if validation.get("path_format") != "repository-relative-posix":
        report.add("ERROR", "UNSUPPORTED_PATH_FORMAT", repr(validation.get("path_format")))
    if validation.get("case_sensitive") is not True:
        report.add("ERROR", "UNSUPPORTED_CASE_MODE", "case_sensitive must be true")
    if validation.get("submodule_inherits_parent_policy") is not False:
        report.add(
            "ERROR",
            "SUBMODULE_INHERITANCE_UNSAFE",
            "submodule_inherits_parent_policy must be false",
        )
    patterns = validate_string_array(
        report,
        "validation.untracked_exclude_patterns",
        validation.get("untracked_exclude_patterns"),
    )
    for pattern in patterns:
        if not safe_relative_path(pattern):
            report.add("ERROR", "UNTRACKED_EXCLUDE_UNSAFE", pattern)


def validate_schema(policy: dict[str, Any], report: Report) -> None:
    validation = policy.get("validation", {})
    if not isinstance(validation, dict):
        validation = {}
        report.add("ERROR", "POLICY_TABLE_TYPE", "[validation] must be a table")

    require_keys(report, "policy", policy, TOP_LEVEL_REQUIRED)
    unknown_keys(report, validation, "policy", policy, TOP_LEVEL_KEYS)

    tables = (
        ("validation", VALIDATION_KEYS),
        ("metadata", METADATA_KEYS),
        ("legacy", LEGACY_KEYS),
        ("governance", GOVERNANCE_KEYS),
        ("target_validation", TARGET_VALIDATION_KEYS),
    )
    for table_name, allowed in tables:
        table = policy.get(table_name)
        if not isinstance(table, dict):
            report.add("ERROR", "POLICY_TABLE_TYPE", f"[{table_name}] must be a table")
            continue
        unknown_keys(report, validation, table_name, table, allowed)
        require_keys(report, table_name, table, allowed)

    arrays = (
        ("rights_holders", RIGHTS_HOLDER_KEYS),
        ("domains", DOMAIN_KEYS),
        ("targets", TARGET_KEYS),
        ("submodules", SUBMODULE_KEYS),
    )
    for key, allowed in arrays:
        records = policy.get(key)
        if not isinstance(records, list):
            report.add("ERROR", "POLICY_ARRAY_TYPE", f"{key} must be an array")
            continue
        for index, record in enumerate(records, 1):
            if not isinstance(record, dict):
                report.add("ERROR", "POLICY_RECORD_TYPE", f"{key}[{index}]")
                continue
            unknown_keys(report, validation, f"{key}[{index}]", record, allowed)
            required = DOMAIN_REQUIRED_KEYS if key == "domains" else allowed
            require_keys(report, f"{key}[{index}]", record, required)

    validate_action_table(report, validation)


def validate_policy_tables(policy: dict[str, Any], report: Report) -> None:
    validation = policy["validation"]
    metadata = policy["metadata"]
    legacy = policy["legacy"]
    governance = policy["governance"]
    target_validation = policy["target_validation"]

    for key in (
        "policy_revision",
        "policy_status",
        "repository_state",
        "reviewed_branch",
        "reviewed_commit",
        "reviewed_project_version",
        "decision_record",
        "effective_from_commit",
    ):
        if not isinstance(policy.get(key), str):
            report.add("ERROR", "POLICY_STRING_FIELD", key)

    validate_string_array(report, "approved_by", policy.get("approved_by"))

    precedence = validate_string_array(
        report,
        "metadata.precedence",
        metadata.get("precedence"),
        allow_empty=False,
    )
    if len(precedence) != len(set(precedence)):
        report.add("ERROR", "METADATA_PRECEDENCE_DUPLICATE", repr(precedence))
    expected_sources = {
        "applicable-legal-grant",
        "verified-upstream-terms",
        "embedded-spdx",
        "sidecar",
        "reuse-dep5",
        "effective-migration-record",
        "policy-record",
    }
    missing_sources = sorted(expected_sources - set(precedence))
    if missing_sources:
        report.add(
            "ERROR",
            "METADATA_PRECEDENCE_INCOMPLETE",
            repr(missing_sources),
        )
    if metadata.get("conflict_action") not in ACTION_TO_SEVERITY:
        report.add(
            "ERROR",
            "INVALID_METADATA_CONFLICT_ACTION",
            repr(metadata.get("conflict_action")),
        )
    if metadata.get("conflict_action") != validation.get(
        "metadata_conflict_action"
    ):
        report.add(
            "ERROR",
            "METADATA_CONFLICT_ACTION_MISMATCH",
            "metadata.conflict_action must equal "
            "validation.metadata_conflict_action",
        )

    if not isinstance(legacy.get("path_rules_path"), str) or not safe_relative_path(
        legacy["path_rules_path"]
    ):
        report.add("ERROR", "LEGACY_PATH_RULES_PATH", repr(legacy.get("path_rules_path")))
    for key in ("duplicate_license_paths",):
        paths = validate_string_array(
            report,
            f"legacy.{key}",
            legacy.get(key),
            allow_empty=False,
        )
        for path in paths:
            if not safe_relative_path(path):
                report.add("ERROR", "LEGACY_PATH_UNSAFE", path)
    notice_path = legacy.get("notice_path")
    if not isinstance(notice_path, str) or not safe_relative_path(notice_path):
        report.add("ERROR", "LEGACY_NOTICE_PATH", repr(notice_path))
    if legacy.get("cleanup_status") not in {"pending", "complete"}:
        report.add(
            "ERROR",
            "LEGACY_CLEANUP_STATUS",
            repr(legacy.get("cleanup_status")),
        )
    if not isinstance(legacy.get("files_are_normative_until_cleanup"), bool):
        report.add("ERROR", "LEGACY_NORMATIVE_FLAG", "boolean required")

    documents = validate_string_array(
        report,
        "governance.required_documents",
        governance.get("required_documents"),
        allow_empty=False,
    )
    for path in documents:
        if not safe_relative_path(path):
            report.add("ERROR", "GOVERNANCE_PATH_UNSAFE", path)
    for key in ("contributing_document", "contributing_section_source"):
        path = governance.get(key)
        if not isinstance(path, str) or not safe_relative_path(path):
            report.add("ERROR", "GOVERNANCE_PATH", f"{key}={path!r}")
    begin = governance.get("contributing_marker_begin")
    end = governance.get("contributing_marker_end")
    if not isinstance(begin, str) or not begin:
        report.add("ERROR", "GOVERNANCE_MARKER", "begin")
    if not isinstance(end, str) or not end:
        report.add("ERROR", "GOVERNANCE_MARKER", "end")
    if begin == end:
        report.add("ERROR", "GOVERNANCE_MARKER_COLLISION", repr(begin))
    if governance.get("contributing_update_status") not in {
        "required-before-foundation-commit",
        "complete",
    }:
        report.add(
            "ERROR",
            "CONTRIBUTING_UPDATE_STATUS",
            repr(governance.get("contributing_update_status")),
        )

    for key in (
        "configured_graph_required_in_report",
        "configured_graph_required_in_strict",
    ):
        if not isinstance(target_validation.get(key), bool):
            report.add("ERROR", "TARGET_VALIDATION_BOOL", key)
    validate_string_array(
        report,
        "target_validation.unrecorded_target_patterns",
        target_validation.get("unrecorded_target_patterns"),
        allow_empty=False,
    )
    allowed_modes = {
        "source_domain_comparison": {"exact-when-graph-available"},
        "dependency_comparison": {"exact", "declared-subset", "informational"},
        "install_surface_comparison": {"declared-vs-file-api"},
    }
    for key, allowed in allowed_modes.items():
        if target_validation.get(key) not in allowed:
            report.add(
                "ERROR",
                "TARGET_VALIDATION_MODE",
                f"{key}={target_validation.get(key)!r}",
            )


def validate_spdx(
    root: Path,
    report: Report,
    expression: Any,
    context: str,
) -> bool:
    if not isinstance(expression, str):
        report.add("ERROR", "SPDX_EXPRESSION_TYPE", context)
        return False
    try:
        refs = validate_spdx_expression(expression)
    except ValueError as exc:
        report.add("ERROR", "SPDX_EXPRESSION_INVALID", f"{context}: {exc}")
        return False
    for identifier in refs:
        path = root / "LICENSES" / f"{identifier}.txt"
        if not path.is_file():
            report.add(
                "ERROR",
                "LICENSEREF_TEXT_MISSING",
                f"{context}: {path.relative_to(root)}",
            )
    return True


def parse_rights_holders(policy: dict[str, Any], report: Report) -> set[str]:
    seen: set[str] = set()
    for index, record in enumerate(policy.get("rights_holders", []), 1):
        if not isinstance(record, dict):
            continue
        identifier = record.get("id")
        if not isinstance(identifier, str) or not identifier:
            report.add("ERROR", "RIGHTS_HOLDER_ID", f"rights_holders[{index}]")
            continue
        if identifier in seen:
            report.add("ERROR", "RIGHTS_HOLDER_DUPLICATE", identifier)
        seen.add(identifier)
        for key in ("legal_name", "status", "basis", "relicensing_authority"):
            if not isinstance(record.get(key), str) or not record.get(key):
                report.add("ERROR", "RIGHTS_HOLDER_FIELD", f"{identifier}.{key}")
    return seen


def parse_domains(
    root: Path,
    policy: dict[str, Any],
    rights_holders: set[str],
    report: Report,
) -> list[Domain]:
    result: list[Domain] = []
    seen: set[str] = set()

    for index, raw in enumerate(policy.get("domains", []), 1):
        if not isinstance(raw, dict):
            continue
        domain_id = raw.get("id")
        if not isinstance(domain_id, str) or not domain_id:
            report.add("ERROR", "DOMAIN_ID", f"domains[{index}]")
            continue
        if domain_id in seen:
            report.add("ERROR", "DOMAIN_DUPLICATE", domain_id)
        seen.add(domain_id)

        priority = raw.get("priority")
        if not isinstance(priority, int):
            report.add("ERROR", "DOMAIN_PRIORITY", domain_id)
            continue

        migration = raw.get("migration_status")
        paths = validate_string_array(
            report,
            f"{domain_id}.paths",
            raw.get("paths"),
            allow_empty=(migration == "removed"),
        )
        for pattern in paths:
            if not safe_relative_path(pattern):
                report.add("ERROR", "DOMAIN_PATH_UNSAFE", f"{domain_id}: {pattern!r}")

        historical_paths = raw.get("historical_paths", [])
        if historical_paths:
            historical_paths = validate_string_array(
                report,
                f"{domain_id}.historical_paths",
                historical_paths,
                allow_empty=False,
            )
            for pattern in historical_paths:
                if not safe_relative_path(pattern):
                    report.add(
                        "ERROR",
                        "DOMAIN_HISTORICAL_PATH_UNSAFE",
                        f"{domain_id}: {pattern!r}",
                    )

        if migration == "removed":
            if paths:
                report.add("ERROR", "REMOVED_DOMAIN_HAS_ACTIVE_PATHS", domain_id)
            if raw.get("required") is not False:
                report.add("ERROR", "REMOVED_DOMAIN_REQUIRED", domain_id)
            if not historical_paths:
                report.add("ERROR", "REMOVED_DOMAIN_HISTORY_MISSING", domain_id)

        refs = validate_string_array(
            report,
            f"{domain_id}.rights_holder_refs",
            raw.get("rights_holder_refs"),
        )
        unknown_refs = sorted(set(refs) - rights_holders)
        if unknown_refs:
            report.add("ERROR", "RIGHTS_HOLDER_UNKNOWN", f"{domain_id}: {unknown_refs}")

        current_mode = raw.get("current_license_mode")
        target_mode = raw.get("target_license_mode")
        requirement = raw.get("resolution_requirement")
        current_expression = raw.get("current_license_expression")
        target_expression = raw.get("target_license_expression")
        effective = raw.get("effective_commit")

        if current_mode not in ALLOWED_DOMAIN_MODES:
            report.add("ERROR", "LICENSE_MODE", f"{domain_id}.current={current_mode!r}")
        if target_mode not in ALLOWED_DOMAIN_MODES:
            report.add("ERROR", "LICENSE_MODE", f"{domain_id}.target={target_mode!r}")
        if requirement not in ALLOWED_REQUIREMENTS:
            report.add("ERROR", "RESOLUTION_REQUIREMENT", f"{domain_id}: {requirement!r}")

        for side, mode, expression in (
            ("current", current_mode, current_expression),
            ("target", target_mode, target_expression),
        ):
            if not isinstance(expression, str):
                report.add("ERROR", "LICENSE_EXPRESSION_TYPE", f"{domain_id}.{side}")
            elif mode == "fixed-spdx":
                validate_spdx(root, report, expression, f"{domain_id}.{side}")
            elif expression:
                report.add(
                    "ERROR",
                    "LICENSE_EXPRESSION_MUST_BE_EMPTY",
                    f"{domain_id}.{side}: mode={mode}",
                )

        if migration not in ALLOWED_MIGRATION_STATUS:
            report.add("ERROR", "MIGRATION_STATUS", f"{domain_id}: {migration!r}")
        if not isinstance(effective, str):
            report.add("ERROR", "EFFECTIVE_COMMIT_TYPE", domain_id)
        elif effective and not FULL_COMMIT_RE.fullmatch(effective):
            report.add("ERROR", "EFFECTIVE_COMMIT_FORMAT", f"{domain_id}: {effective}")

        if migration == "effective" and not effective:
            report.add("ERROR", "EFFECTIVE_WITHOUT_COMMIT", domain_id)
        if migration != "effective" and effective:
            report.add("ERROR", "COMMIT_WITHOUT_EFFECTIVE_STATUS", domain_id)
        if (
            migration == "unchanged"
            and current_mode == "fixed-spdx"
            and target_mode == "fixed-spdx"
            and current_expression != target_expression
        ):
            report.add("ERROR", "UNCHANGED_LICENSE_DIFFERS", domain_id)

        if not isinstance(raw.get("required"), bool):
            report.add("ERROR", "DOMAIN_REQUIRED_TYPE", domain_id)

        result.append(Domain(raw, domain_id, priority, paths, bool(raw.get("required"))))
    return result


def classify(
    path: str,
    domains: list[Domain],
    *,
    case_sensitive: bool,
) -> tuple[Domain | None, list[Domain]]:
    matches = [
        domain
        for domain in domains
        if any(
            gitwildmatch(path, pattern, case_sensitive=case_sensitive)
            for pattern in domain.paths
        )
    ]
    if not matches:
        return None, []
    highest = max(domain.priority for domain in matches)
    winners = [domain for domain in matches if domain.priority == highest]
    if len(winners) != 1:
        return None, winners
    return winners[0], winners


def list_repository_files(
    root: Path,
    include_untracked: bool,
    untracked_exclude_patterns: list[str],
    *,
    case_sensitive: bool,
) -> list[str]:
    tracked = {
        line
        for line in run_git(root, "ls-files", "--cached").splitlines()
        if line
    }
    deleted = {
        line
        for line in run_git(root, "ls-files", "--deleted").splitlines()
        if line
    }
    files = tracked - deleted
    if include_untracked:
        for line in run_git(
            root,
            "ls-files",
            "--others",
            "--exclude-standard",
        ).splitlines():
            if not line:
                continue
            if any(
                gitwildmatch(
                    line,
                    pattern,
                    case_sensitive=case_sensitive,
                )
                for pattern in untracked_exclude_patterns
            ):
                continue
            files.add(line)
    return sorted(files)


def tracked_modes(root: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for line in run_git(root, "ls-files", "-s").splitlines():
        match = re.match(r"^(\d+) [0-9a-f]{40} \d+\t(.+)$", line)
        if match:
            mode, path = match.groups()
            result[path] = mode
    return result


def read_text_prefix(path: Path) -> str | None:
    try:
        data = path.read_bytes()[:TEXT_SCAN_LIMIT]
    except OSError:
        return None
    if b"\x00" in data:
        return None
    return data.decode("utf-8", errors="replace")


def embedded_spdx(path: Path) -> str:
    text = read_text_prefix(path)
    if text is None:
        return ""
    match = SPDX_LINE_RE.search(text)
    return match.group(1).strip() if match else ""


def sidecar_spdx(path: Path) -> str:
    sidecar = Path(str(path) + ".license")
    return embedded_spdx(sidecar) if sidecar.is_file() else ""


def parse_dep5(root: Path, report: Report) -> list[tuple[list[str], str]]:
    path = root / ".reuse" / "dep5"
    if not path.is_file():
        return []
    records: list[tuple[list[str], str]] = []
    text = path.read_text(encoding="utf-8", errors="replace")
    for index, paragraph in enumerate(re.split(r"\n\s*\n", text), 1):
        files_match = re.search(
            r"^Files:\s*(.+(?:\n[ \t].+)*)$",
            paragraph,
            re.MULTILINE,
        )
        license_match = re.search(
            r"^License:\s*([^\r\n]+)$",
            paragraph,
            re.MULTILINE,
        )
        if not files_match and not license_match:
            continue
        if not files_match or not license_match:
            report.add("ERROR", "DEP5_INCOMPLETE_STANZA", f"paragraph {index}")
            continue
        raw_files = re.sub(r"\n[ \t]+", " ", files_match.group(1)).strip()
        expression = re.sub(r"\n[ \t]+", " ", license_match.group(1)).strip()
        patterns = raw_files.split()
        if not patterns:
            report.add("ERROR", "DEP5_EMPTY_FILES", f"paragraph {index}")
            continue
        validate_spdx(root, report, expression, f".reuse/dep5 paragraph {index}")
        records.append((patterns, expression))
    return records


def dep5_spdx(
    path: str,
    records: list[tuple[list[str], str]],
    *,
    case_sensitive: bool,
) -> list[str]:
    return [
        expression
        for patterns, expression in records
        if any(
            gitwildmatch(path, pattern, case_sensitive=case_sensitive)
            for pattern in patterns
        )
    ]


def canonical_manifest_paths(root: Path, report: Report) -> set[str]:
    path = root / "LICENSE_TEXT_SOURCES.toml"
    if not path.is_file():
        report.add("ERROR", "CANONICAL_MANIFEST_MISSING", str(path.relative_to(root)))
        return set()
    try:
        data = tomllib.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:
        report.add("ERROR", "CANONICAL_MANIFEST_PARSE", str(exc))
        return set()
    return {
        record.get("path")
        for record in data.get("files", [])
        if isinstance(record, dict) and isinstance(record.get("path"), str)
    }


def deepest_submodule_boundary(path: str, submodule_paths: set[str]) -> str | None:
    matches = [
        candidate
        for candidate in submodule_paths
        if path == candidate or path.startswith(candidate + "/")
    ]
    return max(matches, key=len) if matches else None


def upstream_license_evidence(
    root: Path,
    relative: str,
    submodule_paths: set[str],
) -> str:
    candidate_path = root / relative
    current = candidate_path if candidate_path.is_dir() else candidate_path.parent
    boundary_text = deepest_submodule_boundary(relative, submodule_paths)
    boundary = root / boundary_text if boundary_text else root / "Vendor"
    names = (
        "LICENSE",
        "LICENSE.txt",
        "LICENSE.TXT",
        "License.txt",
        "LICENSE",
        "COPYING",
        "COPYING.txt",
    )
    while True:
        for name in names:
            candidate = current / name
            if candidate.is_file():
                try:
                    return candidate.relative_to(root).as_posix()
                except ValueError:
                    return ""
        if current.resolve() == boundary.resolve():
            break
        if boundary.resolve() not in current.resolve().parents:
            break
        current = current.parent
    return ""


def resolve_file(
    root: Path,
    path: str,
    domain: Domain,
    dep5_records: list[tuple[list[str], str]],
    manifest_paths: set[str],
    submodule_paths: set[str],
    policy: dict[str, Any],
    report: Report,
) -> Resolution:
    validation = policy["validation"]
    case_sensitive = bool(validation["case_sensitive"])
    requirement = domain.raw["resolution_requirement"]
    current_mode = domain.raw["current_license_mode"]
    current_expression = domain.raw["current_license_expression"]

    metadata: list[tuple[str, str]] = []
    embedded = embedded_spdx(root / path)
    if embedded:
        metadata.append(("embedded-spdx", embedded))
    sidecar = sidecar_spdx(root / path)
    if sidecar:
        metadata.append(("sidecar", sidecar))
    for expression in dep5_spdx(path, dep5_records, case_sensitive=case_sensitive):
        metadata.append(("reuse-dep5", expression))

    valid_metadata: list[tuple[str, str]] = []
    for source, expression in metadata:
        if validate_spdx(root, report, expression, f"{path}:{source}"):
            valid_metadata.append((source, expression))

    expressions = {expression for _, expression in valid_metadata}
    if len(expressions) > 1:
        emit_action(
            report,
            validation,
            "metadata_conflict_action",
            "LICENSE_METADATA_CONFLICT",
            f"{path}: {valid_metadata}",
        )
        return Resolution(domain.id, True, False, evidence=[str(valid_metadata)])

    if valid_metadata:
        expression = valid_metadata[0][1]
        if (
            current_mode == "fixed-spdx"
            and current_expression
            and expression != current_expression
        ):
            emit_action(
                report,
                validation,
                "metadata_conflict_action",
                "LICENSE_POLICY_CONFLICT",
                f"{path}: metadata={expression}, policy={current_expression}",
            )
            return Resolution(
                domain.id,
                True,
                False,
                expression,
                valid_metadata[0][0],
                [f"{source}:{expr}" for source, expr in valid_metadata],
            )
        precedence = policy["metadata"]["precedence"]
        rank = {name: index for index, name in enumerate(precedence)}
        source, expression = min(
            valid_metadata,
            key=lambda item: rank.get(item[0], len(rank)),
        )
        return Resolution(
            domain.id,
            True,
            True,
            expression,
            source,
            [f"{src}:{expr}" for src, expr in valid_metadata],
        )

    if requirement == "metadata-or-policy-default":
        if current_mode == "fixed-spdx" and current_expression:
            return Resolution(
                domain.id,
                True,
                True,
                current_expression,
                "policy-current-default",
            )

    if requirement == "canonical-manifest" and path in manifest_paths:
        return Resolution(
            domain.id,
            True,
            True,
            "FILE-SPECIFIC",
            "canonical-manifest",
        )

    if requirement == "document-index":
        return Resolution(domain.id, True, True, "FILE-SPECIFIC", "document-index")

    if requirement == "upstream-document":
        return Resolution(domain.id, True, True, "UPSTREAM", "upstream-document")

    if requirement == "upstream-evidence-required":
        evidence = upstream_license_evidence(root, path, submodule_paths)
        if evidence:
            return Resolution(
                domain.id,
                True,
                True,
                "UPSTREAM",
                "bounded-upstream-license",
                [evidence],
            )

    if requirement in {"user-selected", "not-applicable"}:
        return Resolution(
            domain.id,
            True,
            True,
            requirement.upper(),
            requirement,
        )

    emit_action(
        report,
        validation,
        "unresolved_license_action",
        "LICENSE_UNRESOLVED",
        f"{path}: domain={domain.id}, requirement={requirement}",
    )
    return Resolution(domain.id, True, False)


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


def validate_files(
    root: Path,
    files: list[str],
    domains: list[Domain],
    submodule_paths: set[str],
    policy: dict[str, Any],
    report: Report,
) -> None:
    validation = policy["validation"]
    case_sensitive = bool(validation["case_sensitive"])
    matched_by_domain = {domain.id: 0 for domain in domains}
    dep5_records = parse_dep5(root, report)
    manifest_paths = canonical_manifest_paths(root, report)

    for path in files:
        domain, winners = classify(
            path,
            domains,
            case_sensitive=case_sensitive,
        )
        if domain is None:
            if winners:
                emit_action(
                    report,
                    validation,
                    "overlap_action",
                    "AMBIGUOUS_DOMAIN",
                    f"{path}: {sorted(item.id for item in winners)}",
                )
            else:
                emit_action(
                    report,
                    validation,
                    "unclassified_action",
                    "UNCLASSIFIED",
                    path,
                )
            continue

        matched_by_domain[domain.id] += 1
        report.classified[path] = domain.id
        report.resolutions[path] = resolve_file(
            root,
            path,
            domain,
            dep5_records,
            manifest_paths,
            submodule_paths,
            policy,
            report,
        )

    for domain in domains:
        if domain.required and matched_by_domain[domain.id] == 0:
            emit_action(
                report,
                validation,
                "orphan_required_rule_action",
                "ORPHAN_REQUIRED_DOMAIN",
                f"{domain.id}: no file selected",
            )


def validate_commits(root: Path, policy: dict[str, Any], report: Report) -> None:
    for domain in policy.get("domains", []):
        commit = domain.get("effective_commit", "")
        if not commit:
            continue
        result = run(
            ["git", "cat-file", "-e", f"{commit}^{{commit}}"],
            root,
            check=False,
        )
        if result.returncode != 0:
            report.add(
                "ERROR",
                "EFFECTIVE_COMMIT_MISSING",
                f"{domain.get('id')}: {commit}",
            )

    for label, commit in (
        ("reviewed_commit", policy.get("reviewed_commit", "")),
        ("effective_from_commit", policy.get("effective_from_commit", "")),
    ):
        if not commit:
            continue
        if not isinstance(commit, str) or not FULL_COMMIT_RE.fullmatch(commit):
            report.add("ERROR", "COMMIT_FORMAT", f"{label}: {commit!r}")
            continue
        result = run(
            ["git", "cat-file", "-e", f"{commit}^{{commit}}"],
            root,
            check=False,
        )
        if result.returncode != 0:
            report.add("ERROR", "COMMIT_MISSING", f"{label}: {commit}")

    reviewed = policy.get("reviewed_commit", "")
    if isinstance(reviewed, str) and FULL_COMMIT_RE.fullmatch(reviewed):
        from fnmatch import fnmatchcase

        current = run_git(root, "rev-parse", "HEAD").strip()
        if current != reviewed:
            ancestor = run(
                ["git", "merge-base", "--is-ancestor", reviewed, current],
                root,
                check=False,
            )

            if ancestor.returncode != 0:
                report.add(
                    "WARNING",
                    "EVIDENCE_BASELINE_DIVERGED",
                    f"reviewed={reviewed}, current={current}",
                )
            else:
                changed_paths = [
                    line.strip()
                    for line in run_git(
                        root,
                        "diff",
                        "--name-only",
                        f"{reviewed}..{current}",
                    ).splitlines()
                    if line.strip()
                ]

                allowed_patterns = policy.get("review_record_paths", [])
                if (
                    not isinstance(allowed_patterns, list)
                    or any(
                        not isinstance(pattern, str) or not pattern
                        for pattern in allowed_patterns
                    )
                ):
                    report.add(
                        "ERROR",
                        "REVIEW_RECORD_PATHS_INVALID",
                        "policy.review_record_paths",
                    )
                    allowed_patterns = []

                unreviewed_paths = [
                    changed
                    for changed in changed_paths
                    if not any(
                        isinstance(pattern, str)
                        and fnmatchcase(changed, pattern)
                        for pattern in allowed_patterns
                    )
                ]

                if unreviewed_paths:
                    evaluation = ", ".join(unreviewed_paths[:8])
                    if len(unreviewed_paths) > 8:
                        evaluation += f", ... (+{len(unreviewed_paths) - 8})"

                    report.add(
                        "WARNING",
                        "EVIDENCE_BASELINE_DRIFT",
                        (
                            f"reviewed={reviewed}, current={current}, "
                            f"unreviewed_paths={evaluation}"
                        ),
                    )

    if policy.get("policy_status") in {"active", "effective"}:
        if not policy.get("approved_by"):
            report.add("ERROR", "APPROVAL_MISSING", "active policy")
        if not policy.get("decision_record"):
            report.add("ERROR", "DECISION_RECORD_MISSING", "active policy")


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


def validate_symlinks(root: Path, policy: dict[str, Any], report: Report) -> None:
    validation = policy["validation"]
    for path, mode in tracked_modes(root).items():
        if mode != "120000":
            continue
        link_path = root / path
        if not link_path.exists() and not link_path.is_symlink():
            continue
        target = link_path.resolve(strict=False)
        try:
            target.relative_to(root)
        except ValueError:
            emit_action(
                report,
                validation,
                "external_symlink_action",
                "EXTERNAL_SYMLINK",
                f"{path} -> {target}",
            )


def validate_lfs(
    root: Path,
    files: list[str],
    policy: dict[str, Any],
    report: Report,
) -> None:
    validation = policy["validation"]
    if not files:
        return
    payload = "\n".join(files) + "\n"
    attrs = run(
        ["git", "check-attr", "filter", "--stdin"],
        root,
        check=False,
        input_text=payload,
    )
    attributed = []
    if attrs.returncode == 0:
        for line in attrs.stdout.splitlines():
            match = re.match(r"^(.+): filter: lfs$", line)
            if match:
                attributed.append(match.group(1))

    tracked = set(run_git(root, "ls-files").splitlines())
    pointer_prefix = b"version https://git-lfs.github.com/spec/v1"
    for path in attributed:
        if path not in tracked:
            report.add("INFO", "LFS_UNTRACKED_WILL_BE_CLEANED", path)
            continue
        blob = run_bytes(["git", "show", f":{path}"], root)
        if blob.returncode != 0:
            emit_action(
                report,
                validation,
                "lfs_pointer_action",
                "LFS_INDEX_BLOB_MISSING",
                path,
            )
            continue
        if not blob.stdout.startswith(pointer_prefix):
            emit_action(
                report,
                validation,
                "lfs_pointer_action",
                "LFS_INDEX_POINTER_INVALID",
                path,
            )

    lfs = run(["git", "lfs", "version"], root, check=False)
    if attributed and lfs.returncode != 0:
        report.add(
            "WARNING",
            "GIT_LFS_UNAVAILABLE",
            f"{len(attributed)} attributed paths",
        )
    elif lfs.returncode == 0:
        fsck = run(["git", "lfs", "fsck", "--pointers"], root, check=False)
        if fsck.returncode != 0:
            emit_action(
                report,
                validation,
                "lfs_pointer_action",
                "GIT_LFS_FSCK",
                fsck.stderr.strip() or fsck.stdout.strip(),
            )


def validate_renames(
    root: Path,
    base_ref: str | None,
    domains: list[Domain],
    policy: dict[str, Any],
    report: Report,
) -> None:
    validation = policy["validation"]
    comparisons: list[tuple[str, list[str]]] = [
        ("working-tree", ["git", "diff", "--name-status", "-M"]),
        ("index", ["git", "diff", "--cached", "--name-status", "-M"]),
    ]
    if base_ref:
        comparisons.append(
            (
                f"{base_ref}...HEAD",
                ["git", "diff", "--name-status", "-M", f"{base_ref}...HEAD"],
            )
        )

    case_sensitive = bool(validation["case_sensitive"])
    seen: set[tuple[str, str]] = set()

    for label, command in comparisons:
        result = run(command, root, check=False)
        if result.returncode != 0:
            if base_ref and label == f"{base_ref}...HEAD":
                report.add("ERROR", "BASE_REF_INVALID", base_ref)
            else:
                report.add("ERROR", "RENAME_DIFF_FAILED", label)
            continue

        for line in result.stdout.splitlines():
            parts = line.split("\t")
            if len(parts) != 3 or not parts[0].startswith("R"):
                continue
            old, new_path = parts[1], parts[2]
            pair = (old, new_path)
            if pair in seen:
                continue
            seen.add(pair)

            old_domain, _ = classify(
                old,
                domains,
                case_sensitive=case_sensitive,
            )
            new_domain, _ = classify(
                new_path,
                domains,
                case_sensitive=case_sensitive,
            )
            old_id = old_domain.id if old_domain else "UNCLASSIFIED"
            new_id = new_domain.id if new_domain else "UNCLASSIFIED"
            if old_id != new_id:
                emit_action(
                    report,
                    validation,
                    "rename_domain_change_action",
                    "RENAME_DOMAIN_CHANGE",
                    f"[{label}] {old} ({old_id}) -> "
                    f"{new_path} ({new_id})",
                )



def latest_cmake_codemodel(reply_dir: Path) -> Path:
    indexes = sorted(
        reply_dir.glob("index-*.json"),
        key=lambda path: path.stat().st_mtime,
    )
    if indexes:
        index = json.loads(indexes[-1].read_text(encoding="utf-8"))
        candidates: list[str] = []

        def walk(value: Any) -> None:
            if isinstance(value, dict):
                if value.get("kind") == "codemodel" and value.get("jsonFile"):
                    version = value.get("version", {})
                    if version.get("major") == 2:
                        candidates.append(value["jsonFile"])
                for child in value.values():
                    walk(child)
            elif isinstance(value, list):
                for child in value:
                    walk(child)

        walk(index)
        if candidates:
            return reply_dir / candidates[-1]

    codemodels = sorted(
        reply_dir.glob("codemodel-v2-*.json"),
        key=lambda path: path.stat().st_mtime,
    )
    if not codemodels:
        raise FileNotFoundError(f"No CMake File API codemodel in {reply_dir}")
    return codemodels[-1]


def load_cmake_graph(
    build_dir: Path,
    selected_config: str | None,
) -> dict[str, dict[str, dict[str, Any]]]:
    reply_dir = build_dir / ".cmake" / "api" / "v1" / "reply"
    codemodel = json.loads(
        latest_cmake_codemodel(reply_dir).read_text(encoding="utf-8")
    )
    result: dict[str, dict[str, dict[str, Any]]] = {}

    for configuration in codemodel.get("configurations", []):
        config_name = configuration.get("name", "") or "<default>"
        if selected_config and config_name != selected_config:
            continue
        id_to_name = {
            item.get("id"): item.get("name")
            for item in configuration.get("targets", [])
        }
        targets: dict[str, dict[str, Any]] = {}
        for summary in configuration.get("targets", []):
            data = json.loads(
                (reply_dir / summary["jsonFile"]).read_text(encoding="utf-8")
            )
            dependencies = {
                id_to_name.get(dep.get("id"), dep.get("id", ""))
                for dep in data.get("dependencies", [])
            }
            installs = []
            install = data.get("install")
            if isinstance(install, dict):
                installs = [
                    item.get("path")
                    for item in install.get("destinations", [])
                    if item.get("path")
                ]
            targets[data.get("name", summary.get("name", ""))] = {
                "type": data.get("type", ""),
                "source_base": data.get("paths", {}).get("source", ""),
                "build_base": data.get("paths", {}).get("build", ""),
                "sources": [
                    {
                        "path": item.get("path"),
                        "generated": bool(item.get("isGenerated", False)),
                    }
                    for item in data.get("sources", [])
                    if item.get("path")
                ],
                "dependencies": {item for item in dependencies if item},
                "installs": installs,
            }
        result[config_name] = targets

    if selected_config and selected_config not in result:
        raise ValueError(f"CMake configuration not found: {selected_config}")
    if not result:
        raise ValueError("CMake codemodel contains no selected configuration")
    return result


def normalize_cmake_source(
    root: Path,
    build_dir: Path,
    source_base: str,
    build_base: str,
    source: str,
) -> tuple[str | None, str]:
    source_path = Path(source)
    candidates: list[Path] = []
    if source_path.is_absolute():
        candidates.append(source_path)
    else:
        candidates.extend(
            [
                root / source_path,
                root / source_base / source_path,
                build_dir / source_path,
                build_dir / build_base / source_path,
            ]
        )
    for candidate in candidates:
        resolved = candidate.resolve()
        try:
            return resolved.relative_to(root).as_posix(), resolved.as_posix()
        except ValueError:
            if build_dir.resolve() in resolved.parents or resolved == build_dir.resolve():
                return None, resolved.as_posix()
    return None, source


def parse_targets(
    root: Path,
    policy: dict[str, Any],
    domain_ids: set[str],
    report: Report,
) -> dict[str, dict[str, Any]]:
    result: dict[str, dict[str, Any]] = {}
    for index, record in enumerate(policy.get("targets", []), 1):
        if not isinstance(record, dict):
            continue
        name = record.get("name")
        if not isinstance(name, str) or not name:
            report.add("ERROR", "TARGET_NAME", f"targets[{index}]")
            continue
        if name in result:
            report.add("ERROR", "TARGET_DUPLICATE", name)

        source_domains = validate_string_array(
            report,
            f"{name}.source_domains",
            record.get("source_domains"),
            allow_empty=False,
        )
        unknown = sorted(set(source_domains) - domain_ids)
        if unknown:
            report.add("ERROR", "TARGET_UNKNOWN_DOMAIN", f"{name}: {unknown}")

        for field_name in (
            "public_dependencies",
            "private_dependencies",
            "generated_source_patterns",
            "runtime_dependencies",
            "current_license_expressions",
            "blockers",
        ):
            validate_string_array(report, f"{name}.{field_name}", record.get(field_name))

        for field_name in (
            "expected_installed_artifact",
            "expected_exported_target",
            "graph_required",
        ):
            if not isinstance(record.get(field_name), bool):
                report.add("ERROR", "TARGET_BOOL_FIELD", f"{name}.{field_name}")

        current_mode = record.get("current_license_mode")
        if current_mode not in ALLOWED_TARGET_CURRENT_MODES:
            report.add("ERROR", "TARGET_CURRENT_MODE", f"{name}: {current_mode!r}")
        expressions = record.get("current_license_expressions", [])
        if current_mode == "fixed-spdx" and len(expressions) != 1:
            report.add("ERROR", "TARGET_FIXED_EXPRESSION_COUNT", name)
        if current_mode == "mixed-source-composition" and len(expressions) < 2:
            report.add("ERROR", "TARGET_MIXED_EXPRESSION_COUNT", name)
        for expression in expressions:
            validate_spdx(root, report, expression, f"{name}.current")

        target_mode = record.get("target_license_mode")
        if target_mode not in ALLOWED_TARGET_MODES:
            report.add("ERROR", "TARGET_LICENSE_MODE", f"{name}: {target_mode!r}")
        validate_spdx(
            root,
            report,
            record.get("target_license_expression"),
            f"{name}.target",
        )

        if record.get("generated_source_action") not in ACTION_TO_SEVERITY:
            report.add("ERROR", "TARGET_GENERATED_ACTION", name)
        export_validation = record.get("export_validation")
        if export_validation not in {
            "manual-pending",
            "manual-complete",
            "external-evidence",
        }:
            report.add("ERROR", "TARGET_EXPORT_VALIDATION", name)
        elif (
            record.get("expected_exported_target") is True
            and export_validation == "manual-pending"
        ):
            report.add(
                "WARNING",
                "TARGET_EXPORT_REVIEW_PENDING",
                name,
            )

        for field_name in ("distribution", "review"):
            if not isinstance(record.get(field_name), str) or not record.get(field_name):
                report.add("ERROR", "TARGET_STRING_FIELD", f"{name}.{field_name}")

        result[name] = record
    return result


def validate_cmake_targets(
    root: Path,
    build_dir: Path | None,
    cmake_config: str | None,
    policy: dict[str, Any],
    domains: list[Domain],
    records: dict[str, dict[str, Any]],
    report: Report,
    strict: bool,
) -> None:
    validation = policy["validation"]
    target_validation = policy["target_validation"]
    required = (
        target_validation["configured_graph_required_in_strict"]
        if strict
        else target_validation["configured_graph_required_in_report"]
    )
    if build_dir is None:
        action = "deny" if required else validation["cmake_graph_action"]
        report.add(
            action_severity(action),
            "CMAKE_GRAPH_NOT_CHECKED",
            "no --cmake-build-dir provided",
        )
        return

    try:
        configurations = load_cmake_graph(build_dir, cmake_config)
    except Exception as exc:
        action = "deny" if required else validation["cmake_graph_action"]
        report.add(action_severity(action), "CMAKE_GRAPH_UNAVAILABLE", str(exc))
        return

    case_sensitive = bool(validation["case_sensitive"])
    for config_name, actual in configurations.items():
        prefix = f"[{config_name}]"
        for name, record in records.items():
            target = actual.get(name)
            if target is None:
                if record["graph_required"]:
                    emit_action(
                        report,
                        validation,
                        "target_drift_action",
                        "TARGET_NOT_IN_CONFIGURATION",
                        f"{prefix} {name}",
                    )
                continue

            actual_domains: set[str] = set()
            generated_paths: list[str] = []
            for item in target["sources"]:
                relative, normalized = normalize_cmake_source(
                    root,
                    build_dir,
                    target["source_base"],
                    target["build_base"],
                    item["path"],
                )
                if item["generated"]:
                    generated_paths.append(normalized)
                    continue
                if relative is None:
                    continue
                domain, winners = classify(
                    relative,
                    domains,
                    case_sensitive=case_sensitive,
                )
                if domain:
                    actual_domains.add(domain.id)
                elif winners:
                    report.add(
                        "ERROR",
                        "TARGET_SOURCE_AMBIGUOUS",
                        f"{prefix} {name}: {relative}",
                    )
                else:
                    report.add(
                        "ERROR",
                        "TARGET_SOURCE_UNCLASSIFIED",
                        f"{prefix} {name}: {relative}",
                    )

            expected_domains = set(record["source_domains"])
            if actual_domains != expected_domains:
                emit_action(
                    report,
                    validation,
                    "target_drift_action",
                    "TARGET_SOURCE_DOMAIN_DRIFT",
                    f"{prefix} {name}: expected={sorted(expected_domains)}, "
                    f"actual={sorted(actual_domains)}",
                )

            patterns = record["generated_source_patterns"]
            unmatched_generated = [
                path
                for path in generated_paths
                if not any(
                    gitwildmatch(
                        PurePosixPath(path).as_posix().lstrip("/"),
                        pattern,
                        case_sensitive=case_sensitive,
                    )
                    for pattern in patterns
                )
            ]
            if unmatched_generated:
                report.add(
                    action_severity(record["generated_source_action"]),
                    "TARGET_GENERATED_SOURCE_UNRECORDED",
                    f"{prefix} {name}: {unmatched_generated[:10]}",
                )

            declared = set(record["public_dependencies"]) | set(
                record["private_dependencies"]
            )
            dependency_mode = target_validation["dependency_comparison"]

            if dependency_mode == "exact":
                missing = declared - target["dependencies"]
                extra = target["dependencies"] - declared

                if missing:
                    emit_action(
                        report,
                        validation,
                        "target_drift_action",
                        "TARGET_DEPENDENCY_MISSING",
                        f"{prefix} {name}: {sorted(missing)}",
                    )

                if extra:
                    emit_action(
                        report,
                        validation,
                        "target_drift_action",
                        "TARGET_DEPENDENCY_UNDECLARED",
                        f"{prefix} {name}: {sorted(extra)}",
                    )

            elif dependency_mode == "declared-subset":
                missing = declared - target["dependencies"]

                if missing:
                    emit_action(
                        report,
                        validation,
                        "target_drift_action",
                        "TARGET_DEPENDENCY_MISSING",
                        f"{prefix} {name}: {sorted(missing)}",
                    )

            elif dependency_mode != "informational":
                report.add(
                    "ERROR",
                    "TARGET_DEPENDENCY_MODE",
                    repr(dependency_mode),
                )

            installed = bool(target["installs"])
            if installed != record["expected_installed_artifact"]:
                emit_action(
                    report,
                    validation,
                    "target_drift_action",
                    "TARGET_INSTALL_DRIFT",
                    f"{prefix} {name}: policy="
                    f"{record['expected_installed_artifact']}, actual={installed}",
                )

        patterns = target_validation["unrecorded_target_patterns"]
        inventory_target_types = {
            "STATIC_LIBRARY",
            "SHARED_LIBRARY",
            "MODULE_LIBRARY",
            "OBJECT_LIBRARY",
            "INTERFACE_LIBRARY",
        }

        for name in sorted(set(actual) - set(records)):
            target = actual[name]
            relevant = (
                target.get("type") in inventory_target_types
                or bool(target.get("installs"))
            )

            if relevant and any(
                gitwildmatch(name, pattern, case_sensitive=case_sensitive)
                for pattern in patterns
            ):
                emit_action(
                    report,
                    validation,
                    "target_drift_action",
                    "TARGET_UNRECORDED",
                    f"{prefix} {name}",
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
    validate_commits(root, policy, report)
    validate_legacy(root, policy, report)
    validate_governance(root, policy, report)
    validate_submodules(root, submodule_map, policy, report)
    validate_symlinks(root, policy, report)
    validate_lfs(root, files, policy, report)
    validate_renames(root, base_ref, domains, policy, report)
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


def print_report(report: Report) -> None:
    order = {"ERROR": 0, "WARNING": 1, "INFO": 2}
    for finding in sorted(
        report.findings,
        key=lambda item: (
            order.get(item.severity, 9),
            item.code,
            item.message,
        ),
    ):
        print(finding.render())

    resolved = sum(item.resolved for item in report.resolutions.values())
    unresolved = sum(not item.resolved for item in report.resolutions.values())
    errors = sum(item.severity == "ERROR" for item in report.findings)
    warnings = sum(item.severity == "WARNING" for item in report.findings)
    infos = sum(item.severity == "INFO" for item in report.findings)

    print()
    print(f"Domain-matched files: {len(report.classified)}")
    print(f"License-resolved files: {resolved}")
    print(f"License-unresolved files: {unresolved}")
    print(f"Errors: {errors}")
    print(f"Warnings: {warnings}")
    print(f"Info: {infos}")


def write_json_report(path: Path, report: Report) -> None:
    payload = {
        "findings": [asdict(item) for item in report.findings],
        "classified": report.classified,
        "resolutions": {
            key: asdict(value)
            for key, value in report.resolutions.items()
        },
        "summary": {
            "domain_matched": len(report.classified),
            "license_resolved": sum(
                item.resolved for item in report.resolutions.values()
            ),
            "license_unresolved": sum(
                not item.resolved for item in report.resolutions.values()
            ),
            "errors": sum(
                item.severity == "ERROR" for item in report.findings
            ),
            "warnings": sum(
                item.severity == "WARNING" for item in report.findings
            ),
        },
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Validate SagaEngine licensing policy and repository evidence.",
    )
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--report", action="store_true")
    mode.add_argument("--strict", action="store_true")
    parser.add_argument("--root", type=Path)
    parser.add_argument("--policy", type=Path)
    parser.add_argument("--cmake-build-dir", type=Path)
    parser.add_argument("--cmake-config")
    parser.add_argument("--base-ref")
    parser.add_argument("--json-output", type=Path)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        root = repository_root(args.root)
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2

    policy_path = (
        args.policy.resolve()
        if args.policy
        else root / "LICENSE_POLICY.toml"
    )
    if not policy_path.is_file():
        print(f"ERROR: policy not found: {policy_path}", file=sys.stderr)
        return 2

    build_dir = args.cmake_build_dir.resolve() if args.cmake_build_dir else None
    report = validate_policy(
        root,
        policy_path,
        build_dir=build_dir,
        cmake_config=args.cmake_config,
        base_ref=args.base_ref,
        strict=args.strict,
    )
    print_report(report)
    if args.json_output:
        write_json_report(args.json_output, report)
    return 1 if args.strict and report.errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
