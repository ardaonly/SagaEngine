/// @file PrismAction.cpp
/// @brief ASTFrontendAction and factory implementation.

#include "PrismAction.h"

#include <llvm/ADT/StringRef.h>

namespace prism {

// ─── PrismAction ──────────────────────────────────────────────────────────────

PrismAction::PrismAction(const std::string& repo_root,
                         ResultAccumulator& results)
    : m_repo_root(repo_root)
    , m_results(results)
{}

std::unique_ptr<clang::ASTConsumer> PrismAction::CreateASTConsumer(
    clang::CompilerInstance& /*ci*/,
    llvm::StringRef          in_file)
{
    const std::string tu_file = in_file.str();

    return std::make_unique<PrismConsumer>(
        m_repo_root,
        tu_file,
        [this](FileRecord file, std::vector<SymbolRecord> syms)
        {
            std::lock_guard<std::mutex> lock(m_results.mu);
            m_results.files.push_back(std::move(file));
            for (auto& s : syms)
                m_results.symbols.push_back(std::move(s));
        }
    );
}

// ─── PrismActionFactory ───────────────────────────────────────────────────────

PrismActionFactory::PrismActionFactory(const std::string& repo_root,
                                       ResultAccumulator& results)
    : m_repo_root(repo_root)
    , m_results(results)
{}

std::unique_ptr<clang::FrontendAction> PrismActionFactory::create()
{
    return std::make_unique<PrismAction>(m_repo_root, m_results);
}

} // namespace prism
