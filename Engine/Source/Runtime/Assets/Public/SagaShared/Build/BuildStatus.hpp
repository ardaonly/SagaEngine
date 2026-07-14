/// @file BuildStatus.hpp
/// @brief Shared build status vocabulary.

#pragma once

#include <cstdint>

namespace SagaShared::Build
{

/// Neutral build result state for reports and product/tool summaries.
enum class BuildStatus : std::uint8_t
{
    Unknown,
    Planned,
    Running,
    Succeeded,
    SucceededWithWarnings,
    Failed,
    Blocked,
    Canceled,
};

} // namespace SagaShared::Build
