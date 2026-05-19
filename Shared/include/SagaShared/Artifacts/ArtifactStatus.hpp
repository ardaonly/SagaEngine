/// @file ArtifactStatus.hpp
/// @brief Shared artifact lifecycle status vocabulary.

#pragma once

#include <cstdint>

namespace SagaShared::Artifacts
{

/// Neutral artifact state for build, cook, validation, and packaging reports.
enum class ArtifactStatus : std::uint8_t
{
    Unknown,
    Planned,
    Generated,
    Cooked,
    Packaged,
    Valid,
    Invalid,
    Missing,
    Stale,
};

} // namespace SagaShared::Artifacts
