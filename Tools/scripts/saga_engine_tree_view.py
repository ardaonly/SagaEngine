"""
@file saga_engine_tree_view.py
@brief Print a human-readable directory tree for the SagaEngine repository.
"""

from __future__ import annotations

from pathlib import Path
from typing import Iterable


MARKER_FILE = "SagaEngineRoot.marker"

IGNORE_DIRS = {
    ".git",
    "build",
    "Build",
    "cache",
    "bin",
    "Thirdparty",
    ".vscode",
    ".thirdparty_build",
    "__pycache__",
    ".toolchain",
}

IGNORE_FILES = {
    MARKER_FILE,
}


def find_engine_root(start: Path | None = None) -> Path:
    """
    Walk upward from the current file location until the engine root marker is found.
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


def should_ignore(path: Path) -> bool:
    """
    Return True if the path should be skipped from the printed tree.
    """
    if path.is_dir():
        return path.name in IGNORE_DIRS

    return path.name in IGNORE_FILES


def iter_children(path: Path) -> list[Path]:
    """
    Return filtered children sorted with directories first, then files.
    """
    items = []
    try:
        for child in path.iterdir():
            if should_ignore(child):
                continue
            items.append(child)
    except OSError:
        return []

    return sorted(
        items,
        key=lambda p: (
            p.is_file(),
            p.name.lower(),
        ),
    )


def format_name(path: Path) -> str:
    """
    Format directory names with a trailing slash for readability.
    """
    return f"{path.name}/" if path.is_dir() else path.name


def print_tree(path: Path, prefix: str = "", is_last: bool = True) -> None:
    """
    Print a tree view for the given path.
    """
    connector = "" if not prefix else ("└── " if is_last else "├── ")
    print(f"{prefix}{connector}{format_name(path)}")

    if path.is_file():
        return

    children = iter_children(path)
    next_prefix = prefix + ("    " if is_last else "│   ")

    for index, child in enumerate(children):
        last_child = index == len(children) - 1
        print_tree(child, next_prefix, last_child)


def main() -> None:
    root = find_engine_root()
    print("ENGINE STRUCTURE\n")
    print_tree(root)


if __name__ == "__main__":
    main()