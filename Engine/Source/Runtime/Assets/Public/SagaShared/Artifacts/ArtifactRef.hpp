/// @file ArtifactRef.hpp
/// @brief Shared artifact reference contract.

#pragma once

#include "SagaShared/Artifacts/ArtifactHash.hpp"
#include "SagaShared/Artifacts/ArtifactId.hpp"
#include "SagaShared/Artifacts/ArtifactKind.hpp"
#include "SagaShared/Artifacts/ArtifactStatus.hpp"
#include "SagaShared/Diagnostics/DiagnosticSummary.hpp"

#include <map>
#include <string>

namespace SagaShared::Artifacts
{

/// Data-only reference to one artifact without depending on a producing subsystem.
struct ArtifactRef
{
    ArtifactId id;                 ///< Stable artifact identifier.
    ArtifactKind kind = ArtifactKind::Data;
    ArtifactStatus status = ArtifactStatus::Unknown;
    std::string path;              ///< Package, manifest, or project-relative artifact path.
    ArtifactHash hash;             ///< Optional content hash.
    std::string schemaId;          ///< Optional schema identifier for structured artifacts.
    std::string sourceResourceId;  ///< Optional original source/resource identifier.
    std::string buildProfile;      ///< Optional producing build profile token.
    std::string targetPlatform;    ///< Optional target platform token.
    std::string producingStepId;   ///< Optional producing build/cook step id.
    Diagnostics::DiagnosticSummary diagnostics; ///< Optional diagnostics summary for this artifact.
    std::map<std::string, std::string> metadata; ///< Serialization-safe extension metadata.
};

} // namespace SagaShared::Artifacts
