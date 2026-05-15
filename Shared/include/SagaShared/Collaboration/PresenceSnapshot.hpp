/// @file PresenceSnapshot.hpp
/// @brief Presence snapshot contract for collaborative authoring.

#pragma once

#include "SagaShared/Collaboration/ParticipantId.hpp"

#include <cstdint>
#include <string>

namespace SagaShared::Collaboration
{

/// Serializable participant presence state without transport or UI ownership.
struct PresenceSnapshot
{
    ParticipantId participant; ///< Actor represented by this snapshot.
    std::string mode;          ///< Current product mode, such as editor or runtime.
    std::string openResource;  ///< Asset, scene, or graph currently in focus.
    std::string selectedEntity; ///< Optional selected entity/resource id.
    std::string currentTool;   ///< Optional active tool id.
    bool online = false;       ///< True when the participant is currently connected.
    bool idle = false;         ///< True when the participant is inactive.
    std::uint64_t revision = 0; ///< Monotonic presence revision.
};

} // namespace SagaShared::Collaboration
