/// @file ProjectDescriptor.hpp
/// @brief Neutral project descriptor shared by product, editor, runtime, and tools.

#pragma once

#include "SagaShared/Project/ProjectValidationState.hpp"
#include "SagaShared/Project/ProjectVersion.hpp"

#include <string>

namespace SagaShared::Project
{

/// Stable project identity and validation metadata without product-shell behavior.
struct ProjectDescriptor
{
    std::string projectId;       ///< Stable project identifier used across modules.
    std::string displayName;     ///< User-facing project name without UI ownership.
    std::string projectRoot;     ///< Local or mounted project root path.
    ProjectVersion version;      ///< Project schema/content version.
    std::string packageManifest; ///< Optional package manifest reference.
    std::string activeWorkspace; ///< Optional active workspace identifier.
    std::string collaborationMode; ///< Collaboration mode hint, empty when local-only.
    ProjectValidationState validationState = ProjectValidationState::Unknown; ///< Last known validation state.
};

} // namespace SagaShared::Project
