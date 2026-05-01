# @file graph/builder.py
# @brief Build the AI memory graph from a RawExtraction.
#
# The builder runs five deterministic phases:
#
#   Phase 1 — Deduplication
#       Multiple TUs may emit the same canonical declaration.
#       Symbols are merged by stable ID; definition sites win for documentation.
#
#   Phase 2 — USR Index
#       A mapping from USR string → stable ID is built from all emitted symbols.
#       This index is the join key between deps (stored as USRs by the extractor)
#       and SymbolNodes (keyed by stable ID in the graph).
#
#   Phase 3 — Dependency Resolution
#       USR strings in SymbolNode.deps are replaced with resolved stable IDs.
#       USRs with no match (system types, external libraries) are dropped.
#
#   Phase 4 — File & Module Aggregation
#       FileNodes are built from RawFiles; line counts are read from disk.
#       ModuleNodes aggregate per-file counts.
#
#   Phase 5 — Statistics
#       Numeric counters for the `stats` block.

from __future__ import annotations

from pathlib import Path
from typing  import Dict, List, Set

from ..log    import PrismLogger
from ..schema import (
    FileNode, GraphData, ModuleNode,
    RawExtraction, RawSymbol, SCHEMA_VERSION, SymbolNode,
)
from .classifier import categorize, module_from_path

_TOOL_TAG: str = "prism-graph 1.0.0"


# ─── Phase 1 — Symbol Deduplication ──────────────────────────────────────────

def _to_node(raw: RawSymbol) -> SymbolNode:
    """Convert a RawSymbol to a SymbolNode (deps still hold USR strings)."""
    return SymbolNode(
        id             = raw.id,
        name           = raw.name,
        qualified_name = raw.qualified_name,
        kind           = raw.kind,
        access         = raw.access,
        is_definition  = raw.is_definition,
        brief          = raw.brief,
        signature      = raw.signature,
        result_type    = raw.result_type,
        type_spelling  = raw.type_spelling,
        file           = raw.location.file,
        line           = raw.location.line,
        column         = raw.location.column,
        bases          = list(raw.bases),
        deps           = list(raw.deps),
        children       = list(raw.children),
    )


def _merge(existing: SymbolNode, incoming: RawSymbol) -> None:
    """
    Merge *incoming* into an already-registered *existing* node.
    The definition site wins for documentation fields.
    Relationship lists are union-merged.
    """
    if incoming.is_definition and not existing.is_definition:
        existing.is_definition = True
        if incoming.brief:
            existing.brief = incoming.brief
        if incoming.raw_comment:
            pass  # raw_comment not stored in SymbolNode to keep it lean

    existing.bases    = _union_sorted(existing.bases,    incoming.bases)
    existing.deps     = _union_sorted(existing.deps,     incoming.deps)
    existing.children = _union_sorted(existing.children, incoming.children)


def _union_sorted(a: List[str], b: List[str]) -> List[str]:
    return sorted(set(a) | set(b), key=str.lower)


def _deduplicate(raw_symbols: List[RawSymbol],
                 log: PrismLogger) -> Dict[str, SymbolNode]:
    """Return a stable-ID → SymbolNode map with all redeclarations merged."""
    nodes: Dict[str, SymbolNode] = {}
    log.progress_start(len(raw_symbols), "Deduplicating")

    for raw in raw_symbols:
        log.progress_step(raw.qualified_name or raw.name)
        if not raw.id:
            continue
        existing = nodes.get(raw.id)
        if existing is None:
            nodes[raw.id] = _to_node(raw)
        else:
            _merge(existing, raw)

    log.progress_done()
    return nodes


# ─── Phase 2 — USR Index ──────────────────────────────────────────────────────

def _build_usr_index(nodes: Dict[str, SymbolNode],
                     raw_symbols: List[RawSymbol]) -> Dict[str, str]:
    """Return USR → stable-ID for all symbols present in *nodes*."""
    index: Dict[str, str] = {}
    for raw in raw_symbols:
        if raw.usr and raw.id in nodes:
            index[raw.usr] = raw.id
    return index


# ─── Phase 3 — Dependency Resolution ─────────────────────────────────────────

def _resolve_deps(nodes: Dict[str, SymbolNode],
                  usr_index: Dict[str, str]) -> None:
    """
    Replace USR strings in every SymbolNode.deps with resolved stable IDs.
    USRs not present in the index belong to system types or external libs
    and are silently dropped — they are outside the repository graph.
    """
    for sym in nodes.values():
        resolved: Set[str] = set()
        for entry in sym.deps:
            target_id = usr_index.get(entry)
            if target_id and target_id != sym.id:  # skip self-references
                resolved.add(target_id)
        sym.deps = sorted(resolved, key=str.lower)


