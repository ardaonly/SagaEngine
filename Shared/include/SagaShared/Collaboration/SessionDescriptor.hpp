/// @file SessionDescriptor.hpp
/// @brief Shared collaboration session descriptor.

#pragma once

#include "SagaShared/Collaboration/CollaborationDiagnostic.hpp"
#include "SagaShared/Collaboration/ParticipantId.hpp"
#include "SagaShared/Collaboration/PermissionGrant.hpp"
#include "SagaShared/Collaboration/PresenceSnapshot.hpp"
#include "SagaShared/Collaboration/ResourceClaim.hpp"
#include "SagaShared/Collaboration/ResourceLock.hpp"
#include "SagaShared/Collaboration/RoomCode.hpp"
#include "SagaShared/Collaboration/SessionId.hpp"
#include "SagaShared/Workspace/WorkspaceDescriptor.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace SagaShared::Collaboration
{

/// Collaboration session mode understood across product, editor, runtime, and server code.
enum class SessionMode : std::uint8_t
{
    Local,
    Quick,
    Team,
};

/// Serializable session snapshot without lifecycle, transport, or backend ownership.
struct SessionDescriptor
{
    SessionId sessionId; ///< Stable collaboration session id.
    std::string projectId; ///< Owning project id.
    SessionMode mode = SessionMode::Local; ///< Session operating mode.
    ParticipantId host; ///< Current host or authority actor.
    RoomCode roomCode; ///< Optional quick-join room code.
    SagaShared::Workspace::WorkspaceDescriptor workspace; ///< Workspace context.
    std::vector<ParticipantId> participants; ///< Known participants.
    std::vector<PermissionGrant> permissions; ///< Current permission grants.
    std::vector<PresenceSnapshot> presence; ///< Current presence snapshots.
    std::vector<ResourceClaim> claims; ///< Active soft claims.
    std::vector<ResourceLock> locks; ///< Active hard locks.
    std::vector<CollaborationDiagnostic> diagnostics; ///< Current session diagnostics.
    std::uint64_t streamRevision = 0; ///< Last known change-stream revision.
};

} // namespace SagaShared::Collaboration
