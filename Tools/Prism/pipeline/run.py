# @file run.py
# @brief prism-graph CLI — reads prism.raw.json, builds the graph, writes outputs.

from __future__ import annotations

import sys
from pathlib import Path
from typing  import List, Optional


def _build_parser() -> "argparse.ArgumentParser":
    import argparse
    from prism.config import DEFAULT_OUT_JSON, DEFAULT_OUT_TXT, DEFAULT_RAW_JSON

    p = argparse.ArgumentParser(
        prog        = "prism-graph",
        description = (
            "prism-graph — build the AI memory graph from prism-extract output.\n\n"
            "Reads the raw extraction JSON produced by prism-extract,\n"
            "resolves cross-file dependencies, and writes the final graph."
        ),
        formatter_class = argparse.RawDescriptionHelpFormatter,
        allow_abbrev    = False,
    )
    p.add_argument(
        "raw_json",
        nargs   = "?",
        type    = Path,
        default = None,
        metavar = "RAW_JSON",
        help    = f"Path to prism-extract output (default: {DEFAULT_RAW_JSON}).",
    )
    p.add_argument(
        "--out-dir",
        type    = Path,
        default = None,
        metavar = "DIR",
        help    = "Output directory (default: same directory as RAW_JSON).",
    )
    p.add_argument(
        "--json-name",
        type    = str,
        default = DEFAULT_OUT_JSON,
        metavar = "NAME",
        help    = f"Graph JSON filename (default: {DEFAULT_OUT_JSON}).",
    )
    p.add_argument(
        "--txt-name",
        type    = str,
        default = DEFAULT_OUT_TXT,
        metavar = "NAME",
        help    = f"Plain-text filename (default: {DEFAULT_OUT_TXT}).",
    )
    p.add_argument(
        "--no-txt",
        action  = "store_true",
        default = False,
        help    = "Skip the plain-text output.",
    )
    verbosity = p.add_mutually_exclusive_group()
    verbosity.add_argument("-v", "--verbose", action="store_true", default=False)
    verbosity.add_argument("-q", "--quiet",   action="store_true", default=False)
    return p


def main(argv: Optional[List[str]] = None) -> int:
    from prism.config  import PipelineConfig
    from prism.errors  import ConfigError, ExportError, PrismError, RawJsonNotFoundError, RawJsonSchemaError
    from prism.export  import JsonExporter, TxtExporter
    from prism.graph   import build_graph, load_raw
    from prism.log     import Level, init as init_log

    parser = _build_parser()
    args   = parser.parse_args(argv)

    level = Level.SILENT if args.quiet else (Level.DEBUG if args.verbose else Level.INFO)
    log   = init_log(level)

    try:
        cfg = PipelineConfig.from_args(
            raw_json  = args.raw_json,
            out_dir   = args.out_dir,
            json_name = args.json_name,
            txt_name  = args.txt_name,
            emit_txt  = not args.no_txt,
            verbose   = args.verbose,
            quiet     = args.quiet,
        )
    except ConfigError as exc:
        log.error(str(exc))
        return 1

    log.info(f"Input:  {cfg.raw_json}")
    log.info(f"Output: {cfg.out_dir}")

    try:
        raw   = load_raw(cfg.raw_json)
        graph = build_graph(raw, log)
    except (RawJsonNotFoundError, RawJsonSchemaError) as exc:
        log.error(str(exc))
        return 1
    except PrismError as exc:
        log.error(f"Graph build failed: {exc}")
        return 1

    try:
        JsonExporter().write(graph, cfg.graph_json_path)
        log.result(f"[OK]  JSON  →  {cfg.graph_json_path}")

        if cfg.emit_txt:
            TxtExporter().write(graph, cfg.txt_path)
            log.result(f"[OK]  TXT   →  {cfg.txt_path}")

    except ExportError as exc:
        log.error(str(exc))
        return 1

    s = graph.stats
    log.result(
        f"[OK]  Done  —  "
        f"{s['total_symbols']} symbols  "
        f"{s['total_files']} files  "
        f"{s['total_lines']} lines"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
