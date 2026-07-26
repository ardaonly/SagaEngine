# SPDX-License-Identifier: MPL-2.0
"""Validate SagaEngine licensing policy and repository evidence."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

from licensing_common import repository_root
from licensing_policy import *
from licensing_policy import governance_validation as _governance_validation
from licensing_policy import repository_validation as _repository_validation


# Compatibility adapters preserve the historical monkey-patch surface used by
# focused tests and downstream policy tooling while implementations live in the
# shared package.
def validate_reviewed_commit(root, policy, report):
    _repository_validation.run = run
    return _repository_validation.validate_reviewed_commit(root, policy, report)


def validate_renames(root, reviewed_commit, domains, policy, report):
    _repository_validation.run = run
    return _repository_validation.validate_renames(
        root, reviewed_commit, domains, policy, report
    )


def validate_lfs(root, files, policy, report):
    _repository_validation.run = run
    _repository_validation.run_git = run_git
    _repository_validation.run_bytes = run_bytes
    return _repository_validation.validate_lfs(root, files, policy, report)


def validate_symlinks(root, policy, report):
    _repository_validation.tracked_modes = tracked_modes
    return _repository_validation.validate_symlinks(root, policy, report)


def validate_submodules(root, submodule_map, policy, report):
    _governance_validation.recursive_submodule_status = recursive_submodule_status
    return _governance_validation.validate_submodules(
        root, submodule_map, policy, report
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--report", action="store_true")
    mode.add_argument("--strict", action="store_true")
    parser.add_argument("--root", type=Path)
    parser.add_argument("--policy", type=Path)
    parser.add_argument("--cmake-build-dir", type=Path)
    parser.add_argument("--cmake-config")
    parser.add_argument("--base-ref")
    parser.add_argument("--json-output", type=Path)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        root = repository_root(args.root)
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2
    policy_path = args.policy.resolve() if args.policy else root / "LICENSE_POLICY.toml"
    if not policy_path.is_file():
        print(f"ERROR: policy not found: {policy_path}", file=sys.stderr)
        return 2
    build_dir = args.cmake_build_dir.resolve() if args.cmake_build_dir else None
    report = validate_policy(root, policy_path, build_dir=build_dir,
        cmake_config=args.cmake_config, base_ref=args.base_ref, strict=args.strict)
    print_report(report)
    if args.json_output:
        write_json_report(args.json_output, report)
    return 1 if args.strict and report.errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
