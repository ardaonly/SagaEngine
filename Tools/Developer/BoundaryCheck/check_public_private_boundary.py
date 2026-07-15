#!/usr/bin/env python3
"""Validate module Public/Private ownership and runtime ABI neutrality."""

from __future__ import annotations

import argparse
from pathlib import Path
import re


HEADER_SUFFIXES = {".h", ".hh", ".hpp", ".hxx"}
PRIVATE_INCLUDE_RE = re.compile(r"#\s*include\s*[<\"]([^>\"]*Private/[^>\"]*)[>\"]")
VENDOR_TOKENS = ("Diligent::", "Rml::")


def module_dirs(repo_root: Path) -> list[Path]:
    source_root = repo_root / "Engine" / "Source"
    groups = [source_root / "Runtime", source_root / "Editor", source_root / "Programs"]
    modules: list[Path] = []
    for group in groups:
        if not group.is_dir():
            raise FileNotFoundError(f"missing module group: {group}")
        modules.extend(sorted(path for path in group.iterdir() if path.is_dir()))
    return modules


def find_violations(repo_root: Path) -> list[str]:
    violations: list[str] = []
    for module in module_dirs(repo_root):
        manifest = module / "CMakeLists.txt"
        if not manifest.is_file():
            violations.append(f"{manifest.relative_to(repo_root)}: missing module manifest")

        public_root = module / "Public"
        if not public_root.is_dir():
            continue
        for path in sorted(public_root.rglob("*")):
            if not path.is_file() or path.suffix not in HEADER_SUFFIXES:
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            relative = path.relative_to(repo_root)
            if PRIVATE_INCLUDE_RE.search(text):
                violations.append(f"{relative}: public header includes a Private path")
            if "/Runtime/" in path.as_posix():
                for token in VENDOR_TOKENS:
                    if token in text:
                        violations.append(f"{relative}: public runtime header exposes {token}")
    return violations


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    args = parser.parse_args()
    repo_root = Path(args.repo_root).resolve()
    try:
        violations = find_violations(repo_root)
    except FileNotFoundError as error:
        print(f"[public_private_boundary] ERROR - {error}")
        return 1
    if violations:
        print("[public_private_boundary] ERROR")
        for violation in violations:
            print(f"  {violation}")
        return 1
    print("[public_private_boundary] OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
