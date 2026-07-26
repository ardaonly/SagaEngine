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


class PolicyResolver:
    """Resolve files through the single metadata-to-policy precedence chain."""

    def __init__(self, context: LoadedPolicyContext, report: Report | None = None):
        self.context = context
        self.report = report or Report()
        self.dep5_records = parse_dep5(context.root, self.report)
        self.manifest_paths = canonical_manifest_paths(context.root, self.report)
        self.submodule_paths = set(policy_submodule_paths(context.policy))

    def decide(self, path: str, *, text: str | None = None) -> FilePolicyDecision:
        domain, winners = classify(
            path, self.context.domains,
            case_sensitive=self.context.case_sensitive,
        )
        before = len(self.report.findings)
        if domain is None:
            resolution = Resolution("", False, False)
            diagnostics = []
            if winners:
                diagnostics.append(Finding(
                    "ERROR", "AMBIGUOUS_DOMAIN",
                    f"{path}: {sorted(item.id for item in winners)}",
                ))
            return FilePolicyDecision(path, None, resolution,
                diagnostics=diagnostics, modification_allowed=False)

        resolution = resolve_file(
            self.context.root, path, domain, self.dep5_records,
            self.manifest_paths, self.submodule_paths,
            self.context.policy, self.report,
        )
        if text is None:
            text = read_text_prefix(self.context.root / path) or ""
        allowed = (
            resolution.resolved
            and domain.raw.get("current_license_mode") == "fixed-spdx"
            and not is_generated_file(path, text)
            and not is_third_party_path(path)
            and comment_prefix(Path(path)) is not None
        )
        sources = [resolution.source] if resolution.source else []
        sources.extend(resolution.evidence)
        return FilePolicyDecision(
            path=path,
            domain=domain,
            resolution=resolution,
            metadata_sources=sources,
            diagnostics=list(self.report.findings[before:]),
            modification_allowed=allowed,
        )


def policy_submodule_paths(policy: dict[str, Any]) -> list[str]:
    return sorted(
        record["path"] for record in policy.get("submodules", [])
        if isinstance(record, dict) and isinstance(record.get("path"), str)
    )


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
