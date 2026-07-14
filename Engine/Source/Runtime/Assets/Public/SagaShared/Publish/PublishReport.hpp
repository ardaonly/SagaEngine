/// @file PublishReport.hpp
/// @brief Shared publish report contract.

#pragma once

#include "SagaShared/Build/BuildId.hpp"
#include "SagaShared/Diagnostics/DiagnosticSummary.hpp"
#include "SagaShared/Packages/PackageId.hpp"
#include "SagaShared/Publish/PublishReadiness.hpp"

#include <map>
#include <string>

namespace SagaShared::Publish
{

/// Data-only publish report shared by product, tools, and CI.
struct PublishReport
{
    std::string reportId;
    Build::BuildId buildId;
    Packages::PackageId packageId;
    PublishReadiness readiness;
    Diagnostics::DiagnosticSummary diagnosticSummary;
    std::map<std::string, std::string> metadata;
};

} // namespace SagaShared::Publish
