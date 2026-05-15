/// @file ChangeStreamService.hpp
/// @brief Ordered change-stream service boundary for collaboration events.

#pragma once

#include "SagaShared/Collaboration/ParticipantId.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaCollaboration
{

/// Serializable collaboration change event.
struct ChangeStreamEntry
{
    std::uint64_t revision = 0; ///< Monotonic revision assigned by the service.
    SagaShared::Collaboration::ParticipantId participant; ///< Actor responsible for the change.
    std::string resourceId; ///< Affected resource identifier.
    std::string operation;  ///< Stable operation name.
    std::string summary;    ///< Short human-readable change summary.
};

/// Appends ordered collaboration events without owning persistence.
class ChangeStreamService
{
public:
    /// Append a change and assign its revision.
    [[nodiscard]] ChangeStreamEntry Append(ChangeStreamEntry entry);

    /// Return all changes after the supplied revision.
    [[nodiscard]] std::vector<ChangeStreamEntry> Since(std::uint64_t revision) const;

private:
    std::uint64_t m_nextRevision = 1; ///< Next revision assigned to an appended entry.
    std::vector<ChangeStreamEntry> m_entries; ///< Ordered in-memory change stream.
};

} // namespace SagaCollaboration
