"""
@file saga_source_finder.py
@brief Find C++ source and header files in the SagaEngine repository.
"""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Iterable

ROOT_MARKER = "SagaEngineRoot.marker"

VALID_EXTENSIONS = {".cpp", ".h", ".hpp", ".cc", ".cxx"}

IGNORE_DIRS = {
    ".git",
    "build",
    "Build",
    "bin",
    "Thirdparty",
    ".vscode",
    ".thirdparty_build",
    "__pycache__",
    ".toolchain",
}

IGNORE_FILES = {
    ROOT_MARKER,
}


def find_repo_root(start: Path | None = None) -> Path:
    """
    Walk upward from the given start path until the repository marker is found.
    """
    current = (start or Path.cwd()).resolve()

    for candidate in (current, *current.parents):
        if (candidate / ROOT_MARKER).is_file():
            return candidate

    raise RuntimeError(
        f"Repository root not found. Missing '{ROOT_MARKER}'."
    )


def normalize_query(query: str) -> tuple[str, str | None]:
    """
    Normalize a user query into a file name fragment and optional extension.
    """
    value = query.strip()
    if not value:
        return "", None

    name = Path(value).name.lower()
    suffix = Path(value).suffix.lower()

    if suffix in VALID_EXTENSIONS:
        return name, suffix

    return value.lower(), None


def should_skip_directory(name: str) -> bool:
    return name in IGNORE_DIRS


def should_skip_file(name: str) -> bool:
    return name in IGNORE_FILES


def is_match(path: Path, query_name: str, query_ext: str | None) -> bool:
    """
    Return True if the path matches the query.
    """
    file_name = path.name.lower()
    stem = path.stem.lower()
    ext = path.suffix.lower()

    if ext not in VALID_EXTENSIONS:
        return False

    if query_ext is not None:
        return file_name == query_name

    return query_name in file_name or query_name in stem


def search_sources(root: Path, query: str) -> list[Path]:
    """
    Search the repository for matching source/header files.
    """
    query_name, query_ext = normalize_query(query)
    if not query_name:
        return []

    results: list[Path] = []

    for dirpath, dirnames, filenames in root.walk():
        dirnames[:] = [d for d in dirnames if not should_skip_directory(d)]

        for filename in filenames:
            if should_skip_file(filename):
                continue

            file_path = dirpath / filename
            if is_match(file_path, query_name, query_ext):
                results.append(file_path.resolve())

    return sorted(set(results), key=lambda p: p.as_posix().lower())


def print_results(root: Path, query: str, results: Iterable[Path]) -> None:
    """
    Print search results in a readable format.
    """
    results = list(results)

    if not results:
        print(f"No matching source file found for: {query}")
        return

    print(f"Repository root: {root}")
    print(f"Matches for: {query}\n")

    for path in results:
        try:
            relative = path.relative_to(root).as_posix()
        except ValueError:
            relative = path.as_posix()

        print(relative)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Find C++ source and header files in the SagaEngine repository."
    )
    parser.add_argument(
        "query",
        nargs="?",
        help="File name or fragment to search for, for example Input or SDLInputBackend.h",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    try:
        root = find_repo_root()
    except RuntimeError as exc:
        print(exc)
        return 1

    query = args.query
    if query is None:
        query = input("Which .cpp or .h file are you looking for? ").strip()

    if not query:
        print("Search term cannot be empty.")
        return 1

    results = search_sources(root, query)
    print_results(root, query, results)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())