"""Git status discovery for SagaSync."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

from .commands import run_command


@dataclass(frozen=True)
class GitStatus:
    repo_root: Path
    branch: str
    head: str
    upstream: str
    ahead: int
    behind: int
    status_lines: tuple[str, ...]

    @property
    def dirty_count(self) -> int:
        return len(self.status_lines)

    @property
    def clean(self) -> bool:
        return self.dirty_count == 0


def discover_repo_root(start: Path) -> Path:
    """Return the git repository root for start, falling back to known markers."""

    result = run_command(["git", "rev-parse", "--show-toplevel"], start)
    if result.passed and result.stdout.strip():
        return Path(result.stdout.strip()).resolve()

    current = start.resolve()
    for candidate in (current, *current.parents):
        if (candidate / "Tools" / "Developer" / "RepoAudit" / "export-manifest.json").exists():
            return candidate
    return current


def _first_line(value: str, fallback: str = "") -> str:
    for line in value.splitlines():
        line = line.strip()
        if line:
            return line
    return fallback


def _parse_ahead_behind(value: str) -> tuple[int, int]:
    parts = value.strip().split()
    if len(parts) != 2:
        return 0, 0
    try:
        behind = int(parts[0])
        ahead = int(parts[1])
    except ValueError:
        return 0, 0
    return ahead, behind


def normalize_status_lines(lines: Iterable[str]) -> tuple[str, ...]:
    return tuple(line.rstrip() for line in lines if line.strip())


def get_git_status(repo_root: Path) -> GitStatus:
    branch = _first_line(
        run_command(["git", "branch", "--show-current"], repo_root).stdout,
        fallback="(detached)",
    )
    head = _first_line(
        run_command(["git", "rev-parse", "--short=12", "HEAD"], repo_root).stdout,
        fallback="unknown",
    )

    upstream_result = run_command(["git", "rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{upstream}"], repo_root)
    upstream = _first_line(upstream_result.stdout) if upstream_result.passed else ""
    ahead = 0
    behind = 0
    if upstream:
        counts = run_command(["git", "rev-list", "--left-right", "--count", f"{upstream}...HEAD"], repo_root)
        if counts.passed:
            ahead, behind = _parse_ahead_behind(counts.stdout)

    status = run_command(["git", "status", "--short"], repo_root)
    status_lines = normalize_status_lines(status.stdout.splitlines())

    return GitStatus(
        repo_root=repo_root,
        branch=branch,
        head=head,
        upstream=upstream,
        ahead=ahead,
        behind=behind,
        status_lines=status_lines,
    )
