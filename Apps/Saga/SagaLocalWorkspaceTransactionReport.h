/// @file SagaLocalWorkspaceTransactionReport.h
/// @brief Report-only local workspace transaction boundary proof.

#pragma once

#include <filesystem>
#include <string>

namespace SagaProduct
{

struct SagaLocalWorkspaceTransactionRequest
{
    std::filesystem::path projectManifestPath;
    std::string workspaceSelector = "builtin:basic";
    std::string actorId = "local.actor";
    std::string operationKind = "InspectProject";
    std::filesystem::path reportPath;
};

struct SagaLocalWorkspaceTransactionResult
{
    bool ok = false;
    std::filesystem::path reportPath;
    std::string status;
    std::string error;
};

[[nodiscard]] SagaLocalWorkspaceTransactionResult
WriteLocalWorkspaceTransactionReport(
    const SagaLocalWorkspaceTransactionRequest& request);

} // namespace SagaProduct
