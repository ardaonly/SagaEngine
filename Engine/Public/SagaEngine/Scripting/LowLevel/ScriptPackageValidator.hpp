/// @file ScriptPackageValidator.hpp
/// @brief Low-level SagaScript package validation contract.

#pragma once

#include "SagaEngine/Scripting/LowLevel/ISagaScriptHost.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace SagaEngine::Scripting
{

struct ScriptPackageValidationOptions
{
    std::string expectedPackageDestination;
    std::string expectedTargetFramework = "net10.0";
    bool requireRuntimeConfig = true;
    bool enforceAuthorityDestination = true;
};

struct ScriptPackageValidationResult
{
    bool succeeded = false;
    std::size_t artifactCount = 0;
    std::vector<ScriptDiagnostic> diagnostics;

    [[nodiscard]] bool Succeeded() const noexcept { return succeeded; }
};

class ScriptPackageValidator
{
public:
    [[nodiscard]] static ScriptPackageValidationResult ValidateLoadRequest(
        const ScriptPackageLoadRequest& request,
        const ScriptPackageValidationOptions& options = {});
};

} // namespace SagaEngine::Scripting
