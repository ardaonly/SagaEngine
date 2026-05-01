/// @file AST/SymbolVisitor.h
/// @brief RecursiveASTVisitor that drives DeclExtractor and RelationshipCollector.
///
/// Architecture decision — unified VisitNamedDecl:
///   Rather than 8+ individual Visit* overrides (one per DeclKind), we use a
///   single VisitNamedDecl entry point and let DeclExtractor handle
///   kind-specific logic internally.  This mirrors the pattern used by
///   clangd's SymbolCollector and avoids duplicating the ShouldIndex guard.
///
/// Template instantiation policy:
///   shouldVisitTemplateInstantiations() returns true so that implicit
///   template specialisations are indexed.  Explicit specialisations are
///   indexed via the normal NamedDecl path.

#pragma once

#include "../Record.h"
#include "DeclExtractor.h"
#include "RelationshipCollector.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/RecursiveASTVisitor.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace prism::ast {

/// Visits every named declaration in one translation unit and collects symbols.
class SymbolVisitor : public clang::RecursiveASTVisitor<SymbolVisitor>
{
public:
    explicit SymbolVisitor(clang::ASTContext& ctx,
                           const std::string& repo_root,
                           const std::string& tu_path);

    // ── RecursiveASTVisitor policy ────────────────────────────────────────────

    /// Index implicit template instantiations (e.g. vector<int>).
    bool shouldVisitTemplateInstantiations() const { return true; }

    /// Do not descend into lambda bodies — their internals add noise.
    bool shouldVisitLambdaBody() const { return false; }

    // ── Visitor callbacks ─────────────────────────────────────────────────────

    /// Unified entry point for all named declarations.
    bool VisitNamedDecl(clang::NamedDecl* decl);

    /// Collect #include directives into the FileRecord.
    bool VisitInclusionDirective(clang::InclusionDirective* directive);

    // ── Result accessors ──────────────────────────────────────────────────────

    const std::vector<SymbolRecord>& Symbols() const { return m_symbols; }
    const FileRecord&                File()    const { return m_file; }

private:
    // ── Helpers ───────────────────────────────────────────────────────────────

    /// Deduplicate and register *record*; update m_file.symbol_ids.
    void Register(SymbolRecord record, const clang::Decl* canonical);

    // ── Collaborators ─────────────────────────────────────────────────────────

    DeclExtractor          m_extractor;
    RelationshipCollector  m_relationships;

    // ── State ─────────────────────────────────────────────────────────────────

    std::string                m_repo_root;
    std::string                m_tu_path;

    std::vector<SymbolRecord>  m_symbols;   ///< Collected records in visitation order.
    FileRecord                 m_file;      ///< Metadata for this translation unit.

    /// Canonical-pointer set — prevents duplicate registration across redecls.
    std::unordered_map<const clang::Decl*, std::string> m_canonical_to_id;
};

} // namespace prism::ast
