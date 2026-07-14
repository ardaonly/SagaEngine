/// @file ClaimService.hpp
/// @brief Soft resource claim service boundary for collaboration workflows.

#pragma once

#include "SagaShared/Collaboration/ParticipantId.hpp"
#include "SagaShared/Collaboration/ResourceClaim.hpp"

#include <string>
#include <vector>

namespace SagaCollaboration
{

/// Tracks non-blocking resource claims for coordination.
class ClaimService
{
public:
    /// Add or replace a claim for the same resource and participant.
    void Claim(SagaShared::Collaboration::ResourceClaim claim);

    /// Release claims matching the resource and participant.
    void Release(const std::string& resourceId,
                 const SagaShared::Collaboration::ParticipantId& participant);

    /// Return all active claims.
    [[nodiscard]] const std::vector<SagaShared::Collaboration::ResourceClaim>&
    Claims() const noexcept;

private:
    std::vector<SagaShared::Collaboration::ResourceClaim> m_claims; ///< Active soft claims.
};

} // namespace SagaCollaboration
