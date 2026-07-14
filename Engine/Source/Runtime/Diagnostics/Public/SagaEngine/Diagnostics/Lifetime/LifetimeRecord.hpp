/// @file LifetimeRecord.hpp
/// @brief Defines logical lifetime state for diagnostics tracking.

#pragma once

#include <cstdint>
#include <string>

namespace SagaEngine::Diagnostics
{

/// Captures creation, ownership, and destruction state for one logical object.
struct LifetimeRecord
{
    std::uint64_t objectId = 0;       ///< Diagnostics-local object identifier.
    std::uint64_t externalId = 0;     ///< Optional engine/server identifier supplied by the caller.
    std::uint64_t ownerObjectId = 0;  ///< Optional parent object tracked by LifetimeTracker.
    std::string typeName;             ///< Logical type, such as Session or PlayerEntity.
    std::string ownerSystem;          ///< System responsible for creating the object.
    std::uint64_t createdTick = 0;    ///< Server tick at creation when available.
    std::uint64_t destroyedTick = 0;  ///< Server tick at destruction when available.
    bool destroyed = false;           ///< True once the logical object is marked destroyed.
};

} // namespace SagaEngine::Diagnostics
