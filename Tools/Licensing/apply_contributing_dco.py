#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Arda Koyuncu
# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

from pathlib import Path
import argparse
import re
import tomllib

from licensing_common import repository_root


def section_body(source: Path) -> str:
    text = source.read_text(encoding="utf-8")
    text = re.sub(r"\A<!--.*?-->\s*", "", text, count=1, flags=re.DOTALL)
    return text.strip()


def expected_block(policy: dict, root: Path) -> str:
    governance = policy["governance"]
    body = section_body(root / governance["contributing_section_source"])
    return (
        governance["contributing_marker_begin"]
        + "\n"
        + body
        + "\n"
        + governance["contributing_marker_end"]
    )


def apply_block(current: str, block: str, begin: str, end: str) -> str:
    pattern = re.compile(
        re.escape(begin) + r".*?" + re.escape(end),
        re.DOTALL,
    )
    if pattern.search(current):
        updated = pattern.sub(block, current, count=1)
    else:
        updated = current.rstrip() + "\n\n" + block + "\n"
    return updated


def main() -> int:
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--apply", action="store_true")
    parser.add_argument("--root", type=Path)
    args = parser.parse_args()

    root = repository_root(args.root)
    policy = tomllib.loads(
        (root / "LICENSE_POLICY.toml").read_text(encoding="utf-8")
    )
    governance = policy["governance"]
    destination = root / governance["contributing_document"]
    if not destination.is_file():
        raise SystemExit(f"Missing {governance['contributing_document']}")

    block = expected_block(policy, root)
    current = destination.read_text(encoding="utf-8")
    updated = apply_block(
        current,
        block,
        governance["contributing_marker_begin"],
        governance["contributing_marker_end"],
    )

    if args.check:
        if current != updated:
            print("CONTRIBUTING DCO section is missing or stale.")
            return 1
        print("CONTRIBUTING DCO section is current.")
        return 0

    if current != updated:
        destination.write_text(updated, encoding="utf-8", newline="\n")
        print(f"Updated {destination.relative_to(root)}")
    else:
        print(f"No change: {destination.relative_to(root)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
