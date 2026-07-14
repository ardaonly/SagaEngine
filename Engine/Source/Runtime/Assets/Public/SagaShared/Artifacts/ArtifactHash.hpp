/// @file ArtifactHash.hpp
/// @brief Shared artifact content hash value.

#pragma once

#include <string>

namespace SagaShared::Artifacts
{

/// Optional content hash metadata for artifact integrity and stale checks.
struct ArtifactHash
{
    std::string algorithm; ///< Hash algorithm token, empty when unspecified.
    std::string value;     ///< Hash value, empty when unavailable.
};

} // namespace SagaShared::Artifacts
