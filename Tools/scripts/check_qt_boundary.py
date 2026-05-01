#!/usr/bin/env python3
"""Fail the build if Qt sneaks back outside Editor/.../UI/Qt/.

Run from the repo root:
    python tools/check_qt_boundary.py

Exits 0 when the boundary is clean, 1 when something includes a Qt
header outside the permitted folder. The CI pre-merge job runs this
before every editor build so the boundary cannot regress unnoticed.

The rule is documented in EDITOR_ROADMAP.md: Qt headers may only
appear inside `Editor/include/SagaEditor/UI/Qt/` and
`Editor/src/SagaEditor/UI/Qt/`. Every Qt-using `.cpp` lives there.
The framework-free public header stays in its original folder so
call sites never learn about Qt.
"""

from __future__ import annotations

import os
import re
import sys
from typing import List, Tuple

# ─── Configuration ────────────────────────────────────────────────────────────

EDITOR_ROOT  = "Editor"
PERMITTED    = (
    "Editor/include/SagaEditor/UI/Qt",
    "Editor/src/SagaEditor/UI/Qt",
)
EXTENSIONS   = (".h", ".hpp", ".cpp", ".cc", ".cxx", ".inl")

# Match `#include <QXxx[/...]>` and `#include "QXxx[/...]"` where the
# leading basename is `Q` followed by an upper-case letter (Qt convention).
QT_INCLUDE_RE = re.compile(
    r'^\s*#\s*include\s*[<"](?P<inc>(?:[^>"]+/)?Q[A-Z][^>"]*)[>"]',
    re.MULTILINE,
)


def normalise(path: str) -> str:
    return path.replace("\\", "/")


def is_under(path: str, prefix: str) -> bool:
    np = normalise(path)
    pp = normalise(prefix)
    return np.startswith(pp + "/") or np == pp


def find_violations(repo_root: str) -> List[Tuple[str, List[str]]]:
    edit_root = os.path.join(repo_root, EDITOR_ROOT)
    if not os.path.isdir(edit_root):
        print(f"  warning: {edit_root!r} not found; nothing to check", file=sys.stderr)
        return []

    permitted_abs = [os.path.normpath(os.path.join(repo_root, p)) for p in PERMITTED]
    violations: List[Tuple[str, List[str]]] = []

    for root, _, files in os.walk(edit_root):
        if any(is_under(root, perm) for perm in permitted_abs):
            continue
        for fname in files:
            if not fname.endswith(EXTENSIONS):
                continue
            full = os.path.join(root, fname)
            try:
                text = open(full, encoding="utf-8", errors="replace").read()
            except OSError:
                continue
            hits = [m.group("inc") for m in QT_INCLUDE_RE.finditer(text)]
            if hits:
                rel = os.path.relpath(full, repo_root).replace("\\", "/")
                violations.append((rel, hits))
    return violations


def main(argv: List[str]) -> int:
    repo_root = argv[1] if len(argv) > 1 else os.getcwd()

    violations = find_violations(repo_root)

    if not violations:
        print("[check_qt_boundary] OK — zero Qt includes outside Editor/.../UI/Qt/")
        return 0

    print("[check_qt_boundary] FAIL — Qt includes leaked outside the permitted folder.\n")
    for rel, hits in violations:
        print(f"  {rel}")
        for h in hits[:5]:
            print(f"      #include <{h}>")
        if len(hits) > 5:
            print(f"      ... and {len(hits) - 5} more")
    print()
    print("Permitted Qt-bearing folders:")
    for p in PERMITTED:
        print(f"  - {p}")
    print()
    print("Move the Qt-using .cpp into Editor/src/SagaEditor/UI/Qt/<topic>/")
    print("and keep the public header (which must stay Qt-free) where it is.")
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
