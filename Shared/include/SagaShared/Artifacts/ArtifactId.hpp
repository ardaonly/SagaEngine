/// @file ArtifactId.hpp
/// @brief Shared stable artifact identifier.

#pragma once

#include <string>

namespace SagaShared::Artifacts
{

/// Stable identifier for a generated, cooked, packaged, or report artifact.
struct ArtifactId
{
    std::string value;
};

} // namespace SagaShared::Artifacts
