/// @file ProjectValidationState.hpp
/// @brief Validation state vocabulary for shared project descriptors.

#pragma once

#include <cstdint>

namespace SagaShared::Project
{

/// Describes whether shared project data is suitable for runtime consumption.
enum class ProjectValidationState : std::uint8_t
{
    Unknown,
    Valid,
    Invalid,
    Pending,
};

} // namespace SagaShared::Project
