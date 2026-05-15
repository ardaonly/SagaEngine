/// @file ConflictService.hpp
/// @brief Conflict record service boundary for collaboration workflows.

#pragma once

#include "SagaShared/Collaboration/ParticipantId.hpp"

#include <string>
#include <vector>

namespace SagaCollaboration
{

/// Conflict record shared through collaboration service APIs.
struct ConflictRecord
{
    std::string conflictId; ///< Stable conflict identifier.
    std::string resourceId; ///< Affected resource identifier.
    SagaShared::Collaboration::ParticipantId localParticipant; ///< Local actor.
    SagaShared::Collaboration::ParticipantId remoteParticipant; ///< Remote actor.
    std::string reason; ///< Machine-readable reason or short text.
    bool resolved = false; ///< True after a resolution has been accepted.
};

/// Stores unresolved and resolved conflict records without owning conflict UI.
class ConflictService
{
public:
    /// Add or replace a conflict record.
    void Report(ConflictRecord record);

    /// Mark a conflict as resolved when it exists.
    void Resolve(const std::string& conflictId);

    /// Return all known conflict records.
    [[nodiscard]] const std::vector<ConflictRecord>& Conflicts() const noexcept;

private:
    std::vector<ConflictRecord> m_conflicts; ///< Known conflicts.
};

} // namespace SagaCollaboration
