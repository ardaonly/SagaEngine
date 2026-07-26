// SPDX-License-Identifier: MPL-2.0

#include "SagaEditor/Scripting/ScriptProjectionSession.h"

#include <utility>

namespace SagaEditor
{
void ScriptProjectionSession::NoDocument()
{ snapshot_ = {}; }

void ScriptProjectionSession::Dirty()
{
    snapshot_.state = ScriptProjectionState::DirtyNeedsSave;
    snapshot_.projection.reset();
    snapshot_.artifactHash.clear();
}

void ScriptProjectionSession::Begin(ScriptProjectionRequestIdentity identity)
{
    snapshot_ = {};
    snapshot_.state = ScriptProjectionState::Analyzing;
    snapshot_.identity = std::move(identity);
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
    return true;
}

void ScriptProjectionSession::Fail(const ScriptProjectionRequestIdentity& identity,
    std::string diagnostic)
{
    if (snapshot_.state != ScriptProjectionState::Analyzing || snapshot_.identity != identity) return;
    snapshot_.state = ScriptProjectionState::Failed;
    snapshot_.diagnostic = std::move(diagnostic);
}

void ScriptProjectionSession::Cancel()
{
    if (snapshot_.state == ScriptProjectionState::Analyzing)
        snapshot_.state = ScriptProjectionState::Cancelled;
}

void ScriptProjectionSession::ProjectClosed()
{
    snapshot_.state = ScriptProjectionState::ProjectClosed;
    snapshot_.projection.reset();
}

void ScriptProjectionSession::PackageGenerationChanged(std::string generationId,
    std::string generationHash)
{
    if (snapshot_.identity.packageGenerationId == generationId &&
        snapshot_.identity.packageGenerationHash == generationHash) return;
    snapshot_.state = ScriptProjectionState::PackageGenerationChanged;
    snapshot_.projection.reset();
}
}
