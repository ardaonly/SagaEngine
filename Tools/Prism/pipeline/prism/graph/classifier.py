# @file graph/classifier.py
# @brief File category assignment and logical module derivation from POSIX paths.
#
# Both functions are pure and side-effect-free.

from __future__ import annotations

from pathlib import Path
from typing  import List, Tuple

# ─── Constants ────────────────────────────────────────────────────────────────

_KNOWN_SCOPES: frozenset[str] = frozenset({
    "Engine", "Editor", "Server", "Apps",
    "Tests", "Tools", "Shaders", "Content",
})

# Ordered rules — first match wins.
_CATEGORY_RULES: List[Tuple[str, Tuple[str, ...]]] = [
    ("headers", (".h", ".hpp", ".hh", ".hxx")),
    ("sources", (".cpp", ".c", ".cc", ".cxx", ".mm", ".m")),
    ("cmake",   (".cmake",)),
    ("others",  (".txt", ".md", ".json", ".yaml", ".yml")),
]

# ─── Public API ───────────────────────────────────────────────────────────────

def categorize(path: str) -> str:
    """
    Assign a snapshot category to *path*.
    Returns one of: headers | sources | cmake | others.
    """
    name = Path(path).name.lower()

    if name == "cmakelists.txt":
        return "cmake"

    suffix = Path(path).suffix.lower()
    for category, extensions in _CATEGORY_RULES:
        if suffix in extensions:
            return category

    return "others"


def module_from_path(rel_posix: str) -> str:
    """
    Derive a two-level logical module name from *rel_posix*.

    Examples:
        "Engine/Renderer/RenderGraph.h"  →  "Engine/Renderer"
        "Editor/UI/Toolbar.cpp"          →  "Editor/UI"
        "CMakeLists.txt"                 →  "root"
    """
    parts = rel_posix.split("/")

    if not parts or not parts[0]:
        return "root"

    if len(parts) == 1:
        return "root"

    # Two-level grouping regardless of whether the top level is a known scope.
    return f"{parts[0]}/{parts[1]}"
