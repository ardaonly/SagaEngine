# SPDX-License-Identifier: MPL-2.0
"""Small shared path and output helpers for Repo Xray."""

from pathlib import Path


def normalize_rel(path: str) -> str:
    return path.replace("\\", "/")


def wiki_area(path: str) -> str:
    parts = normalize_rel(path).split("/")
    if len(parts) >= 4 and parts[:2] == ["SagaWiki", "library"]:
        return parts[2]
    if len(parts) >= 3 and parts[0] == "SagaWiki":
        return parts[1]
    return "<wiki-root>"


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


# ─────────────────────────────────────────────────────────────────────────────
# Main analyzer
# ─────────────────────────────────────────────────────────────────────────────
