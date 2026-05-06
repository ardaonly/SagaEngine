/// @file PrismAction.cpp
/// @brief ASTFrontendAction and factory implementation.

#include "PrismAction.h"

#include "../Support/PathUtils.h"

#include <clang/Frontend/CompilerInstance.h>
#include <clang/Lex/PPCallbacks.h>
#include <clang/Lex/Preprocessor.h>
#include <llvm/ADT/StringRef.h>

#include <memory>
#include <vector>

namespace prism {

// ─── InclusionTracker ─────────────────────────────────────────────────────────

namespace {

class InclusionTracker final : public clang::PPCallbacks
{
public:
    InclusionTracker(const std::string&                        repo_root,
                     std::shared_ptr<std::vector<std::string>> out)
        : m_repo_root(repo_root), m_out(std::move(out))
    {}

    void InclusionDirective(
        clang::SourceLocation,
        const clang::Token&,
        llvm::StringRef,
        bool,
        clang::CharSourceRange,
        clang::OptionalFileEntryRef File,
        llvm::StringRef,
        llvm::StringRef,
        const clang::Module*,
        bool,
        clang::SrcMgr::CharacteristicKind) override
    {
        if (!File)
            return;
        const std::string abs(File->getName());
        if (!support::IsSystemPath(abs))
            m_out->push_back(support::MakeRelativePosix(abs, m_repo_root));
    }

private:
    std::string                               m_repo_root;
    std::shared_ptr<std::vector<std::string>> m_out;
};

} // namespace

// ─── PrismAction ──────────────────────────────────────────────────────────────

PrismAction::PrismAction(const std::string& repo_root,
                         ResultAccumulator& results)
    : m_repo_root(repo_root)
    , m_results(results)
{}

std::unique_ptr<clang::ASTConsumer> PrismAction::CreateASTConsumer(
    clang::CompilerInstance& ci,
    llvm::StringRef          in_file)
{
    const std::string tu_file = in_file.str();

    auto includes = std::make_shared<std::vector<std::string>>();
    ci.getPreprocessor().addPPCallbacks(
        std::make_unique<InclusionTracker>(m_repo_root, includes));

    return std::make_unique<PrismConsumer>(
        m_repo_root,
        tu_file,
        includes,
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
