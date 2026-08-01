// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Scripting/ScriptProjectionSession.h"

#include <utility>

namespace SagaEditor
{
void ScriptProjectionSession::NoDocument()
{ snapshot_ = {}; Notify(); }

void ScriptProjectionSession::Dirty(std::uint64_t documentRevision, std::string sourceHash)
{
    snapshot_.state = ScriptProjectionState::DirtyNeedsSave;
    snapshot_.projection.reset();
    snapshot_.artifactHash.clear();
    snapshot_.identity.documentRevision = documentRevision;
    snapshot_.identity.savedDiskHash = std::move(sourceHash);
    Notify();
}

bool ScriptProjectionSession::PublishUnsaved(std::uint64_t documentRevision,
    const std::string& sourceHash,
    std::shared_ptr<const VisualBlocks::ReadOnlyScriptProjection> projection)
{
    if (snapshot_.state != ScriptProjectionState::DirtyNeedsSave ||
        snapshot_.identity.documentRevision != documentRevision ||
        snapshot_.identity.savedDiskHash != sourceHash || !projection ||
        projection->SourceRevision() != std::to_string(documentRevision) ||
        projection->ArtifactHash().empty())
        return false;
    snapshot_.state = ScriptProjectionState::Current;
    snapshot_.artifactHash = projection->ArtifactHash();
    snapshot_.projection = std::move(projection);
    Notify();
    return true;
}

void ScriptProjectionSession::Begin(ScriptProjectionRequestIdentity identity)
{
    snapshot_ = {};
    snapshot_.state = ScriptProjectionState::Analyzing;
    snapshot_.identity = std::move(identity);
    Notify();
}

bool ScriptProjectionSession::Publish(const ScriptProjectionRequestIdentity& identity,
    std::string artifactHash,
    std::shared_ptr<const VisualBlocks::ReadOnlyScriptProjection> projection)
{
    if (snapshot_.state != ScriptProjectionState::Analyzing ||
        snapshot_.identity != identity || !projection || artifactHash.empty() ||
        projection->ArtifactHash() != artifactHash)
    {
        if (snapshot_.state == ScriptProjectionState::Analyzing)
            snapshot_.state = ScriptProjectionState::Stale;
        return false;
    }
    snapshot_.state = ScriptProjectionState::Current;
    snapshot_.artifactHash = std::move(artifactHash);
    snapshot_.projection = std::move(projection);
    Notify();
    return true;
}

void ScriptProjectionSession::Fail(const ScriptProjectionRequestIdentity& identity,
    std::string diagnostic)
{
    if (snapshot_.state != ScriptProjectionState::Analyzing || snapshot_.identity != identity) return;
    snapshot_.state = ScriptProjectionState::Failed;
    snapshot_.diagnostic = std::move(diagnostic);
    Notify();
}

void ScriptProjectionSession::Cancel()
{
    if (snapshot_.state == ScriptProjectionState::Analyzing)
    {
        snapshot_.state = ScriptProjectionState::Cancelled;
        Notify();
    }
}

void ScriptProjectionSession::ProjectClosed()
{
    snapshot_.state = ScriptProjectionState::ProjectClosed;
    snapshot_.projection.reset();
    Notify();
}

void ScriptProjectionSession::PackageGenerationChanged(std::string generationId,
    std::string generationHash)
{
    if (snapshot_.identity.packageGenerationId == generationId &&
        snapshot_.identity.packageGenerationHash == generationHash) return;
    snapshot_.state = ScriptProjectionState::PackageGenerationChanged;
    snapshot_.projection.reset();
    Notify();
}

ScriptProjectionSession::ChangeSubscription ScriptProjectionSession::Subscribe(
    ChangeCallback callback)
{
    if (!callback) return InvalidChangeSubscription;
    const auto subscription = nextSubscription_++;
    callbacks_.emplace(subscription, std::move(callback));
    callbacks_.at(subscription)(snapshot_);
    return subscription;
}

bool ScriptProjectionSession::Unsubscribe(ChangeSubscription subscription)
{ return callbacks_.erase(subscription) != 0; }

void ScriptProjectionSession::Notify()
{
    const auto callbacks = callbacks_;
    for (const auto& [id, callback] : callbacks)
    {
        (void)id;
        if (callback) callback(snapshot_);
    }
}
}
