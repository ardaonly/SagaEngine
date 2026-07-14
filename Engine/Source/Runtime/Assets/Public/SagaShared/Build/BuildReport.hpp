/// @file BuildReport.hpp
/// @brief Shared build report contract.

#pragma once

#include "SagaShared/Artifacts/ArtifactRef.hpp"
#include "SagaShared/Build/BuildId.hpp"
#include "SagaShared/Build/BuildProfileId.hpp"
#include "SagaShared/Build/BuildStatus.hpp"
#include "SagaShared/Build/BuildStepResult.hpp"
#include "SagaShared/Build/TargetPlatform.hpp"
#include "SagaShared/Diagnostics/DiagnosticSummary.hpp"

#include <map>
#include <string>
#include <vector>

namespace SagaShared::Build
{

/// Data-only build report shared by product, tools, CI, and publish checks.
struct BuildReport
{
    BuildId buildId;
    BuildProfileId profileId;
    TargetPlatform targetPlatform;
    BuildStatus status = BuildStatus::Unknown;
    std::vector<BuildStepResult> steps;
    std::vector<Artifacts::ArtifactRef> artifactOutputs;
    Diagnostics::DiagnosticSummary diagnosticSummary;
    std::map<std::string, std::string> cacheDecisions;
    std::map<std::string, std::string> metadata;
};

} // namespace SagaShared::Build
