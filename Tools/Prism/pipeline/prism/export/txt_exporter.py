# @file export/txt_exporter.py
# @brief Human-readable plain-text graph renderer.
#
# Layout conventions:
#   - Double-rule (═) separators for top-level sections.
#   - Single-rule (─) dividers for per-file symbol groups.
#   - Fixed 80-column width throughout.
#   - All text is ASCII-safe except for the box-drawing characters.

from __future__ import annotations

from io      import StringIO
from pathlib import Path
from typing  import List

from ..errors import ExportError
from ..schema import GraphData, SymbolNode
from .base    import Exporter

# ─── Layout ───────────────────────────────────────────────────────────────────

_W = 80  # document width in columns


def _double_rule() -> str:
    return "═" * _W


def _single_rule(label: str = "") -> str:
    if label:
        prefix = f"  ── {label} "
        return prefix + "─" * max(2, _W - len(prefix))
    return "  " + "─" * (_W - 2)


def _section(title: str) -> str:
    return f"\n{_double_rule()}\n  {title}\n{_double_rule()}\n"


def _kv(key: str, value: object, indent: int = 4) -> str:
    return " " * indent + f"{key:<18}{value}"


def _bullet(text: str, indent: int = 6) -> str:
    return " " * indent + f"· {text}"


def _trunc(text: str, width: int = 56) -> str:
    return text if len(text) <= width else text[: width - 1] + "…"


# ─── Section Renderers ────────────────────────────────────────────────────────

def _render_header(buf: StringIO, graph: GraphData) -> None:
    buf.write(_double_rule() + "\n")
    buf.write("  PRISM  —  AI Memory Graph\n")
    buf.write(_double_rule() + "\n\n")
    buf.write(_kv("repo_root",    graph.repo_root,      indent=2) + "\n")
    buf.write(_kv("generated_by", graph.generated_by,   indent=2) + "\n")
    buf.write(_kv("schema",       graph.schema_version, indent=2) + "\n")


def _render_stats(buf: StringIO, graph: GraphData) -> None:
    buf.write(_section("STATISTICS"))
    for key, value in graph.stats.items():
        if isinstance(value, dict):
            buf.write(_kv(key, "", indent=2) + "\n")
            for k, v in sorted(value.items()):
                buf.write(_kv(k, v, indent=6) + "\n")
        else:
            buf.write(_kv(key, value, indent=2) + "\n")


def _render_modules(buf: StringIO, graph: GraphData) -> None:
    buf.write(_section("MODULE INDEX"))
    for name, mod in sorted(graph.modules.items(), key=lambda kv: kv[0].lower()):
        buf.write(f"\n  [{name}]\n")
        buf.write(_kv("files",    len(mod.files),    indent=4) + "\n")
        buf.write(_kv("symbols",  mod.symbol_count,  indent=4) + "\n")
        buf.write(_kv("includes", mod.include_count, indent=4) + "\n")
        for f in mod.files:
            buf.write(_bullet(f, indent=6) + "\n")


def _render_files(buf: StringIO, graph: GraphData) -> None:
    buf.write(_section("FILE INDEX"))
    for path, fn in sorted(graph.files.items(), key=lambda kv: kv[0].lower()):
        buf.write(f"\n  [{fn.category.upper():7}]  {path}\n")
        buf.write(_kv("module",   fn.module,     indent=4) + "\n")
        buf.write(_kv("lines",    fn.line_count, indent=4) + "\n")

        if fn.brief:
            buf.write(_kv("brief", _trunc(fn.brief), indent=4) + "\n")

        if fn.includes:
            buf.write(_kv("includes", f"({len(fn.includes)})", indent=4) + "\n")
            for inc in fn.includes:
                buf.write(_bullet(inc, indent=6) + "\n")

        if fn.symbol_ids:
            buf.write(_kv("symbols", f"({len(fn.symbol_ids)})", indent=4) + "\n")
            for sid in fn.symbol_ids:
                sym = graph.symbols.get(sid)
                if not sym:
                    continue
                label  = sym.signature or sym.qualified_name or sym.name
                suffix = f"   — {_trunc(sym.brief, 34)}" if sym.brief else ""
                buf.write(_bullet(f"[{sym.kind}] {_trunc(label, 44)}{suffix}", indent=6) + "\n")


def _render_symbols(buf: StringIO, graph: GraphData) -> None:
    buf.write(_section("SYMBOL INDEX"))

    ordered: List[SymbolNode] = sorted(
        graph.symbols.values(),
        key=lambda s: (s.file.lower(), s.line, s.column),
    )

    current_file = ""
    for sym in ordered:
        if sym.file != current_file:
            current_file = sym.file
            buf.write(f"\n{_single_rule(current_file)}\n")

        buf.write(f"\n    [{sym.kind}]  {sym.qualified_name or sym.name}\n")
        buf.write(_kv("id",     sym.id,                                  indent=6) + "\n")
        buf.write(_kv("loc",    f"{sym.file}:{sym.line}:{sym.column}",  indent=6) + "\n")

        if sym.signature:
            buf.write(_kv("signature",  _trunc(sym.signature),  indent=6) + "\n")
        if sym.brief:
            buf.write(_kv("brief",      _trunc(sym.brief),      indent=6) + "\n")
        if sym.access:
            buf.write(_kv("access",     sym.access,             indent=6) + "\n")
        if sym.is_definition:
            buf.write(_kv("definition", "true",                 indent=6) + "\n")

        if sym.bases:
            buf.write(_kv("bases", f"({len(sym.bases)})", indent=6) + "\n")
            for b in sym.bases:
                buf.write(_bullet(b, indent=8) + "\n")

        if sym.deps:
            buf.write(_kv("deps", f"({len(sym.deps)})", indent=6) + "\n")
            shown = sym.deps[:10]
            for d in shown:
                dep   = graph.symbols.get(d)
                label = dep.qualified_name if dep else d
                buf.write(_bullet(_trunc(label, 62), indent=8) + "\n")
            if len(sym.deps) > 10:
                buf.write(_bullet(f"… and {len(sym.deps) - 10} more", indent=8) + "\n")


# ─── Exporter ─────────────────────────────────────────────────────────────────

class TxtExporter(Exporter):
    """Render the AI memory graph as a human-readable UTF-8 plain-text document."""

    extension = "txt"

    def write(self, graph: GraphData, out_path: Path) -> None:
        buf = StringIO()

        _render_header (buf, graph)
        _render_stats  (buf, graph)
        _render_modules(buf, graph)
        _render_files  (buf, graph)
        _render_symbols(buf, graph)

        buf.write("\n")

        try:
            out_path.parent.mkdir(parents=True, exist_ok=True)
            with out_path.open("w", encoding="utf-8") as fh:
                fh.write(buf.getvalue())
        except OSError as exc:
            raise ExportError(out_path, str(exc)) from exc
