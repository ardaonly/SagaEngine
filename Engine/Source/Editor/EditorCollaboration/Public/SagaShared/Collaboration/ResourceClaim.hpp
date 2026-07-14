/// @file ResourceClaim.hpp
/// @brief Soft resource claim contract for collaboration coordination.

#pragma once

#include "SagaShared/Collaboration/ParticipantId.hpp"

#include <cstdint>
#include <string>

namespace SagaShared::Collaboration
{

/// Non-blocking signal that a participant or tool is actively working on a resource.
struct ResourceClaim
{
    std::string resourceId;       ///< Claimed resource identifier.
    ParticipantId participant;    ///< Claim owner.
    std::string reason;           ///< Short reason shown to consumers.
    std::uint64_t expiresAtUnixMs = 0; ///< Optional expiry timestamp; zero means unspecified.
};

} // namespace SagaShared::Collaboration
