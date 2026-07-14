/// @file LifetimeHandle.hpp
/// @brief Defines opaque handles for tracked logical object lifetimes.

#pragma once

#include <cstdint>

namespace SagaEngine::Diagnostics
{

/// Stable reference to a logical object tracked by LifetimeTracker.
struct LifetimeHandle
{
    std::uint64_t objectId = 0;  ///< Diagnostics-local object identifier.

    /// Return whether the handle refers to a registered object.
    [[nodiscard]] bool IsValid() const noexcept
    {
        return objectId != 0;
    }

    friend bool operator==(LifetimeHandle lhs, LifetimeHandle rhs) noexcept
    {
        return lhs.objectId == rhs.objectId;
    }
};

} // namespace SagaEngine::Diagnostics
