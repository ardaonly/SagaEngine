#!/usr/bin/env python3
"""
@file report_boundary_status.py
@brief Emit a non-blocking Phase 1 report for SagaEngine path boundaries.
"""

from __future__ import annotations

import argparse
import fnmatch
import json
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence


DEFAULT_MANIFEST = Path("core/manifest/path_rules.json")
APACHE = "Apache-2.0"
EDITOR_LICENSE = "SAGA-EDITOR-READONLY-NOAI"

SOURCE_EXTENSIONS = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".inl",
}

SCAN_ROOTS = (
    "Engine",
    "Runtime",
    "Server",
    "Backends",
    "Tools",
)

EDITOR_PATH_TOKENS = (
    "Editor/",
    "Apps/Editor/",
    "Apps/EditorLab/",
    "SagaEditor",
    "SagaEditorLab",
)

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"](?P<include>[^>"]+)[>"]', re.MULTILINE)


@dataclass(frozen=True)
class Rule:
    pattern: str
    license: str


def run_git(repo_root: Path, args: Sequence[str]) -> list[str]:
    result = subprocess.run(
        ["git", "-C", str(repo_root), *args],
        check=True,
        capture_output=True,
        text=True,
    )

    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


def resolve_repo_root(start: Path) -> Path:
    try:
        result = subprocess.run(
            ["git", "-C", str(start), "rev-parse", "--show-toplevel"],
            check=True,
            capture_output=True,
            text=True,
        )
    except subprocess.CalledProcessError:
        return start.resolve()

    return Path(result.stdout.strip()).resolve()


def load_rules(manifest_path: Path) -> list[Rule]:
    data = json.loads(manifest_path.read_text(encoding="utf-8"))

    rules = []
    for item in data.get("rules", []):
        pattern = item.get("pattern")
        license_id = item.get("license")
        if not isinstance(pattern, str) or not isinstance(license_id, str):
            raise ValueError(f"Invalid path rule in {manifest_path}: {item!r}")
        rules.append(Rule(pattern=pattern, license=license_id))

    if not rules:
        raise ValueError(f"No path rules found in {manifest_path}")

    return rules


def list_repo_files(repo_root: Path) -> list[str]:
    files = run_git(repo_root, ["ls-files", "--cached", "--others", "--exclude-standard"])
    return sorted(path for path in files if not path.startswith(".git/"))


def match_rule(path: str, rules: Iterable[Rule]) -> Rule | None:
    for rule in rules:
        if fnmatch.fnmatchcase(path, rule.pattern):
            return rule
    return None


def collect_unmatched(files: Iterable[str], rules: Sequence[Rule]) -> list[str]:
    return [path for path in files if match_rule(path, rules) is None]


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def collect_editor_include_warnings(repo_root: Path) -> list[str]:
    warnings: list[str] = []

    for root_name in SCAN_ROOTS:
        scan_root = repo_root / root_name
        if not scan_root.exists():
            continue

        for path in scan_root.rglob("*"):
            if not path.is_file() or path.suffix not in SOURCE_EXTENSIONS:
                continue

            try:
                text = read_text(path)
            except OSError:
                continue

            hits = []
            for match in INCLUDE_RE.finditer(text):
                include = match.group("include")
                if include.startswith("SagaEditor/") or include.startswith("SagaEditorLab/"):
                    hits.append(include)

            if hits:
                rel = path.relative_to(repo_root).as_posix()
                joined = ", ".join(sorted(set(hits)))
                warnings.append(f"{rel}: includes restricted editor header(s): {joined}")

    return warnings


def collect_metadata_warnings(repo_root: Path) -> list[str]:
    warnings: list[str] = []

    for license_file in (repo_root / "Tools/Forge/LICENSE",):
        if not license_file.exists():
            warnings.append(f"{license_file.relative_to(repo_root)}: missing standalone license file")
            continue
        if "Apache License" not in read_text(license_file):
            warnings.append(f"{license_file.relative_to(repo_root)}: does not look like Apache-2.0")

    return warnings


def print_section(title: str, items: Sequence[str], clean_message: str) -> None:
    print(f"\n[{title}]")
    if not items:
        print(f"  OK - {clean_message}")
        return

    print(f"  WARN - {len(items)} item(s)")
    for item in items[:40]:
        print(f"  - {item}")
    if len(items) > 40:
        print(f"  ... {len(items) - 40} more")


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Report SagaEngine boundary drift without failing Phase 1 CI."
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=Path.cwd(),
        help="Repository root. Defaults to the current working directory.",
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=DEFAULT_MANIFEST,
        help="Path to path_rules.json, relative to the repository root unless absolute.",
    )
    args = parser.parse_args(argv)

    repo_root = resolve_repo_root(args.repo_root)
    manifest_path = args.manifest if args.manifest.is_absolute() else repo_root / args.manifest

    print("[boundary] SagaEngine Phase 1 boundary report")
    print(f"[boundary] repo     : {repo_root}")
    print(f"[boundary] manifest : {manifest_path}")
    print("[boundary] mode     : observation-only")

    rules = load_rules(manifest_path)
    files = list_repo_files(repo_root)

    unmatched = collect_unmatched(files, rules)
    editor_includes = collect_editor_include_warnings(repo_root)
    metadata = collect_metadata_warnings(repo_root)

    print_section("path inventory", unmatched, "all files match a declared path rule")
    print_section("editor include drift", editor_includes, "no editor includes found outside editor roots")
    print_section("license metadata", metadata, "tool metadata matches the target license model")

    total_warnings = len(unmatched) + len(editor_includes) + len(metadata)
    print(f"\n[boundary] warnings : {total_warnings}")
    print("[boundary] result   : report-only; warnings do not fail Phase 1 CI")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
