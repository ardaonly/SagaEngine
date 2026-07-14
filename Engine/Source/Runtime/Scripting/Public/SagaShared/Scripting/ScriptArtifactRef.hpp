/// @file ScriptArtifactRef.hpp
/// @brief Shared script artifact reference contract.

#pragma once

#include "SagaShared/Artifacts/ArtifactRef.hpp"

#include <map>
#include <string>

namespace SagaShared::Scripting
{

/// Data-only reference to a packaged script artifact and its script owner.
struct ScriptArtifactRef
{
    std::string scriptId;             ///< Stable script identifier associated with the artifact.
    Artifacts::ArtifactRef artifact;  ///< Underlying build/package artifact reference.
    std::string packageDestination;   ///< Intended destination such as client, server, editor, build, or tool.
    std::string entryPoint;           ///< Optional script entry point exported by the artifact.
    std::map<std::string, std::string> metadata; ///< Serialization-safe extension metadata.
};

} // namespace SagaShared::Scripting
