"""
@file saga_engine_ai_snapshot.py
@brief Generate a machine-readable and human-readable snapshot of the SagaEngine repository.
"""

from __future__ import annotations

import os
from pathlib import Path
from typing import Dict, Iterable, List, Tuple


MARKER_FILE = "SagaEngineRoot.marker"
OUTPUT_FILE = "engine_ai_snapshot.txt"

IGNORE_DIRS = {
    ".git",
    "build",
    "Build",
    "bin",
    "cache",
    "Thirdparty",
    ".vscode",
    ".thirdparty_build",
    ".toolchain",
    "__pycache__",
}

IGNORE_FILES = {
    MARKER_FILE,
    OUTPUT_FILE,
    "LICENSE",
}

IGNORE_EXTENSIONS = {
    ".log",
    ".tmp",
    ".cache",
    ".bin",
}

IGNORE_PATTERNS = (
    ".gitignore",
    ".DS_Store",
)

CATEGORY_RULES: List[Tuple[str, Tuple[str, ...]]] = [
    ("headers", (".h", ".hpp", ".hh")),
    ("sources", (".cpp", ".c", ".cc", ".cxx")),
    ("cmake", (".cmake",)),
    ("others", (".txt", ".md", ".json", ".yaml", ".yml")),
]


def find_engine_root(start: Path | None = None) -> Path:
    """
    Walk upward until the repository root marker is found.
    """
    current = (start or Path(__file__).resolve()).parent

    while True:
        if (current / MARKER_FILE).is_file():
            return current

        parent = current.parent
        if parent == current:
            raise RuntimeError(
                f"Root not found or '{MARKER_FILE}' is missing."
            )

        current = parent


def should_ignore_directory(dir_name: str) -> bool:
    return dir_name in IGNORE_DIRS


def should_ignore_file(file_name: str) -> bool:
    if file_name in IGNORE_FILES:
        return True

    suffix = Path(file_name).suffix.lower()
    if suffix in IGNORE_EXTENSIONS:
        return True

    return any(pattern in file_name for pattern in IGNORE_PATTERNS)


def categorize_file(file_name: str) -> str:
    """
    Assign a file to one of the snapshot categories.
    """
    lower_name = file_name.lower()

    for category, extensions in CATEGORY_RULES:
        if category == "cmake" and lower_name == "cmakelists.txt":
            return category

        if any(lower_name.endswith(ext) for ext in extensions):
            return category

    return "others"


def collect_files(root: Path) -> Dict[str, List[Path]]:
    """
    Walk the repository and collect files by category.
    """
    categorized: Dict[str, List[Path]] = {
        "headers": [],
        "sources": [],
        "cmake": [],
        "others": [],
    }

    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if not should_ignore_directory(d)]

        for file_name in filenames:
            if should_ignore_file(file_name):
                continue

            full_path = Path(dirpath) / file_name
            category = categorize_file(file_name)
            categorized[category].append(full_path)

    return categorized


def count_lines(files: Iterable[Path]) -> int:
    total = 0

    for file_path in files:
        try:
            with file_path.open("r", encoding="utf-8", errors="ignore") as fh:
                total += sum(1 for _ in fh)
        except OSError:
            continue

    return total


def relative_path(root: Path, path: Path) -> str:
    return path.relative_to(root).as_posix()


def write_snapshot(output_path: Path, root: Path, categorized_files: Dict[str, List[Path]]) -> None:
    """
    Write a structured text snapshot for AI and human inspection.
    """
    with output_path.open("w", encoding="utf-8", errors="ignore") as out:
        out.write("===== SAGAENGINE AI SNAPSHOT =====\n\n")

        out.write("===== FILE INDEX =====\n")
        for category in ["headers", "sources", "cmake", "others"]:
            files = sorted(categorized_files.get(category, []), key=lambda p: relative_path(root, p).lower())
            out.write(f"\n[{category.upper()}] {len(files)} files\n")
            for file_path in files:
                out.write(f"{relative_path(root, file_path)}\n")

        out.write("\n===== FILE CONTENTS =====\n")
        for category in ["headers", "sources", "cmake", "others"]:
            files = sorted(categorized_files.get(category, []), key=lambda p: relative_path(root, p).lower())
            out.write(f"\n[{category.upper()}]\n")

            for file_path in files:
                rel = relative_path(root, file_path)
                out.write("\n" + "=" * 90 + "\n")
                out.write(f"FILE: /{rel}\n")
                out.write("=" * 90 + "\n\n")

                try:
                    with file_path.open("r", encoding="utf-8", errors="ignore") as fh:
                        out.write(fh.read())
                        if not str(file_path).endswith("\n"):
                            out.write("\n")
                except OSError as exc:
                    out.write(f"[ERROR READING FILE: {exc}]\n")

        out.write("\n===== STATISTICS =====\n")
        total_files = 0
        total_lines = 0

        for category in ["headers", "sources", "cmake", "others"]:
            files = categorized_files.get(category, [])
            lines = count_lines(files)
            total_files += len(files)
            total_lines += lines
            out.write(f"{category}: {len(files)} files, {lines} lines\n")

        out.write(f"Total files: {total_files}\n")
        out.write(f"Total lines: {total_lines}\n")


def main() -> None:
    root = find_engine_root()
    categorized_files = collect_files(root)
    output_path = root / OUTPUT_FILE
    write_snapshot(output_path, root, categorized_files)
    print(f"Engine AI snapshot created: {output_path}")


if __name__ == "__main__":
    main()