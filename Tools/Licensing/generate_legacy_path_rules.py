#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Arda Koyuncu
# SPDX-License-Identifier: MPL-2.0

from __future__ import annotations

from pathlib import Path
import argparse
import json
import tomllib

from licensing_common import repository_root


def build_payload(policy: dict) -> dict:
    rules = []
    for domain in policy.get("domains", []):
        mode = domain.get("current_license_mode")
        expression = domain.get("current_license_expression", "")
        if mode == "fixed-spdx":
            license_value = expression
        elif mode == "upstream-file-specific":
            license_value = "UPSTREAM"
        elif mode == "file-specific":
            license_value = "FILE-SPECIFIC"
        elif mode == "user-selected":
            license_value = "USER-SELECTED"
        else:
            license_value = "NOT-APPLICABLE"

        for pattern in domain.get("paths", []):
            rules.append(
                {
                    "pattern": pattern,
                    "license": license_value,
                    "domain_id": domain["id"],
                    "priority": domain["priority"],
                }
            )

    return {
        "schema": "sagaengine.path_rules.v1",
        "version": "1.0",
        "mode": "deprecated-compatibility",
        "authoritative": False,
        "source_of_truth": "LICENSE_POLICY.toml",
        "generated_by": "Tools/Licensing/generate_legacy_path_rules.py",
        "description": (
            "Compatibility projection only. LICENSE_POLICY.toml is authoritative."
        ),
        "rules": sorted(
            rules,
            key=lambda item: (
                -item["priority"],
                item["pattern"],
                item["domain_id"],
            ),
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path)
    parser.add_argument(
        "--output",
        default="Tools/Developer/BoundaryCheck/Policies/path_rules.json",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="fail without writing when the generated compatibility rules drift",
    )
    args = parser.parse_args()

    root = repository_root(args.root)
    policy = tomllib.loads(
        (root / "LICENSE_POLICY.toml").read_text(encoding="utf-8")
    )
    output = root / args.output
    expected = json.dumps(build_payload(policy), indent=2) + "\n"
    if args.check:
        current = output.read_text(encoding="utf-8") if output.is_file() else ""
        if current != expected:
            print(f"{output.relative_to(root)} is stale.")
            return 1
        print(f"{output.relative_to(root)} is current.")
        return 0

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(expected, encoding="utf-8", newline="\n")
    print(output.relative_to(root))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
