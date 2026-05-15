/// @file SdeSourceLoader.h
/// @brief Internal native .sde source loader.
///
/// This header is intentionally under src/. Lexer/parser/AST style details are
/// compiler internals, not stable consumer contracts.

#pragma once

#include "SDE/IO/ModelLoader.h"
#include "SDE/Model/ModelDefinition.h"
#include "SDE/Model/TypeNode.h"
#include "SDE/Validation/Diagnostic.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SDE::Internal
{

struct SdeSourceFileSummary
{
    std::filesystem::path path;
    int                   languageVersion = 0;
    std::string           packageName;
};

struct SdeSourceLoadResult
{
    std::vector<ModelDefinition>    definitions;
    std::vector<ModelInstance>      instances;
    std::vector<SdeSourceFileSummary> files;
};

[[nodiscard]] SdeSourceLoadResult LoadSdeSourceFile(
    const std::filesystem::path& path,
    TypeRegistry&                types,
    std::vector<Diagnostic>&     diagnostics);

} // namespace SDE::Internal
