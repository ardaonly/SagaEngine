# @file export/json_exporter.py
# @brief Serialize a GraphData to machine-readable JSON.
#
# The JSON schema here is the public contract consumed by AI tooling.
# Field names must not change without bumping schema_version.

from __future__ import annotations

import json
from pathlib import Path
from typing  import Any, Dict

from ..errors import ExportError
from ..schema import FileNode, GraphData, ModuleNode, SymbolNode
from .base    import Exporter


# ─── Record Serializers ───────────────────────────────────────────────────────

def _sym_dict(s: SymbolNode) -> Dict[str, Any]:
    return {
        "id":             s.id,
        "name":           s.name,
        "qualified_name": s.qualified_name,
        "kind":           s.kind,
        "access":         s.access,
        "is_definition":  s.is_definition,
        "brief":          s.brief,
        "signature":      s.signature,
        "result_type":    s.result_type,
        "type_spelling":  s.type_spelling,
        "file":           s.file,
        "line":           s.line,
        "column":         s.column,
        "bases":          s.bases,
        "deps":           s.deps,
        "children":       s.children,
    }


def _file_dict(f: FileNode) -> Dict[str, Any]:
    return {
        "path":       f.path,
        "module":     f.module,
        "category":   f.category,
        "line_count": f.line_count,
        "brief":      f.brief,
        "includes":   f.includes,
        "symbol_ids": f.symbol_ids,
    }


def _module_dict(m: ModuleNode) -> Dict[str, Any]:
    return {
        "name":          m.name,
        "files":         m.files,
        "symbol_count":  m.symbol_count,
        "include_count": m.include_count,
    }


# ─── Graph Serializer ─────────────────────────────────────────────────────────

def graph_to_dict(graph: GraphData) -> Dict[str, Any]:
    """Convert *graph* to a plain dict suitable for json.dump."""
    return {
        "schema_version": graph.schema_version,
        "generated_by":   graph.generated_by,
        "repo_root":      graph.repo_root,
        "stats":          graph.stats,
        "modules": {
            k: _module_dict(v)
            for k, v in sorted(graph.modules.items(), key=lambda kv: kv[0].lower())
        },
        "files": {
            k: _file_dict(v)
            for k, v in sorted(graph.files.items(), key=lambda kv: kv[0].lower())
        },
        "symbols": {
            k: _sym_dict(v)
            for k, v in sorted(
                graph.symbols.items(),
                key=lambda kv: (kv[1].file.lower(), kv[1].line),
            )
        },
    }


class JsonExporter(Exporter):
    """Serialize the graph as indented, deterministically-ordered UTF-8 JSON."""

    extension = "json"

    def write(self, graph: GraphData, out_path: Path) -> None:
        payload = graph_to_dict(graph)
        try:
            out_path.parent.mkdir(parents=True, exist_ok=True)
            with out_path.open("w", encoding="utf-8") as fh:
                json.dump(payload, fh, ensure_ascii=False, indent=2)
                fh.write("\n")
        except OSError as exc:
            raise ExportError(out_path, str(exc)) from exc
