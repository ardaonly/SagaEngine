/// @file PrismConsumer.h
/// @brief ASTConsumer that drives the SymbolVisitor after a TU is fully parsed.

#pragma once

#include "Record.h"
#include "SymbolVisitor.h"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>

#include <functional>
#include <memory>
#include <string>

namespace prism {

/// Callback type invoked once per translation unit with the collected records.
using TUCallback = std::function<void(FileRecord, std::vector<SymbolRecord>)>;

/// ASTConsumer implementation — launches the visitor on HandleTranslationUnit.
class PrismConsumer : public clang::ASTConsumer
{
public:
    PrismConsumer(const std::string& repo_root,
                  const std::string& tu_file,
                  TUCallback         callback);

    void HandleTranslationUnit(clang::ASTContext& ctx) override;

private:
    std::string m_repo_root; ///< Absolute repo root for path relativization.
    std::string m_tu_file;   ///< Absolute path of the TU being processed.
    TUCallback  m_callback;  ///< Receives results after the visitor completes.
};

} // namespace prism
