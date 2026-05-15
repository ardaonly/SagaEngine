/// @file SessionService.hpp
/// @brief In-memory session lifecycle service boundary for Saga collaboration.

#pragma once

#include "SagaShared/Collaboration/ParticipantId.hpp"
#include "SagaShared/Collaboration/SessionDescriptor.hpp"
#include "SagaShared/Collaboration/SessionId.hpp"
#include "SagaShared/Workspace/WorkspaceDescriptor.hpp"

#include <optional>
#include <string>

namespace SagaCollaboration
{

/// Owns local collaboration session state without product-shell or editor UI ownership.
class SessionService
{
public:
    /// Start a local development session for the supplied workspace.
    [[nodiscard]] SagaShared::Collaboration::SessionDescriptor StartLocalSession(
        SagaShared::Workspace::WorkspaceDescriptor workspace,
        SagaShared::Collaboration::ParticipantId host);

    /// End the current session and clear transient session state.
    void LeaveSession() noexcept;

    /// Return the current session snapshot if one is active.
    [[nodiscard]] const std::optional<SagaShared::Collaboration::SessionDescriptor>&
    CurrentSession() const noexcept;

private:
    std::optional<SagaShared::Collaboration::SessionDescriptor> m_currentSession; ///< Active local session.
};

/// Build a deterministic local session id from workspace and host ids.
[[nodiscard]] SagaShared::Collaboration::SessionId MakeLocalSessionId(
    const SagaShared::Workspace::WorkspaceDescriptor& workspace,
    const SagaShared::Collaboration::ParticipantId& host);

} // namespace SagaCollaboration
