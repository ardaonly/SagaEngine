"""Export health decisions for SagaSync."""

from __future__ import annotations

from dataclasses import dataclass

from .export_status import ExportManifest, ExportState, ToolExport
from .git_status import GitStatus


HEALTH_READY = "Ready"
HEALTH_STALE = "Stale"
HEALTH_BLOCKED = "Blocked"
HEALTH_UNKNOWN = "Unknown"


@dataclass(frozen=True)
class ExportHealth:
    tool: str
    status: str
    reasons: tuple[str, ...]
    source: str
    subtree: str
    remote: str


def _status_paths(status: GitStatus) -> tuple[str, ...]:
    paths: list[str] = []
    for line in status.status_lines:
        candidate = line[3:].strip() if len(line) > 3 else line.strip()
        if " -> " in candidate:
            candidate = candidate.split(" -> ", 1)[1]
        if candidate:
            paths.append(candidate)
    return tuple(paths)


def _matches_path(path: str, prefix: str) -> bool:
    normalized = prefix.rstrip("/") + "/"
    return path == prefix or path.startswith(normalized)


def _tool_path_dirty(tool: ToolExport, status: GitStatus) -> bool:
    return any(_matches_path(path, tool.path) for path in _status_paths(status))


def evaluate_tool_export_health(
    tool: ToolExport,
    state: ExportState | None,
    status: GitStatus,
) -> ExportHealth:
    if state is None:
        return ExportHealth(
            tool=tool.name,
            status=HEALTH_UNKNOWN,
            reasons=("export state is missing",),
            source="",
            subtree="",
            remote="",
        )

    reasons: list[str] = []
    health = HEALTH_READY

    if _tool_path_dirty(tool, status):
        health = HEALTH_BLOCKED
        reasons.append("tool path has uncommitted changes")

    if state.source_short != status.head and state.source_commit != status.head:
        if health != HEALTH_BLOCKED:
            health = HEALTH_STALE
        reasons.append("export source does not match current HEAD")

    if not state.synced:
        if health != HEALTH_BLOCKED:
            health = HEALTH_STALE
        reasons.append("remote head does not match subtree")

    if not reasons:
        reasons.append("export state is synced with current HEAD")

    return ExportHealth(
        tool=tool.name,
        status=health,
        reasons=tuple(reasons),
        source=state.source_short or state.source_commit,
        subtree=state.subtree_commit[:12] if state.subtree_commit else "",
        remote=state.remote_head_after[:12] if state.remote_head_after else "",
    )


def evaluate_export_health(
    manifest: ExportManifest,
    states: dict[str, ExportState],
    status: GitStatus,
) -> tuple[ExportHealth, ...]:
    return tuple(
        evaluate_tool_export_health(tool, states.get(tool.name), status)
        for tool in manifest.tools
    )
