/// @file PresenceService.hpp
/// @brief Presence service boundary for collaboration participants.

#pragma once

#include "SagaShared/Collaboration/ParticipantId.hpp"
#include "SagaShared/Collaboration/PresenceSnapshot.hpp"

#include <vector>

namespace SagaCollaboration
{

/// Stores latest participant presence snapshots for service consumers.
class PresenceService
{
public:
    /// Add or replace the presence snapshot for one participant.
    void UpdatePresence(SagaShared::Collaboration::PresenceSnapshot snapshot);

    /// Remove presence for a participant that left the session.
    void RemovePresence(const SagaShared::Collaboration::ParticipantId& participant);

    /// Return all known presence snapshots.
    [[nodiscard]] std::vector<SagaShared::Collaboration::PresenceSnapshot>
    ListPresence() const;

private:
    std::vector<SagaShared::Collaboration::PresenceSnapshot> m_presence; ///< Latest presence by participant.
};

} // namespace SagaCollaboration
