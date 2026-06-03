/// @file SagaProductWorkflowSmokeReport.h
/// @brief Report-only Product Shell workflow smoke model.

#pragma once

#include <filesystem>
#include <string>

namespace SagaProduct
{

struct SagaProductWorkflowSmokeRequest
{
    std::filesystem::path projectManifestPath;
    std::string profile = "technical_preview";
    std::filesystem::path reportPath;
};

struct SagaProductWorkflowSmokeResult
{
    bool ok = false;
    std::filesystem::path reportPath;
    std::string status;
    std::string error;
};

[[nodiscard]] SagaProductWorkflowSmokeResult WriteProductWorkflowSmokeReport(
    const SagaProductWorkflowSmokeRequest& request);

} // namespace SagaProduct
