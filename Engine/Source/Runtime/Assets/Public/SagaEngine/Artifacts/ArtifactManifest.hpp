/// @file ArtifactManifest.hpp
/// @brief Defines the parsed runtime artifact manifest representation.

#pragma once

#include <SagaEngine/Artifacts/ArtifactRef.hpp>

#include <cstdint>
#include <vector>

namespace SagaEngine::Artifacts
{

/// Parsed artifact manifest consumed by runtime and server systems.
struct ArtifactManifest
{
    std::uint32_t schemaVersion = 0;       ///< Manifest schema version accepted by the loader.
    std::vector<ArtifactRef> artifacts;    ///< Valid artifact references parsed from the file.
};

} // namespace SagaEngine::Artifacts
