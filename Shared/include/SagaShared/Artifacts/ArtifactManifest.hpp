/// @file ArtifactManifest.hpp
/// @brief Shared artifact manifest contract.

#pragma once

#include "SagaShared/Artifacts/ArtifactDependency.hpp"
#include "SagaShared/Artifacts/ArtifactRef.hpp"
#include "SagaShared/Diagnostics/DiagnosticSummary.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace SagaShared::Artifacts
{

/// Data-only artifact manifest suitable for build/package/report exchange.
struct ArtifactManifest
{
    std::uint32_t schemaVersion = 0;
    std::string manifestId;
    std::vector<ArtifactRef> artifacts;
    std::vector<ArtifactDependency> dependencies;
    Diagnostics::DiagnosticSummary diagnostics;
    std::map<std::string, std::string> metadata;
};

} // namespace SagaShared::Artifacts
