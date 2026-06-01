/// @file ResourceRecord.hpp
/// @brief Defines explicit diagnostics resource records.

#pragma once

#include <SagaEngine/Diagnostics/Resource/ResourceType.hpp>

#include <cstdint>
#include <string>

namespace SagaEngine::Diagnostics
{

/// Explicitly registered non-memory resource record.
struct ResourceRecord
{
    std::uint64_t resourceId = 0;       ///< Diagnostics-local resource id.
    ResourceType resourceType = ResourceType::Other; ///< Logical resource type.
    std::string ownerSystem;            ///< System that registered the resource.
    std::string debugName;              ///< Stable human-readable resource name.
    std::uint64_t createdTick = 0;      ///< Creation tick when available.
    std::uint64_t releasedTick = 0;     ///< Release tick when available.
    bool released = false;              ///< True after explicit release.
};

} // namespace SagaEngine::Diagnostics
