"""Read-only commit plan preview for SagaSync."""

from __future__ import annotations

from dataclasses import dataclass

from .export_status import ExportManifest
from .git_status import GitStatus


GROUP_SOURCE = "source"
GROUP_TOOL = "tool"
GROUP_DOCS = "docs"
GROUP_MIXED = "mixed"
GROUP_UNKNOWN = "unknown"


@dataclass(frozen=True)
class CommitPlanFile:
    path: str
    status: str


@dataclass(frozen=True)
class CommitPlanGroup:
    order: int
    kind: str
    label: str
    files: tuple[CommitPlanFile, ...]
    suggested_message: str
    blocked: bool
    reason: str

    @property
    def file_count(self) -> int:
        return len(self.files)


@dataclass(frozen=True)
class CommitPlan:
    groups: tuple[CommitPlanGroup, ...]

    @property
    def blocked_count(self) -> int:
        return sum(1 for group in self.groups if group.blocked)

    @property
    def suggested_messages(self) -> tuple[str, ...]:
        return tuple(group.suggested_message for group in self.groups)


def parse_status_line(line: str) -> CommitPlanFile | None:
    if not line.strip():
        return None

    status = line[:2].strip() or "?"
    path = line[3:].strip() if len(line) > 3 else line.strip()
    if " -> " in path:
        path = path.split(" -> ", 1)[1]
    if not path:
        return None
    return CommitPlanFile(path=path, status=status)


def _tool_name_from_path(path: str) -> str | None:
    parts = path.split("/")
    if len(parts) >= 2 and parts[0] == "Tools" and parts[1]:
        return parts[1]
    return None


def _known_tool_names(manifest: ExportManifest) -> set[str]:
    return {tool.name.lower() for tool in manifest.tools}


def _group_key(file: CommitPlanFile, manifest: ExportManifest) -> tuple[str, str]:
    tool_name = _tool_name_from_path(file.path)
    if tool_name is not None:
        return (GROUP_TOOL, tool_name)
    if file.path.startswith("docs/"):
        return (GROUP_DOCS, "Documentation")
    if "/" not in file.path or file.path.startswith(("core/", "src/", "include/", "tests/")):
        return (GROUP_SOURCE, "SagaEngine")
    if file.path:
        return (GROUP_SOURCE, "SagaEngine")
    return (GROUP_UNKNOWN, "Uncategorized")


def _message_for(kind: str, label: str, manifest: ExportManifest) -> str:
    if kind == GROUP_TOOL:
        scope = label.lower().replace(" ", "-")
        known = scope in _known_tool_names(manifest)
        action = "sync" if known else "update"
        return f"feat({scope}): {action} {label} changes"
    if kind == GROUP_DOCS:
        return "docs: update development notes"
    if kind == GROUP_SOURCE:
        return "feat: update SagaEngine source changes"
    return "chore: review uncategorized changes"


def _reason_for(order: int, kind: str, label: str) -> tuple[bool, str]:
    if kind == GROUP_UNKNOWN:
        return True, "review uncategorized paths before committing"
    if order > 1:
        return True, "commit earlier plan groups first"
    return False, f"{label} group is first in the suggested order"


def summarize_files(files: tuple[CommitPlanFile, ...], limit: int = 5) -> str:
    shown = [item.path for item in files[:limit]]
    remaining = len(files) - len(shown)
    if remaining > 0:
        shown.append(f"+{remaining} more")
    return ", ".join(shown)


def build_commit_plan(status: GitStatus, manifest: ExportManifest) -> CommitPlan:
    grouped: dict[tuple[str, str], list[CommitPlanFile]] = {}
    for line in status.status_lines:
        parsed = parse_status_line(line)
        if parsed is None:
            continue
        grouped.setdefault(_group_key(parsed, manifest), []).append(parsed)

    ordered_keys = sorted(
        grouped,
        key=lambda key: (
            {
                GROUP_SOURCE: 0,
                GROUP_TOOL: 1,
                GROUP_DOCS: 2,
                GROUP_MIXED: 3,
                GROUP_UNKNOWN: 4,
            }.get(key[0], 4),
            key[1].lower(),
        ),
    )

    groups: list[CommitPlanGroup] = []
    for order, key in enumerate(ordered_keys, start=1):
        kind, label = key
        files = tuple(sorted(grouped[key], key=lambda item: item.path))
        blocked, reason = _reason_for(order, kind, label)
        groups.append(
            CommitPlanGroup(
                order=order,
                kind=kind,
                label=label,
                files=files,
                suggested_message=_message_for(kind, label, manifest),
                blocked=blocked,
                reason=reason,
            )
        )

    return CommitPlan(groups=tuple(groups))
