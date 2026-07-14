/// @file ArtifactDependency.hpp
/// @brief Shared artifact dependency reference.

#pragma once

#include "SagaShared/Artifacts/ArtifactId.hpp"

#include <string>

namespace SagaShared::Artifacts
{

/// Directed dependency between two artifacts.
struct ArtifactDependency
{
    ArtifactId fromArtifact;
    ArtifactId toArtifact;
    std::string relationship; ///< Stable relationship token, empty when unspecified.
    bool required = true;
};

} // namespace SagaShared::Artifacts
