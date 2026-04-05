"""
@file saga_structure_manifest.py
@brief Build a machine-readable manifest of the SagaEngine directory architecture.

This script inspects folders and filenames only.
It does not read source file contents.
"""

from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Optional


# ─── Configuration ─────────────────────────────────────────────────────────────

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
    ".DS_Store",
}

TEXTUAL_DOC_FILES = {
    "README",
    "README.md",
    "CHANGELOG",
    "LICENSE",
    "CONTRIBUTING",
}

ROLE_HINTS: list[tuple[re.Pattern[str], str]] = [
    (re.compile(r"(^|[\\/])(src|source|sources)([\\/]|$)", re.IGNORECASE), "source"),
    (re.compile(r"(^|[\\/])(include|headers?)([\\/]|$)", re.IGNORECASE), "public_api"),
    (re.compile(r"(^|[\\/])tests?([\\/]|$)", re.IGNORECASE), "tests"),
    (re.compile(r"(^|[\\/])tools?([\\/]|$)", re.IGNORECASE), "tools"),
    (re.compile(r"(^|[\\/])docs?([\\/]|$)", re.IGNORECASE), "documentation"),
    (re.compile(r"(^|[\\/])assets?([\\/]|$)", re.IGNORECASE), "assets"),
    (re.compile(r"(^|[\\/])shaders?([\\/]|$)", re.IGNORECASE), "shaders"),
    (re.compile(r"(^|[\\/])platform([\\/]|$)", re.IGNORECASE), "platform"),
    (re.compile(r"(^|[\\/])net(work|code)?([\\/]|$)", re.IGNORECASE), "network"),
    (re.compile(r"(^|[\\/])input([\\/]|$)", re.IGNORECASE), "input"),
    (re.compile(r"(^|[\\/])render(ing)?([\\/]|$)", re.IGNORECASE), "rendering"),
    (re.compile(r"(^|[\\/])audio([\\/]|$)", re.IGNORECASE), "audio"),
    (re.compile(r"(^|[\\/])editor([\\/]|$)", re.IGNORECASE), "editor"),
]

EXTENSION_KIND = {
    ".h": "header",
    ".hpp": "header",
    ".hh": "header",
    ".inl": "inline",
    ".cpp": "source",
    ".cc": "source",
    ".cxx": "source",
    ".c": "source",
    ".py": "script",
    ".md": "document",
    ".txt": "document",
    ".cmake": "build_script",
    ".ini": "config",
    ".json": "config",
    ".yml": "config",
    ".yaml": "config",
    ".toml": "config",
    ".xml": "config",
}


# ─── Data Model ────────────────────────────────────────────────────────────────

@dataclass
class Node:
    """A directory or file entry in the engine architecture tree."""

    name: str
    path: str
    kind: str  # "directory" or "file"
    depth: int
    role: Optional[str] = None
    extension: Optional[str] = None
    tags: list[str] = field(default_factory=list)
    children: list["Node"] = field(default_factory=list)

    def to_dict(self) -> dict[str, Any]:
        data: dict[str, Any] = {
            "name": self.name,
            "path": self.path,
            "kind": self.kind,
            "depth": self.depth,
        }

        if self.role is not None:
            data["role"] = self.role

        if self.extension is not None:
            data["extension"] = self.extension

        if self.tags:
            data["tags"] = self.tags

        if self.children:
            data["children"] = [child.to_dict() for child in self.children]

        return data


# ─── Root Discovery ───────────────────────────────────────────────────────────

def find_engine_root(start: Optional[Path] = None) -> Path:
    """Find the engine root by walking upward until the marker file is found."""
    current = (start or Path(__file__).resolve()).parent

    while True:
        if (current / MARKER_FILE).is_file():
            return current

        if current.parent == current:
            raise RuntimeError(f"Root not found. Missing '{MARKER_FILE}'.")

        current = current.parent


# ─── Classification ───────────────────────────────────────────────────────────

def should_ignore_dir(name: str) -> bool:
    """Return True when a directory should be skipped."""
    return name in IGNORE_DIRS


def should_ignore_file(name: str) -> bool:
    """Return True when a file should be skipped."""
    return name in IGNORE_FILES


def classify_extension(path: Path) -> str:
    """Map a file extension to a coarse file kind."""
    suffix = path.suffix.lower()
    return EXTENSION_KIND.get(suffix, "file")


