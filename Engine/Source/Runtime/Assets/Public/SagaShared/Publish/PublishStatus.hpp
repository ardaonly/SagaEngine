/// @file PublishStatus.hpp
/// @brief Shared publish readiness status vocabulary.

#pragma once

#include <cstdint>

namespace SagaShared::Publish
{

/// Neutral publish readiness state.
enum class PublishStatus : std::uint8_t
{
    Unknown,
    Ready,
    ReadyWithWarnings,
    Blocked,
    Failed,
};

} // namespace SagaShared::Publish
