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


def validate_reviewed_commit(
    root: Path,
    policy: dict[str, Any],
    report: Report,
) -> tuple[str, str] | None:
    shallow = run(
        ["git", "rev-parse", "--is-shallow-repository"],
        root,
        check=False,
    )
    shallow_state = shallow.stdout.strip().lower()
    if shallow.returncode != 0 or shallow_state not in {"true", "false"}:
        report.add(
            "ERROR",
            "SHALLOW_REPOSITORY_UNVERIFIED",
            shallow.stderr.strip() or shallow.stdout.strip() or "git state unavailable",
        )
        return None
    if shallow_state == "true":
        report.add(
            "ERROR",
            "SHALLOW_REPOSITORY",
            "reviewed_commit requires a complete Git history",
        )
        return None

    reviewed = policy.get("reviewed_commit", "")
    if not isinstance(reviewed, str) or not FULL_COMMIT_RE.fullmatch(reviewed):
        report.add("ERROR", "COMMIT_FORMAT", f"reviewed_commit: {reviewed!r}")
        return None

    commit_type = run(
        ["git", "cat-file", "-t", reviewed],
        root,
        check=False,
    )
    if commit_type.returncode != 0:
        report.add("ERROR", "COMMIT_MISSING", f"reviewed_commit: {reviewed}")
        return None
    if commit_type.stdout.strip() != "commit":
        report.add(
            "ERROR",
            "REVIEWED_OBJECT_NOT_COMMIT",
            f"reviewed_commit={reviewed}, type={commit_type.stdout.strip() or 'unknown'}",
        )
        return None

    head = run(["git", "rev-parse", "HEAD"], root, check=False)
    current = head.stdout.strip()
    if head.returncode != 0 or not FULL_COMMIT_RE.fullmatch(current):
        report.add(
            "ERROR",
            "HEAD_COMMIT_UNVERIFIED",
            head.stderr.strip() or current or "HEAD unavailable",
        )
        return None

    ancestor = run(
        ["git", "merge-base", "--is-ancestor", reviewed, current],
        root,
        check=False,
    )
    if ancestor.returncode != 0:
        report.add(
            "ERROR",
            "EVIDENCE_BASELINE_NOT_ANCESTOR",
            f"reviewed={reviewed}, current={current}",
        )
        return None

    return reviewed, current


def validate_commits(
    root: Path,
    policy: dict[str, Any],
    report: Report,
) -> str | None:
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

    validated_review = validate_reviewed_commit(root, policy, report)
    if validated_review is not None:
        from fnmatch import fnmatchcase

        reviewed, current = validated_review
        if current != reviewed:
            changed = run(
                ["git", "diff", "--name-only", f"{reviewed}..{current}"],
                root,
                check=False,
            )
            if changed.returncode != 0:
                report.add(
                    "ERROR",
                    "EVIDENCE_BASELINE_DIFF_FAILED",
                    changed.stderr.strip() or f"{reviewed}..{current}",
                )
            else:
                changed_paths = [
                    line.strip()
                    for line in changed.stdout.splitlines()
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

    return validated_review[0] if validated_review is not None else None


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
    reviewed_commit: str | None,
    domains: list[Domain],
    policy: dict[str, Any],
    report: Report,
) -> None:
    validation = policy["validation"]
    comparisons: list[tuple[str, list[str]]] = [
        ("working-tree", ["git", "diff", "--name-status", "-M"]),
        ("index", ["git", "diff", "--cached", "--name-status", "-M"]),
    ]
    if reviewed_commit:
        comparisons.append(
            (
                f"{reviewed_commit}..HEAD",
                [
                    "git",
                    "diff",
                    "--name-status",
                    "-M",
                    f"{reviewed_commit}..HEAD",
                ],
            )
        )

    case_sensitive = bool(validation["case_sensitive"])
    seen: set[tuple[str, str]] = set()

    for label, command in comparisons:
        result = run(command, root, check=False)
        if result.returncode != 0:
            if reviewed_commit and label == f"{reviewed_commit}..HEAD":
                report.add("ERROR", "REVIEWED_DIFF_FAILED", reviewed_commit)
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
