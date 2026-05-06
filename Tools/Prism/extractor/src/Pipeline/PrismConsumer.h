/// @file PrismConsumer.h
/// @brief ASTConsumer that drives the SymbolVisitor after a TU is fully parsed.

#pragma once

#include "Record.h"
#include "AST/SymbolVisitor.h"

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace prism {

/// Callback type invoked once per translation unit with the collected records.
using TUCallback = std::function<void(FileRecord, std::vector<SymbolRecord>)>;

/// ASTConsumer implementation — launches the visitor on HandleTranslationUnit.
class PrismConsumer : public clang::ASTConsumer
{
public:
    /// includes is written by a PPCallbacks instance registered by PrismAction
    /// and is fully populated by the time HandleTranslationUnit fires.
    PrismConsumer(const std::string&                          repo_root,
                  const std::string&                          tu_file,
                  std::shared_ptr<std::vector<std::string>>   includes,
                  TUCallback                                  callback);

    void HandleTranslationUnit(clang::ASTContext& ctx) override;

private:
    std::string                                 m_repo_root;
    std::string                                 m_tu_file;
    std::shared_ptr<std::vector<std::string>>   m_includes;
    TUCallback                                  m_callback;
};

} // namespace prism
