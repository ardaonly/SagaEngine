// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "SagaEditor/Documents/Document.h"
#include "SagaEditor/Processes/ManagedToolRuntime.h"
#include "SagaEditor/Processes/ToolProcessRunner.h"

namespace SagaEditor
{
struct ScriptWorkerInput
{
    std::filesystem::path path;
    std::string hash;
};

struct ScriptWorkerSource
{
    std::filesystem::path relativePath;
    std::string hash;
};

struct ScriptWorkerRuntimeBridge
{
    std::string logicalReferenceId;
    ScriptWorkerInput descriptor;
    ScriptWorkerInput assembly;
    std::string targetFramework;
    std::string runtimeCompatibility;
    std::string trust;
};

struct ScriptWorkerRequest
{
    std::string operationId;
    std::string projectId;
    ScriptWorkerInput project;
    std::string sourceRevision;
    std::vector<ScriptWorkerSource> sources;
    ScriptWorkerInput packageLock;
    std::string packageGenerationId;
    std::string packageGenerationHash;
    std::string packageGenerationManifestHash;
    std::string packageGraphHash;
    ScriptWorkerInput referenceManifest;
    ScriptWorkerInput apiSurfaceManifest;
    ScriptWorkerRuntimeBridge runtimeBridge;
    std::vector<std::string> operations{"analyze"};
    std::filesystem::path outputRoot;
    std::filesystem::path transportRoot;
    std::function<bool()> cancellationRequested;
    std::function<void(std::string_view)> progress;
};

struct ScriptWorkerResult
{
    bool passed = false;
    bool cancelled = false;
    bool stale = false;
    std::string status;
    std::string failureClass;
    std::string operationId;
    std::string sourceRevision;
    std::string currentManifestHash;
    std::string packageGenerationId;
    std::string packageGenerationHash;
    std::string packageGenerationManifestHash;
    std::string packageGraphHash;
    std::vector<DocumentDiagnostic> diagnostics;
    std::vector<std::string> progressStages;
    ToolProcessResult process;
};

class SagaScriptWorkerClient
{
public:
    SagaScriptWorkerClient(ManagedToolRuntimeOptions runtime = {},
        std::chrono::milliseconds timeout = std::chrono::minutes(2));
    [[nodiscard]] ScriptWorkerResult Run(const ScriptWorkerRequest& request) const;
private:
    ManagedToolRuntime runtime_;
    std::chrono::milliseconds timeout_;
};
}
