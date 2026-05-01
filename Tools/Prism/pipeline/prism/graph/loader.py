# @file graph/loader.py
# @brief Deserialize the raw prism-extract JSON into typed RawExtraction objects.
#
# Design contract:
#   - All parsing is defensive: missing or wrong-typed fields fall back to
#     sensible empty values rather than raising KeyError / TypeError.
#   - Schema version is checked but not enforced — forward compatibility is
#     preferred over hard breakage.
#   - Only this module knows the raw JSON field names.

from __future__ import annotations

import json
from pathlib import Path
from typing  import Any, Dict

from ..errors import RawJsonNotFoundError, RawJsonSchemaError
from ..schema import (
    RawExtent, RawExtraction, RawFile, RawLocation, RawSymbol,
)


# ─── Primitive Decoders ───────────────────────────────────────────────────────

def _s(d: Dict, key: str, default: str = "") -> str:
    v = d.get(key, default)
    return v if isinstance(v, str) else default

def _b(d: Dict, key: str, default: bool = False) -> bool:
    return bool(d.get(key, default))

def _i(d: Dict, key: str, default: int = 0) -> int:
    v = d.get(key, default)
    return int(v) if isinstance(v, (int, float)) else default

def _sl(d: Dict, key: str) -> list[str]:
    v = d.get(key, [])
    return [str(x) for x in v] if isinstance(v, list) else []

def _decode_location(raw: Any) -> RawLocation:
    if not isinstance(raw, dict):
        return RawLocation(file="", line=0, column=0)
    return RawLocation(file=_s(raw, "file"), line=_i(raw, "line"), column=_i(raw, "column"))

def _decode_extent(raw: Any) -> RawExtent:
    if not isinstance(raw, dict):
        return RawExtent(0, 0, 0, 0)
    return RawExtent(
        start_line   = _i(raw, "start_line"),
        start_column = _i(raw, "start_column"),
        end_line     = _i(raw, "end_line"),
        end_column   = _i(raw, "end_column"),
    )

def _decode_symbol(raw: Dict) -> RawSymbol:
    return RawSymbol(
        id             = _s(raw, "id"),
        usr            = _s(raw, "usr"),
        name           = _s(raw, "name"),
        qualified_name = _s(raw, "qualified_name"),
        display_name   = _s(raw, "display_name"),
        kind           = _s(raw, "kind"),
        access         = _s(raw, "access"),
        is_definition  = _b(raw, "is_definition"),
        brief          = _s(raw, "brief"),
        raw_comment    = _s(raw, "raw_comment"),
        signature      = _s(raw, "signature"),
        result_type    = _s(raw, "result_type"),
        type_spelling  = _s(raw, "type_spelling"),
        location       = _decode_location(raw.get("location")),
        extent         = _decode_extent(raw.get("extent")),
        bases          = _sl(raw, "bases"),
        deps           = _sl(raw, "deps"),
        children       = _sl(raw, "children"),
    )

def _decode_file(raw: Dict) -> RawFile:
    return RawFile(
        path       = _s(raw, "path"),
        brief      = _s(raw, "brief"),
        includes   = _sl(raw, "includes"),
        symbol_ids = _sl(raw, "symbol_ids"),
    )


# ─── Public API ───────────────────────────────────────────────────────────────

def load_raw(path: Path) -> RawExtraction:
    """
    Read and deserialize the prism-extract JSON at *path*.

    Raises:
        RawJsonNotFoundError  — when the file does not exist.
        RawJsonSchemaError    — when the file cannot be parsed or root is not an object.
    """
    if not path.is_file():
        raise RawJsonNotFoundError(path)

    try:
        with path.open("r", encoding="utf-8") as fh:
            data = json.load(fh)
    except json.JSONDecodeError as exc:
        raise RawJsonSchemaError(f"JSON parse error at {path}: {exc}") from exc

    if not isinstance(data, dict):
        raise RawJsonSchemaError(f"Root of '{path}' is not a JSON object")

    try:
        symbols = [_decode_symbol(s) for s in (data.get("symbols") or [])]
        files   = [_decode_file(f)   for f in (data.get("files")   or [])]
    except Exception as exc:
        raise RawJsonSchemaError(str(exc)) from exc

    return RawExtraction(
        schema_version = _s(data, "schema_version"),
        generated_by   = _s(data, "generated_by"),
        generated_at   = _s(data, "generated_at"),
        repo_root      = _s(data, "repo_root"),
        files          = files,
        symbols        = symbols,
    )
