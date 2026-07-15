/// @file SagaProductWorkflowSmokeReport.h
/// @brief Typed schema-2 Product Shell workflow status/report model.

#pragma once

#include <filesystem>
#include <string>

namespace SagaProduct
{

struct SagaProductWorkflowSmokeRequest
{
    std::filesystem::path projectManifestPath;
    std::string profile = "project_readiness";
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
