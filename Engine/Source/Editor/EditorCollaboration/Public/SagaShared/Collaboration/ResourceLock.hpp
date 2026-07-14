/// @file ResourceLock.hpp
/// @brief Hard resource lock contract for collaboration coordination.

#pragma once

#include "SagaShared/Collaboration/ParticipantId.hpp"

#include <cstdint>
#include <string>

namespace SagaShared::Collaboration
{

/// Blocking lock for resources that cannot be edited concurrently.
struct ResourceLock
{
    std::string lockId;           ///< Stable lock identifier.
    std::string resourceId;       ///< Locked resource identifier.
    ParticipantId participant;    ///< Lock owner.
    std::string reason;           ///< Short lock reason.
    bool revocable = true;        ///< Whether authorized users may revoke the lock.
    std::uint64_t expiresAtUnixMs = 0; ///< Optional expiry timestamp; zero means unspecified.
};

} // namespace SagaShared::Collaboration
