#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Generate the ignored source-only repository snapshot through Repomix."""

from __future__ import annotations

import argparse
import pathlib
import subprocess


INCLUDED_ROOTS = {
    ".github", "Engine", "Tools", "Tests", "cmake", "Samples", "SagaWiki",
    "scripts",
}
EXCLUDED_ROOTS = {".git", ".saga-private", ".agents", ".codex", "Vendor", "Build", "build"}
EXCLUDED_SUFFIXES = {
    ".a", ".avi", ".bmp", ".cache", ".dll", ".dylib", ".exe", ".gif", ".ico",
    ".jpeg", ".jpg", ".log", ".mov", ".mp4", ".o", ".obj", ".otf", ".pdb",
    ".png", ".so", ".ttf", ".webm", ".woff", ".woff2",
}
EXCLUDED_GENERATED = {"SagaWiki/index.html"}


def selected_source_paths(paths: list[str]) -> list[str]:
    selected: list[str] = []
    for value in paths:
        path = pathlib.PurePosixPath(value)
        if not path.parts or path.parts[0] in EXCLUDED_ROOTS:
            continue
        if path.parts[0] not in INCLUDED_ROOTS:
            continue
        if value in EXCLUDED_GENERATED or "__pycache__" in path.parts:
            continue
        if path.suffix.lower() in EXCLUDED_SUFFIXES:
            continue
        selected.append(value)
    return sorted(set(selected))


def tracked_paths(root: pathlib.Path) -> list[str]:
    result = subprocess.run(
        ["git", "ls-files", "-z"], cwd=root, check=True, stdout=subprocess.PIPE)
    return [value.decode("utf-8") for value in result.stdout.split(b"\0") if value]


def generate(root: pathlib.Path, output: pathlib.Path, executable: str = "repomix") -> None:
    paths = selected_source_paths(tracked_paths(root))
    completed = subprocess.run(
        [
            executable,
            "--stdin",
            "--no-gitignore",
            "--no-default-patterns",
            "--style",
            "xml",
            "--quiet",
            "--output",
            str(output),
        ],
        cwd=root, input="\n".join(paths) + "\n", text=True, check=False)
    if completed.returncode != 0:
        raise RuntimeError(f"repomix failed with exit code {completed.returncode}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=pathlib.Path, default=pathlib.Path.cwd())
    parser.add_argument("--output", type=pathlib.Path, default=pathlib.Path("repomix-output.xml"))
    parser.add_argument("--repomix", default="repomix")
    args = parser.parse_args()
    root = args.repo_root.resolve()
    output = args.output if args.output.is_absolute() else root / args.output
    generate(root, output, args.repomix)
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
