// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/VisualBlocks/ReadOnlyScriptProjection.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace SagaEditor
{
enum class ScriptProjectionState
{
    NoDocument,
    DirtyNeedsSave,
    Analyzing,
    Current,
    Stale,
    Failed,
    Cancelled,
    ProjectClosed,
    PackageGenerationChanged,
};

struct ScriptProjectionRequestIdentity final
{
    std::uint64_t workspaceGeneration = 0;
    std::string savedDiskHash;
    std::uint64_t documentRevision = 0;
    std::string projectHash;
    std::string packageGenerationId;
    std::string packageGenerationHash;
    std::string operationId;

    bool operator==(const ScriptProjectionRequestIdentity&) const = default;
};

struct ScriptProjectionSnapshot final
{
    ScriptProjectionState state = ScriptProjectionState::NoDocument;
    ScriptProjectionRequestIdentity identity;
    std::string artifactHash;
    std::string diagnostic;
    std::shared_ptr<const VisualBlocks::ReadOnlyScriptProjection> projection;
};

/// Qt-free publication barrier for saved-source projection results.
class ScriptProjectionSession final
{
public:
    using ChangeSubscription = std::uint64_t;
    using ChangeCallback = std::function<void(const ScriptProjectionSnapshot&)>;
    static constexpr ChangeSubscription InvalidChangeSubscription = 0;
    [[nodiscard]] const ScriptProjectionSnapshot& Snapshot() const noexcept { return snapshot_; }
    void NoDocument();
    void Dirty();
    void Begin(ScriptProjectionRequestIdentity identity);
    [[nodiscard]] bool Publish(const ScriptProjectionRequestIdentity& identity,
        std::string artifactHash,
        std::shared_ptr<const VisualBlocks::ReadOnlyScriptProjection> projection);
    void Fail(const ScriptProjectionRequestIdentity& identity, std::string diagnostic);
    void Cancel();
    void ProjectClosed();
    void PackageGenerationChanged(std::string generationId, std::string generationHash);
    [[nodiscard]] ChangeSubscription Subscribe(ChangeCallback callback);
    bool Unsubscribe(ChangeSubscription subscription);

private:
    void Notify();
    ScriptProjectionSnapshot snapshot_;
    std::map<ChangeSubscription, ChangeCallback> callbacks_;
    ChangeSubscription nextSubscription_ = 1;
};
}
