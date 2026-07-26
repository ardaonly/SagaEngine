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


def composed_object_source(root: Path, generated_source: str) -> str | None:
    """Recover the owned source encoded by a CMake OBJECT library artifact."""
    normalized = PurePosixPath(generated_source).as_posix()
    marker = ".dir/"
    if marker not in normalized:
        return None

    candidate = normalized.split(marker, 1)[1]
    if candidate.startswith("./"):
        candidate = candidate[2:]
    for suffix in (".o", ".obj"):
        if candidate.endswith(suffix):
            candidate = candidate[: -len(suffix)]
            break
    else:
        return None

    if safe_relative_path(candidate) and (root / candidate).is_file():
        return candidate
    return None


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
                    relative = composed_object_source(root, normalized)
                    if relative is None:
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
