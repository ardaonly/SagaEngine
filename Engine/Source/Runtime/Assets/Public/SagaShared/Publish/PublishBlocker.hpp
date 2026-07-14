/// @file PublishBlocker.hpp
/// @brief Shared publish blocker contract.

#pragma once

#include "SagaShared/Artifacts/ArtifactRef.hpp"
#include "SagaShared/Diagnostics/DiagnosticPayload.hpp"
#include "SagaShared/Packages/PackageId.hpp"
#include "SagaShared/Publish/PublishBlockerKind.hpp"

#include <map>
#include <string>
#include <vector>

namespace SagaShared::Publish
{

/// Data-only publish blocker with affected resources and diagnostics.
struct PublishBlocker
{
    PublishBlockerKind kind = PublishBlockerKind::Unknown;
    std::string message;
    std::string suggestedAction;
    Packages::PackageId packageId;
    std::vector<std::string> affectedResourceIds;
    std::vector<Artifacts::ArtifactRef> affectedArtifacts;
    std::vector<Diagnostics::DiagnosticPayload> diagnostics;
    std::map<std::string, std::string> metadata;
};

} // namespace SagaShared::Publish
