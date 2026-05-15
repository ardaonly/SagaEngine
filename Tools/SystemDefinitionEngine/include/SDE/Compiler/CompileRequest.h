/// @file CompileRequest.h
/// @brief Stable high-level SDE compile request contract.

#pragma once

#include "SDE/Compiler/CompileContext.h"

#include <filesystem>
#include <string>

namespace SDE
{

struct CompileRequest
{
    std::filesystem::path workspaceRoot;
    std::filesystem::path outputRoot;
    std::string           domain = "Saga.Editor.Customization";
    std::string           artifactKind = "EditorCustomizationGraph";
    CompileContext        context;
};

} // namespace SDE
