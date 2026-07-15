#!/usr/bin/env python3
"""
@file export_tool_mirrors.py
@brief Export monorepo tool subtrees to mirror repositories.
"""

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Mapping, Sequence


DEFAULT_MANIFEST = Path("Tools/Developer/RepoAudit/export-manifest.json")
DEFAULT_STATE_DIR = Path(".core/export/state")
EXPECTED_SCHEMA = "sagaengine.tool_export.v0"
SUPPORTED_VERSION = "0.1"
ZERO_SHA = "0" * 40

TOOL_NAME_RE = re.compile(r"^[A-Za-z][A-Za-z0-9_-]*$")
REPO_SLUG_RE = re.compile(r"^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$")
BRANCH_RE = re.compile(r"^[A-Za-z0-9._/-]+$")


class ExportError(RuntimeError):
    pass


@dataclass(frozen=True)
class ToolExport:
    name: str
    path: str
    repo: str


@dataclass(frozen=True)
class ExportManifest:
    target_branch: str
    allowed_repos: tuple[str, ...]
    tools: tuple[ToolExport, ...]


@dataclass(frozen=True)
class RemoteInfo:
    url: str
    redacted_url: str


def log(message: str) -> None:
    print(f"[tool-export] {message}")


def run(
    args: Sequence[str],
    *,
    cwd: Path,
    check: bool = True,
    capture: bool = True,
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        list(args),
        cwd=str(cwd),
        check=False,
        capture_output=capture,
        text=True,
    )

    if check and result.returncode != 0:
        command = redact_url(" ".join(args))
        detail = redact_url(result.stderr.strip() if result.stderr else result.stdout.strip())
        raise ExportError(f"command failed ({result.returncode}): {command}\n{detail}")

    return result


def git(repo_root: Path, args: Sequence[str], *, check: bool = True) -> str:
    result = run(["git", *args], cwd=repo_root, check=check)
    return result.stdout.strip()


def resolve_repo_root(start: Path) -> Path:
    result = run(["git", "-C", str(start), "rev-parse", "--show-toplevel"], cwd=start, check=False)
    if result.returncode != 0:
        raise ExportError(f"{start} is not inside a git repository")
    return Path(result.stdout.strip()).resolve()


def load_json(path: Path) -> Mapping[str, Any]:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except OSError as exc:
        raise ExportError(f"could not read {path}: {exc}") from exc
    except json.JSONDecodeError as exc:
        raise ExportError(f"invalid JSON in {path}: {exc}") from exc

    if not isinstance(data, dict):
        raise ExportError(f"{path} must contain a JSON object")
    return data


def validate_repo_slug(repo: str, allowed_repos: Sequence[str]) -> None:
    if not REPO_SLUG_RE.match(repo):
        raise ExportError(f"invalid repository slug: {repo!r}")

    if not any(fnmatch.fnmatchcase(repo, pattern) for pattern in allowed_repos):
        allowed = ", ".join(allowed_repos)
        raise ExportError(f"repository {repo!r} is outside allowed_repos: {allowed}")


def validate_branch_name(branch: str) -> None:
    invalid = (
        not BRANCH_RE.match(branch),
        branch.startswith("/"),
        branch.endswith("/"),
        ".." in branch,
        "@{" in branch,
        "\\" in branch,
    )
    if any(invalid):
        raise ExportError(f"invalid target_branch in export manifest: {branch!r}")


def validate_tool_path(repo_root: Path, rel_path: str) -> None:
    path = Path(rel_path)
    if path.is_absolute() or ".." in path.parts:
        raise ExportError(f"tool path must be a safe relative path: {rel_path!r}")

    full = (repo_root / path).resolve()
    if not full.is_relative_to(repo_root):
        raise ExportError(f"tool path escapes repository root: {rel_path!r}")
    if not full.is_dir():
        raise ExportError(f"tool path does not exist or is not a directory: {rel_path!r}")


