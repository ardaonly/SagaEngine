#!/usr/bin/env python3
"""Thin SagaSync entrypoint."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from core import build_smoke_payload, discover_repo_root
from ui.app import run_gui


def run_smoke(repo_root: Path) -> int:
    payload = build_smoke_payload(repo_root)
    print(json.dumps(payload, indent=2, sort_keys=True))
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="SagaEngine multirepo orchestration dashboard.")
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument("--smoke", action="store_true", help="Run non-GUI model checks and exit.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = discover_repo_root(args.repo_root)
    if args.smoke:
        return run_smoke(repo_root)
    return run_gui(repo_root)


if __name__ == "__main__":
    raise SystemExit(main())
