/// @file CollaborationService.hpp
/// @brief Aggregate collaboration service facade consumed by Saga modules.

#pragma once

#include "SagaCollaboration/ChangeStreamService.hpp"
#include "SagaCollaboration/ClaimService.hpp"
#include "SagaCollaboration/CollaborationModel.hpp"
#include "SagaCollaboration/ConflictService.hpp"
#include "SagaCollaboration/LockService.hpp"
#include "SagaCollaboration/PermissionService.hpp"
#include "SagaCollaboration/PresenceService.hpp"
#include "SagaCollaboration/SessionService.hpp"

namespace SagaCollaboration
{

/// Stable service facade for product, editor, runtime, and server consumers.
class CollaborationService
{
public:
    [[nodiscard]] SessionService& Sessions() noexcept;
    [[nodiscard]] const SessionService& Sessions() const noexcept;

    [[nodiscard]] PresenceService& Presence() noexcept;
    [[nodiscard]] const PresenceService& Presence() const noexcept;

    [[nodiscard]] PermissionService& Permissions() noexcept;
    [[nodiscard]] const PermissionService& Permissions() const noexcept;

    [[nodiscard]] ClaimService& Claims() noexcept;
    [[nodiscard]] const ClaimService& Claims() const noexcept;

    [[nodiscard]] LockService& Locks() noexcept;
    [[nodiscard]] const LockService& Locks() const noexcept;

    [[nodiscard]] ChangeStreamService& Changes() noexcept;
    [[nodiscard]] const ChangeStreamService& Changes() const noexcept;

    [[nodiscard]] ConflictService& Conflicts() noexcept;
    [[nodiscard]] const ConflictService& Conflicts() const noexcept;

private:
    SessionService m_sessions;       ///< Session lifecycle API.
    PresenceService m_presence;      ///< Presence API.
    PermissionService m_permissions; ///< Permission API.
    ClaimService m_claims;           ///< Soft claim API.
    LockService m_locks;             ///< Hard lock API.
    ChangeStreamService m_changes;   ///< Ordered change-stream API.
    ConflictService m_conflicts;     ///< Conflict record API.
};

} // namespace SagaCollaboration
