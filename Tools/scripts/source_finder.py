from __future__ import annotations

from pathlib import Path
import os
from typing import Iterable


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
    "SagaEngineRoot.marker",
}

ROOT_MARKER = "SagaEngineRoot.marker"


def find_repo_root(start: Path) -> Path | None:
    for current in [start, *start.parents]:
        marker = current / ROOT_MARKER
        if marker.is_file():
            return current

    for current in [start, *start.parents]:
        for marker in current.rglob(ROOT_MARKER):
            return marker.parent

    return None


def normalize_query(query: str) -> tuple[str, str | None]:
    value = query.strip()
    if not value:
        return "", None

    suffix = Path(value).suffix.lower()
    if suffix in VALID_EXTENSIONS:
        return Path(value).name.lower(), suffix

    return value.lower(), None


def matches_file(path: Path, query_name: str, query_ext: str | None) -> bool:
    name = path.name.lower()
    stem = path.stem.lower()
    ext = path.suffix.lower()

    if query_ext is not None:
        return name == query_name

    return ext in VALID_EXTENSIONS and (query_name in name or query_name in stem)


def search_sources(root: Path, query: str) -> list[Path]:
    query_name, query_ext = normalize_query(query)
    if not query_name:
        return []

    results: list[Path] = []

    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in IGNORE_DIRS]

        for filename in filenames:
            if filename in IGNORE_FILES:
                continue

            file_path = Path(dirpath) / filename
            if file_path.suffix.lower() not in VALID_EXTENSIONS:
                continue

            if matches_file(file_path, query_name, query_ext):
                results.append(file_path.resolve())

    return sorted(results)


def main() -> int:
    start = Path.cwd()
    root = find_repo_root(start)

    if root is None:
        print(f"Repository root not found. '{ROOT_MARKER}' is missing.")
        return 1

    query = input("Which .cpp or .h file are you looking for? ").strip()
    if not query:
        print("Search term cannot be empty.")
        return 1

    results = search_sources(root, query)

    if not results:
        print(f"No matching source file found for: {query}")
        return 0

    print(f"Repository root: {root}")
    print(f"Matches for: {query}")
    for path in results:
        print(path)
        try:
            print(f"  Relative: {path.relative_to(root)}")
        except ValueError:
            pass

    return 0


if __name__ == "__main__":
    raise SystemExit(main())