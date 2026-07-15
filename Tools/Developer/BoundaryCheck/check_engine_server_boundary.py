#!/usr/bin/env python3
"""Validate that server-authority implementation stays out of Networking."""

from __future__ import annotations

import argparse
from pathlib import Path
import re


AUTHORITY_TOKENS = (
    "ActorOwnershipRegistry",
    "AuthoritativeMovement",
    "ShardManager",
    "ZoneServer",
)


def find_violations(repo_root: Path) -> list[Path]:
    runtime_root = repo_root / "Engine" / "Source" / "Runtime"
    networking_root = runtime_root / "Networking"
    authority_root = runtime_root / "ServerAuthority"
    if not networking_root.is_dir() or not authority_root.is_dir():
        raise FileNotFoundError("Networking and ServerAuthority modules must exist")

    violations: list[Path] = []
    for path in sorted(networking_root.rglob("*")):
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        owns_authority_type = any(
            token in path.name or re.search(rf"\b(class|struct)\s+{token}\b", text)
            for token in AUTHORITY_TOKENS
        )
        if owns_authority_type:
            violations.append(path.relative_to(repo_root))
    return violations


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    args = parser.parse_args()
    repo_root = Path(args.repo_root).resolve()
    try:
        violations = find_violations(repo_root)
    except FileNotFoundError as error:
        print(f"[engine_server_boundary] ERROR - {error}")
        return 1
    if violations:
        print("[engine_server_boundary] ERROR - authority ownership found in Networking")
        for path in violations:
            print(f"  {path}")
        return 1
    print("[engine_server_boundary] OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
