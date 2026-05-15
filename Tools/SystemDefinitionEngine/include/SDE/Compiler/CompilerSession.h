/// @file CompilerSession.h
/// @brief Project-level compiler facade with explicit lifecycle ownership.

#pragma once

#include "SDE/Compilation/ModelCompiler.h"
#include "SDE/Compiler/CompileContext.h"
#include "SDE/Compiler/CompileResult.h"
#include "SDE/Compiler/DependencyManifest.h"
#include "SDE/Core/Version.h"
#include "SDE/Model/EnumRegistry.h"
#include "SDE/Model/TypeNode.h"
#include "SDE/Validation/Rule.h"
#include "SDE/Validation/Validator.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SDE
{

class SharedRegistrySet
{
public:
    TypeRegistry types;
    RuleRegistry rules;
    EnumRegistry enums;

    void Freeze();
    [[nodiscard]] bool IsFrozen() const noexcept;
};

struct ProjectLayout
{
    std::filesystem::path root;
    std::filesystem::path sourceDir  = "source";
    std::filesystem::path schemasDir = "schemas";
    std::filesystem::path dataDir    = "data";
};

class CompilerSession
{
public:
    explicit CompilerSession(ProjectLayout layout);

    [[nodiscard]] ProjectCompilationResult Validate(CompileContext context = {});
    [[nodiscard]] ProjectCompilationResult Compile(CompileContext context = {});

private:
    ProjectLayout mLayout;

    [[nodiscard]] ProjectCompilationResult Run(bool buildGraph, CompileContext context);
};

} // namespace SDE
