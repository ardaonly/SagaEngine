/// @file PermissionGrant.hpp
/// @brief Permission grant contract for collaboration access checks.

#pragma once

#include "SagaShared/Collaboration/ParticipantId.hpp"

#include <string>

namespace SagaShared::Collaboration
{

/// Resource-scoped permission assignment without enforcement implementation.
struct PermissionGrant
{
    ParticipantId participant; ///< Participant receiving the grant.
    std::string role;          ///< Role name such as owner, maintainer, or guest.
    std::string permission;    ///< Permission domain or capability id.
    std::string resourceId;    ///< Optional scoped resource id.
};

} // namespace SagaShared::Collaboration