def load_manifest(repo_root: Path, manifest_path: Path) -> ExportManifest:
    data = load_json(manifest_path)
    if data.get("schema") != EXPECTED_SCHEMA:
        raise ExportError(f"unsupported export manifest schema: {data.get('schema')!r}")
    if data.get("version") != SUPPORTED_VERSION:
        raise ExportError(f"unsupported export manifest version: {data.get('version')!r}")

    target_branch = data.get("target_branch")
    if not isinstance(target_branch, str) or not target_branch:
        raise ExportError("manifest target_branch must be a non-empty string")
    validate_branch_name(target_branch)

    allowed = data.get("allowed_repos")
    if not isinstance(allowed, list) or not all(isinstance(item, str) for item in allowed):
        raise ExportError("manifest allowed_repos must be a list of strings")
    if not allowed:
        raise ExportError("manifest allowed_repos must not be empty")

    raw_tools = data.get("tools")
    if not isinstance(raw_tools, dict) or not raw_tools:
        raise ExportError("manifest tools must be a non-empty object")

    tools: list[ToolExport] = []
    seen_paths: set[str] = set()
    seen_repos: set[str] = set()

    for name, item in raw_tools.items():
        if not isinstance(name, str) or not TOOL_NAME_RE.match(name):
            raise ExportError(f"invalid tool name: {name!r}")
        if not isinstance(item, dict):
            raise ExportError(f"tool {name} must be an object")

        rel_path = item.get("path")
        repo = item.get("repo")
        if not isinstance(rel_path, str) or not rel_path:
            raise ExportError(f"tool {name} path must be a non-empty string")
        if not isinstance(repo, str) or not repo:
            raise ExportError(f"tool {name} repo must be a non-empty string")

        validate_tool_path(repo_root, rel_path)
        validate_repo_slug(repo, allowed)

        if rel_path in seen_paths:
            raise ExportError(f"duplicate export path in manifest: {rel_path}")
        if repo.lower() in seen_repos:
            raise ExportError(f"duplicate export repository in manifest: {repo}")
        seen_paths.add(rel_path)
        seen_repos.add(repo.lower())

        tools.append(ToolExport(name=name, path=rel_path, repo=repo))

    return ExportManifest(
        target_branch=target_branch,
        allowed_repos=tuple(allowed),
        tools=tuple(sorted(tools, key=lambda tool: tool.name.lower())),
    )


def sanitize_tool_env_name(name: str) -> str:
    return re.sub(r"[^A-Za-z0-9]", "_", name).upper()


def redact_url(url: str) -> str:
    return re.sub(r"(https://x-access-token:)[^@]+(@github.com/)", r"\1***\2", url)


def validate_override_url(url: str, repo: str) -> None:
    owner, name = repo.split("/", 1)
    patterns = (
        f"git@github.com:{owner}/{name}.git",
        f"https://github.com/{owner}/{name}.git",
        f"https://github.com/{owner}/{name}",
    )
    if url in patterns:
        return

    token_pattern = re.compile(
        rf"^https://x-access-token:[^@]+@github\.com/{re.escape(owner)}/{re.escape(name)}(?:\.git)?$"
    )
    if token_pattern.match(url):
        return

    raise ExportError(
        f"remote override does not match the manifest repo {repo}: {redact_url(url)}"
    )


def build_remote(tool: ToolExport, *, mirror_token: str | None) -> RemoteInfo:
    env_name = f"SAGA_EXPORT_REMOTE_{sanitize_tool_env_name(tool.name)}"
    override = os.environ.get(env_name)
    if override:
        validate_override_url(override, tool.repo)
        return RemoteInfo(url=override, redacted_url=redact_url(override))

    if mirror_token:
        url = f"https://x-access-token:{mirror_token}@github.com/{tool.repo}.git"
    else:
        url = f"git@github.com:{tool.repo}.git"

    return RemoteInfo(url=url, redacted_url=redact_url(url))


def state_path_for(state_dir: Path, tool: ToolExport) -> Path:
    return state_dir / f"{sanitize_tool_env_name(tool.name)}.json"


def load_state(path: Path) -> Mapping[str, Any] | None:
    if not path.exists():
        return None
    data = load_json(path)
    return data


