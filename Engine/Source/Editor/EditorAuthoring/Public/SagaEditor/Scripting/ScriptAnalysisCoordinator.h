// SPDX-FileCopyrightText: 2026 Arda Koyuncu
// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Documents/Document.h"
#include "SagaEditor/Play/PlaySourceSnapshot.h"
#include "SagaEditor/Scripting/SagaScriptWorkerClient.h"

#include <chrono>
#include <filesystem>
#include <functional>

namespace SagaEditor
{
class EditorDocumentWorkspace;

struct ScriptAnalysisToolchain
{
    std::filesystem::path sagaProjectKit;
    std::filesystem::path sagaPackager;
    std::filesystem::path sagaScript;
    std::filesystem::path engineRoot;
    std::filesystem::path runtimeBridge;
    std::chrono::milliseconds timeout{std::chrono::minutes(2)};
};

struct ScriptAnalysisRequest
{
    std::function<bool()> cancellationRequested;
    std::function<void(std::string_view)> progress;
};

struct ScriptAnalysisResult
{
    bool passed = false;
    bool cancelled = false;
    bool stale = false;
    std::string operationId;
    std::vector<DocumentDiagnostic> diagnostics;
    ToolProcessResult packageProcess;
    ScriptWorkerResult worker;
};

/// Qt-free orchestration of Campaign 1 package-generation verification and the
/// SagaScript worker. It consumes saved canonical documents; it neither
/// resolves packages nor acts as a compiler.
class ScriptAnalysisCoordinator
{
public:
    explicit ScriptAnalysisCoordinator(ScriptAnalysisToolchain toolchain = {});
    [[nodiscard]] ScriptAnalysisResult Analyze(
        EditorDocumentWorkspace& workspace,
        const ScriptAnalysisRequest& request = {}) const;
    [[nodiscard]] ScriptAnalysisResult Analyze(
        const PlaySourceSnapshot& snapshot,
        const ScriptAnalysisRequest& request = {}) const;

private:
    ScriptAnalysisToolchain toolchain_;
};

[[nodiscard]] ScriptAnalysisToolchain DiscoverScriptAnalysisToolchain();
} // namespace SagaEditor
