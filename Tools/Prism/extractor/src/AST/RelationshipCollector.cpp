/// @file AST/RelationshipCollector.cpp
/// @brief Relationship extraction — bases and type-level dependency USRs.

#include "RelationshipCollector.h"

#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Type.h>
#include <clang/Index/USRGeneration.h>
#include <llvm/ADT/SmallString.h>

#include <algorithm>

namespace prism::ast {

RelationshipCollector::RelationshipCollector(clang::ASTContext& ctx)
    : m_ctx(ctx)
{}

// ─── Public API ───────────────────────────────────────────────────────────────

void RelationshipCollector::Collect(const clang::NamedDecl* decl,
                                     SymbolRecord&            out) const
{
    if (!decl)
        return;

    if (const auto* rd = clang::dyn_cast<clang::CXXRecordDecl>(decl))
        CollectBases(rd, out);

    CollectTypeDeps(decl, out);

    // Deduplicate both lists while preserving a stable order.
    auto Dedup = [](std::vector<std::string>& v)
    {
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
    };

    Dedup(out.bases);
    Dedup(out.deps);
}

// ─── Collectors ───────────────────────────────────────────────────────────────

void RelationshipCollector::CollectBases(const clang::CXXRecordDecl* rd,
                                          SymbolRecord&               out) const
{
    if (!rd->hasDefinition())
        return;

    for (const clang::CXXBaseSpecifier& base : rd->bases())
    {
        // Record base class as a human-readable type spelling.
        out.bases.push_back(base.getType().getAsString());

        // Also record as a dependency USR for graph resolution.
        AppendTypeUSR(base.getType(), out.deps);
    }
}

void RelationshipCollector::CollectTypeDeps(const clang::NamedDecl* decl,
                                             SymbolRecord&            out) const
{
    // ── Callable declarations ─────────────────────────────────────────────────
    if (const auto* fd = clang::dyn_cast<clang::FunctionDecl>(decl))
    {
        AppendTypeUSR(fd->getReturnType(), out.deps);

        for (const clang::ParmVarDecl* param : fd->parameters())
            AppendTypeUSR(param->getType(), out.deps);
    }

    // ── Variable / field declarations ─────────────────────────────────────────
    else if (const auto* vd = clang::dyn_cast<clang::ValueDecl>(decl))
    {
        AppendTypeUSR(vd->getType(), out.deps);
    }

    // ── Typedef / type alias ──────────────────────────────────────────────────
    else if (const auto* td = clang::dyn_cast<clang::TypedefNameDecl>(decl))
    {
        AppendTypeUSR(td->getUnderlyingType(), out.deps);
    }
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

void RelationshipCollector::AppendUSR(const clang::NamedDecl*   target,
                                       std::vector<std::string>& out) const
{
    if (!target)
        return;
    llvm::SmallString<256> buf;
    if (!clang::index::generateUSRForDecl(target, buf) && !buf.empty())
        out.push_back(buf.str().str());
}

void RelationshipCollector::AppendTypeUSR(clang::QualType          qt,
                                           std::vector<std::string>& out) const
{
    // Strip pointers, references, and cv-qualifiers to reach the underlying decl.
    const clang::Type* ty = qt.getTypePtrOrNull();
    if (!ty)
        return;

    ty = ty->getPointeeOrArrayElementType();
    if (!ty)
        return;

    // Resolve elaborated / typedef sugar.
    ty = ty->getUnqualifiedDesugaredType();
    if (!ty)
        return;

    if (const clang::CXXRecordDecl* rd = ty->getAsCXXRecordDecl())
        AppendUSR(rd, out);
    else if (const auto* td = ty->getAs<clang::TypedefType>())
        AppendUSR(td->getDecl(), out);
}

} // namespace prism::ast
