# @file schema.py
# @brief Data model for both the raw extractor output and the final graph.
#
# Two distinct schema families:
#
#   Raw* — mirror the prism-extract JSON exactly. No business logic.
#           Used only in graph/loader.py and graph/builder.py.
#
#   *Node / GraphData — the public AI-facing output. These types are the
#           contract that downstream consumers (AI tooling, editors, CI) depend on.
#           Fields are never removed; new fields are always added as Optional.

from __future__ import annotations

from dataclasses import dataclass, field
from typing      import Dict, List, Optional

SCHEMA_VERSION: str = "1.0"

# ─── Raw Extractor Types ──────────────────────────────────────────────────────

@dataclass(slots=True)
class RawLocation:
    """Source point as emitted by prism-extract."""
    file:   str
    line:   int
    column: int

@dataclass(slots=True)
class RawExtent:
    """Source range as emitted by prism-extract."""
    start_line:   int
    start_column: int
    end_line:     int
    end_column:   int

@dataclass(slots=True)
class RawSymbol:
    """One declaration record as emitted by prism-extract."""
    id:             str
    usr:            str
    name:           str
    qualified_name: str
    display_name:   str
    kind:           str
    access:         str
    is_definition:  bool
    brief:          str
    raw_comment:    str
    signature:      str
    result_type:    str
    type_spelling:  str
    location:       RawLocation
    extent:         RawExtent
    bases:          List[str]   # base class type spellings
    deps:           List[str]   # outbound USR strings (resolved to IDs by builder)
    children:       List[str]   # direct child symbol IDs

@dataclass(slots=True)
class RawFile:
    """One file record as emitted by prism-extract."""
    path:       str
    brief:      str
    includes:   List[str]
    symbol_ids: List[str]

@dataclass(slots=True)
class RawExtraction:
    """Complete deserialized prism-extract output."""
    schema_version: str
    generated_by:   str
    generated_at:   str
    repo_root:      str
    files:          List[RawFile]
    symbols:        List[RawSymbol]

# ─── Graph Node Types ─────────────────────────────────────────────────────────

@dataclass(slots=True)
class SymbolNode:
    """
    One vertex in the AI memory graph — one C++ declaration.

    After graph construction:
      - `deps` contains stable symbol IDs (not USR strings).
      - `children` contains stable symbol IDs of direct members.
      - `bases` contains human-readable type spellings.
    """

    # Identity
    id:             str
    name:           str
    qualified_name: str
    kind:           str         # CXXRecord | Function | CXXMethod | Enum | …
    access:         str         # public | protected | private | none
    is_definition:  bool

    # Documentation
    brief:          str

    # Type information
    signature:      str
    result_type:    str
    type_spelling:  str

    # Location
    file:           str
    line:           int
    column:         int

    # Relationships (resolved to IDs)
    bases:    List[str] = field(default_factory=list)
    deps:     List[str] = field(default_factory=list)
    children: List[str] = field(default_factory=list)


@dataclass(slots=True)
class FileNode:
    """
    One vertex representing a source file and its top-level declarations.
    `module` is a two-level logical grouping derived from the file path.
    """

    path:       str
    module:     str         # e.g. Engine/Renderer
    category:   str         # headers | sources | cmake | others
    line_count: int
    brief:      str
    includes:   List[str]   # repository-relative include paths
    symbol_ids: List[str]   # top-level symbol IDs declared in this file


@dataclass(slots=True)
class ModuleNode:
    """Aggregate view of one logical module for high-level graph navigation."""

    name:          str
    files:         List[str]   # sorted file paths
    symbol_count:  int
    include_count: int


@dataclass(slots=True)
class GraphData:
    """
    The complete AI memory graph produced by prism-graph.

    Keying strategy:
      - `symbols` keyed by stable 16-char hex ID.
      - `files`   keyed by repository-relative POSIX path.
      - `modules` keyed by module name (e.g. "Engine/Renderer").
    """

    schema_version: str
    generated_by:   str
    repo_root:      str
    symbols:        Dict[str, SymbolNode]
    files:          Dict[str, FileNode]
    modules:        Dict[str, ModuleNode]
    stats:          Dict[str, object]
