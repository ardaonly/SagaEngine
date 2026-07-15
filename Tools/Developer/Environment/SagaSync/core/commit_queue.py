"""Commit queue suggestions for the SagaEngine multirepo workflow."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from .export_status import ExportManifest
from .git_status import GitStatus


@dataclass(frozen=True)
class CommitQueueItem:
    order: int
    title: str
    detail: str
    command_hint: str
    blocked: bool = False


def _paths_from_status(status: GitStatus) -> set[str]:
    paths: set[str] = set()
    for line in status.status_lines:
        candidate = line[3:].strip() if len(line) > 3 else line.strip()
        if " -> " in candidate:
            candidate = candidate.split(" -> ", 1)[1]
        if candidate:
            paths.add(candidate)
    return paths


def _matches_path(path: str, prefix: str) -> bool:
    normalized = prefix.rstrip("/") + "/"
    return path == prefix or path.startswith(normalized)


def build_commit_queue(status: GitStatus, manifest: ExportManifest) -> tuple[CommitQueueItem, ...]:
    """Build a conservative suggested workflow from current dirty paths."""

    paths = _paths_from_status(status)
    items: list[CommitQueueItem] = []
    order = 1

    if paths:
        items.append(
            CommitQueueItem(
                order=order,
                title="Prepare SagaEngine source change",
                detail=f"{len(paths)} changed file(s) in the source repository.",
                command_hint="Review source changes and record them in the monorepo.",
            )
        )
        order += 1

    for tool in manifest.tools:
        if any(_matches_path(path, tool.path) for path in paths):
            items.append(
                CommitQueueItem(
                    order=order,
                title=f"Export {tool.name} mirror",
                detail=f"{tool.path} changed; run export dry-run before mirror push.",
                command_hint=f"scripts/export {tool.name.lower()} --dry-run",
                    blocked=bool(paths),
                )
            )
            order += 1

    if any(path.startswith("SagaWiki/") for path in paths):
        items.append(
            CommitQueueItem(
                order=order,
                title="Review roadmap/docs sync",
                detail="Documentation changed; verify roadmap checkboxes match shipped behavior.",
                command_hint="git diff -- docs Tools/*/*ROADMAP.md",
            )
        )
        order += 1

    if not items:
        items.append(
            CommitQueueItem(
                order=1,
                title="No commit needed",
                detail="Source repository is clean.",
                command_hint="scripts/export plan",
            )
        )

    return tuple(items)


def repo_label(repo_root: Path) -> str:
    return repo_root.name or str(repo_root)
