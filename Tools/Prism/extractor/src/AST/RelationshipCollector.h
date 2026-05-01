/// @file AST/RelationshipCollector.h
/// @brief Collects inter-symbol relationships for a single declaration.
///
/// Relationships are stored as USR strings at extraction time so they are
/// independent of visitation order.  The Python builder resolves USRs to
/// stable symbol IDs after all TUs have been parsed.
///
/// Currently collected:
///   - Base classes (CXXRecord only).
///   - Type-level dependencies: return type, parameter types, field types.
///   - Direct #include paths (delegated from the TU inclusion callback).

#pragma once

#include "../Record.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/DeclCXX.h>

#include <string>
#include <vector>

namespace prism::ast {

/// Stateless relationship collector — all methods are const.
class RelationshipCollector
{
public:
    explicit RelationshipCollector(clang::ASTContext& ctx);

    /// Populate *out_record*.bases and *out_record*.deps from *decl*.
    void Collect(const clang::NamedDecl* decl, SymbolRecord& out_record) const;

private:
    // ── Collectors ────────────────────────────────────────────────────────────

    void CollectBases(const clang::CXXRecordDecl* rd,
                      SymbolRecord&               out) const;

    void CollectTypeDeps(const clang::NamedDecl* decl,
                         SymbolRecord&            out) const;

    /// Append the USR for *target* to *out* when it is non-null and has a USR.
    void AppendUSR(const clang::NamedDecl* target,
                   std::vector<std::string>& out) const;

    /// Append the USR for the CXXRecordDecl (if any) behind *qt*.
    void AppendTypeUSR(clang::QualType qt, std::vector<std::string>& out) const;

    // ── State ─────────────────────────────────────────────────────────────────

    clang::ASTContext& m_ctx;
};

} // namespace prism::ast
