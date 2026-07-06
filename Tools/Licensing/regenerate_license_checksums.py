#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Arda Koyuncu
# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

from pathlib import Path
import argparse
import tomllib

from licensing_common import (
    expected_checksum_paths,
    repository_root,
    sha256,
)


def render(root: Path, include_legacy: bool) -> str:
    paths = expected_checksum_paths(
        root,
        include_existing_legacy=include_legacy,
    )
    missing = sorted(path for path in paths if not (root / path).is_file())
    if missing:
        raise RuntimeError(f"Missing checksum inputs: {missing}")
    return "\n".join(
        f"{sha256(root / path)}  {path}"
        for path in sorted(paths)
    ) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--include-existing-legacy", action="store_true")
    mode.add_argument("--exclude-legacy", action="store_true")
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--root", type=Path)
    args = parser.parse_args()

    root = repository_root(args.root)
    policy = tomllib.loads(
        (root / "LICENSE_POLICY.toml").read_text(encoding="utf-8")
    )

    if args.include_existing_legacy:
        include_legacy = True
    elif args.exclude_legacy:
        include_legacy = False
    else:
        include_legacy = policy["legacy"]["cleanup_status"] != "complete"

    expected = render(root, include_legacy)
    destination = root / "SHA256SUMS"

    if args.check:
        current = (
            destination.read_text(encoding="utf-8")
            if destination.is_file()
            else ""
        )
        if current != expected:
            print("SHA256SUMS is stale.")
            return 1
        print("SHA256SUMS is current.")
        return 0

    destination.write_text(expected, encoding="utf-8", newline="\n")
    print(
        f"Wrote {len(expected.splitlines())} checksum records "
        f"(legacy={'included' if include_legacy else 'excluded'})."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
