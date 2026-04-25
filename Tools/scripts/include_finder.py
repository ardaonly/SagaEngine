#!/usr/bin/env python3
"""
@file include_finder.py
@brief Scan a C++ repository and report where headers are included.
"""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable

ROOT_MARKER = "SagaEngineRoot.marker"

SOURCE_EXTENSIONS = {".h", ".hpp", ".hh", ".hxx", ".cpp", ".cc", ".cxx", ".inl"}
HEADER_EXTENSIONS = {".h", ".hpp", ".hh", ".hxx"}

IGNORE_DIRS = {
    ".git",
    ".vscode",
    "__pycache__",
    "build",
    "Build",
    "bin",
    "dist",
    "Thirdparty",
    ".thirdparty_build",
    ".toolchain",
}

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^">]+)[">]')


@dataclass(frozen=True)
class IncludeRef:
    """One include occurrence inside a source file."""
    source_file: Path
    line_number: int
    include_text: str
    raw_line: str = field(repr=False)


def find_repo_root(start: Path | None = None) -> Path:
    """
    Walk upward until the repository marker is found.
    """
    current = (start or Path.cwd()).resolve()

    for candidate in (current, *current.parents):
        if (candidate / ROOT_MARKER).is_file():
            return candidate

    raise RuntimeError(f"Repository root not found. Missing '{ROOT_MARKER}'.")


def should_skip_dir(name: str) -> bool:
    return name in IGNORE_DIRS


def iter_source_files(root: Path) -> Iterable[Path]:
    """
    Yield all C/C++ source and header files in the repository.
    """
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix.lower() not in SOURCE_EXTENSIONS:
            continue
        if any(part in IGNORE_DIRS for part in path.parts):
            continue
        yield path


def parse_includes(file_path: Path) -> list[IncludeRef]:
    """
    Extract #include directives from a file.
    """
    try:
        text = file_path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return []

    refs: list[IncludeRef] = []
    for line_number, line in enumerate(text.splitlines(), start=1):
        match = INCLUDE_RE.match(line)
        if not match:
            continue

        include_text = match.group(2).strip()
        refs.append(
            IncludeRef(
                source_file=file_path,
                line_number=line_number,
                include_text=include_text,
                raw_line=line,
            )
        )
    return refs


def normalize_rel_path(path: Path) -> str:
    return path.as_posix().lower()


def build_include_index(root: Path) -> dict[str, list[IncludeRef]]:
    """
    Build a reverse index: include text -> list of files that include it.
    """
    index: dict[str, list[IncludeRef]] = {}

    for file_path in iter_source_files(root):
        for ref in parse_includes(file_path):
            key = ref.include_text.replace("\\", "/").lower()
            index.setdefault(key, []).append(ref)

    return index


def candidate_keys_for_query(query: str, root: Path) -> set[str]:
    """
    Generate reasonable lookup keys for a user query.
    """
    q = query.strip().replace("\\", "/").lower()
    keys = {q}

    if q:
        keys.add(Path(q).name.lower())

    query_path = Path(q)
    if query_path.suffix:
        keys.add(query_path.name.lower())

    # If the query points to an existing file, add repo-relative variants too.
    possible_paths = []
    if query_path.is_absolute():
        possible_paths.append(query_path)
    else:
        possible_paths.append(root / query_path)
        possible_paths.extend(root.rglob(query_path.name))

    for p in possible_paths:
        try:
            rel = p.resolve().relative_to(root.resolve())
            rel_text = normalize_rel_path(rel)
            keys.add(rel_text)
            keys.add(Path(rel_text).name.lower())
        except Exception:
            pass

    return keys


def matches_query(include_text: str, query_keys: set[str]) -> bool:
    """
    Match include text against query keys with a few practical fallbacks.
    """
    value = include_text.replace("\\", "/").lower()
    basename = Path(value).name.lower()

    if value in query_keys or basename in query_keys:
        return True

    for key in query_keys:
        if not key:
            continue
        if value.endswith("/" + key):
            return True
        if basename == key:
            return True
        if key in value:
            return True

    return False


def find_header_includers(root: Path, query: str) -> list[IncludeRef]:
    """
    Find every source file that includes the queried header.
    """
    query_keys = candidate_keys_for_query(query, root)
    results: list[IncludeRef] = []

    for file_path in iter_source_files(root):
        for ref in parse_includes(file_path):
            if matches_query(ref.include_text, query_keys):
                results.append(ref)

    results.sort(
        key=lambda r: (
            r.source_file.as_posix().lower(),
            r.line_number,
            r.include_text.lower(),
        )
    )
    return results


def print_results(root: Path, query: str, refs: list[IncludeRef]) -> None:
    """
    Print reverse-include results in a readable format.
    """
    if not refs:
        print(f"No include references found for: {query}")
        return

    print(f"Repository root: {root}")
    print(f"Query: {query}")
    print(f"Matches: {len(refs)}\n")

    current_source: Path | None = None
    for ref in refs:
        if ref.source_file != current_source:
            current_source = ref.source_file
            try:
                rel_source = current_source.relative_to(root)
            except ValueError:
                rel_source = current_source
            print(f"{rel_source.as_posix()}")

        print(f"  line {ref.line_number}: #include \"{ref.include_text}\"")


def print_all_reverse_index(root: Path) -> None:
    """
    Print every include target and the files that reference it.
    """
    index: dict[str, list[IncludeRef]] = {}
    for file_path in iter_source_files(root):
        for ref in parse_includes(file_path):
            key = ref.include_text.replace("\\", "/").lower()
            index.setdefault(key, []).append(ref)

    for include_text in sorted(index.keys()):
        print(f"\n#include \"{include_text}\"")
        for ref in index[include_text]:
            try:
                rel_source = ref.source_file.relative_to(root)
            except ValueError:
                rel_source = ref.source_file
            print(f"  {rel_source.as_posix()}:{ref.line_number}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Find where a C++ header is included in the SagaEngine repository."
    )
    parser.add_argument(
        "query",
        nargs="?",
        help="Header name or path fragment, for example SDLInputBackend.h or Renderer/Renderer.h",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Print the full reverse include map for the repository.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    try:
        root = find_repo_root()
    except RuntimeError as exc:
        print(exc)
        return 1

    if args.all:
        print_all_reverse_index(root)
        return 0

    query = args.query
    if query is None:
        query = input("Which header are you looking for? ").strip()

    if not query:
        print("Search term cannot be empty.")
        return 1

    refs = find_header_includers(root, query)
    print_results(root, query, refs)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())