/// @file PrismAction.h
/// @brief ASTFrontendAction that creates a PrismConsumer per translation unit.

#pragma once

#include "Record.h"
#include "PrismConsumer.h"

#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace prism {

/// Shared accumulator written to from multiple translation-unit actions.
/// Mutex-protected so ClangTool can run actions concurrently (future-proof).
struct ResultAccumulator
{
    std::mutex                mu;
    std::vector<FileRecord>   files;
    std::vector<SymbolRecord> symbols;
};

/// FrontendAction that instantiates a PrismConsumer for each input file.
class PrismAction : public clang::ASTFrontendAction
{
public:
    explicit PrismAction(const std::string& repo_root,
                         ResultAccumulator& results);

    std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
        clang::CompilerInstance& ci,
        llvm::StringRef          in_file) override;

private:
    std::string        m_repo_root; ///< Absolute repository root path.
    ResultAccumulator& m_results;   ///< Shared output accumulator.
};

/// FrontendActionFactory that hands each action the shared ResultAccumulator.
class PrismActionFactory : public clang::tooling::FrontendActionFactory
{
public:
    PrismActionFactory(const std::string& repo_root,
                       ResultAccumulator& results);

    std::unique_ptr<clang::FrontendAction> create() override;

private:
    std::string        m_repo_root;
    ResultAccumulator& m_results;
};

} // namespace prism
