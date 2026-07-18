// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Documents/Document.h"
#include "SagaEditor/Play/PlaySourceSnapshot.h"
#include "SagaEditor/Project/SagaProjectKitClient.h"
#include "SagaEditor/Scripting/ScriptAnalysisCoordinator.h"

#include <filesystem>
#include <functional>
#include <map>

namespace SagaEditor
{
class EditorDocumentWorkspace;

struct PlayPreparationRequest
{
    std::string playSessionId;
    std::filesystem::path runtimeExecutable;
    std::function<bool()> cancellationRequested;
    std::function<void(std::string_view)> progress;
    /// Deterministic publication barrier used by concurrency/freshness tests.
    /// Production callers leave this empty.
    std::function<void()> beforeLaunchPublication;
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

/// Coordinates Campaign 1 owners into a saved-source Play generation. It does
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
