/// @file SagaWorkspaceResolver.h
/// @brief Resolves and validates product-backed Saga workspace definitions.

#pragma once

#include "Projects/SagaSessionModel.h"

#include <filesystem>
#include <string>
#include <vector>

namespace SagaProduct
{

/// Request for resolving a workspace selector into a product workspace contract.
struct SagaWorkspaceResolveRequest
{
    std::string           selector = "builtin:basic";
    std::filesystem::path builtInBasicRoot;
};

/// Workspace resolution result with deterministic failure details.
struct SagaWorkspaceResolveResult
{
    bool                    ok = false;
    SagaWorkspaceDefinition workspace;
    std::string             error;
    std::vector<std::string> diagnostics;
};

/// Owns product-level workspace resolution policy.
class SagaWorkspaceResolver
{
public:
    /// Resolve and validate the requested workspace definition.
    [[nodiscard]] SagaWorkspaceResolveResult Resolve(
        const SagaWorkspaceResolveRequest& request) const;
};

} // namespace SagaProduct