def classify_role(path: Path, root: Path) -> Optional[str]:
    """
    Infer an architectural role from the relative path only.
    No file contents are inspected.
    """
    rel = path.relative_to(root).as_posix()

    for pattern, role in ROLE_HINTS:
        if pattern.search(rel):
            return role

    parts = [p.lower() for p in path.parts]

    if path.is_dir():
        if "include" in parts:
            return "public_api"
        if "src" in parts or "source" in parts:
            return "source"
        if "tests" in parts:
            return "tests"
        if "tools" in parts:
            return "tools"

    if path.is_file():
        name = path.stem.lower()
        if name in {x.lower() for x in TEXTUAL_DOC_FILES}:
            return "documentation"
        if name.endswith("test") or name.endswith("tests"):
            return "tests"

    return None


def add_tags(node: Node, path: Path) -> None:
    """Add useful machine-readable tags based on path structure."""
    lower_name = path.name.lower()
    lower_path = path.as_posix().lower()

    if path.is_dir():
        node.tags.append("directory")
    else:
        node.tags.append("file")
        if node.extension:
            node.tags.append(node.extension.lstrip("."))
        if node.role:
            node.tags.append(node.role)

    if "engine" in lower_name or "engine" in lower_path:
        node.tags.append("engine")
    if "saga" in lower_name or "saga" in lower_path:
        node.tags.append("saga")
    if "platform" in lower_path:
        node.tags.append("platform")
    if "thirdparty" in lower_path or "third_party" in lower_path:
        node.tags.append("thirdparty")
    if "tools" in lower_path:
        node.tags.append("tools")
    if "tests" in lower_path:
        node.tags.append("tests")


# ─── Tree Building ────────────────────────────────────────────────────────────

def build_tree(path: Path, root: Path, depth: int = 0) -> Node:
    """Build a recursive representation of the directory structure."""
    rel_path = "." if path == root else path.relative_to(root).as_posix()

    if path.is_dir():
        node = Node(
            name=root.name if path == root else path.name,
            path=rel_path,
            kind="directory",
            depth=depth,
            role=classify_role(path, root),
        )
        add_tags(node, path)

        children: list[Node] = []
        entries = sorted(path.iterdir(), key=lambda p: (not p.is_dir(), p.name.lower()))

        for entry in entries:
            if entry.is_dir() and should_ignore_dir(entry.name):
                continue
            if entry.is_file() and should_ignore_file(entry.name):
                continue
            children.append(build_tree(entry, root, depth + 1))

        node.children = children
        return node

    node = Node(
        name=path.name,
        path=rel_path,
        kind="file",
        depth=depth,
        role=classify_role(path, root),
        extension=path.suffix.lower() or None,
    )
    add_tags(node, path)
    return node


# ─── Output Formats ────────────────────────────────────────────────────────────

def to_markdown(node: Node) -> list[str]:
    """Render the tree as a compact Markdown outline."""
    lines: list[str] = []

    def walk(current: Node, indent: int = 0) -> None:
        pad = "  " * indent
        label = current.name + ("/" if current.kind == "directory" else "")
        extras = []

        if current.role:
            extras.append(current.role)
        if current.extension:
            extras.append(current.extension)
        if current.tags:
            extras.append(", ".join(current.tags[:4]))

        suffix = f" — {' | '.join(extras)}" if extras else ""
        lines.append(f"{pad}- {label}{suffix}")

        for child in current.children:
            walk(child, indent + 1)

    walk(node)
    return lines


def write_outputs(root: Path, tree: Node) -> None:
    """Write JSON and Markdown manifests into the engine root."""
    json_path = root / "saga_structure_manifest.json"
    md_path = root / "saga_structure_manifest.md"

    json_path.write_text(
        json.dumps(tree.to_dict(), indent=2, ensure_ascii=False),
        encoding="utf-8",
    )

    md_lines = [
        "# SagaEngine Structure Manifest",
        "",
        "Generated from directory structure only.",
        "Source file contents are not inspected.",
        "",
    ]
    md_lines.extend(to_markdown(tree))
    md_lines.append("")

    md_path.write_text("\n".join(md_lines), encoding="utf-8")


# ─── Main ─────────────────────────────────────────────────────────────────────

def main() -> None:
    """Entry point."""
    root = find_engine_root()
    tree = build_tree(root, root)
    write_outputs(root, tree)

    print("Generated:")
    print(f"- {root / 'saga_structure_manifest.json'}")
    print(f"- {root / 'saga_structure_manifest.md'}")


if __name__ == "__main__":
    main()