# ─── Phase 4 — File & Module Aggregation ─────────────────────────────────────

def _line_count(repo_root: str, rel_path: str) -> int:
    try:
        p = Path(repo_root) / rel_path
        with p.open("r", encoding="utf-8", errors="ignore") as fh:
            return sum(1 for _ in fh)
    except OSError:
        return 0


def _build_file_nodes(raw: RawExtraction) -> Dict[str, FileNode]:
    """
    Deduplicate FileRecords by path and merge symbol/include lists.
    Line counts are read from disk using repo_root from the raw extraction.
    """
    nodes: Dict[str, FileNode] = {}

    for rf in raw.files:
        if not rf.path:
            continue

        existing = nodes.get(rf.path)
        if existing is None:
            nodes[rf.path] = FileNode(
                path       = rf.path,
                module     = module_from_path(rf.path),
                category   = categorize(rf.path),
                line_count = _line_count(raw.repo_root, rf.path),
                brief      = rf.brief,
                includes   = sorted(set(rf.includes), key=str.lower),
                symbol_ids = list(rf.symbol_ids),
            )
        else:
            existing.symbol_ids = _union_sorted(existing.symbol_ids, rf.symbol_ids)
            existing.includes   = _union_sorted(existing.includes,   rf.includes)
            if not existing.brief and rf.brief:
                existing.brief = rf.brief

    return nodes


def _build_module_nodes(file_nodes: Dict[str, FileNode]) -> Dict[str, ModuleNode]:
    """Aggregate FileNodes by module name into ModuleNodes."""
    buckets:  Dict[str, List[FileNode]] = {}
    for fn in file_nodes.values():
        buckets.setdefault(fn.module, []).append(fn)

    modules: Dict[str, ModuleNode] = {}
    for mod, files in sorted(buckets.items(), key=lambda kv: kv[0].lower()):
        all_sym_ids: Set[str] = set()
        all_includes: Set[str] = set()
        for fn in files:
            all_sym_ids.update(fn.symbol_ids)
            all_includes.update(fn.includes)

        modules[mod] = ModuleNode(
            name          = mod,
            files         = sorted((fn.path for fn in files), key=str.lower),
            symbol_count  = len(all_sym_ids),
            include_count = len(all_includes),
        )
    return modules


# ─── Phase 5 — Statistics ─────────────────────────────────────────────────────

def _build_stats(
    symbols: Dict[str, SymbolNode],
    files:   Dict[str, FileNode],
) -> Dict[str, object]:
    by_kind:     Dict[str, int] = {}
    by_category: Dict[str, int] = {}

    for sym in symbols.values():
        by_kind[sym.kind] = by_kind.get(sym.kind, 0) + 1

    for fn in files.values():
        by_category[fn.category] = by_category.get(fn.category, 0) + 1

    return {
        "total_symbols":  len(symbols),
        "total_files":    len(files),
        "total_lines":    sum(fn.line_count for fn in files.values()),
        "by_kind":        dict(sorted(by_kind.items())),
        "by_category":    dict(sorted(by_category.items())),
    }


# ─── Public Entry Point ───────────────────────────────────────────────────────

def build_graph(raw: RawExtraction, log: PrismLogger) -> GraphData:
    """
    Execute all five graph-construction phases and return a GraphData.
    This function is the single public API of this module.
    """
    log.info(f"Building graph from {len(raw.symbols)} raw symbols across {len(raw.files)} files")

    log.info("Phase 1/5 — Deduplication")
    symbols = _deduplicate(raw.symbols, log)
    log.info(f"  → {len(symbols)} unique symbols")

    log.info("Phase 2/5 — USR index")
    usr_index = _build_usr_index(symbols, raw.symbols)
    log.info(f"  → {len(usr_index)} USR entries mapped")

    log.info("Phase 3/5 — Dependency resolution")
    _resolve_deps(symbols, usr_index)

    log.info("Phase 4/5 — File and module aggregation")
    file_nodes   = _build_file_nodes(raw)
    module_nodes = _build_module_nodes(file_nodes)
    log.info(f"  → {len(file_nodes)} files, {len(module_nodes)} modules")

    log.info("Phase 5/5 — Statistics")
    stats = _build_stats(symbols, file_nodes)

    return GraphData(
        schema_version = SCHEMA_VERSION,
        generated_by   = _TOOL_TAG,
        repo_root      = raw.repo_root,
        symbols        = symbols,
        files          = file_nodes,
        modules        = module_nodes,
        stats          = stats,
    )
