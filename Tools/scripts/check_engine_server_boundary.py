#!/usr/bin/env python3
"""Guard the temporary Engine public-header dependency on SagaServer.

This check does not make the current architecture debt acceptable forever. It
keeps the known Phase 1 compatibility exception from spreading while the packet
contracts are moved to SagaShared or engine-owned networking primitives.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys


ALLOWED_PUBLIC_HEADER_LEAKS: set[Path] = set()

LEAK_PATTERNS = (
    re.compile(r'#\s*include\s*[<"]SagaServer/'),
    re.compile(r"\bSagaServer::"),
)


def _is_header(path: Path) -> bool:
    return path.suffix in {".h", ".hpp", ".hh", ".hxx"}


def _relative(path: Path, root: Path) -> Path:
    return path.resolve().relative_to(root.resolve())


def find_unapproved_leaks(repo_root: Path) -> list[tuple[Path, int, str]]:
    engine_include = repo_root / "Engine" / "include"
    if not engine_include.exists():
        raise FileNotFoundError(f"missing Engine public include root: {engine_include}")

    leaks: list[tuple[Path, int, str]] = []
    for path in sorted(p for p in engine_include.rglob("*") if p.is_file() and _is_header(p)):
        rel = _relative(path, repo_root)
        if rel in ALLOWED_PUBLIC_HEADER_LEAKS:
            continue

        for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
            if any(pattern.search(line) for pattern in LEAK_PATTERNS):
                leaks.append((rel, line_no, line.strip()))

    return leaks


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--repo-root",
        default=".",
        help="Repository root to scan. Defaults to the current directory.",
    )
    args = parser.parse_args()

    repo_root = Path(args.repo_root).resolve()
    leaks = find_unapproved_leaks(repo_root)
    if leaks:
        print("[engine_server_boundary] ERROR - unapproved SagaServer public-header leaks:")
        for path, line_no, line in leaks:
            print(f"  {path}:{line_no}: {line}")
        print()
        print("[engine_server_boundary] Allowed temporary exceptions:")
        for path in sorted(ALLOWED_PUBLIC_HEADER_LEAKS):
            print(f"  {path}")
        return 1

    print("[engine_server_boundary] OK - zero Engine public-header SagaServer leaks")
    return 0


if __name__ == "__main__":
    sys.exit(main())
