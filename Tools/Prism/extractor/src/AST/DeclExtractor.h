/// @file AST/DeclExtractor.h
/// @brief Converts a single Clang declaration node into a SymbolRecord.
///
/// Design notes:
///   DeclExtractor is stateless beyond its ASTContext reference.
///   It is called from SymbolVisitor once per declaration that passes the
///   belonging-to-TU guard.  All Clang API calls are isolated here so
///   SymbolVisitor stays focused on traversal logic.

#pragma once

#include "../Record.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>

#include <string>

namespace prism::ast {

/// Extracts metadata from a Clang NamedDecl and produces a SymbolRecord.
///
/// The returned record has:
///   - A stable ID derived from the USR.
///   - Fully qualified name, kind, access, location, and extent.
///   - Brief from the documentation comment.
///   - Bases and deps populated by the RelationshipCollector.
///
/// The `children` field is always empty here; it is populated by
/// SymbolVisitor after all children have been visited.
class DeclExtractor
{
public:
    explicit DeclExtractor(clang::ASTContext& ctx,
                           const std::string& repo_root);

    /// Produce a SymbolRecord for *decl*.
    /// Returns false via the out-param when the declaration should be skipped
    /// (e.g. anonymous, implicit, outside the repo, in a system header).
    bool Extract(const clang::NamedDecl* decl,
                 SymbolRecord&           out_record) const;

private:
    // ── Metadata helpers ──────────────────────────────────────────────────────

    std::string GetUSR          (const clang::NamedDecl* decl) const;
    std::string GetQualifiedName(const clang::NamedDecl* decl) const;
    std::string GetKindLabel    (const clang::Decl*      decl) const;
    std::string GetAccessStr    (clang::AccessSpecifier  as)   const;
    std::string GetRawComment   (const clang::Decl*      decl) const;
    std::string GetSignature    (const clang::FunctionDecl* fd) const;

    SourceLocation MakeLocation(clang::SourceLocation loc) const;
    SourceExtent   MakeExtent  (clang::SourceRange    range) const;

    /// Returns false when the declaration should not be indexed.
    bool ShouldIndex(const clang::NamedDecl* decl) const;

    // ── State ─────────────────────────────────────────────────────────────────

    clang::ASTContext&          m_ctx;
    const clang::SourceManager& m_sm;
    std::string                 m_repo_root;
};

} // namespace prism::ast
