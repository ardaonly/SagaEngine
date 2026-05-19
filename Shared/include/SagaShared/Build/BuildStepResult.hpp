/// @file BuildStepResult.hpp
/// @brief Shared build step result contract.

#pragma once

#include "SagaShared/Artifacts/ArtifactRef.hpp"
#include "SagaShared/Build/BuildStatus.hpp"
#include "SagaShared/Build/BuildStepKind.hpp"
#include "SagaShared/Diagnostics/DiagnosticPayload.hpp"
#include "SagaShared/Diagnostics/DiagnosticSummary.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace SagaShared::Build
{

/// Data-only result for one build pipeline step.
struct BuildStepResult
{
    std::string stepId;
    BuildStepKind stepKind = BuildStepKind::Unknown;
    BuildStatus status = BuildStatus::Unknown;
    std::int32_t exitCode = 0;
    std::vector<Artifacts::ArtifactRef> outputs;
    std::vector<Diagnostics::DiagnosticPayload> diagnostics;
    Diagnostics::DiagnosticSummary diagnosticSummary;
    std::map<std::string, std::string> metadata;
};

} // namespace SagaShared::Build
