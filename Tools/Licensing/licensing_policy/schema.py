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


def validate_unique_string_array(
    report: Report,
    context: str,
    value: Any,
    *,
    allow_empty: bool = True,
) -> list[str]:
    items = validate_string_array(
        report,
        context,
        value,
        allow_empty=allow_empty,
    )
    duplicates = sorted(
        item for item in set(items) if items.count(item) > 1
    )
    if duplicates:
        report.add(
            "ERROR",
            "DUPLICATE_STRING_ARRAY_ENTRY",
            f"{context}: {duplicates!r}",
        )
    return items


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

    review_record_paths = validate_unique_string_array(
        report,
        "policy.review_record_paths",
        policy.get("review_record_paths"),
        allow_empty=False,
    )
    for path in review_record_paths:
        if not safe_relative_path(path):
            report.add("ERROR", "REVIEW_RECORD_PATH_UNSAFE", path)

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

    documents = validate_unique_string_array(
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
