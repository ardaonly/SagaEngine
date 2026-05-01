/// @file PrismConsumer.cpp
/// @brief ASTConsumer implementation.

#include "PrismConsumer.h"

namespace prism {

PrismConsumer::PrismConsumer(const std::string& repo_root,
                             const std::string& tu_file,
                             TUCallback         callback)
    : m_repo_root(repo_root)
    , m_tu_file(tu_file)
    , m_callback(std::move(callback))
{}

void PrismConsumer::HandleTranslationUnit(clang::ASTContext& ctx)
{
    SymbolVisitor visitor(ctx, m_repo_root, m_tu_file);
    visitor.TraverseDecl(ctx.getTranslationUnitDecl());

    m_callback(visitor.File(), visitor.Symbols());
}

} // namespace prism
