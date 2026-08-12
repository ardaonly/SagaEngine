# SPDX-License-Identifier: MPL-2.0

"""Enforce SagaEngine's security contract for active GitHub workflows."""

from __future__ import annotations

from dataclasses import dataclass
import argparse
from pathlib import Path
import re


ACTION_RE = re.compile(r"^\s*-?\s*uses:\s*([^\s#]+)", re.MULTILINE)
PINNED_ACTION_RE = re.compile(r"^[^@]+@[0-9a-f]{40}$")
JOB_RE = re.compile(r"^  ([A-Za-z0-9_-]+):\s*$", re.MULTILINE)
JOB_NAME_RE = re.compile(r"^    name:\s*([^#\n]+?)\s*$", re.MULTILINE)
CONTINUE_ON_ERROR_RE = re.compile(
    r"^\s*continue-on-error\s*:\s*(?:true|yes|on)\s*(?:#.*)?$",
    re.IGNORECASE | re.MULTILINE,
)
WRITE_ALL_RE = re.compile(
    r"^\s*permissions\s*:\s*write-all\s*(?:#.*)?$",
    re.IGNORECASE | re.MULTILINE,
)
REPOSITORY_WRITE_RE = re.compile(
    r"^\s*(?:actions|checks|contents|deployments|issues|packages|pages|"
    r"pull-requests|repository-projects|security-events|statuses)\s*:\s*"
    r"write\s*(?:#.*)?$",
    re.IGNORECASE | re.MULTILINE,
)


@dataclass(frozen=True)
class Finding:
    path: str
    rule: str
    message: str


def workflow_jobs(text: str) -> dict[str, str]:
    jobs_marker = text.find("\njobs:\n")
    if jobs_marker < 0:
        return {}
    body = text[jobs_marker + len("\njobs:\n") :]
    matches = list(JOB_RE.finditer(body))
    return {
        match.group(1): body[
            match.start() : matches[index + 1].start()
            if index + 1 < len(matches)
            else len(body)
        ]
        for index, match in enumerate(matches)
    }


def job_display_names(texts: dict[str, str]) -> dict[str, list[str]]:
    names: dict[str, list[str]] = {}
    for path, text in texts.items():
        for job_id, body in workflow_jobs(text).items():
            match = JOB_NAME_RE.search(body)
            name = (
                match.group(1).strip().strip('"\'')
                if match
                else job_id
            )
            names.setdefault(name, []).append(f"{path}:{job_id}")
    return names


def check_repository_job_contracts(texts: dict[str, str]) -> list[Finding]:
    findings: list[Finding] = []
    for name, owners in job_display_names(texts).items():
        if len(owners) > 1:
            findings.append(
                Finding(
                    ", ".join(owners),
                    "check-name-unique",
                    f"job display name is duplicated: {name}",
                )
            )

    return findings


def check_workflow(path: str, text: str) -> list[Finding]:
    findings: list[Finding] = []

    if "\npermissions:\n  contents: read\n" not in text:
        findings.append(Finding(path, "permissions", "contents must be read-only"))
    if "\nconcurrency:\n" not in text:
        findings.append(Finding(path, "concurrency", "top-level concurrency is required"))
    if CONTINUE_ON_ERROR_RE.search(text):
        findings.append(Finding(path, "failure-visibility", "errors may not be hidden"))
    if WRITE_ALL_RE.search(text) or REPOSITORY_WRITE_RE.search(text):
        findings.append(
            Finding(path, "permissions", "workflow and job permissions must remain read-only")
        )

    for action in ACTION_RE.findall(text):
        if action.startswith("./"):
            continue
        if not PINNED_ACTION_RE.fullmatch(action):
            findings.append(
                Finding(path, "action-pin", f"action is not pinned to a full SHA: {action}")
            )

    jobs = workflow_jobs(text)
    if not jobs:
        findings.append(Finding(path, "jobs", "workflow has no parseable jobs"))
    for name, body in jobs.items():
        if "\n    timeout-minutes:" not in body:
            findings.append(Finding(path, "timeout", f"job {name} has no timeout"))

    return findings


def validate_repository(root: Path) -> list[Finding]:
    workflow_root = root / ".github" / "workflows"
    active = {"export-tools.yml"}
    retired = {
        "build.yml", "heavy-evidence.yml", "licensing.yml",
        "repository-contracts.yml",
    }
    findings: list[Finding] = []
    texts: dict[str, str] = {}
    workflow_paths = sorted(
        {
            *workflow_root.glob("*.yml"),
            *workflow_root.glob("*.yaml"),
        }
    )
    discovered = {path.name for path in workflow_paths if path.is_file()}
    for name in active:
        path = workflow_root / name
        if name not in discovered:
            findings.append(Finding(str(path.relative_to(root)), "required-workflow", "missing"))
    for name in sorted(discovered & retired):
        findings.append(Finding(
            f".github/workflows/{name}", "retired-test-workflow",
            "retired test automation must not return before the roadmap architecture is implemented",
        ))
    for path in workflow_paths:
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8")
        texts[path.name] = text
        findings.extend(check_workflow(str(path.relative_to(root)), text))

    findings.extend(check_repository_job_contracts(texts))

    roadmap = root / ".github" / "TESTING_ROADMAP.md"
    if not roadmap.is_file():
        findings.append(Finding(
            ".github/TESTING_ROADMAP.md", "testing-roadmap", "missing"))

    dependabot = root / ".github" / "dependabot.yml"
    if not dependabot.is_file() or 'package-ecosystem: "github-actions"' not in dependabot.read_text(encoding="utf-8"):
        findings.append(Finding(".github/dependabot.yml", "dependency-updates", "github-actions updates are required"))
    return findings


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    args = parser.parse_args()
    root = args.repo_root.resolve()
    findings = validate_repository(root)
    for finding in findings:
        print(f"{finding.path}: {finding.rule}: {finding.message}")
    if findings:
        print(f"GitHub workflow contract failed with {len(findings)} finding(s).")
        return 1
    print("GitHub workflow contract passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
