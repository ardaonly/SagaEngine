/// @file PermissionService.hpp
/// @brief Permission grant service boundary for collaboration workflows.

#pragma once

#include "SagaShared/Collaboration/ParticipantId.hpp"
#include "SagaShared/Collaboration/PermissionGrant.hpp"

#include <string>
#include <vector>

namespace SagaCollaboration
{

/// Stores collaboration permission grants without owning authentication.
class PermissionService
{
public:
    /// Add or replace a permission grant for a participant and resource scope.
    void Grant(SagaShared::Collaboration::PermissionGrant grant);

    /// Return true when a participant has the requested permission in scope.
    [[nodiscard]] bool CanPerform(
        const SagaShared::Collaboration::ParticipantId& participant,
        const std::string& permission,
        const std::string& resourceId = {}) const;

    /// Return all known permission grants.
    [[nodiscard]] const std::vector<SagaShared::Collaboration::PermissionGrant>&
    Grants() const noexcept;

private:
    std::vector<SagaShared::Collaboration::PermissionGrant> m_grants; ///< In-memory grant table.
};

} // namespace SagaCollaboration
