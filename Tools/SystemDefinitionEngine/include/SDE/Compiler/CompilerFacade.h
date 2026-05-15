/// @file CompilerFacade.h
/// @brief Stable high-level compiler facade for tools and editor integrations.

#pragma once

#include "SDE/Compiler/CompileRequest.h"
#include "SDE/Compiler/CompileResult.h"

namespace SDE
{

class CompilerFacade
{
public:
    [[nodiscard]] CompilerFacadeResult Validate(const CompileRequest& request) const;
    [[nodiscard]] CompilerFacadeResult Compile(const CompileRequest& request) const;
};

} // namespace SDE
