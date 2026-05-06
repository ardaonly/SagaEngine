/// @file PrismConsumer.cpp
/// @brief ASTConsumer implementation.

#include "PrismConsumer.h"

namespace prism {

PrismConsumer::PrismConsumer(const std::string&                          repo_root,
                             const std::string&                          tu_file,
                             std::shared_ptr<std::vector<std::string>>   includes,
                             TUCallback                                  callback)
    : m_repo_root(repo_root)
    , m_tu_file(tu_file)
    , m_includes(std::move(includes))
    , m_callback(std::move(callback))
{}

void PrismConsumer::HandleTranslationUnit(clang::ASTContext& ctx)
{
    prism::ast::SymbolVisitor visitor(ctx, m_repo_root, m_tu_file);
    visitor.TraverseDecl(ctx.getTranslationUnitDecl());

    FileRecord file = visitor.File();
    if (m_includes)
        file.includes = *m_includes;

    m_callback(std::move(file), visitor.Symbols());
}

} // namespace prism
