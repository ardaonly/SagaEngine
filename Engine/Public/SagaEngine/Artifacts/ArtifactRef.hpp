/// @file ArtifactRef.hpp
/// @brief Defines one artifact reference from a runtime artifact manifest.

#pragma once

#include <SagaEngine/Artifacts/ArtifactKind.hpp>

#include <optional>
#include <string>

namespace SagaEngine::Artifacts
{

/// Runtime-facing reference to one packaged or generated artifact.
struct ArtifactRef
{
    std::string id;                  ///< Stable artifact id from the manifest.
    ArtifactKind kind;               ///< Runtime category used by consumers.
    std::string path;                ///< Manifest-relative or package-relative artifact path.
    std::optional<std::string> hash; ///< Optional integrity hash reserved for later validation.
};

} // namespace SagaEngine::Artifacts
