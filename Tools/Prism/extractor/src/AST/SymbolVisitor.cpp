/// @file AST/SymbolVisitor.cpp
/// @brief SymbolVisitor implementation — traversal, dispatch, and deduplication.

#include "SymbolVisitor.h"

#include "../Support/PathUtils.h"

#include <clang/AST/DeclCXX.h>
#include <clang/Lex/Preprocessor.h>

#include <filesystem>

namespace prism::ast {

// ─── Construction ─────────────────────────────────────────────────────────────

SymbolVisitor::SymbolVisitor(clang::ASTContext& ctx,
                             const std::string& repo_root,
                             const std::string& tu_path)
    : m_extractor(ctx, repo_root)
    , m_relationships(ctx)
    , m_repo_root(repo_root)
    , m_tu_path(tu_path)
{
    m_file.path = support::MakeRelativePosix(tu_path, repo_root);
}

// ─── VisitNamedDecl — unified dispatch ───────────────────────────────────────

bool SymbolVisitor::VisitNamedDecl(clang::NamedDecl* decl)
{
    SymbolRecord record;

    // DeclExtractor runs the ShouldIndex guard and fills all scalar fields.
    if (!m_extractor.Extract(decl, record))
        return true; // continue traversal even when skipped

    // RelationshipCollector appends to bases and deps.
    m_relationships.Collect(decl, record);

    // Deduplicate by canonical declaration pointer.
    Register(std::move(record), decl->getCanonicalDecl());

    return true; // always continue — we never abort the traversal
}

// ─── VisitInclusionDirective ─────────────────────────────────────────────────

bool SymbolVisitor::VisitInclusionDirective(clang::InclusionDirective* directive)
{
    if (!directive)
        return true;

    const clang::FileEntry* fe = directive->getFile();
    if (!fe)
        return true;

    const std::string abs(fe->getName());
    if (!support::IsSystemPath(abs))
        m_file.includes.push_back(support::MakeRelativePosix(abs, m_repo_root));

    return true;
}

// ─── Register ────────────────────────────────────────────────────────────────

void SymbolVisitor::Register(SymbolRecord        record,
                              const clang::Decl* canonical)
{
    const auto [it, inserted] =
        m_canonical_to_id.emplace(canonical, record.id);

    if (inserted)
    {
        // First time we see this canonical declaration — store it.
        m_file.symbol_ids.push_back(record.id);
        m_symbols.push_back(std::move(record));
    }
    else
    {
        // Redeclaration of an already-indexed symbol — merge relationships.
        const std::string& existing_id = it->second;

        for (auto& existing : m_symbols)
        {
            if (existing.id != existing_id)
                continue;

            // Prefer the definition site for documentation.
            if (record.is_definition && !existing.is_definition)
            {
                existing.is_definition = true;
                if (!record.brief.empty())
                    existing.brief = record.brief;
                if (!record.raw_comment.empty())
                    existing.raw_comment = record.raw_comment;
            }

            // Merge relationship lists.
            for (const auto& b : record.bases)
                existing.bases.push_back(b);
            for (const auto& d : record.deps)
                existing.deps.push_back(d);

            break;
        }
    }
}

} // namespace prism::ast
