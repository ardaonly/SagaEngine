#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Arda Koyuncu
# SPDX-License-Identifier: MPL-2.0

"""Validate DCO trailers for every non-merge commit in a revision range."""

from __future__ import annotations

from dataclasses import dataclass
import argparse
from pathlib import Path
import re
import subprocess

from licensing_common import repository_root


SIGNOFF_RE = re.compile(
    r"^Signed-off-by:\s+.+\s+<[^<>\s]+@[^<>\s]+>\s*$",
    re.IGNORECASE | re.MULTILINE,
)


@dataclass(frozen=True)
class CommitEvidence:
    commit: str
    parent_count: int
    message: str


@dataclass(frozen=True)
class DcoResult:
    checked: tuple[str, ...]
    skipped_merges: tuple[str, ...]
    failures: tuple[str, ...]


def valid_signoff(message: str) -> bool:
    return SIGNOFF_RE.search(message) is not None


def evaluate_commits(commits: list[CommitEvidence]) -> DcoResult:
    checked: list[str] = []
    skipped: list[str] = []
    failures: list[str] = []
    for evidence in commits:
        if evidence.parent_count > 1:
            skipped.append(evidence.commit)
            continue
        checked.append(evidence.commit)
        if not valid_signoff(evidence.message):
            failures.append(evidence.commit)
    return DcoResult(tuple(checked), tuple(skipped), tuple(failures))


def git(root: Path, *args: str) -> str:
    result = subprocess.run(
        ["git", *args],
        cwd=root,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or "git command failed")
    return result.stdout


def collect_commits(root: Path, base_ref: str, head_ref: str) -> list[CommitEvidence]:
    commits = git(root, "rev-list", "--reverse", f"{base_ref}..{head_ref}").splitlines()
    evidence: list[CommitEvidence] = []
    for commit in commits:
        parents = git(root, "rev-list", "--parents", "-n", "1", commit).split()
        evidence.append(
            CommitEvidence(
                commit=commit,
                parent_count=max(0, len(parents) - 1),
                message=git(root, "show", "-s", "--format=%B", commit),
            )
        )
    return evidence


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path)
    parser.add_argument("--base-ref", required=True)
    parser.add_argument("--head-ref", default="HEAD")
    args = parser.parse_args()

    try:
        root = repository_root(args.root)
        result = evaluate_commits(
            collect_commits(root, args.base_ref, args.head_ref)
        )
    except Exception as exc:
        print(f"::error title=DCO validation failed::{exc}")
        return 2

    for commit in result.skipped_merges:
        print(f"Skipping merge commit: {commit}")
    for commit in result.checked:
        if commit not in result.failures:
            print(f"DCO sign-off present: {commit}")
    for commit in result.failures:
        print(
            "::error title=DCO sign-off missing::"
            f"Commit {commit} does not contain a valid Signed-off-by trailer."
        )
    print(
        f"DCO checked={len(result.checked)} "
        f"skipped_merges={len(result.skipped_merges)} "
        f"failures={len(result.failures)}"
    )
    return 1 if result.failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
