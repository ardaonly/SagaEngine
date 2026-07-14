/// @file ProjectVersion.hpp
/// @brief Serializable project version tuple shared across Saga modules.

#pragma once

#include <cstdint>
#include <string>

namespace SagaShared::Project
{

/// Version tuple for project compatibility checks.
struct ProjectVersion
{
    std::uint32_t major = 0; ///< Breaking project format version.
    std::uint32_t minor = 0; ///< Backward-compatible project format version.
    std::uint32_t patch = 0; ///< Patch-level project format version.
    std::string label;       ///< Optional pre-release or build label.
};

} // namespace SagaShared::Project