def write_state(path: Path, data: Mapping[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def is_zero_sha(value: str | None) -> bool:
    return value is None or value == "" or value == ZERO_SHA


def ref_exists(repo_root: Path, ref: str) -> bool:
    result = run(["git", "cat-file", "-e", f"{ref}^{{commit}}"], cwd=repo_root, check=False)
    return result.returncode == 0


def tool_changed_since(repo_root: Path, base: str | None, tool: ToolExport) -> bool:
    if is_zero_sha(base):
        return True
    if not ref_exists(repo_root, base or ""):
        log(f"{tool.name}: base ref {base!r} is not available; treating tool as changed")
        return True

    result = run(["git", "diff", "--quiet", f"{base}..HEAD", "--", tool.path], cwd=repo_root, check=False)
    if result.returncode == 0:
        return False
    if result.returncode == 1:
        return True

    detail = result.stderr.strip() or result.stdout.strip()
    raise ExportError(f"{tool.name}: git diff failed while checking changes\n{detail}")


def source_commit_from_state(state: Mapping[str, Any] | None) -> str | None:
    if not state:
        return None

    source_commit = state.get("source_commit")
    if not isinstance(source_commit, str) or not source_commit:
        return None
    return source_commit


def subtree_split(repo_root: Path, tool: ToolExport) -> str:
    log(f"{tool.name}: running git subtree split for {tool.path}")
    output = git(repo_root, ["subtree", "split", f"--prefix={tool.path}", "HEAD"])
    sha = output.splitlines()[-1].strip()
    if not re.fullmatch(r"[0-9a-f]{40}", sha):
        raise ExportError(f"{tool.name}: unexpected subtree split output: {output!r}")
    return sha


def remote_head(repo_root: Path, remote_url: str, branch: str) -> str | None:
    result = run(["git", "ls-remote", "--heads", remote_url, branch], cwd=repo_root, check=False)
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip()
        raise ExportError(f"git ls-remote failed for {redact_url(remote_url)}\n{detail}")

    line = result.stdout.strip()
    if not line:
        return None

    return line.split()[0]


def export_hash(tool: ToolExport, target_branch: str, source_commit: str, subtree_commit: str) -> str:
    payload = "\0".join((tool.name, tool.path, tool.repo, target_branch, source_commit, subtree_commit))
    return hashlib.sha256(payload.encode("utf-8")).hexdigest()


def make_state(
    tool: ToolExport,
    *,
    target_branch: str,
    source_commit: str,
    source_short: str,
    subtree_commit: str,
    before: str | None,
    after: str | None,
) -> dict[str, Any]:
    return {
        "tool": tool.name,
        "path": tool.path,
        "repo": tool.repo,
        "target_branch": target_branch,
        "source_commit": source_commit,
        "source_short": source_short,
        "subtree_commit": subtree_commit,
        "remote_head_before": before,
        "remote_head_after": after,
        "export_hash": export_hash(tool, target_branch, source_commit, subtree_commit),
        "exported_at": datetime.now(timezone.utc).isoformat(),
    }


def push_subtree(repo_root: Path, remote: RemoteInfo, subtree_commit: str, branch: str, expected_head: str | None) -> None:
    refspec = f"{subtree_commit}:refs/heads/{branch}"
    cmd = ["git", "push"]
    if expected_head:
        cmd.append(f"--force-with-lease=refs/heads/{branch}:{expected_head}")
    cmd.extend([remote.url, refspec])
    log(f"push: {subtree_commit[:12]} -> {remote.redacted_url} {branch}")
    run(cmd, cwd=repo_root, check=True, capture=True)


def select_tools(manifest: ExportManifest, names: Sequence[str]) -> tuple[ToolExport, ...]:
    if not names:
        return manifest.tools

    by_name = {tool.name.lower(): tool for tool in manifest.tools}
    selected = []
    for name in names:
        tool = by_name.get(name.lower())
        if tool is None:
            available = ", ".join(tool.name for tool in manifest.tools)
            raise ExportError(f"unknown tool {name!r}; available tools: {available}")
        selected.append(tool)
    return tuple(selected)


def export_tool(
    repo_root: Path,
    tool: ToolExport,
    *,
    target_branch: str,
    since: str | None,
    state_dir: Path,
    dry_run: bool,
    refresh_state: bool,
    mirror_token: str | None,
    source_commit: str,
    source_short: str,
) -> str:
    state_file = state_path_for(state_dir, tool)
    state = load_state(state_file)
    change_base = since if since is not None else source_commit_from_state(state)
    if change_base is None:
        log(f"{tool.name}: no export state found; treating tool as changed")

    if not tool_changed_since(repo_root, change_base, tool):
        log(f"{tool.name}: skip unchanged since {change_base[:12]}")
        return "skipped"

    if not dry_run and mirror_token is None and os.environ.get("GITHUB_ACTIONS") == "true":
        raise ExportError(f"{tool.name}: MIRROR_TOKEN is required in CI when an export is required")

    remote = build_remote(tool, mirror_token=mirror_token)

    if state and not refresh_state:
        same_source = state.get("source_commit") == source_commit
        state_subtree = state.get("subtree_commit")
        if same_source and isinstance(state_subtree, str):
            log(f"{tool.name}: state hit for source@{source_short}")
            if dry_run:
                return "skipped"

            before = remote_head(repo_root, remote.url, target_branch)
            if before == state_subtree:
                log(f"{tool.name}: skip already synced at {state_subtree[:12]}")
                return "skipped"

            log(f"{tool.name}: remote drift detected; re-pushing recorded subtree {state_subtree[:12]}")
            push_subtree(repo_root, remote, state_subtree, target_branch, before)
            after = remote_head(repo_root, remote.url, target_branch)
            write_state(
                state_file,
                make_state(
                    tool,
                    target_branch=target_branch,
                    source_commit=source_commit,
                    source_short=source_short,
                    subtree_commit=state_subtree,
                    before=before,
                    after=after,
                ),
            )
            return "exported"

    subtree_commit = subtree_split(repo_root, tool)
    sync_message = f"sync: source@{source_short} ({tool.name})"
    log(f"{tool.name}: subtree {subtree_commit}")
    log(f"{tool.name}: sync message {sync_message!r}")

    if dry_run:
        log(f"{tool.name}: dry-run skip remote check and push")
        return "planned"

    before = remote_head(repo_root, remote.url, target_branch)
    if before == subtree_commit:
        log(f"{tool.name}: skip already synced at {subtree_commit[:12]}")
        write_state(
            state_file,
            make_state(
                tool,
                target_branch=target_branch,
                source_commit=source_commit,
                source_short=source_short,
                subtree_commit=subtree_commit,
                before=before,
                after=before,
            ),
        )
        return "skipped"

    push_subtree(repo_root, remote, subtree_commit, target_branch, before)
    after = remote_head(repo_root, remote.url, target_branch)
    write_state(
        state_file,
        make_state(
            tool,
            target_branch=target_branch,
            source_commit=source_commit,
            source_short=source_short,
            subtree_commit=subtree_commit,
            before=before,
            after=after,
        ),
    )
    return "exported"


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Export SagaEngine tool subtrees to mirror repositories.")
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--state-dir", type=Path, default=DEFAULT_STATE_DIR)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--refresh-state", action="store_true")
    parser.add_argument("--since", help="Base ref used to skip unchanged tools.")
    parser.add_argument("--tool", action="append", default=[], help="Tool name to export. May be repeated.")
    args = parser.parse_args(argv)

    try:
        repo_root = resolve_repo_root(args.repo_root)
        manifest_path = args.manifest if args.manifest.is_absolute() else repo_root / args.manifest
        state_dir = args.state_dir if args.state_dir.is_absolute() else repo_root / args.state_dir

        manifest = load_manifest(repo_root, manifest_path)
        selected_tools = select_tools(manifest, args.tool)
        source_commit = git(repo_root, ["rev-parse", "HEAD"])
        source_short = git(repo_root, ["rev-parse", "--short=12", "HEAD"])
        mirror_token = os.environ.get("MIRROR_TOKEN") or None

        log(f"repo     : {repo_root}")
        log(f"manifest : {manifest_path}")
        log(f"state    : {state_dir}")
        log(f"source   : {source_short}")
        log(f"branch   : {manifest.target_branch}")
        log(f"mode     : {'dry-run' if args.dry_run else 'push'}")

        results: dict[str, int] = {"exported": 0, "planned": 0, "skipped": 0}
        for tool in selected_tools:
            result = export_tool(
                repo_root,
                tool,
                target_branch=manifest.target_branch,
                since=args.since,
                state_dir=state_dir,
                dry_run=args.dry_run,
                refresh_state=args.refresh_state,
                mirror_token=mirror_token,
                source_commit=source_commit,
                source_short=source_short,
            )
            results[result] = results.get(result, 0) + 1

        log(
            "summary  : "
            f"exported={results.get('exported', 0)} "
            f"planned={results.get('planned', 0)} "
            f"skipped={results.get('skipped', 0)}"
        )
        return 0
    except ExportError as exc:
        print(f"[tool-export] ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
