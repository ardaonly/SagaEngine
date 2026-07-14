/// @file CollaborationContracts.cpp
/// @brief Small validation helpers for neutral collaboration identifiers.

#include "SagaShared/Collaboration/ParticipantId.hpp"
#include "SagaShared/Collaboration/RoomCode.hpp"
#include "SagaShared/Collaboration/SessionId.hpp"

namespace SagaShared::Collaboration
{

bool IsValid(const ParticipantId& id) noexcept
{
    return !id.value.empty();
}

bool IsValid(const SessionId& id) noexcept
{
    return !id.value.empty();
}

bool IsValid(const RoomCode& code) noexcept
{
    return !code.value.empty();
}

} // namespace SagaShared::Collaboration
