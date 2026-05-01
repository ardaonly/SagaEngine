/// @file Record.h
/// @brief Plain-data structures shared between the AST layer and the emit layer.
///
/// Design contract:
///   - No Clang headers are included here.
///   - All strings use std::string; no llvm::StringRef crosses this boundary.
///   - Records are value types: cheap to move, copyable when necessary.
///   - This header is the only file the JsonWriter needs to include from AST/.

#pragma once

#include <string>
#include <vector>

namespace prism {

// ─── Source Positions ─────────────────────────────────────────────────────────

/// Single point in a source file.
struct SourceLocation
{
    std::string file;    ///< Repository-relative POSIX path.
    unsigned    line   = 0;
    unsigned    column = 0;
};

/// Closed half-open source range covering a declaration's full text extent.
struct SourceExtent
{
    unsigned start_line   = 0;
    unsigned start_column = 0;
    unsigned end_line     = 0;
    unsigned end_column   = 0;
};

// ─── Symbol Record ────────────────────────────────────────────────────────────

/// All metadata extracted for one C++ declaration.
///
/// Invariants after construction by DeclExtractor:
///   - `id` is always 16 hex characters derived from USR or fallback.
///   - `kind` is the Clang DeclKind label string (CXXRecord, Function, …).
///   - `qualified_name` is the fully scope-qualified name (Ns::Class::Sym).
///   - `deps` contains USR strings at extraction time; the Python builder
///     resolves them to stable IDs during graph construction.
///   - All string fields are empty rather than null when data is unavailable.
struct SymbolRecord
{
    // ── Identity ──────────────────────────────────────────────────────────────
    std::string id;             ///< Stable 16-char hex ID (SHA-1 of USR).
    std::string usr;            ///< Clang Unified Symbol Resolution string.

    // ── Naming ────────────────────────────────────────────────────────────────
    std::string name;           ///< Simple unqualified spelling.
    std::string qualified_name; ///< Fully qualified name (Ns::Class::Symbol).
    std::string display_name;   ///< Full display name including template params.

    // ── Classification ────────────────────────────────────────────────────────
    std::string kind;           ///< Clang DeclKind label (CXXRecord, Function, …).
    std::string access;         ///< public | protected | private | none.
    bool        is_definition = false; ///< True when this cursor is the definition.

    // ── Documentation ─────────────────────────────────────────────────────────
    std::string brief;          ///< First sentence of the documentation comment.
    std::string raw_comment;    ///< Complete raw documentation comment block.

    // ── Type Information ──────────────────────────────────────────────────────
    std::string signature;      ///< Human-readable declaration signature.
    std::string result_type;    ///< Return type spelling for callables.
    std::string type_spelling;  ///< Full type as spelled by Clang (non-callables).

    // ── Position ──────────────────────────────────────────────────────────────
    SourceLocation location;    ///< Primary declaration site.
    SourceExtent   extent;      ///< Full declaration text extent.

    // ── Relationships ─────────────────────────────────────────────────────────
    std::vector<std::string> bases;    ///< Base class type spellings.
    std::vector<std::string> deps;     ///< Outbound dependency USRs.
    std::vector<std::string> children; ///< Direct child symbol IDs.
};

// ─── File Record ──────────────────────────────────────────────────────────────

/// Per-translation-unit metadata assembled after visiting the full AST.
///
/// One FileRecord is emitted per unique source file that was parsed as a main
/// TU. The Python builder deduplicates records from the same physical file
/// across multiple TU runs and merges their symbol_ids lists.
struct FileRecord
{
    std::string              path;       ///< Repository-relative POSIX path.
    std::string              brief;      ///< File-level @brief from header comment.
    std::vector<std::string> includes;   ///< Direct #include paths as spelled.
    std::vector<std::string> symbol_ids; ///< IDs of top-level symbols declared here.
};

// ─── Extraction Result ────────────────────────────────────────────────────────

/// Aggregated output of one prism-extract run over N translation units.
struct ExtractionResult
{
    std::vector<FileRecord>   files;   ///< One entry per parsed translation unit.
    std::vector<SymbolRecord> symbols; ///< All collected declarations (flat, may duplicate across TUs).
};

} // namespace prism