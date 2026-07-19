// SPDX-License-Identifier: MPL-2.0
/// @file LocalCollaborationMetadataStore.cpp
/// @brief Local/offline collaboration metadata store implementation.

#include "SagaCollaboration/CollaborationModel.hpp"

#include <algorithm>
#include <utility>

namespace SagaCollaboration
{

bool LocalCollaborationMetadataStore::RegisterActor(CollaborationActor actor)
{
    if (!IsValid(actor))
    {
        return false;
    }

    auto it = std::find_if(m_actors.begin(), m_actors.end(),
        [&actor](const CollaborationActor& item)
        {
            return item.actorId == actor.actorId;
        });
    if (it == m_actors.end())
    {
        m_actors.push_back(std::move(actor));
        return true;
    }

    *it = std::move(actor);
    return true;
}

void LocalCollaborationMetadataStore::RegisterArtifact(CollaborationArtifactRecord artifact)
{
    auto it = std::find_if(m_artifacts.begin(), m_artifacts.end(),
        [&artifact](const CollaborationArtifactRecord& item)
        {
            return item.kind == artifact.kind && item.path == artifact.path;
        });
    if (it == m_artifacts.end())
    {
        m_artifacts.push_back(std::move(artifact));
        return;
    }

    *it = std::move(artifact);
}

void LocalCollaborationMetadataStore::UpdatePresence(ArtifactPresence presence)
{
    auto it = std::find_if(m_presence.begin(), m_presence.end(),
        [&presence](const ArtifactPresence& item)
        {
            return item.actorId == presence.actorId;
        });
    if (it == m_presence.end())
    {
        m_presence.push_back(std::move(presence));
        return;
    }

    *it = std::move(presence);
}

bool LocalCollaborationMetadataStore::TryAcquireLock(SagaShared::Collaboration::ResourceLock lock)
{
    if (lock.resourceId.empty())
    {
        return false;
    }

    const SagaShared::Collaboration::ResourceLock* existing = FindLock(lock.resourceId);
    if (existing != nullptr && existing->lockId != lock.lockId)
    {
        return false;
    }

    ReleaseLock(lock.lockId);
    m_locks.push_back(std::move(lock));
    return true;
}

void LocalCollaborationMetadataStore::ReleaseLock(const std::string& lockId)
{
    m_locks.erase(
        std::remove_if(m_locks.begin(), m_locks.end(),
            [&lockId](const SagaShared::Collaboration::ResourceLock& item)
            {
                return item.lockId == lockId;
            }),
        m_locks.end());
}

void LocalCollaborationMetadataStore::RegisterComment(ArtifactComment comment)
{
    auto it = std::find_if(m_comments.begin(), m_comments.end(),
        [&comment](const ArtifactComment& item)
        {
            return item.commentId == comment.commentId;
        });
    if (it == m_comments.end())
    {
        m_comments.push_back(std::move(comment));
        return;
    }

    *it = std::move(comment);
}

void LocalCollaborationMetadataStore::RegisterReviewRequest(ReviewRequest review)
{
    auto it = std::find_if(m_reviews.begin(), m_reviews.end(),
        [&review](const ReviewRequest& item)
        {
            return item.reviewId == review.reviewId;
        });
    if (it == m_reviews.end())
    {
        m_reviews.push_back(std::move(review));
        return;
    }

    *it = std::move(review);
}

void LocalCollaborationMetadataStore::RegisterDangerousOperationCheck(
    DangerousOperationCheck check)
{
    auto it = std::find_if(m_dangerousOperationChecks.begin(), m_dangerousOperationChecks.end(),
        [&check](const DangerousOperationCheck& item)
        {
            return item.operationId == check.operationId;
        });
    if (it == m_dangerousOperationChecks.end())
    {
        m_dangerousOperationChecks.push_back(std::move(check));
        return;
    }

    *it = std::move(check);
}

TransactionSubmission LocalCollaborationMetadataStore::SubmitTransaction(
    SemanticTransaction transaction)
{
    if (!IsSemanticTransactionOperation(transaction.operationKind))
    {
        return Reject(std::move(transaction),
                      CollaborationConflictCategory::UnsupportedOperation,
                      "operation is not a supported metadata transaction");
    }

    if (transaction.sequence != m_nextSequence)
    {
        return Reject(std::move(transaction),
                      CollaborationConflictCategory::OutOfOrderOperation,
                      "operation sequence is not the next expected value");
    }

    if (!HasActor(transaction.actorId))
    {
        return Reject(std::move(transaction),
                      CollaborationConflictCategory::UnknownActor,
                      "operation actor is not registered in local workspace identity");
    }

    const CollaborationArtifactRecord* artifact =
        FindArtifact(transaction.targetArtifactKind, transaction.targetPath);
    if (artifact == nullptr)
    {
        return Reject(std::move(transaction),
                      CollaborationConflictCategory::MissingTargetArtifact,
                      "operation target artifact is not registered");
    }

    const std::string resourceId =
        MakeArtifactResourceId(transaction.targetArtifactKind, transaction.targetPath);
    const SagaShared::Collaboration::ResourceLock* lock = FindLock(resourceId);
    if (lock != nullptr && lock->participant.value != transaction.actorId)
    {
        return Reject(std::move(transaction),
                      CollaborationConflictCategory::LockedArtifact,
                      "operation target artifact is locked by another actor");
    }

    if (transaction.baseSourceHash.has_value() &&
        *transaction.baseSourceHash != artifact->sourceHash)
    {
        return Reject(std::move(transaction),
                      CollaborationConflictCategory::StaleSourceHash,
                      "operation base source hash does not match target artifact");
    }

    if (transaction.operationKind == CollaborationOperationKind::ReviewPatchEvaluation)
    {
        const auto it = std::find_if(m_transactions.begin(), m_transactions.end(),
            [&transaction](const SemanticTransaction& item)
            {
                return item.operationKind == CollaborationOperationKind::ReviewPatchEvaluation &&
                       item.targetArtifactKind == transaction.targetArtifactKind &&
                       item.targetPath == transaction.targetPath &&
                       item.actorId != transaction.actorId;
            });
        if (it != m_transactions.end())
        {
            return Reject(std::move(transaction),
                          CollaborationConflictCategory::SamePatchEvaluationReviewedByMultipleActors,
                          "patch evaluation was already reviewed by another actor");
        }
    }

    transaction.status = CollaborationTransactionStatus::Accepted;
    m_transactions.push_back(transaction);
    ++m_nextSequence;
    return TransactionSubmission{transaction, true, std::nullopt};
}

bool LocalCollaborationMetadataStore::RecordPresenceGestureAsTransaction(
    const ArtifactPresence& presence)
{
    UpdatePresence(presence);
    return false;
}

const std::vector<CollaborationActor>& LocalCollaborationMetadataStore::Actors() const noexcept
{
    return m_actors;
}

const std::vector<ArtifactPresence>& LocalCollaborationMetadataStore::Presence() const noexcept
{
    return m_presence;
}

const std::vector<SagaShared::Collaboration::ResourceLock>&
LocalCollaborationMetadataStore::Locks() const noexcept
{
    return m_locks;
}

const std::vector<ArtifactComment>&
LocalCollaborationMetadataStore::Comments() const noexcept
{
    return m_comments;
}

const std::vector<ReviewRequest>&
LocalCollaborationMetadataStore::ReviewRequests() const noexcept
{
    return m_reviews;
}

const std::vector<DangerousOperationCheck>&
LocalCollaborationMetadataStore::DangerousOperationChecks() const noexcept
{
    return m_dangerousOperationChecks;
}

const std::vector<SemanticTransaction>&
LocalCollaborationMetadataStore::Transactions() const noexcept
{
    return m_transactions;
}

const std::vector<CollaborationConflict>&
LocalCollaborationMetadataStore::Conflicts() const noexcept
{
    return m_conflicts;
}

TeamRoomDashboardView LocalCollaborationMetadataStore::BuildDashboardView() const
{
    TeamRoomDashboardView dashboard;
    dashboard.actors = m_actors;
    dashboard.presence = m_presence;
    dashboard.locks = m_locks;
    dashboard.recentTransactions = m_transactions;
    dashboard.conflicts = m_conflicts;
    dashboard.comments = m_comments;
    dashboard.reviewQueue = m_reviews;
    dashboard.dangerousOperationChecks = m_dangerousOperationChecks;
    dashboard.packageStatusPlaceholder = "Deferred";
    dashboard.publishStatusPlaceholder = "Deferred";

    for (const CollaborationArtifactRecord& artifact : m_artifacts)
    {
        if (artifact.kind == CollaborationArtifactKind::DiagnosticsReview)
        {
            dashboard.diagnosticsReviewStatus.push_back(artifact);
        }
    }
    return dashboard;
}

bool LocalCollaborationMetadataStore::HasActor(const std::string& actorId) const
{
    return std::any_of(m_actors.begin(), m_actors.end(),
        [&actorId](const CollaborationActor& actor)
        {
            return actor.actorId == actorId;
        });
}

const CollaborationArtifactRecord* LocalCollaborationMetadataStore::FindArtifact(
    CollaborationArtifactKind kind,
    const std::string& path) const
{
    auto it = std::find_if(m_artifacts.begin(), m_artifacts.end(),
        [kind, &path](const CollaborationArtifactRecord& artifact)
        {
            return artifact.kind == kind && artifact.path == path;
        });
    return it == m_artifacts.end() ? nullptr : &(*it);
}

const SagaShared::Collaboration::ResourceLock* LocalCollaborationMetadataStore::FindLock(
    const std::string& resourceId) const
{
    auto it = std::find_if(m_locks.begin(), m_locks.end(),
        [&resourceId](const SagaShared::Collaboration::ResourceLock& lock)
        {
            return lock.resourceId == resourceId;
        });
    return it == m_locks.end() ? nullptr : &(*it);
}

CollaborationConflict LocalCollaborationMetadataStore::MakeConflict(
    const SemanticTransaction& transaction,
    CollaborationConflictCategory category,
    std::string reason) const
{
    CollaborationConflict conflict;
    conflict.conflictId = "conflict:" + transaction.operationId + ":" + ToString(category);
    conflict.category = category;
    conflict.operationId = transaction.operationId;
    conflict.actorId = transaction.actorId;
    conflict.targetArtifactKind = transaction.targetArtifactKind;
    conflict.targetPath = transaction.targetPath;
    conflict.reason = std::move(reason);
    return conflict;
}

TransactionSubmission LocalCollaborationMetadataStore::Reject(
    SemanticTransaction transaction,
    CollaborationConflictCategory category,
    std::string reason)
{
    transaction.status = CollaborationTransactionStatus::Rejected;
    CollaborationConflict conflict = MakeConflict(transaction, category, std::move(reason));
    m_conflicts.push_back(conflict);
    return TransactionSubmission{transaction, false, conflict};
}

} // namespace SagaCollaboration

