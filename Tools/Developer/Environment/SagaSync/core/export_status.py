"""Export manifest and state parsing for SagaSync."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import json
from typing import Any, Mapping


@dataclass(frozen=True)
class ToolExport:
    name: str
    path: str
    repo: str


@dataclass(frozen=True)
class ExportManifest:
    target_branch: str
    tools: tuple[ToolExport, ...]


@dataclass(frozen=True)
class ExportState:
    tool: str
    path: str
    repo: str
    target_branch: str
    source_commit: str
    source_short: str
    subtree_commit: str
    remote_head_before: str
    remote_head_after: str
    exported_at: str

    @property
    def synced(self) -> bool:
        return bool(self.subtree_commit) and self.remote_head_after == self.subtree_commit


def _read_json(path: Path) -> Mapping[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise ValueError(f"{path} must contain a JSON object")
    return data


def load_export_manifest(repo_root: Path) -> ExportManifest:
    path = repo_root / "Tools" / "Developer" / "RepoAudit" / "export-manifest.json"
    data = _read_json(path)
    raw_tools = data.get("tools", {})
    if not isinstance(raw_tools, dict):
        raise ValueError("export manifest tools must be an object")

    tools: list[ToolExport] = []
    for name, item in sorted(raw_tools.items(), key=lambda pair: pair[0].lower()):
        if not isinstance(item, dict):
            continue
        rel_path = item.get("path")
        repo = item.get("repo")
        if isinstance(name, str) and isinstance(rel_path, str) and isinstance(repo, str):
            tools.append(ToolExport(name=name, path=rel_path, repo=repo))

    target_branch = data.get("target_branch", "main")
    return ExportManifest(
        target_branch=target_branch if isinstance(target_branch, str) else "main",
        tools=tuple(tools),
    )


def _state_file_for(repo_root: Path, tool_name: str) -> Path:
    safe_name = "".join(ch if ch.isalnum() else "_" for ch in tool_name).upper()
    return repo_root / ".core" / "export" / "state" / f"{safe_name}.json"


def load_export_states(repo_root: Path, manifest: ExportManifest) -> dict[str, ExportState]:
    states: dict[str, ExportState] = {}
    for tool in manifest.tools:
        path = _state_file_for(repo_root, tool.name)
        if not path.exists():
            continue
        data = _read_json(path)
        states[tool.name] = ExportState(
            tool=str(data.get("tool", tool.name)),
            path=str(data.get("path", tool.path)),
            repo=str(data.get("repo", tool.repo)),
            target_branch=str(data.get("target_branch", manifest.target_branch)),
            source_commit=str(data.get("source_commit", "")),
            source_short=str(data.get("source_short", "")),
            subtree_commit=str(data.get("subtree_commit", "")),
            remote_head_before=str(data.get("remote_head_before", "")),
            remote_head_after=str(data.get("remote_head_after", "")),
            exported_at=str(data.get("exported_at", "")),
        )
    return states
