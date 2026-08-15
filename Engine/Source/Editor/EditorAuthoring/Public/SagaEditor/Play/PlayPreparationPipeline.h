// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Documents/Document.h"
#include "SagaEditor/Play/PlaySourceSnapshot.h"
#include "SagaEditor/Project/SagaProjectKitClient.h"
#include "SagaEditor/Scripting/ScriptAnalysisCoordinator.h"

#include <filesystem>
#include <functional>
#include <map>
#include <utility>

namespace SagaEditor
{
class EditorDocumentWorkspace;

struct PlayPreparationRequest
{
    PlayPreparationRequest() = default;
    PlayPreparationRequest(std::string sessionId,
        std::filesystem::path executable,
        std::function<bool()> cancellation = {},
        std::function<void(std::string_view)> progressCallback = {},
        std::function<void()> publicationBarrier = {},
        std::filesystem::path capturedRoot = {})
        : playSessionId(std::move(sessionId)),
          runtimeExecutable(std::move(executable)),
          cancellationRequested(std::move(cancellation)),
          progress(std::move(progressCallback)),
          beforeLaunchPublication(std::move(publicationBarrier)),
          capturedRuntimeRoot(std::move(capturedRoot))
    {
    }

    std::string playSessionId;
    std::filesystem::path runtimeExecutable;
    std::function<bool()> cancellationRequested;
    std::function<void(std::string_view)> progress;
    /// Deterministic publication barrier used by concurrency/freshness tests.
    /// Production callers leave this empty.
    std::function<void()> beforeLaunchPublication;
    /// Additional owned root for a captured standalone runtime. Normal Editor
    /// Play leaves this empty and trusts only the project and engine roots.
    std::filesystem::path capturedRuntimeRoot;
};

struct PlayPreparationResult
{
    bool passed = false;
    bool cancelled = false;
    bool stale = false;
    std::string playSessionId;
    std::filesystem::path buildRoot;
    std::filesystem::path canonicalManifestPath;
    std::string canonicalManifestHash;
    std::filesystem::path localEnvelopePath;
    std::filesystem::path resultPath;
    std::filesystem::path stopPath;
    std::map<std::string, std::string> canonicalHashes;
    std::vector<DocumentDiagnostic> diagnostics;
    ProjectToolResult projectValidation;
    ScriptAnalysisResult scriptAnalysis;
    ToolProcessResult contentProcess;
};

/// Coordinates source owners into a saved-source Play generation. It does
/// not parse project, compile content, resolve packages, or compile C# itself.
class PlayPreparationPipeline
{
public:
    explicit PlayPreparationPipeline(ScriptAnalysisToolchain toolchain = {});
    [[nodiscard]] PlayPreparationResult Prepare(
        EditorDocumentWorkspace& workspace,
        const PlayPreparationRequest& request) const;
    [[nodiscard]] PlayPreparationResult Prepare(
        const PlaySourceSnapshot& snapshot,
        const PlayPreparationRequest& request) const;

private:
    ScriptAnalysisToolchain toolchain_;
};
} // namespace SagaEditor
