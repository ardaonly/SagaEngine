#!/usr/bin/env python3
"""
prism.py - Unified Prism CLI.

Provides a single entry point for the staged Prism tools:
  - prism extract ...  -> prism-extract ...
  - prism graph ...    -> prism-graph ...
  - prism run ...      -> prism-extract followed by prism-graph
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


HERE = Path(__file__).resolve().parent
BIN_DIR = HERE / "bin"

EXTRACTOR = BIN_DIR / "prism-extract"
GRAPH = BIN_DIR / "prism-graph"


def _tool_path(path: Path, name: str) -> str:
    if path.exists():
        return str(path)
    raise FileNotFoundError(
        f"{name} was not found at {path}. Run Tools/Prism/build.py first."
    )


def _run(cmd: list[str]) -> int:
    try:
        return subprocess.call(cmd)
    except KeyboardInterrupt:
        return 130


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="prism",
        description="Unified CLI for Prism extraction and graph generation.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        allow_abbrev=False,
    )

    subcommands = parser.add_subparsers(dest="command")

    extract = subcommands.add_parser(
        "extract",
        help="Pass arguments directly to prism-extract.",
        allow_abbrev=False,
        add_help=False,
    )
    extract.add_argument("args", nargs=argparse.REMAINDER)

    graph = subcommands.add_parser(
        "graph",
        help="Pass arguments directly to prism-graph.",
        allow_abbrev=False,
        add_help=False,
    )
    graph.add_argument("args", nargs=argparse.REMAINDER)

    run = subcommands.add_parser(
        "run",
        help="Run prism-extract, then prism-graph.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
        allow_abbrev=False,
    )
    run.add_argument("--repo-root", default=".", metavar="PATH",
                     help="Repository root passed to prism-extract.")
    run.add_argument("-p", "--build-dir", default="build", metavar="DIR",
                     help="Build directory containing compile_commands.json.")
    run.add_argument("-o", "--raw-json", default="prism.raw.json", metavar="FILE",
                     help="Raw extraction JSON written by prism-extract.")
    run.add_argument("--out-dir", default=None, metavar="DIR",
                     help="Output directory passed to prism-graph.")
    run.add_argument("--json-name", default=None, metavar="NAME",
                     help="Graph JSON filename passed to prism-graph.")
    run.add_argument("--txt-name", default=None, metavar="NAME",
                     help="Text summary filename passed to prism-graph.")
    run.add_argument("--no-txt", action="store_true",
                     help="Skip prism.txt generation.")
    verbosity = run.add_mutually_exclusive_group()
    verbosity.add_argument("-v", "--verbose", action="store_true",
                           help="Enable verbose prism-graph logging.")
    verbosity.add_argument("-q", "--quiet", action="store_true",
                           help="Suppress prism-graph informational output.")
    run.add_argument("--extra-arg", action="append", default=[], metavar="ARG",
                     help="Append one compiler argument for prism-extract.")
    run.add_argument("--extra-arg-before", action="append", default=[], metavar="ARG",
                     help="Prepend one compiler argument for prism-extract.")
    run.add_argument("sources", nargs="*",
                     help="Optional source files to pass to prism-extract.")

    return parser


def _run_extract(tool_args: list[str]) -> int:
    try:
        extractor = _tool_path(EXTRACTOR, "prism-extract")
    except FileNotFoundError as exc:
        sys.stderr.write(f"prism: {exc}\n")
        return 1

    return _run([extractor, *tool_args])


def _run_graph(tool_args: list[str]) -> int:
    try:
        graph = _tool_path(GRAPH, "prism-graph")
    except FileNotFoundError as exc:
        sys.stderr.write(f"prism: {exc}\n")
        return 1

    return _run([graph, *tool_args])


def _run_pipeline(args: argparse.Namespace) -> int:
    try:
        extractor = _tool_path(EXTRACTOR, "prism-extract")
        graph = _tool_path(GRAPH, "prism-graph")
    except FileNotFoundError as exc:
        sys.stderr.write(f"prism: {exc}\n")
        return 1

    raw_json = str(Path(args.raw_json))
    extract_cmd = [
        extractor,
        "--repo-root", args.repo_root,
        "-p", args.build_dir,
        "-o", raw_json,
    ]

    sources = list(args.sources)
    if not sources:
        sources = _sources_from_compile_database(Path(args.build_dir))

    for value in args.extra_arg:
        extract_cmd.extend(["--extra-arg", value])
    for value in args.extra_arg_before:
        extract_cmd.extend(["--extra-arg-before", value])
    extract_cmd.extend(sources)

    rc = _run(extract_cmd)
    if rc != 0:
        return rc

    graph_cmd = [graph, raw_json]
    if args.out_dir:
        graph_cmd.extend(["--out-dir", args.out_dir])
    if args.json_name:
        graph_cmd.extend(["--json-name", args.json_name])
    if args.txt_name:
        graph_cmd.extend(["--txt-name", args.txt_name])
    if args.no_txt:
        graph_cmd.append("--no-txt")
    if args.verbose:
        graph_cmd.append("--verbose")
    if args.quiet:
        graph_cmd.append("--quiet")

    return _run(graph_cmd)


def _sources_from_compile_database(build_dir: Path) -> list[str]:
    compile_db = build_dir / "compile_commands.json"
    if not compile_db.is_file():
        return []

    try:
        entries = json.loads(compile_db.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return []

    if not isinstance(entries, list):
        return []

    sources: list[str] = []
    seen: set[str] = set()

    for entry in entries:
        if not isinstance(entry, dict):
            continue

        file_value = entry.get("file")
        if not isinstance(file_value, str) or not file_value:
            continue

        if file_value not in seen:
            seen.add(file_value)
            sources.append(file_value)

    return sources


def main(argv: list[str] | None = None) -> int:
    raw_args = list(sys.argv[1:] if argv is None else argv)
    parser = _build_parser()

    if not raw_args or raw_args[0] in ("-h", "--help"):
        parser.print_help()
        return 0

    if raw_args[0] == "extract":
        return _run_extract(raw_args[1:])
    if raw_args[0] == "graph":
        return _run_graph(raw_args[1:])

    args = parser.parse_args(raw_args)

    if args.command == "extract":
        return _run_extract(args.args)
    if args.command == "graph":
        return _run_graph(args.args)
    if args.command == "run":
        return _run_pipeline(args)

    parser.print_help()
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
