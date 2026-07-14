/// @file CollaborationServices.cpp
/// @brief In-memory collaboration service boundary implementations.

#include "SagaCollaboration/CollaborationService.hpp"

#include "SagaShared/Collaboration/ParticipantId.hpp"
#include "SagaShared/Collaboration/RoomCode.hpp"
#include "SagaShared/Collaboration/SessionId.hpp"

#include <algorithm>
#include <utility>

namespace SagaCollaboration
{
namespace
{

[[nodiscard]] bool SameParticipant(const SagaShared::Collaboration::ParticipantId& lhs,
                                   const SagaShared::Collaboration::ParticipantId& rhs)
{
    return lhs.value == rhs.value;
}

} // namespace

SagaShared::Collaboration::SessionId MakeLocalSessionId(
    const SagaShared::Workspace::WorkspaceDescriptor& workspace,
    const SagaShared::Collaboration::ParticipantId& host)
{
    SagaShared::Collaboration::SessionId id;
    id.value = "local:" + workspace.workspaceId + ":" + host.value;
    return id;
}

SagaShared::Collaboration::SessionDescriptor SessionService::StartLocalSession(
    SagaShared::Workspace::WorkspaceDescriptor workspace,
    SagaShared::Collaboration::ParticipantId host)
{
    SagaShared::Collaboration::SessionDescriptor descriptor;
    descriptor.sessionId = MakeLocalSessionId(workspace, host);
    descriptor.projectId = workspace.projectId;
    descriptor.mode = SagaShared::Collaboration::SessionMode::Local;
    descriptor.host = host;
    descriptor.workspace = std::move(workspace);
    descriptor.participants.push_back(host);
    m_currentSession = descriptor;
    return descriptor;
}

void SessionService::LeaveSession() noexcept
{
    m_currentSession.reset();
}

const std::optional<SagaShared::Collaboration::SessionDescriptor>&
SessionService::CurrentSession() const noexcept
{
    return m_currentSession;
}

void PresenceService::UpdatePresence(
    SagaShared::Collaboration::PresenceSnapshot snapshot)
{
    auto it = std::find_if(m_presence.begin(), m_presence.end(),
        [&snapshot](const SagaShared::Collaboration::PresenceSnapshot& item)
        {
            return SameParticipant(item.participant, snapshot.participant);
        });

    if (it == m_presence.end())
    {
        m_presence.push_back(std::move(snapshot));
        return;
    }

    *it = std::move(snapshot);
}

void PresenceService::RemovePresence(
    const SagaShared::Collaboration::ParticipantId& participant)
{
    m_presence.erase(
        std::remove_if(m_presence.begin(), m_presence.end(),
            [&participant](const SagaShared::Collaboration::PresenceSnapshot& item)
            {
                return SameParticipant(item.participant, participant);
            }),
        m_presence.end());
}

std::vector<SagaShared::Collaboration::PresenceSnapshot>
PresenceService::ListPresence() const
{
    return m_presence;
}

void PermissionService::Grant(
    SagaShared::Collaboration::PermissionGrant grant)
{
    auto it = std::find_if(m_grants.begin(), m_grants.end(),
        [&grant](const SagaShared::Collaboration::PermissionGrant& item)
        {
            return SameParticipant(item.participant, grant.participant) &&
                   item.permission == grant.permission &&
                   item.resourceId == grant.resourceId;
        });

    if (it == m_grants.end())
    {
        m_grants.push_back(std::move(grant));
        return;
    }

    *it = std::move(grant);
}

bool PermissionService::CanPerform(
    const SagaShared::Collaboration::ParticipantId& participant,
    const std::string& permission,
    const std::string& resourceId) const
{
    return std::any_of(m_grants.begin(), m_grants.end(),
        [&participant, &permission, &resourceId](
            const SagaShared::Collaboration::PermissionGrant& grant)
        {
            const bool participantMatches = SameParticipant(grant.participant, participant);
            const bool permissionMatches = grant.permission == permission ||
                                           grant.permission == "*";
            const bool resourceMatches = grant.resourceId.empty() ||
                                         grant.resourceId == resourceId;
            return participantMatches && permissionMatches && resourceMatches;
        });
}

const std::vector<SagaShared::Collaboration::PermissionGrant>&
PermissionService::Grants() const noexcept
{
    return m_grants;
}

void ClaimService::Claim(SagaShared::Collaboration::ResourceClaim claim)
{
    auto it = std::find_if(m_claims.begin(), m_claims.end(),
        [&claim](const SagaShared::Collaboration::ResourceClaim& item)
        {
            return item.resourceId == claim.resourceId &&
                   SameParticipant(item.participant, claim.participant);
        });

    if (it == m_claims.end())
    {
        m_claims.push_back(std::move(claim));
        return;
    }

    *it = std::move(claim);
}

void ClaimService::Release(
    const std::string& resourceId,
    const SagaShared::Collaboration::ParticipantId& participant)
{
    m_claims.erase(
        std::remove_if(m_claims.begin(), m_claims.end(),
            [&resourceId, &participant](const SagaShared::Collaboration::ResourceClaim& item)
            {
                return item.resourceId == resourceId &&
                       SameParticipant(item.participant, participant);
            }),
        m_claims.end());
}

const std::vector<SagaShared::Collaboration::ResourceClaim>&
ClaimService::Claims() const noexcept
{
    return m_claims;
}

bool LockService::TryLock(SagaShared::Collaboration::ResourceLock lock)
{
    const bool alreadyLocked = std::any_of(m_locks.begin(), m_locks.end(),
        [&lock](const SagaShared::Collaboration::ResourceLock& item)
        {
            return item.resourceId == lock.resourceId &&
                   item.lockId != lock.lockId;
        });

    if (alreadyLocked)
    {
        return false;
    }

    Release(lock.lockId);
    m_locks.push_back(std::move(lock));
    return true;
}

void LockService::Release(const std::string& lockId)
{
    m_locks.erase(
        std::remove_if(m_locks.begin(), m_locks.end(),
            [&lockId](const SagaShared::Collaboration::ResourceLock& item)
            {
                return item.lockId == lockId;
            }),
        m_locks.end());
}

const std::vector<SagaShared::Collaboration::ResourceLock>&
LockService::Locks() const noexcept
{
    return m_locks;
}

ChangeStreamEntry ChangeStreamService::Append(ChangeStreamEntry entry)
{
    entry.revision = m_nextRevision++;
    m_entries.push_back(entry);
    return entry;
}

std::vector<ChangeStreamEntry> ChangeStreamService::Since(
    std::uint64_t revision) const
{
    std::vector<ChangeStreamEntry> result;
    for (const ChangeStreamEntry& entry : m_entries)
    {
        if (entry.revision > revision)
        {
            result.push_back(entry);
        }
    }
    return result;
}

void ConflictService::Report(ConflictRecord record)
{
    auto it = std::find_if(m_conflicts.begin(), m_conflicts.end(),
        [&record](const ConflictRecord& item)
        {
            return item.conflictId == record.conflictId;
        });

    if (it == m_conflicts.end())
    {
        m_conflicts.push_back(std::move(record));
        return;
    }

    *it = std::move(record);
}

void ConflictService::Resolve(const std::string& conflictId)
{
    auto it = std::find_if(m_conflicts.begin(), m_conflicts.end(),
        [&conflictId](const ConflictRecord& item)
        {
            return item.conflictId == conflictId;
        });

    if (it != m_conflicts.end())
    {
        it->resolved = true;
    }
}

const std::vector<ConflictRecord>& ConflictService::Conflicts() const noexcept
{
    return m_conflicts;
}

SessionService& CollaborationService::Sessions() noexcept
{
    return m_sessions;
}

const SessionService& CollaborationService::Sessions() const noexcept
{
    return m_sessions;
}

PresenceService& CollaborationService::Presence() noexcept
{
    return m_presence;
}

const PresenceService& CollaborationService::Presence() const noexcept
{
    return m_presence;
}

PermissionService& CollaborationService::Permissions() noexcept
{
    return m_permissions;
}

const PermissionService& CollaborationService::Permissions() const noexcept
{
    return m_permissions;
}

ClaimService& CollaborationService::Claims() noexcept
{
    return m_claims;
}

const ClaimService& CollaborationService::Claims() const noexcept
{
    return m_claims;
}

LockService& CollaborationService::Locks() noexcept
{
    return m_locks;
}

const LockService& CollaborationService::Locks() const noexcept
{
    return m_locks;
}

ChangeStreamService& CollaborationService::Changes() noexcept
{
    return m_changes;
}

const ChangeStreamService& CollaborationService::Changes() const noexcept
{
    return m_changes;
}

ConflictService& CollaborationService::Conflicts() noexcept
{
    return m_conflicts;
}

const ConflictService& CollaborationService::Conflicts() const noexcept
{
    return m_conflicts;
}

} // namespace SagaCollaboration
