#!/usr/bin/env python3
"""Fail when Qt includes appear outside the EditorQt owner module."""

from __future__ import annotations

from pathlib import Path
import re
import sys


EXTENSIONS = {".h", ".hpp", ".cpp", ".cc", ".cxx", ".inl"}
QT_INCLUDE_RE = re.compile(
    r'^\s*#\s*include\s*[<"](?P<include>(?:Qt[^>/"]+/)?Q[A-Z][^>"]*)[>"]',
    re.MULTILINE,
)


def find_violations(repo_root: Path) -> list[tuple[Path, list[str]]]:
    editor_root = repo_root / "Engine" / "Source" / "Editor"
    permitted = editor_root / "EditorQt"
    if not editor_root.is_dir() or not permitted.is_dir():
        raise FileNotFoundError("Editor and EditorQt module roots must exist")

    violations: list[tuple[Path, list[str]]] = []
    for path in sorted(editor_root.rglob("*")):
        if not path.is_file() or path.suffix not in EXTENSIONS or permitted in path.parents:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        hits = [match.group("include") for match in QT_INCLUDE_RE.finditer(text)]
        if hits:
            violations.append((path.relative_to(repo_root), hits))
    return violations


def main(argv: list[str]) -> int:
    repo_root = Path(argv[1] if len(argv) > 1 else ".").resolve()
    try:
        violations = find_violations(repo_root)
    except FileNotFoundError as error:
        print(f"[check_qt_boundary] ERROR - {error}")
        return 1
    if violations:
        print("[check_qt_boundary] ERROR - Qt includes outside EditorQt")
        for path, includes in violations:
            print(f"  {path}: {', '.join(includes)}")
        return 1
    print("[check_qt_boundary] OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
